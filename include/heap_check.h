#ifndef HEAPFORGE_HEAP_CHECK_H
#define HEAPFORGE_HEAP_CHECK_H

#include <stdbool.h>

/**
 * @brief Performs comprehensive heap consistency verification.
 *
 * Verifies:
 * 1. Block alignment (16-byte alignment of headers and payloads).
 * 2. Block sizes (size >= MIN_BLOCK_SIZE and 16-byte aligned).
 * 3. Header/footer symmetry (size_and_flags and canaries match).
 * 4. Physical boundaries (no overlapping, blocks fit within heap break).
 * 5. Free list integrity (acyclic, bidirectional links, no allocated blocks in free list).
 * 6. Segregated class accuracy (every free block resides in correct size_to_class()).
 * 7. Block count cross-check (physical free blocks match free-list free blocks).
 *
 * @return 0 on success (PASS), non-zero error code on corruption.
 */
int heap_check(void);

/**
 * @brief Prints a diagnostic snapshot of all physical blocks in the heap.
 */
void heap_dump(void);

#endif /* HEAPFORGE_HEAP_CHECK_H */
