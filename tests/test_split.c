#include "heapforge.h"
#include "allocator.h"
#include "block.h"
#include "freelist.h"
#include "heap_check.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_RUN(name) do { \
    printf("[RUN     ] %s\n", #name); \
    g_tests_run++; \
    if (name()) { \
        printf("[      OK] %s\n", #name); \
        g_tests_passed++; \
    } else { \
        printf("[  FAILED] %s\n", #name); \
        g_tests_failed++; \
    } \
} while (0)

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "  Assertion failed: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
        return false; \
    } \
} while (0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        fprintf(stderr, "  Assertion failed: %s == %s (%zu != %zu) at %s:%d\n", \
                #a, #b, (size_t)(a), (size_t)(b), __FILE__, __LINE__); \
        return false; \
    } \
} while (0)

/**
 * @brief Test that exact-size requests do not produce any split.
 */
static bool test_exact_fit_no_split(void) {
    void *p = heapforge_malloc(64);
    ASSERT_TRUE(p != NULL);
    heapforge_free(p);

    size_t free_before = freelist_get_count();
    void *reused = heapforge_malloc(64);
    ASSERT_EQ(p, reused);
    ASSERT_EQ(freelist_get_count(), free_before - 1);

    ASSERT_EQ(heap_check(), 0);
    heapforge_free(reused);
    return true;
}

/**
 * @brief Test splitting a large free block into allocated head and free remainder.
 */
static bool test_split_large_block(void) {
    /* Allocate 2048 payload bytes (total block size = 32 overhead + 2048 = 2080, Class 6) */
    void *large = heapforge_malloc(2048);
    ASSERT_TRUE(large != NULL);
    heapforge_free(large);

    block_header_t *large_hdr = payload_to_block(large);
    size_t original_size = block_get_size(large_hdr);
    ASSERT_TRUE(original_size >= 2080);

    /*
     * Allocate medium chunk (512 bytes payload -> 544 bytes total, Class 4).
     * No existing free block fits this, so large_hdr (Class 6) is found and split!
     */
    void *small = heapforge_malloc(512);
    ASSERT_TRUE(small != NULL);
    ASSERT_EQ(large, small); /* Small allocation starts at same address */

    block_header_t *small_hdr = payload_to_block(small);
    ASSERT_EQ(block_get_size(small_hdr), 544);

    /* The remainder block immediately follows small_hdr */
    block_header_t *rem_hdr = (block_header_t *)((char *)small_hdr + 544);
    ASSERT_TRUE(!block_is_allocated(rem_hdr));
    ASSERT_EQ(block_get_size(rem_hdr), original_size - 544);

    /* Remainder must be present in free list */
    ASSERT_TRUE(freelist_contains(rem_hdr));

    /* Heap check must pass with split blocks */
    ASSERT_EQ(heap_check(), 0);

    /* Allocate another chunk (512 bytes payload -> 544 bytes total) from the remainder */
    void *small2 = heapforge_malloc(512);
    ASSERT_TRUE(small2 != NULL);
    ASSERT_EQ(small2, block_to_payload(rem_hdr));
    ASSERT_EQ(heap_check(), 0);

    heapforge_free(small);
    heapforge_free(small2);
    return true;
}

/**
 * @brief Test that remainder smaller than MIN_BLOCK_SIZE is not split.
 */
static bool test_split_remainder_below_min_block_size(void) {
    /* Block with total size 96 (payload 64) */
    void *p = heapforge_malloc(64);
    ASSERT_TRUE(p != NULL);
    heapforge_free(p);

    /*
     * Request 48 bytes payload -> total block size 80 bytes.
     * Potential remainder = 96 - 80 = 16 bytes.
     * 16 bytes < MIN_BLOCK_SIZE (48 bytes), so no split should occur.
     */
    void *reused = heapforge_malloc(48);
    ASSERT_TRUE(reused != NULL);
    ASSERT_EQ(p, reused);

    block_header_t *hdr = payload_to_block(reused);
    /* Block size should remain 96 rather than being split */
    ASSERT_EQ(block_get_size(hdr), 96);

    ASSERT_EQ(heap_check(), 0);
    heapforge_free(reused);
    return true;
}

/**
 * @brief Test progressive splitting of a large block and memory data integrity.
 */
static bool test_progressive_splitting_and_data_integrity(void) {
    const int CHUNKS = 8;
    const size_t CHUNK_SIZE = 64;

    /* Create large 1024-byte block */
    void *base = heapforge_malloc(1024);
    ASSERT_TRUE(base != NULL);
    heapforge_free(base);

    uint8_t *ptrs[CHUNKS];
    for (int i = 0; i < CHUNKS; i++) {
        ptrs[i] = (uint8_t *)heapforge_malloc(CHUNK_SIZE);
        ASSERT_TRUE(ptrs[i] != NULL);
        memset(ptrs[i], (uint8_t)(i + 0x30), CHUNK_SIZE);
    }

    /* Verify payload data retained across progressive splits */
    for (int i = 0; i < CHUNKS; i++) {
        uint8_t expected = (uint8_t)(i + 0x30);
        for (size_t b = 0; b < CHUNK_SIZE; b++) {
            ASSERT_EQ(ptrs[i][b], expected);
        }
    }

    ASSERT_EQ(heap_check(), 0);

    for (int i = 0; i < CHUNKS; i++) {
        heapforge_free(ptrs[i]);
    }

    ASSERT_EQ(heap_check(), 0);
    return true;
}

int main(void) {
    printf("========================================\n");
    printf(" HeapForge - Milestone 4 Test Suite\n");
    printf(" Block Splitting & Remainder Rules\n");
    printf("========================================\n");

    TEST_RUN(test_exact_fit_no_split);
    TEST_RUN(test_split_large_block);
    TEST_RUN(test_split_remainder_below_min_block_size);
    TEST_RUN(test_progressive_splitting_and_data_integrity);

    printf("========================================\n");
    printf("Results: %d/%d passed (%d failed)\n",
           g_tests_passed, g_tests_run, g_tests_failed);
    printf("========================================\n");

    return (g_tests_failed == 0) ? 0 : 1;
}
