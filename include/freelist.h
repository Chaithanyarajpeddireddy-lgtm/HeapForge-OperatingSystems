#ifndef HEAPFORGE_FREELIST_H
#define HEAPFORGE_FREELIST_H

#include "block.h"
#include <stddef.h>
#include <stdbool.h>

/**
 * @brief Initializes the free list foundation.
 */
void freelist_init(void);

/**
 * @brief Inserts a free block into the free list (O(1)).
 *
 * @param block Block header of the block to insert.
 */
void freelist_insert(block_header_t *block);

/**
 * @brief Removes a free block from the free list (O(1)).
 *
 * @param block Block header of the block to remove.
 */
void freelist_remove(block_header_t *block);

/**
 * @brief Finds a free block that can satisfy the requested total size (first-fit).
 *
 * @param required_total_size Total block size required (including overhead).
 * @return Pointer to a matching free block header, or NULL if none found.
 */
block_header_t *freelist_find_fit(size_t required_total_size);

/**
 * @brief Checks if a block is currently present in the free list.
 *
 * @param block Block to check.
 * @return true if present, false otherwise.
 */
bool freelist_contains(const block_header_t *block);

/**
 * @brief Returns the total number of free blocks currently tracked.
 */
size_t freelist_get_count(void);

/**
 * @brief Returns the head of the free list.
 */
block_header_t *freelist_get_head(void);

#endif /* HEAPFORGE_FREELIST_H */
