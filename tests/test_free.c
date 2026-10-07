#include "heapforge.h"
#include "allocator.h"
#include "block.h"
#include "freelist.h"

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
 * @brief Test basic free and immediate reuse of freed block.
 */
static bool test_basic_free_and_reuse(void) {
    void *p1 = heapforge_malloc(64);
    ASSERT_TRUE(p1 != NULL);
    ASSERT_TRUE(HEAPFORGE_IS_ALIGNED(p1));

    size_t count_before = freelist_get_count();
    heapforge_free(p1);
    size_t count_after = freelist_get_count();
    ASSERT_EQ(count_after, count_before + 1);

    /* Subsequent request of identical size should reuse p1 */
    void *p2 = heapforge_malloc(64);
    ASSERT_TRUE(p2 != NULL);
    ASSERT_EQ(p1, p2);

    heapforge_free(p2);
    return true;
}

/**
 * @brief Test header, footer, boundary tag symmetry, and canaries.
 */
static bool test_metadata_and_boundary_tags(void) {
    void *p = heapforge_malloc(128);
    ASSERT_TRUE(p != NULL);

    block_header_t *hdr = payload_to_block(p);
    ASSERT_TRUE(hdr != NULL);
    ASSERT_TRUE(HEAPFORGE_IS_ALIGNED(hdr));
    ASSERT_EQ(hdr->magic, BLOCK_MAGIC_HEADER);
    ASSERT_TRUE(block_is_allocated(hdr));

    block_footer_t *ftr = block_get_footer(hdr);
    ASSERT_TRUE(ftr != NULL);
    ASSERT_EQ(ftr->magic, BLOCK_MAGIC_FOOTER);
    ASSERT_EQ(ftr->size_and_flags, hdr->size_and_flags);

    /* Free and verify status change */
    heapforge_free(p);
    ASSERT_TRUE(!block_is_allocated(hdr));
    ASSERT_EQ(ftr->size_and_flags, hdr->size_and_flags);

    return true;
}

/**
 * @brief Test that double-free does not corrupt or duplicate in the free list.
 */
static bool test_double_free_detection(void) {
    void *p = heapforge_malloc(48);
    ASSERT_TRUE(p != NULL);

    heapforge_free(p);
    size_t count_freed = freelist_get_count();

    /* Attempt second free on identical pointer */
    heapforge_free(p);
    size_t count_double = freelist_get_count();

    /* Count must not have incremented */
    ASSERT_EQ(count_freed, count_double);

    return true;
}

/**
 * @brief Test that memory corruption is caught by block validation.
 */
static bool test_canary_corruption_detection(void) {
    void *p = heapforge_malloc(32);
    ASSERT_TRUE(p != NULL);

    block_header_t *hdr = payload_to_block(p);
    ASSERT_TRUE(block_validate(hdr, NULL, NULL));

    /* Corrupt header magic */
    hdr->magic = 0xCAFEBABE12345678UL;
    ASSERT_TRUE(!block_validate(hdr, NULL, NULL));

    /* Restore header magic */
    hdr->magic = BLOCK_MAGIC_HEADER;
    ASSERT_TRUE(block_validate(hdr, NULL, NULL));

    /* Corrupt footer magic */
    block_footer_t *ftr = block_get_footer(hdr);
    ftr->magic = 0xDEAD0000DEAD0000UL;
    ASSERT_TRUE(!block_validate(hdr, NULL, NULL));

    /* Restore footer magic */
    ftr->magic = BLOCK_MAGIC_FOOTER;
    ASSERT_TRUE(block_validate(hdr, NULL, NULL));

    heapforge_free(p);
    return true;
}

/**
 * @brief Test allocating multiple blocks and recycling them all.
 */
static bool test_multiple_alloc_free_recycle(void) {
    const int N = 8;
    void *ptrs[8];

    for (int i = 0; i < N; i++) {
        ptrs[i] = heapforge_malloc(64);
        ASSERT_TRUE(ptrs[i] != NULL);
    }

    size_t initial_free = freelist_get_count();
    for (int i = 0; i < N; i++) {
        heapforge_free(ptrs[i]);
    }
    ASSERT_EQ(freelist_get_count(), initial_free + N);

    /* Allocate N blocks again: all should be satisfied from free list */
    void *reallocated[8];
    for (int i = 0; i < N; i++) {
        reallocated[i] = heapforge_malloc(64);
        ASSERT_TRUE(reallocated[i] != NULL);
    }
    ASSERT_EQ(freelist_get_count(), initial_free);

    for (int i = 0; i < N; i++) {
        heapforge_free(reallocated[i]);
    }
    return true;
}

int main(void) {
    printf("========================================\n");
    printf(" HeapForge - Milestone 2 Test Suite\n");
    printf(" Block Metadata & Free List Validation\n");
    printf("========================================\n");

    TEST_RUN(test_basic_free_and_reuse);
    TEST_RUN(test_metadata_and_boundary_tags);
    TEST_RUN(test_double_free_detection);
    TEST_RUN(test_canary_corruption_detection);
    TEST_RUN(test_multiple_alloc_free_recycle);

    printf("========================================\n");
    printf("Results: %d/%d passed (%d failed)\n",
           g_tests_passed, g_tests_run, g_tests_failed);
    printf("========================================\n");

    return (g_tests_failed == 0) ? 0 : 1;
}
