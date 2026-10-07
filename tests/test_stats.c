#include "heapforge.h"
#include "allocator.h"
#include "stats.h"
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
 * @brief Test that allocation and free correctly increment and decrement counters.
 */
static bool test_stats_basic_counters(void) {
    heapforge_stats_t s_before;
    heapforge_get_stats(&s_before);

    void *p = heapforge_malloc(128);
    ASSERT_TRUE(p != NULL);

    heapforge_stats_t s_after_alloc;
    heapforge_get_stats(&s_after_alloc);

    ASSERT_EQ(s_after_alloc.total_allocations, s_before.total_allocations + 1);
    ASSERT_EQ(s_after_alloc.active_alloc_count, s_before.active_alloc_count + 1);
    ASSERT_EQ(s_after_alloc.bytes_in_use, s_before.bytes_in_use + 128);

    heapforge_free(p);

    heapforge_stats_t s_after_free;
    heapforge_get_stats(&s_after_free);

    ASSERT_EQ(s_after_free.total_frees, s_before.total_frees + 1);
    ASSERT_EQ(s_after_free.active_alloc_count, s_before.active_alloc_count);
    ASSERT_EQ(s_after_free.bytes_in_use, s_before.bytes_in_use);

    return true;
}

/**
 * @brief Test that peak_bytes_in_use tracks high-water mark and does not drop on free.
 */
static bool test_stats_peak_tracking(void) {
    heapforge_stats_t s0;
    heapforge_get_stats(&s0);
    size_t initial_bytes = s0.bytes_in_use;

    void *p1 = heapforge_malloc(256);
    void *p2 = heapforge_malloc(512);
    ASSERT_TRUE(p1 && p2);

    heapforge_stats_t s_peak;
    heapforge_get_stats(&s_peak);
    ASSERT_TRUE(s_peak.peak_bytes_in_use >= initial_bytes + 768);
    ASSERT_EQ(s_peak.bytes_in_use, initial_bytes + 768);
    size_t recorded_peak = s_peak.peak_bytes_in_use;

    /* Free one block: current bytes drops, peak must stay identical */
    heapforge_free(p2);

    heapforge_stats_t s_drop;
    heapforge_get_stats(&s_drop);
    ASSERT_EQ(s_drop.peak_bytes_in_use, recorded_peak);

    heapforge_free(p1);
    return true;
}

/**
 * @brief Test that block splitting increments total_splits counter.
 */
static bool test_stats_split_counting(void) {
    void *large = heapforge_malloc(2048);
    ASSERT_TRUE(large != NULL);
    heapforge_free(large);

    heapforge_stats_t s_before;
    heapforge_get_stats(&s_before);

    /*
     * Allocate 1024 payload bytes (1056 total).
     * None of the smaller freed blocks (128, 256, 512) fit this request,
     * so the 2048 block (Class 6) is selected and split!
     */
    void *small = heapforge_malloc(1024);
    ASSERT_TRUE(small != NULL);

    heapforge_stats_t s_after;
    heapforge_get_stats(&s_after);

    ASSERT_EQ(s_after.total_splits, s_before.total_splits + 1);

    heapforge_free(small);
    return true;
}

/**
 * @brief Test external fragmentation computation and formatted statistics output.
 */
static bool test_stats_fragmentation_metric(void) {
    /* Allocate 4 separated blocks */
    void *p1 = heapforge_malloc(64);
    void *p2 = heapforge_malloc(64);
    void *p3 = heapforge_malloc(64);
    void *p4 = heapforge_malloc(64);
    ASSERT_TRUE(p1 && p2 && p3 && p4);

    /* Free alternating blocks: p1 and p3 are now separate free fragments */
    heapforge_free(p1);
    heapforge_free(p3);

    heapforge_stats_t s;
    heapforge_get_stats(&s);

    /* Verify free block counts and fragmentation is bounded in [0.0, 1.0] */
    ASSERT_TRUE(s.free_block_count >= 2);
    ASSERT_TRUE(s.free_bytes > 0);
    ASSERT_TRUE(s.external_fragmentation >= 0.0);
    ASSERT_TRUE(s.external_fragmentation <= 1.0);

    /* Output formatted diagnostic snapshot */
    heapforge_print_stats();

    heapforge_free(p2);
    heapforge_free(p4);
    return true;
}

/**
 * @brief Test leak reporting capability.
 */
static bool test_stats_leak_report(void) {
    void *leak1 = heapforge_malloc(120);
    void *leak2 = heapforge_malloc(250);
    ASSERT_TRUE(leak1 && leak2);

    /* Print leak report with active allocations */
    heapforge_leak_report();

    /* Clean up and verify clean report */
    heapforge_free(leak1);
    heapforge_free(leak2);

    heapforge_leak_report();
    return true;
}

int main(void) {
    printf("========================================\n");
    printf(" HeapForge - Milestone 6 Test Suite\n");
    printf(" Allocator Statistics & Fragmentation\n");
    printf("========================================\n");

    TEST_RUN(test_stats_basic_counters);
    TEST_RUN(test_stats_peak_tracking);
    TEST_RUN(test_stats_split_counting);
    TEST_RUN(test_stats_fragmentation_metric);
    TEST_RUN(test_stats_leak_report);

    printf("========================================\n");
    printf("Results: %d/%d passed (%d failed)\n",
           g_tests_passed, g_tests_run, g_tests_failed);
    printf("========================================\n");

    return (g_tests_failed == 0) ? 0 : 1;
}
