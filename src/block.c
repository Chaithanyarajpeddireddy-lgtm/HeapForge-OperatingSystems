#include "block.h"
#include <stdio.h>

void block_init(block_header_t *hdr, size_t total_size, size_t flags) {
    if (hdr == NULL || total_size < MIN_BLOCK_SIZE) {
        return;
    }

    /* Size must be 16-byte aligned */
    size_t aligned_size = HEAPFORGE_ALIGN_UP(total_size);
    size_t packed = (aligned_size & BLOCK_SIZE_MASK) | (flags & BLOCK_FLAGS_MASK);

    hdr->size_and_flags = packed;
    hdr->magic = BLOCK_MAGIC_HEADER;

    /* Initialize matching footer boundary tag */
    block_footer_t *ftr = (block_footer_t *)((char *)hdr + aligned_size - BLOCK_FOOTER_SIZE);
    ftr->size_and_flags = packed;
    ftr->magic = BLOCK_MAGIC_FOOTER;
}

block_header_t *block_split(block_header_t *hdr, size_t needed_total_size) {
    if (hdr == NULL || needed_total_size < MIN_BLOCK_SIZE) {
        return NULL;
    }

    size_t current_size = block_get_size(hdr);
    needed_total_size = HEAPFORGE_ALIGN_UP(needed_total_size);

    if (current_size < needed_total_size) {
        return NULL;
    }

    size_t remainder_size = current_size - needed_total_size;

    /* Only split if the remainder can hold a valid block (MIN_BLOCK_SIZE = 48) */
    if (remainder_size < MIN_BLOCK_SIZE) {
        return NULL;
    }

    /* Initialize the allocated portion */
    block_init(hdr, needed_total_size, BLOCK_FLAG_ALLOCATED);

    /* Initialize the remainder portion as free */
    block_header_t *rem = (block_header_t *)((char *)hdr + needed_total_size);
    block_init(rem, remainder_size, BLOCK_FLAG_FREE);

    return rem;
}

bool block_validate(const block_header_t *hdr, const void *heap_start, const void *heap_end) {
    if (hdr == NULL) {
        return false;
    }

    /* 1. Header address must be 16-byte aligned */
    if (!HEAPFORGE_IS_ALIGNED(hdr)) {
        return false;
    }

    /* 2. Bounds check against heap range */
    if (heap_start != NULL && (const void *)hdr < heap_start) {
        return false;
    }

    /* 3. Canary check */
    if (hdr->magic != BLOCK_MAGIC_HEADER) {
        return false;
    }

    /* 4. Size validation */
    size_t total_size = block_get_size(hdr);
    if (total_size < MIN_BLOCK_SIZE || !HEAPFORGE_IS_ALIGNED(total_size)) {
        return false;
    }

    /* 5. End of block bounds check */
    const char *block_end = (const char *)hdr + total_size;
    if (heap_end != NULL && (const void *)block_end > heap_end) {
        return false;
    }

    /* 6. Footer symmetry check */
    const block_footer_t *ftr = (const block_footer_t *)(block_end - BLOCK_FOOTER_SIZE);
    if (ftr->magic != BLOCK_MAGIC_FOOTER) {
        return false;
    }

    if (ftr->size_and_flags != hdr->size_and_flags) {
        return false;
    }

    return true;
}
