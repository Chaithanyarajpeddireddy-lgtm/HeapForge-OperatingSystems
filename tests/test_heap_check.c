#include "heapforge.h"
#include "allocator.h"
#include "block.h"
#include "freelist.h"
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
 * @brief Test that an active mixed heap passes heap_check and heap_dump.
 */
static bool test_heap_check_valid_heap(void) {
    void *p1 = heapforge_malloc(32);
    void *p2 = heapforge_malloc(64);
    void *p3 = heapforge_malloc(128);
    void *p4 = heapforge_malloc(256);
    void *p5 = heapforge_malloc(512);

    ASSERT_TRUE(p1 && p2 && p3 && p4 && p5);

    /* Free a couple blocks */
    heapforge_free(p2);
    heapforge_free(p4);

    /* Verify consistency */
    int res = heap_check();
    ASSERT_EQ(res, 0);

    /* Execute heap dump */
    heap_dump();

    heapforge_free(p1);
    heapforge_free(p3);
    heapforge_free(p5);
    return true;
}

/**
 * @brief Test that heap_check detects header canary corruption.
 */
static bool test_heap_check_header_corruption(void) {
    void *p = heapforge_malloc(48);
    ASSERT_TRUE(p != NULL);

    block_header_t *hdr = payload_to_block(p);
    uint64_t saved_magic = hdr->magic;

    /* Intentionally corrupt header */
    hdr->magic = 0xBAD00000BAD00000UL;
    int check_bad = heap_check();
    ASSERT_TRUE(check_bad != 0);

    /* Restore and verify clean check */
    hdr->magic = saved_magic;
    int check_good = heap_check();
    ASSERT_EQ(check_good, 0);

    heapforge_free(p);
    return true;
}

/**
 * @brief Test that heap_check detects footer symmetry mismatch.
 */
static bool test_heap_check_footer_mismatch(void) {
    void *p = heapforge_malloc(96);
    ASSERT_TRUE(p != NULL);

    block_header_t *hdr = payload_to_block(p);
    block_footer_t *ftr = block_get_footer(hdr);
    size_t saved_flags = ftr->size_and_flags;

    /* Desynchronize footer */
    ftr->size_and_flags ^= 0x10;
    int check_bad = heap_check();
    ASSERT_TRUE(check_bad != 0);

    /* Restore and verify */
    ftr->size_and_flags = saved_flags;
    int check_good = heap_check();
    ASSERT_EQ(check_good, 0);

    heapforge_free(p);
    return true;
}

/**
 * @brief Test that free list link inconsistency is caught by heap_check.
 */
static bool test_heap_check_freelist_corruption(void) {
    void *p = heapforge_malloc(64);
    ASSERT_TRUE(p != NULL);
    heapforge_free(p);

    block_header_t *hdr = payload_to_block(p);
    free_node_t *node = block_get_free_node(hdr);
    block_header_t *saved_prev = node->prev_free;

    /* Corrupt prev_free pointer (head of free list must have prev_free == NULL) */
    node->prev_free = (block_header_t *)0xDEADBEEFUL;
    int check_bad = heap_check();
    ASSERT_TRUE(check_bad != 0);

    /* Restore and verify clean check */
    node->prev_free = saved_prev;
    int check_good = heap_check();
    ASSERT_EQ(check_good, 0);

    return true;
}

int main(void) {
    printf("========================================\n");
    printf(" HeapForge - Milestone 3 Test Suite\n");
    printf(" Heap Checker & Segregated List Checks\n");
    printf("========================================\n");

    TEST_RUN(test_heap_check_valid_heap);
    TEST_RUN(test_heap_check_header_corruption);
    TEST_RUN(test_heap_check_footer_mismatch);
    TEST_RUN(test_heap_check_freelist_corruption);

    printf("========================================\n");
    printf("Results: %d/%d passed (%d failed)\n",
           g_tests_passed, g_tests_run, g_tests_failed);
    printf("========================================\n");

    return (g_tests_failed == 0) ? 0 : 1;
}
