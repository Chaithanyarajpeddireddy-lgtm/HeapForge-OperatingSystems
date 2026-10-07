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
 * @brief Case A: realloc(NULL, size) behaves as malloc(size).
 */
static bool test_realloc_null_ptr(void) {
    void *p = heapforge_realloc(NULL, 128);
    ASSERT_TRUE(p != NULL);
    ASSERT_TRUE(HEAPFORGE_IS_ALIGNED(p));
    ASSERT_EQ(heap_check(), 0);

    heapforge_free(p);
    return true;
}

/**
 * @brief Case B: realloc(ptr, 0) frees ptr and returns NULL.
 */
static bool test_realloc_zero_size(void) {
    void *p = heapforge_malloc(64);
    ASSERT_TRUE(p != NULL);

    void *res = heapforge_realloc(p, 0);
    ASSERT_TRUE(res == NULL);
    ASSERT_EQ(heap_check(), 0);

    return true;
}

/**
 * @brief Case C: Shrinking in place and splitting excess memory into free list.
 */
static bool test_realloc_shrink(void) {
    size_t orig_size = 512;
    uint8_t *p = (uint8_t *)heapforge_malloc(orig_size);
    ASSERT_TRUE(p != NULL);
    memset(p, 0x5A, orig_size);

    /* Shrink to 64 bytes */
    size_t new_size = 64;
    uint8_t *shrunk = (uint8_t *)heapforge_realloc(p, new_size);
    ASSERT_TRUE(shrunk != NULL);
    ASSERT_EQ(p, shrunk); /* Address must remain identical */

    /* Data in the preserved range must be intact */
    for (size_t i = 0; i < new_size; i++) {
        ASSERT_EQ(shrunk[i], 0x5A);
    }

    ASSERT_EQ(heap_check(), 0);
    heapforge_free(shrunk);
    return true;
}

/**
 * @brief Case D: Growing in place using adjacent free block without copying.
 */
static bool test_realloc_grow_in_place(void) {
    /* Allocate 3 contiguous blocks */
    uint8_t *p1 = (uint8_t *)heapforge_malloc(64);
    void *p2 = heapforge_malloc(256);
    void *p3 = heapforge_malloc(64);
    ASSERT_TRUE(p1 && p2 && p3);

    memset(p1, 0x77, 64);

    /* Free middle block p2: space adjacent to p1 is now free */
    heapforge_free(p2);

    /* Grow p1 into p2's free space (requested size fits in 64 + 288) */
    uint8_t *grown = (uint8_t *)heapforge_realloc(p1, 160);
    ASSERT_TRUE(grown != NULL);
    ASSERT_EQ(p1, grown); /* Grew directly in place! */

    /* Verify original data was preserved */
    for (size_t i = 0; i < 64; i++) {
        ASSERT_EQ(grown[i], 0x77);
    }

    ASSERT_EQ(heap_check(), 0);

    heapforge_free(grown);
    heapforge_free(p3);
    return true;
}

/**
 * @brief Case E: Cannot grow in place -> allocate new, copy, free old.
 */
static bool test_realloc_grow_with_move(void) {
    /* Allocate two adjacent blocks (both allocated, so p1 cannot grow in place) */
    uint8_t *p1 = (uint8_t *)heapforge_malloc(64);
    void *p2 = heapforge_malloc(64);
    ASSERT_TRUE(p1 && p2);

    memset(p1, 0x99, 64);

    /* Request large size requiring movement to new block */
    uint8_t *moved = (uint8_t *)heapforge_realloc(p1, 2048);
    ASSERT_TRUE(moved != NULL);
    ASSERT_TRUE(moved != p1); /* Moved to new location */

    /* Verify original 64 bytes retained */
    for (size_t i = 0; i < 64; i++) {
        ASSERT_EQ(moved[i], 0x99);
    }

    ASSERT_EQ(heap_check(), 0);

    heapforge_free(moved);
    heapforge_free(p2);
    return true;
}

int main(void) {
    printf("========================================\n");
    printf(" HeapForge - Milestone 5 Test Suite\n");
    printf(" realloc() In-Place Growth & Shrink\n");
    printf("========================================\n");

    TEST_RUN(test_realloc_null_ptr);
    TEST_RUN(test_realloc_zero_size);
    TEST_RUN(test_realloc_shrink);
    TEST_RUN(test_realloc_grow_in_place);
    TEST_RUN(test_realloc_grow_with_move);

    printf("========================================\n");
    printf("Results: %d/%d passed (%d failed)\n",
           g_tests_passed, g_tests_run, g_tests_failed);
    printf("========================================\n");

    return (g_tests_failed == 0) ? 0 : 1;
}
