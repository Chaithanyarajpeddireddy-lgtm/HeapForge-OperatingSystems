#include "heapforge.h"
#include "allocator.h"
#include "heap_check.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

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
 * @brief Test calloc edge cases where count or size is zero.
 */
static bool test_calloc_zero_parameters(void) {
    ASSERT_TRUE(heapforge_calloc(0, 100) == NULL);
    ASSERT_TRUE(heapforge_calloc(100, 0) == NULL);
    ASSERT_TRUE(heapforge_calloc(0, 0) == NULL);
    return true;
}

/**
 * @brief Test multiplication overflow in calloc parameters.
 */
static bool test_calloc_overflow(void) {
    ASSERT_TRUE(heapforge_calloc(SIZE_MAX / 2 + 1, 3) == NULL);
    ASSERT_TRUE(heapforge_calloc(SIZE_MAX, 2) == NULL);
    ASSERT_TRUE(heapforge_calloc(SIZE_MAX, SIZE_MAX) == NULL);
    return true;
}

/**
 * @brief Test that memory returned by calloc is completely zeroed.
 */
static bool test_calloc_memory_cleared(void) {
    const size_t count = 256;
    uint32_t *arr = (uint32_t *)heapforge_calloc(count, sizeof(uint32_t));
    ASSERT_TRUE(arr != NULL);
    ASSERT_TRUE(HEAPFORGE_IS_ALIGNED(arr));

    for (size_t i = 0; i < count; i++) {
        ASSERT_EQ(arr[i], 0U);
    }

    ASSERT_EQ(heap_check(), 0);
    heapforge_free(arr);
    ASSERT_EQ(heap_check(), 0);
    return true;
}

/**
 * @brief Test multiple calloc allocations and releases.
 */
static bool test_calloc_multiple(void) {
    void *ptrs[10];
    for (int i = 0; i < 10; i++) {
        ptrs[i] = heapforge_calloc(16, 8);
        ASSERT_TRUE(ptrs[i] != NULL);
        ASSERT_TRUE(HEAPFORGE_IS_ALIGNED(ptrs[i]));
    }

    ASSERT_EQ(heap_check(), 0);

    for (int i = 0; i < 10; i++) {
        heapforge_free(ptrs[i]);
    }

    ASSERT_EQ(heap_check(), 0);
    return true;
}

int main(void) {
    printf("========================================\n");
    printf(" HeapForge - Milestone 5 Test Suite\n");
    printf(" calloc() Verification\n");
    printf("========================================\n");

    TEST_RUN(test_calloc_zero_parameters);
    TEST_RUN(test_calloc_overflow);
    TEST_RUN(test_calloc_memory_cleared);
    TEST_RUN(test_calloc_multiple);

    printf("========================================\n");
    printf("Results: %d/%d passed (%d failed)\n",
           g_tests_passed, g_tests_run, g_tests_failed);
    printf("========================================\n");

    return (g_tests_failed == 0) ? 0 : 1;
}
