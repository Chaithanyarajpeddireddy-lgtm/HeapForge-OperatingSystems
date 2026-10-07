#include "freelist.h"
#include <stddef.h>

static block_header_t *g_freelist_head = NULL;
static size_t g_freelist_count = 0;

void freelist_init(void) {
    g_freelist_head = NULL;
    g_freelist_count = 0;
}

void freelist_insert(block_header_t *block) {
    if (block == NULL) {
        return;
    }

    free_node_t *node = block_get_free_node(block);
    node->prev_free = NULL;
    node->next_free = g_freelist_head;

    if (g_freelist_head != NULL) {
        free_node_t *head_node = block_get_free_node(g_freelist_head);
        head_node->prev_free = block;
    }

    g_freelist_head = block;
    g_freelist_count++;
}

void freelist_remove(block_header_t *block) {
    if (block == NULL) {
        return;
    }

    free_node_t *node = block_get_free_node(block);

    if (node->prev_free != NULL) {
        free_node_t *prev_node = block_get_free_node(node->prev_free);
        prev_node->next_free = node->next_free;
    } else if (g_freelist_head == block) {
        g_freelist_head = node->next_free;
    }

    if (node->next_free != NULL) {
        free_node_t *next_node = block_get_free_node(node->next_free);
        next_node->prev_free = node->prev_free;
    }

    node->prev_free = NULL;
    node->next_free = NULL;

    if (g_freelist_count > 0) {
        g_freelist_count--;
    }
}

block_header_t *freelist_find_fit(size_t required_total_size) {
    block_header_t *curr = g_freelist_head;

    while (curr != NULL) {
        if (block_get_size(curr) >= required_total_size) {
            return curr;
        }
        free_node_t *node = block_get_free_node(curr);
        curr = node->next_free;
    }

    return NULL;
}

bool freelist_contains(const block_header_t *block) {
    if (block == NULL) {
        return false;
    }

    const block_header_t *curr = g_freelist_head;
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
    return g_freelist_count;
}

block_header_t *freelist_get_head(void) {
    return g_freelist_head;
}
