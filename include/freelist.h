#ifndef HEAPFORGE_FREELIST_H
#define HEAPFORGE_FREELIST_H

#include "block.h"
#include <stddef.h>
#include <stdbool.h>

/**
 * Segregated Free List Class Structure (13 classes):
 * Class 0:  48 - 64 bytes (MIN_BLOCK_SIZE is 48)
 * Class 1:  65 - 128 bytes
 * Class 2:  129 - 256 bytes
 * Class 3:  257 - 512 bytes
 * Class 4:  513 - 1024 bytes
 * Class 5:  1025 - 2048 bytes
 * Class 6:  2049 - 4096 bytes
 * Class 7:  4097 - 8192 bytes
 * Class 8:  8193 - 16384 bytes
 * Class 9:  16385 - 32768 bytes
 * Class 10: 32769 - 65536 bytes
 * Class 11: 65537 - 131072 bytes (128 KiB)
 * Class 12: > 131072 bytes
 */
#define NUM_SIZE_CLASSES 13

/**
 * @brief Maps a total block size to its corresponding segregated size class index.
 *
 * @param total_size Total block size in bytes (including overhead).
 * @return Size class index [0, NUM_SIZE_CLASSES - 1].
 */
int size_to_class(size_t total_size);

/**
 * @brief Initializes all segregated free lists.
 */
void freelist_init(void);

/**
 * @brief Inserts a free block into the appropriate segregated free list (O(1)).
 *
 * @param block Block header of the block to insert.
 */
void freelist_insert(block_header_t *block);

/**
 * @brief Removes a free block from its segregated free list (O(1)).
 *
 * @param block Block header of the block to remove.
 */
void freelist_remove(block_header_t *block);

/**
 * @brief Finds a free block that can satisfy the requested total size.
 *
 * Searches the target size class first, and falls back to larger classes.
 *
 * @param required_total_size Total block size required (including overhead).
 * @return Pointer to a matching free block header, or NULL if none found.
 */
block_header_t *freelist_find_fit(size_t required_total_size);

/**
 * @brief Checks if a block is present in any segregated free list.
 *
 * @param block Block to check.
 * @return true if present, false otherwise.
 */
bool freelist_contains(const block_header_t *block);

/**
 * @brief Returns total count of all free blocks across all size classes.
 */
size_t freelist_get_count(void);

/**
 * @brief Returns count of free blocks in a specific size class.
 */
size_t freelist_get_class_count(int cls);

/**
 * @brief Returns the head pointer of a specific size class free list.
 */
block_header_t *freelist_get_head(int cls);

#endif /* HEAPFORGE_FREELIST_H */
