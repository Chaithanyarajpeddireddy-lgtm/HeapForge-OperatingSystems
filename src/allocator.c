#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif

#include "heapforge.h"
#include "allocator.h"

#include <unistd.h>
#include <string.h>
#include <errno.h>

/* Global internal allocator state for Milestone 1 bump allocator */
static struct {
    bool initialized;
    void *initial_break;
    void *current_break;
    size_t total_sbrk_bytes;
    size_t alloc_count;
} g_allocator = {
    .initialized = false,
    .initial_break = NULL,
    .current_break = NULL,
    .total_sbrk_bytes = 0,
    .alloc_count = 0
};

/**
 * @brief Ensures the initial program break is aligned to 16 bytes.
 *
 * @return true on success, false if sbrk failed.
 */
static bool init_allocator_if_needed(void) {
    if (g_allocator.initialized) {
        return true;
    }

    /* Query current program break */
    void *cur = sbrk(0);
    if (cur == (void *)-1) {
        return false;
    }

    /* If current break is not 16-byte aligned, pad to alignment boundary */
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
    return true;
}

void *heapforge_malloc(size_t size) {
    /* Policy: malloc(0) returns NULL */
    if (size == 0) {
        return NULL;
    }

    /* Guard against integer overflow during alignment */
    if (size > HEAPFORGE_MAX_ALLOC_SIZE) {
        errno = ENOMEM;
        return NULL;
    }

    /* Round up to 16-byte alignment */
    size_t aligned_size = HEAPFORGE_ALIGN_UP(size);

    /* Guard against exceeding signed intptr_t range required by sbrk() */
    if (aligned_size > (size_t)INTPTR_MAX) {
        errno = ENOMEM;
        return NULL;
    }

    /* Ensure initial break is established and aligned */
    if (!init_allocator_if_needed()) {
        errno = ENOMEM;
        return NULL;
    }

    /*
     * Milestone 1: Bump Allocator
     * Request memory from the OS kernel by advancing the brk pointer.
     * In sbrk(intptr_t increment), the return value on success is the
     * PREVIOUS program break, which marks the start of our allocated chunk.
     */
    void *chunk = sbrk((intptr_t)aligned_size);
    if (chunk == (void *)-1) {
        errno = ENOMEM;
        return NULL;
    }

    g_allocator.current_break = sbrk(0);
    g_allocator.total_sbrk_bytes += aligned_size;
    g_allocator.alloc_count++;

    return chunk;
}

void heapforge_free(void *ptr) {
    /*
     * In Milestone 1 bump allocator, memory is monotonically grown.
     * Full free-list tracking and reuse will be introduced in Milestone 2
     * alongside block headers, footers, and boundary tags.
     */
    (void)ptr;
}

void *heapforge_calloc(size_t nmemb, size_t size) {
    /* Check for multiplication overflow: nmemb * size */
    if (nmemb != 0 && size > SIZE_MAX / nmemb) {
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

    /*
     * Milestone 1 note: Without block headers (Milestone 2), old block size
     * cannot be inspected. Allocate new memory block.
     */
    void *new_ptr = heapforge_malloc(size);
    if (new_ptr == NULL) {
        return NULL;
    }

    /*
     * In Milestone 2+, memcpy will copy min(old_size, size) based on block header.
     * For Milestone 1 bump allocator, return the newly allocated block.
     */
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
    /* If initialized, attempt to roll back the break pointer for testing */
    if (g_allocator.initialized && g_allocator.initial_break != NULL) {
        void *cur = sbrk(0);
        if (cur != (void *)-1 && cur > g_allocator.initial_break) {
            intptr_t diff = (intptr_t)((char *)cur - (char *)g_allocator.initial_break);
            sbrk(-diff);
        }
    }
    g_allocator.initialized = false;
    g_allocator.initial_break = NULL;
    g_allocator.current_break = NULL;
    g_allocator.total_sbrk_bytes = 0;
    g_allocator.alloc_count = 0;
}
