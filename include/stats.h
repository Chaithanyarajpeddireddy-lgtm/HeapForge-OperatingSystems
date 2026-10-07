#ifndef HEAPFORGE_STATS_H
#define HEAPFORGE_STATS_H

#include "freelist.h"
#include <stddef.h>
#include <stdbool.h>

/**
 * @brief Allocator runtime metrics and fragmentation tracking.
 */
typedef struct heapforge_stats {
    size_t bytes_in_use;           /**< Payload bytes currently allocated */
    size_t peak_bytes_in_use;      /**< High-water mark of allocated payload bytes */
    size_t bytes_from_os;          /**< Total bytes obtained from kernel via sbrk/mmap */
    size_t total_allocations;      /**< Cumulative number of malloc/calloc calls */
    size_t total_frees;            /**< Cumulative number of free calls */
    size_t total_mmap_allocs;      /**< Cumulative mmap allocations */
    size_t total_munmap_frees;     /**< Cumulative munmap deallocations */
    size_t total_splits;           /**< Cumulative number of block splits */
    size_t total_coalesces;        /**< Cumulative number of block coalesces */
    size_t active_alloc_count;     /**< Currently active (unfreed) blocks */
    size_t free_block_count;       /**< Total free blocks currently available */
    size_t free_bytes;             /**< Total free bytes in free lists */
    size_t largest_free_block;     /**< Size of largest free block */
    size_t class_alloc_counts[NUM_SIZE_CLASSES]; /**< Allocations per size class */
    double external_fragmentation; /**< Fragmentation ratio [0.0, 1.0] */
} heapforge_stats_t;

/**
 * @brief Initializes or resets allocator statistics counters.
 */
void stats_init(void);

/**
 * @brief Records memory obtained from the OS kernel.
 */
void stats_on_os_growth(size_t bytes);

/**
 * @brief Records a successful allocation.
 */
void stats_on_alloc(size_t payload_size, size_t total_block_size, bool is_split);

/**
 * @brief Records a block free.
 */
void stats_on_free(size_t payload_size);

/**
 * @brief Records a block split event.
 */
void stats_on_split(void);

/**
 * @brief Records a block coalesce event.
 */
void stats_on_coalesce(void);

/**
 * @brief Records an mmap allocation.
 */
void stats_on_mmap(size_t size);

/**
 * @brief Records a munmap release.
 */
void stats_on_munmap(size_t size);

/**
 * @brief Populates stats struct with current snapshot.
 */
void heapforge_get_stats(heapforge_stats_t *stats);

/**
 * @brief Prints formatted allocator statistics and fragmentation metrics to stdout.
 */
void heapforge_print_stats(void);

/**
 * @brief Generates and prints a detailed heap leak report for outstanding allocations.
 */
void heapforge_leak_report(void);

#endif /* HEAPFORGE_STATS_H */
