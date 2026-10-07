#include "stats.h"
#include "allocator.h"
#include "block.h"
#include "freelist.h"

#include <stdio.h>
#include <string.h>

static heapforge_stats_t g_stats;

void stats_init(void) {
    memset(&g_stats, 0, sizeof(g_stats));
}

void stats_on_os_growth(size_t bytes) {
    g_stats.bytes_from_os += bytes;
}

void stats_on_alloc(size_t payload_size, size_t total_block_size, bool is_split) {
    g_stats.bytes_in_use += payload_size;
    if (g_stats.bytes_in_use > g_stats.peak_bytes_in_use) {
        g_stats.peak_bytes_in_use = g_stats.bytes_in_use;
    }

    g_stats.total_allocations++;
    g_stats.active_alloc_count++;

    int cls = size_to_class(total_block_size);
    if (cls >= 0 && cls < NUM_SIZE_CLASSES) {
        g_stats.class_alloc_counts[cls]++;
    }

    if (is_split) {
        g_stats.total_splits++;
    }
}

void stats_on_free(size_t payload_size) {
    if (g_stats.bytes_in_use >= payload_size) {
        g_stats.bytes_in_use -= payload_size;
    } else {
        g_stats.bytes_in_use = 0;
    }

    g_stats.total_frees++;
    if (g_stats.active_alloc_count > 0) {
        g_stats.active_alloc_count--;
    }
}

void stats_on_split(void) {
    g_stats.total_splits++;
}

void stats_on_coalesce(void) {
    g_stats.total_coalesces++;
}

void stats_on_mmap(size_t size) {
    g_stats.total_mmap_allocs++;
    g_stats.bytes_from_os += size;
}

void stats_on_munmap(size_t size) {
    g_stats.total_munmap_frees++;
    if (g_stats.bytes_from_os >= size) {
        g_stats.bytes_from_os -= size;
    }
}

void heapforge_get_stats(heapforge_stats_t *stats) {
    if (stats == NULL) {
        return;
    }

    /* Compute current free list metrics and fragmentation */
    size_t total_free_bytes = 0;
    size_t largest_free = 0;
    size_t free_count = 0;

    for (int c = 0; c < NUM_SIZE_CLASSES; c++) {
        block_header_t *curr = freelist_get_head(c);
        while (curr != NULL) {
            free_count++;
            size_t sz = block_get_size(curr);
            total_free_bytes += sz;
            if (sz > largest_free) {
                largest_free = sz;
            }
            free_node_t *node = block_get_free_node(curr);
            curr = node->next_free;
        }
    }

    g_stats.free_block_count = free_count;
    g_stats.free_bytes = total_free_bytes;
    g_stats.largest_free_block = largest_free;

    if (total_free_bytes > 0) {
        g_stats.external_fragmentation = 1.0 - ((double)largest_free / (double)total_free_bytes);
    } else {
        g_stats.external_fragmentation = 0.0;
    }

    memcpy(stats, &g_stats, sizeof(heapforge_stats_t));
}

void heapforge_print_stats(void) {
    heapforge_stats_t s;
    heapforge_get_stats(&s);

    printf("\n================ ALLOCATOR STATISTICS ================\n");
    printf("Bytes in Use (Payload) : %zu bytes\n", s.bytes_in_use);
    printf("Peak Bytes in Use      : %zu bytes\n", s.peak_bytes_in_use);
    printf("Bytes from OS          : %zu bytes\n", s.bytes_from_os);
    printf("Active Allocations     : %zu blocks\n", s.active_alloc_count);
    printf("Total Allocations      : %zu calls\n", s.total_allocations);
    printf("Total Frees            : %zu calls\n", s.total_frees);
    printf("Total Splits           : %zu\n", s.total_splits);
    printf("Total Coalesces        : %zu\n", s.total_coalesces);
    printf("Free Blocks Available  : %zu blocks\n", s.free_block_count);
    printf("Total Free Bytes       : %zu bytes\n", s.free_bytes);
    printf("Largest Free Block     : %zu bytes\n", s.largest_free_block);
    printf("External Fragmentation : %.2f%%\n", s.external_fragmentation * 100.0);

    printf("\nAllocation Count Per Size Class:\n");
    for (int i = 0; i < NUM_SIZE_CLASSES; i++) {
        if (s.class_alloc_counts[i] > 0) {
            printf("  Class %2d: %zu allocations\n", i, s.class_alloc_counts[i]);
        }
    }
    printf("======================================================\n\n");
}

void heapforge_leak_report(void) {
    heapforge_state_t st;
    heapforge_get_state(&st);

    printf("\n========== HEAP LEAK REPORT ==========\n");

    if (st.initial_break == NULL || st.initial_break == st.current_break) {
        printf("Outstanding Allocations: 0\n");
        printf("Total Leaked Bytes: 0\n");
        printf("Status: Clean - No memory leaks detected!\n");
        printf("======================================\n\n");
        return;
    }

    size_t leaked_count = 0;
    size_t leaked_bytes = 0;
    block_header_t *curr = (block_header_t *)st.initial_break;

    while ((char *)curr < (char *)st.current_break) {
        if (block_is_allocated(curr)) {
            leaked_count++;
            leaked_bytes += block_get_payload_size(curr);
        }
        curr = (block_header_t *)((char *)curr + block_get_size(curr));
    }

    printf("Outstanding Allocations: %zu\n", leaked_count);
    printf("Total Leaked Bytes     : %zu\n\n", leaked_bytes);

    if (leaked_count > 0) {
        size_t idx = 1;
        curr = (block_header_t *)st.initial_break;
        while ((char *)curr < (char *)st.current_break) {
            if (block_is_allocated(curr)) {
                printf("  Block %zu: Address: %p | Block Size: %zu | Payload: %zu bytes\n",
                       idx++, (void *)curr, block_get_size(curr), block_get_payload_size(curr));
            }
            curr = (block_header_t *)((char *)curr + block_get_size(curr));
        }
    } else {
        printf("Status: Clean - No memory leaks detected!\n");
    }

    printf("======================================\n\n");
}
