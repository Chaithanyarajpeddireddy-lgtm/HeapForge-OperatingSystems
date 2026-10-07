#include "heapforge.h"
#include "allocator.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

/* Simple test framework macros */
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
 * @brief Test that malloc(0) returns NULL according to documented policy.
 */
static bool test_malloc_zero(void) {
    void *p = heapforge_malloc(0);
    ASSERT_TRUE(p == NULL);
    return true;
}

/**
 * @brief Test basic allocations of varied sizes.
 */
static bool test_basic_allocations(void) {
    size_t sizes[] = {1, 7, 8, 15, 16, 24, 32, 64, 127, 256, 1024, 4096, 8192};
    size_t num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    for (size_t i = 0; i < num_sizes; i++) {
        void *p = heapforge_malloc(sizes[i]);
        ASSERT_TRUE(p != NULL);
    }
    return true;
}

/**
 * @brief Test that all returned pointers are strictly 16-byte aligned.
 */
static bool test_alignment_guarantee(void) {
    for (size_t sz = 1; sz <= 256; sz++) {
        void *p = heapforge_malloc(sz);
        ASSERT_TRUE(p != NULL);
        ASSERT_TRUE(HEAPFORGE_IS_ALIGNED(p));
    }
    return true;
}

/**
 * @brief Test memory read/write integrity across sequential allocations.
 */
static bool test_memory_read_write_integrity(void) {
    const int count = 16;
    const size_t sz = 128;
    uint8_t *ptrs[count];

    /* Allocate and write unique patterns */
    for (int i = 0; i < count; i++) {
        ptrs[i] = (uint8_t *)heapforge_malloc(sz);
        ASSERT_TRUE(ptrs[i] != NULL);
        uint8_t fill_byte = (uint8_t)(i + 0xA0);
        memset(ptrs[i], fill_byte, sz);
    }

    /* Verify all allocations preserved their unique contents */
    for (int i = 0; i < count; i++) {
        uint8_t expected_byte = (uint8_t)(i + 0xA0);
        for (size_t j = 0; j < sz; j++) {
            ASSERT_EQ(ptrs[i][j], expected_byte);
        }
    }
    return true;
}

/**
 * @brief Test calloc functionality: zero-initialization and overflow prevention.
 */
static bool test_calloc_zeroed_and_overflow(void) {
    /* Test zero-initialization */
    size_t count = 64;
    size_t elem_size = sizeof(uint64_t);
    uint64_t *arr = (uint64_t *)heapforge_calloc(count, elem_size);
    ASSERT_TRUE(arr != NULL);
    ASSERT_TRUE(HEAPFORGE_IS_ALIGNED(arr));

    for (size_t i = 0; i < count; i++) {
        ASSERT_EQ(arr[i], 0ULL);
    }

    /* Test overflow detection */
    void *overflow1 = heapforge_calloc(SIZE_MAX / 2 + 1, 3);
    ASSERT_TRUE(overflow1 == NULL);

    void *overflow2 = heapforge_calloc(SIZE_MAX, SIZE_MAX);
    ASSERT_TRUE(overflow2 == NULL);

    return true;
}

/**
 * @brief Test that extreme sizes are safely rejected without wrapping or crashing.
 */
static bool test_overflow_protection(void) {
    void *p1 = heapforge_malloc(SIZE_MAX);
    ASSERT_TRUE(p1 == NULL);

    void *p2 = heapforge_malloc(SIZE_MAX - 8);
    ASSERT_TRUE(p2 == NULL);

    void *p3 = heapforge_malloc((size_t)INTPTR_MAX + 1UL);
    ASSERT_TRUE(p3 == NULL);

    return true;
}

/**
 * @brief Test allocator state tracking and heap expansion metrics.
 */
static bool test_allocator_state_tracking(void) {
    heapforge_state_t st;
    heapforge_get_state(&st);

    ASSERT_TRUE(st.initial_break != NULL);
    ASSERT_TRUE(st.current_break != NULL);
    ASSERT_TRUE(st.current_break >= st.initial_break);
    ASSERT_TRUE(st.total_sbrk_bytes > 0);
    ASSERT_TRUE(st.alloc_count > 0);

    return true;
}

int main(void) {
    printf("========================================\n");
    printf(" HeapForge - Milestone 1 Test Suite\n");
    printf(" Minimal sbrk Bump Allocator Verification\n");
    printf("========================================\n");

    TEST_RUN(test_malloc_zero);
    TEST_RUN(test_basic_allocations);
    TEST_RUN(test_alignment_guarantee);
    TEST_RUN(test_memory_read_write_integrity);
    TEST_RUN(test_calloc_zeroed_and_overflow);
    TEST_RUN(test_overflow_protection);
    TEST_RUN(test_allocator_state_tracking);

    printf("========================================\n");
    printf("Results: %d/%d passed (%d failed)\n",
           g_tests_passed, g_tests_run, g_tests_failed);
    printf("========================================\n");

    return (g_tests_failed == 0) ? 0 : 1;
}
