#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif

#include "heapforge.h"
#include "allocator.h"
#include "block.h"
#include "freelist.h"

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

/* Global internal allocator state */
static struct {
    bool initialized;
    void *initial_break;
    void *current_break;
    size_t total_sbrk_bytes;
    size_t alloc_count;
    size_t free_count;
} g_allocator = {
    .initialized = false,
    .initial_break = NULL,
    .current_break = NULL,
    .total_sbrk_bytes = 0,
    .alloc_count = 0,
    .free_count = 0
};

/**
 * @brief Ensures the initial program break is aligned to 16 bytes and free list is initialized.
 */
static bool init_allocator_if_needed(void) {
    if (g_allocator.initialized) {
        return true;
    }

    void *cur = sbrk(0);
    if (cur == (void *)-1) {
        return false;
    }

    uintptr_t addr = (uintptr_t)cur;
    uintptr_t misaligned = addr & (HEAPFORGE_ALIGNMENT - 1UL);
    if (misaligned != 0) {
        intptr_t padding = (intptr_t)(HEAPFORGE_ALIGNMENT - misaligned);
        void *padded = sbrk(padding);
        if (padded == (void *)-1) {
            return false;
        }
    }

    g_allocator.initial_break = sbrk(0);
    g_allocator.current_break = g_allocator.initial_break;
    g_allocator.initialized = true;

    freelist_init();
    return true;
}

void *heapforge_malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    if (size > HEAPFORGE_MAX_ALLOC_SIZE - BLOCK_OVERHEAD) {
        errno = ENOMEM;
        return NULL;
    }

    if (!init_allocator_if_needed()) {
        errno = ENOMEM;
        return NULL;
    }

    /* Compute total block size including Header and Footer */
    size_t payload_aligned = HEAPFORGE_ALIGN_UP(size);
    if (payload_aligned < MIN_PAYLOAD_SIZE) {
        payload_aligned = MIN_PAYLOAD_SIZE;
    }
    size_t total_size = BLOCK_OVERHEAD + payload_aligned;

    /* First, check free list for a reusable block */
    block_header_t *free_block = freelist_find_fit(total_size);
    if (free_block != NULL) {
        freelist_remove(free_block);
        block_header_t *remainder = block_split(free_block, total_size);
        if (remainder != NULL) {
            freelist_insert(remainder);
        } else {
            block_init(free_block, block_get_size(free_block), BLOCK_FLAG_ALLOCATED);
        }
        g_allocator.alloc_count++;
        return block_to_payload(free_block);
    }

    /* Guard against signed intptr_t range */
    if (total_size > (size_t)INTPTR_MAX) {
        errno = ENOMEM;
        return NULL;
    }

    /* Extend heap via sbrk */
    void *chunk = sbrk((intptr_t)total_size);
    if (chunk == (void *)-1) {
        errno = ENOMEM;
        return NULL;
    }

    block_header_t *hdr = (block_header_t *)chunk;
    block_init(hdr, total_size, BLOCK_FLAG_ALLOCATED);

    g_allocator.current_break = sbrk(0);
    g_allocator.total_sbrk_bytes += total_size;
    g_allocator.alloc_count++;

    return block_to_payload(hdr);
}

void heapforge_free(void *ptr) {
    if (ptr == NULL) {
        return;
    }

    /* Convert payload pointer back to block header */
    block_header_t *hdr = payload_to_block(ptr);

    /* Validate block integrity against heap boundaries and canaries */
    if (!block_validate(hdr, g_allocator.initial_break, g_allocator.current_break)) {
        fprintf(stderr, "HeapForge ERROR: Block corruption detected at %p!\n", (void *)hdr);
        return;
    }

    /* Detect double free */
    if (!block_is_allocated(hdr)) {
        fprintf(stderr, "HeapForge ERROR: Double free detected on block at %p!\n", (void *)hdr);
        return;
    }

    /* Mark block as free and insert into free list */
    size_t current_size = block_get_size(hdr);
    block_init(hdr, current_size, BLOCK_FLAG_FREE);
    freelist_insert(hdr);

    g_allocator.free_count++;
}

