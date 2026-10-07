#ifndef HEAPFORGE_ALLOCATOR_H
#define HEAPFORGE_ALLOCATOR_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Memory alignment requirement.
 *
 * 16-byte alignment satisfies:
 * 1. alignof(max_align_t) on x86_64 and AArch64 (typically 16 bytes for long double / __float128).
 * 2. SIMD instructions (SSE/AVX 128-bit load/store alignments).
 * 3. Standard POSIX malloc alignment guarantees.
 */
#define HEAPFORGE_ALIGNMENT 16UL

/**
 * @brief Rounds a size up to the nearest multiple of HEAPFORGE_ALIGNMENT.
 */
#define HEAPFORGE_ALIGN_UP(size) \
    (((size) + (HEAPFORGE_ALIGNMENT - 1UL)) & ~(HEAPFORGE_ALIGNMENT - 1UL))

/**
 * @brief Checks if a pointer address is aligned to HEAPFORGE_ALIGNMENT.
 */
#define HEAPFORGE_IS_ALIGNED(ptr) \
    (((uintptr_t)(ptr) & (HEAPFORGE_ALIGNMENT - 1UL)) == 0)

/**
 * @brief Maximum allocation size supported without integer overflow on alignment.
 */
#define HEAPFORGE_MAX_ALLOC_SIZE (SIZE_MAX - HEAPFORGE_ALIGNMENT)

/**
 * @brief Internal allocator state snapshot for Milestone 1 / debugging.
 */
typedef struct {
    void *initial_break;      /**< Initial program break upon first initialization */
    void *current_break;      /**< Current program break */
    size_t total_sbrk_bytes;  /**< Total bytes requested via sbrk */
    size_t alloc_count;       /**< Total successful allocation calls */
} heapforge_state_t;

/**
 * @brief Retrieves internal allocator state (used for testing and validation).
 *
 * @param state Pointer to state struct to populate.
 */
void heapforge_get_state(heapforge_state_t *state);

/**
 * @brief Resets or cleans up allocator state (if supported, e.g. for testing).
 */
void heapforge_reset(void);

#endif /* HEAPFORGE_ALLOCATOR_H */
