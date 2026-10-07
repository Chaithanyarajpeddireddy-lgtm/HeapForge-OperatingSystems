#ifndef HEAPFORGE_H
#define HEAPFORGE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HEAPFORGE_VERSION_MAJOR 0
#define HEAPFORGE_VERSION_MINOR 1
#define HEAPFORGE_VERSION_PATCH 0

/**
 * @brief Allocates `size` bytes of uninitialized memory.
 *
 * Guarantees 16-byte alignment of the returned pointer.
 *
 * @param size Number of bytes to allocate.
 * @return Pointer to allocated memory, or NULL if size == 0 or allocation failed.
 */
void *heapforge_malloc(size_t size);

/**
 * @brief Deallocates the memory previously allocated by HeapForge.
 *
 * @param ptr Pointer to memory to free. If NULL, no action is performed.
 */
void heapforge_free(void *ptr);

/**
 * @brief Allocates memory for an array of `nmemb` elements of `size` bytes each
 *        and initializes all bytes to zero.
 *
 * Detects integer multiplication overflow and returns NULL safely.
 *
 * @param nmemb Number of elements.
 * @param size  Size of each element in bytes.
 * @return Pointer to zero-initialized allocated memory, or NULL on failure/overflow.
 */
void *heapforge_calloc(size_t nmemb, size_t size);

/**
 * @brief Changes the size of the memory block pointed to by `ptr` to `size` bytes.
 *
 * @param ptr  Pointer to previously allocated block, or NULL (equivalent to malloc).
 * @param size New size in bytes. If 0 and ptr != NULL, behavior is documented policy.
 * @return Pointer to reallocated memory block, or NULL on failure.
 */
void *heapforge_realloc(void *ptr, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* HEAPFORGE_H */