void *heapforge_calloc(size_t nmemb, size_t size) {
    if (nmemb == 0 || size == 0) {
        return NULL;
    }

    /* Check for multiplication overflow */
    if (size > SIZE_MAX / nmemb) {
        errno = ENOMEM;
        return NULL;
    }

    size_t total_size = nmemb * size;
    void *ptr = heapforge_malloc(total_size);
    if (ptr == NULL) {
        return NULL;
    }

    memset(ptr, 0, total_size);
    return ptr;
}

void *heapforge_realloc(void *ptr, size_t size) {
    /* Case A: ptr == NULL behaves identically to malloc(size) */
    if (ptr == NULL) {
        return heapforge_malloc(size);
    }

    /* Case B: size == 0 frees ptr and returns NULL */
    if (size == 0) {
        heapforge_free(ptr);
        return NULL;
    }

    block_header_t *hdr = payload_to_block(ptr);
    if (!block_validate(hdr, g_allocator.initial_break, g_allocator.current_break)) {
        fprintf(stderr, "HeapForge ERROR: realloc() on corrupted block %p!\n", ptr);
        return NULL;
    }

    size_t old_payload_size = block_get_payload_size(hdr);
    size_t current_size = block_get_size(hdr);

    size_t new_payload_aligned = HEAPFORGE_ALIGN_UP(size);
    if (new_payload_aligned < MIN_PAYLOAD_SIZE) {
        new_payload_aligned = MIN_PAYLOAD_SIZE;
    }
    size_t needed_total_size = BLOCK_OVERHEAD + new_payload_aligned;

    /* Case C: Shrinking or identical size */
    if (needed_total_size <= current_size) {
        size_t excess = current_size - needed_total_size;
        if (excess >= MIN_BLOCK_SIZE) {
            /* Split block and free remainder */
            block_init(hdr, needed_total_size, BLOCK_FLAG_ALLOCATED);
            block_header_t *rem = (block_header_t *)((char *)hdr + needed_total_size);
            block_init(rem, excess, BLOCK_FLAG_FREE);
            freelist_insert(rem);
        }
        return ptr;
    }

    /* Case D: Growing in place using adjacent free space */
    block_header_t *next_hdr = block_get_next_physical(hdr, g_allocator.current_break);
    if (next_hdr != NULL && !block_is_allocated(next_hdr)) {
        size_t next_size = block_get_size(next_hdr);
        size_t combined_size = current_size + next_size;

        if (combined_size >= needed_total_size) {
            /* Remove adjacent block from free list */
            freelist_remove(next_hdr);

            size_t leftover = combined_size - needed_total_size;
            if (leftover >= MIN_BLOCK_SIZE) {
                /* Set new size and place remaining excess into free list */
                block_init(hdr, needed_total_size, BLOCK_FLAG_ALLOCATED);
                block_header_t *rem = (block_header_t *)((char *)hdr + needed_total_size);
                block_init(rem, leftover, BLOCK_FLAG_FREE);
                freelist_insert(rem);
            } else {
                /* Absorb the entire adjacent block */
                block_init(hdr, combined_size, BLOCK_FLAG_ALLOCATED);
            }
            return ptr;
        }
    }

    /* Case E: Cannot grow in place -> allocate, copy, free */
    void *new_ptr = heapforge_malloc(size);
    if (new_ptr == NULL) {
        return NULL;
    }

    size_t copy_bytes = (old_payload_size < size) ? old_payload_size : size;
    memcpy(new_ptr, ptr, copy_bytes);
    heapforge_free(ptr);

    return new_ptr;
}

void heapforge_get_state(heapforge_state_t *state) {
    if (state == NULL) {
        return;
    }
    state->initial_break = g_allocator.initial_break;
    state->current_break = g_allocator.current_break;
    state->total_sbrk_bytes = g_allocator.total_sbrk_bytes;
    state->alloc_count = g_allocator.alloc_count;
}

void heapforge_reset(void) {
    if (g_allocator.initialized && g_allocator.initial_break != NULL) {
        void *cur = sbrk(0);
        if (cur != (void *)-1 && cur > g_allocator.initial_break) {
            intptr_t diff = (intptr_t)((char *)cur - (char *)g_allocator.initial_break);
            sbrk(-diff);
        }
    }
    freelist_init();
    g_allocator.initialized = false;
    g_allocator.initial_break = NULL;
    g_allocator.current_break = NULL;
    g_allocator.total_sbrk_bytes = 0;
    g_allocator.alloc_count = 0;
    g_allocator.free_count = 0;
}
