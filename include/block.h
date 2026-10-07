#ifndef HEAPFORGE_BLOCK_H
#define HEAPFORGE_BLOCK_H

#include "allocator.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * Flags packed into the lowest 4 bits of size_and_flags.
 * Since all blocks are 16-byte aligned, the lowest 4 bits are guaranteed zero.
 */
#define BLOCK_FLAG_FREE      0x0UL
#define BLOCK_FLAG_ALLOCATED 0x1UL
#define BLOCK_FLAG_MMAP      0x2UL

#define BLOCK_FLAGS_MASK     0x0FUL
#define BLOCK_SIZE_MASK      (~BLOCK_FLAGS_MASK)

/**
 * 64-bit Canaries for memory corruption detection.
 */
#define BLOCK_MAGIC_HEADER   0xDEADBEEFCAFEFEEDUL
#define BLOCK_MAGIC_FOOTER   0xFEEDCAFEBEEFDEADUL

/**
 * @brief Block Header.
 *
 * Sized at exactly 16 bytes to ensure the payload starting immediately
 * after the header maintains strict 16-byte alignment without extra padding.
 */
typedef struct block_header {
    size_t size_and_flags; /**< Total block size in bytes (masked) + status flags */
    uint64_t magic;        /**< Header canary for corruption detection */
} block_header_t;

/**
 * @brief Block Footer / Boundary Tag.
 *
 * Located at the end of the block. Sized at 16 bytes.
 * Allows O(1) backward traversal for immediate coalescing.
 */
typedef struct block_footer {
    size_t size_and_flags; /**< Identical copy of header's size_and_flags */
    uint64_t magic;        /**< Footer canary for corruption detection */
} block_footer_t;

/**
 * @brief Intrusive doubly linked list node inside free block payload.
 *
 * Only exists while the block is free; overwritten by user payload when allocated.
 */
typedef struct free_node {
    struct block_header *prev_free;
    struct block_header *next_free;
} free_node_t;

/**
 * Total metadata overhead per block: Header (16) + Footer (16) = 32 bytes.
 */
#define BLOCK_HEADER_SIZE sizeof(block_header_t)
#define BLOCK_FOOTER_SIZE sizeof(block_footer_t)
#define BLOCK_OVERHEAD    (BLOCK_HEADER_SIZE + BLOCK_FOOTER_SIZE)

/**
 * Minimum payload must fit intrusive pointers (16 bytes) and be 16-byte aligned.
 */
#define MIN_PAYLOAD_SIZE  sizeof(free_node_t)

/**
 * Minimum block size: Header (16) + Min Payload (16) + Footer (16) = 48 bytes.
 */
#define MIN_BLOCK_SIZE    (BLOCK_HEADER_SIZE + MIN_PAYLOAD_SIZE + BLOCK_FOOTER_SIZE)

/* --- Inline Helper Functions --- */

static inline size_t block_get_size(const block_header_t *hdr) {
    return hdr->size_and_flags & BLOCK_SIZE_MASK;
}

static inline bool block_is_allocated(const block_header_t *hdr) {
    return (hdr->size_and_flags & BLOCK_FLAG_ALLOCATED) != 0;
}

static inline bool block_is_mmap(const block_header_t *hdr) {
    return (hdr->size_and_flags & BLOCK_FLAG_MMAP) != 0;
}

static inline size_t block_get_payload_size(const block_header_t *hdr) {
    size_t total = block_get_size(hdr);
    return (total >= BLOCK_OVERHEAD) ? (total - BLOCK_OVERHEAD) : 0;
}

static inline void *block_to_payload(block_header_t *hdr) {
    return (void *)((char *)hdr + BLOCK_HEADER_SIZE);
}

static inline block_header_t *payload_to_block(void *payload) {
    if (payload == NULL) return NULL;
    return (block_header_t *)((char *)payload - BLOCK_HEADER_SIZE);
}

static inline block_footer_t *block_get_footer(block_header_t *hdr) {
    size_t size = block_get_size(hdr);
    return (block_footer_t *)((char *)hdr + size - BLOCK_FOOTER_SIZE);
}

static inline free_node_t *block_get_free_node(block_header_t *hdr) {
    return (free_node_t *)block_to_payload(hdr);
}

static inline block_header_t *block_get_next_physical(block_header_t *hdr, const void *heap_end) {
    size_t size = block_get_size(hdr);
    char *next = (char *)hdr + size;
    if (heap_end != NULL && (void *)next >= heap_end) {
        return NULL;
    }
    return (block_header_t *)next;
}

static inline block_header_t *block_get_prev_physical(block_header_t *hdr, const void *heap_start) {
    if (heap_start != NULL && (void *)hdr <= heap_start) {
        return NULL;
    }
    /* The previous block's footer is right before this block's header */
    block_footer_t *prev_ftr = (block_footer_t *)((char *)hdr - BLOCK_FOOTER_SIZE);
    if (prev_ftr->magic != BLOCK_MAGIC_FOOTER) {
        return NULL; /* Corruption or invalid boundary */
    }
    size_t prev_size = prev_ftr->size_and_flags & BLOCK_SIZE_MASK;
    if (prev_size < MIN_BLOCK_SIZE) {
        return NULL;
    }
    char *prev = (char *)hdr - prev_size;
    if (heap_start != NULL && (void *)prev < heap_start) {
        return NULL;
    }
    return (block_header_t *)prev;
}

/**
 * @brief Initializes a block's header and footer with given total size and flags.
 */
void block_init(block_header_t *hdr, size_t total_size, size_t flags);

/**
 * @brief Splits a block if the remainder is at least MIN_BLOCK_SIZE.
 *
 * @param hdr Block header to split.
 * @param needed_total_size Size required for the first allocated portion.
 * @return Pointer to the remainder block header if split occurred, or NULL if block was not split.
 */
block_header_t *block_split(block_header_t *hdr, size_t needed_total_size);

/**
 * @brief Validates header, footer, alignment, and canaries of a block.
 *
 * @return true if block metadata is completely valid, false on corruption.
 */
bool block_validate(const block_header_t *hdr, const void *heap_start, const void *heap_end);

#endif /* HEAPFORGE_BLOCK_H */
