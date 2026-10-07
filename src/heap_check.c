#include "heap_check.h"
#include "allocator.h"
#include "block.h"
#include "freelist.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

int heap_check(void) {
    heapforge_state_t st;
    heapforge_get_state(&st);

    void *heap_start = st.initial_break;
    void *heap_end = st.current_break;

    printf("\n========== HEAP CHECK ==========\n");

    if (heap_start == NULL || heap_start == heap_end) {
        printf("Heap status    : EMPTY (Uninitialized / No Allocations)\n");
        printf("HEAP CHECK: PASS\n");
        printf("================================\n\n");
        return 0;
    }

    size_t total_blocks = 0;
    size_t free_blocks_physical = 0;
    size_t allocated_blocks = 0;
    size_t bytes_allocated = 0;
    size_t bytes_free = 0;

    bool pass_alignment = true;
    bool pass_headers = true;
    bool pass_footers = true;
    bool pass_overlap = true;
    bool pass_freelist = true;
    bool pass_classes = true;

    block_header_t *curr = (block_header_t *)heap_start;

    /* 1. Physical Heap Traversal */
    while ((char *)curr < (char *)heap_end) {
        total_blocks++;

        /* Alignment check */
        if (!HEAPFORGE_IS_ALIGNED(curr)) {
            fprintf(stderr, "HeapCheck FAIL: Unaligned block header at %p\n", (void *)curr);
            pass_alignment = false;
            break;
        }

        /* Header canary check */
        if (curr->magic != BLOCK_MAGIC_HEADER) {
            fprintf(stderr, "HeapCheck FAIL: Corrupted header canary at %p (magic=0x%lx)\n",
                    (void *)curr, (unsigned long)curr->magic);
            pass_headers = false;
            break;
        }

        size_t sz = block_get_size(curr);
        if (sz < MIN_BLOCK_SIZE || !HEAPFORGE_IS_ALIGNED(sz)) {
            fprintf(stderr, "HeapCheck FAIL: Invalid block size %zu at %p\n", sz, (void *)curr);
            pass_headers = false;
            break;
        }

        /* Check physical boundary limit */
        char *block_end = (char *)curr + sz;
        if (block_end > (char *)heap_end) {
            fprintf(stderr, "HeapCheck FAIL: Block at %p extends beyond heap end (%p > %p)\n",
                    (void *)curr, (void *)block_end, heap_end);
            pass_overlap = false;
            break;
        }

        /* Footer boundary tag symmetry check */
        block_footer_t *ftr = block_get_footer(curr);
        if (ftr->magic != BLOCK_MAGIC_FOOTER) {
            fprintf(stderr, "HeapCheck FAIL: Corrupted footer canary at %p\n", (void *)ftr);
            pass_footers = false;
            break;
        }

        if (ftr->size_and_flags != curr->size_and_flags) {
            fprintf(stderr, "HeapCheck FAIL: Header/footer mismatch at block %p (hdr=0x%zx, ftr=0x%zx)\n",
                    (void *)curr, curr->size_and_flags, ftr->size_and_flags);
            pass_footers = false;
            break;
        }

        if (block_is_allocated(curr)) {
            allocated_blocks++;
            bytes_allocated += sz;

            /* Allocated block must NOT appear in free list */
            if (freelist_contains(curr)) {
                fprintf(stderr, "HeapCheck FAIL: Allocated block %p found in free list!\n", (void *)curr);
                pass_freelist = false;
            }
        } else {
            free_blocks_physical++;
            bytes_free += sz;

            /* Free block MUST appear in free list */
            if (!freelist_contains(curr)) {
                fprintf(stderr, "HeapCheck FAIL: Free block %p missing from free list!\n", (void *)curr);
                pass_freelist = false;
            }

            /* Free block must reside in correct size class */
            int expected_class = size_to_class(sz);
            block_header_t *head = freelist_get_head(expected_class);
            bool found_in_class = false;
            while (head != NULL) {
                if (head == curr) {
                    found_in_class = true;
                    break;
                }
                free_node_t *node = block_get_free_node(head);
                head = node->next_free;
            }
            if (!found_in_class) {
                fprintf(stderr, "HeapCheck FAIL: Block %p of size %zu not in expected size class %d\n",
                        (void *)curr, sz, expected_class);
                pass_classes = false;
            }
        }

        curr = (block_header_t *)block_end;
    }

    if ((char *)curr != (char *)heap_end) {
        fprintf(stderr, "HeapCheck FAIL: Physical heap end mismatch! curr=%p != heap_end=%p\n",
                (void *)curr, heap_end);
        pass_overlap = false;
    }

    /* 2. Segregated Free List Verification */
    size_t free_blocks_freelist = 0;
    for (int c = 0; c < NUM_SIZE_CLASSES; c++) {
        block_header_t *fcurr = freelist_get_head(c);
        block_header_t *fprev = NULL;

        while (fcurr != NULL) {
            free_blocks_freelist++;
            if (free_blocks_freelist > free_blocks_physical + 10) {
                fprintf(stderr, "HeapCheck FAIL: Cycle detected in free list class %d!\n", c);
                pass_freelist = false;
                break;
            }

            if (block_is_allocated(fcurr)) {
                fprintf(stderr, "HeapCheck FAIL: Allocated block %p present in free list class %d!\n",
                        (void *)fcurr, c);
                pass_freelist = false;
            }

            free_node_t *node = block_get_free_node(fcurr);
            if (node->prev_free != fprev) {
                fprintf(stderr, "HeapCheck FAIL: Inconsistent prev link in free list class %d at block %p\n",
                        c, (void *)fcurr);
                pass_freelist = false;
            }

            fprev = fcurr;
            fcurr = node->next_free;
        }
    }

    if (free_blocks_physical != free_blocks_freelist) {
        fprintf(stderr, "HeapCheck FAIL: Free block count mismatch! Physical=%zu, FreeList=%zu\n",
                free_blocks_physical, free_blocks_freelist);
        pass_freelist = false;
    }

    /* 3. Output Diagnostics */
    printf("Blocks checked : %zu\n", total_blocks);
    printf("Free blocks    : %zu (%zu bytes)\n", free_blocks_physical, bytes_free);
    printf("Allocated      : %zu (%zu bytes)\n", allocated_blocks, bytes_allocated);
    printf("Alignment      : %s\n", pass_alignment ? "PASS" : "FAIL");
    printf("Headers        : %s\n", pass_headers ? "PASS" : "FAIL");
    printf("Footers        : %s\n", pass_footers ? "PASS" : "FAIL");
    printf("Free lists     : %s\n", pass_freelist ? "PASS" : "FAIL");
    printf("Size classes   : %s\n", pass_classes ? "PASS" : "FAIL");
    printf("Overlap        : %s\n", pass_overlap ? "PASS" : "FAIL");

    bool overall_pass = pass_alignment && pass_headers && pass_footers &&
                        pass_freelist && pass_classes && pass_overlap;

    printf("HEAP CHECK: %s\n", overall_pass ? "PASS" : "FAIL");
    printf("================================\n\n");

    return overall_pass ? 0 : 1;
}

void heap_dump(void) {
    heapforge_state_t st;
    heapforge_get_state(&st);

    printf("\n========== HEAP DUMP ==========\n");
    printf("Initial Break : %p\n", st.initial_break);
    printf("Current Break : %p\n", st.current_break);
    printf("Total sbrk    : %zu bytes\n", st.total_sbrk_bytes);

    if (st.initial_break == NULL || st.initial_break == st.current_break) {
        printf("(Heap is currently empty)\n");
        printf("===============================\n\n");
        return;
    }

    block_header_t *curr = (block_header_t *)st.initial_break;
    size_t index = 0;

    while ((char *)curr < (char *)st.current_break) {
        size_t sz = block_get_size(curr);
        bool alloc = block_is_allocated(curr);
        printf("BLOCK %zu\n", index++);
        printf("  Address : %p\n", (void *)curr);
        printf("  Size    : %zu (Payload: %zu)\n", sz, block_get_payload_size(curr));
        printf("  State   : %s\n", alloc ? "ALLOCATED" : "FREE");
        if (!alloc) {
            printf("  Class   : %d\n", size_to_class(sz));
        }
        curr = (block_header_t *)((char *)curr + sz);
    }

    printf("===============================\n\n");
}
