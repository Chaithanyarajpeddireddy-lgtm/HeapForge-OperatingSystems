#include "freelist.h"
#include <stddef.h>

static block_header_t *g_heads[NUM_SIZE_CLASSES];
static size_t g_class_counts[NUM_SIZE_CLASSES];
static size_t g_total_count = 0;

int size_to_class(size_t total_size) {
    if (total_size <= 64)      return 0;
    if (total_size <= 128)     return 1;
    if (total_size <= 256)     return 2;
    if (total_size <= 512)     return 3;
    if (total_size <= 1024)    return 4;
    if (total_size <= 2048)    return 5;
    if (total_size <= 4096)    return 6;
    if (total_size <= 8192)    return 7;
    if (total_size <= 16384)   return 8;
    if (total_size <= 32768)   return 9;
    if (total_size <= 65536)   return 10;
    if (total_size <= 131072)  return 11;
    return 12;
}

void freelist_init(void) {
    for (int i = 0; i < NUM_SIZE_CLASSES; i++) {
        g_heads[i] = NULL;
        g_class_counts[i] = 0;
    }
    g_total_count = 0;
}

void freelist_insert(block_header_t *block) {
    if (block == NULL) {
        return;
    }

    int cls = size_to_class(block_get_size(block));
    free_node_t *node = block_get_free_node(block);

    node->prev_free = NULL;
    node->next_free = g_heads[cls];

    if (g_heads[cls] != NULL) {
        free_node_t *head_node = block_get_free_node(g_heads[cls]);
        head_node->prev_free = block;
    }

    g_heads[cls] = block;
    g_class_counts[cls]++;
    g_total_count++;
}

void freelist_remove(block_header_t *block) {
    if (block == NULL) {
        return;
    }

    int cls = size_to_class(block_get_size(block));
    free_node_t *node = block_get_free_node(block);

    if (node->prev_free != NULL) {
        free_node_t *prev_node = block_get_free_node(node->prev_free);
        prev_node->next_free = node->next_free;
    } else if (g_heads[cls] == block) {
        g_heads[cls] = node->next_free;
    }

    if (node->next_free != NULL) {
        free_node_t *next_node = block_get_free_node(node->next_free);
        next_node->prev_free = node->prev_free;
    }

    node->prev_free = NULL;
    node->next_free = NULL;

    if (g_class_counts[cls] > 0) {
        g_class_counts[cls]--;
    }
    if (g_total_count > 0) {
        g_total_count--;
    }
}

block_header_t *freelist_find_fit(size_t required_total_size) {
    int start_cls = size_to_class(required_total_size);

    for (int c = start_cls; c < NUM_SIZE_CLASSES; c++) {
        block_header_t *curr = g_heads[c];
        while (curr != NULL) {
            if (block_get_size(curr) >= required_total_size) {
                return curr;
            }
            free_node_t *node = block_get_free_node(curr);
            curr = node->next_free;
        }
    }

    return NULL;
}

bool freelist_contains(const block_header_t *block) {
    if (block == NULL) {
        return false;
    }

    int cls = size_to_class(block_get_size(block));
    const block_header_t *curr = g_heads[cls];

    while (curr != NULL) {
        if (curr == block) {
            return true;
        }
        const free_node_t *node = (const free_node_t *)((const char *)curr + BLOCK_HEADER_SIZE);
        curr = node->next_free;
    }

    return false;
}

size_t freelist_get_count(void) {
    return g_total_count;
}

size_t freelist_get_class_count(int cls) {
    if (cls < 0 || cls >= NUM_SIZE_CLASSES) {
        return 0;
    }
    return g_class_counts[cls];
}

block_header_t *freelist_get_head(int cls) {
    if (cls < 0 || cls >= NUM_SIZE_CLASSES) {
        return NULL;
    }
    return g_heads[cls];
}
