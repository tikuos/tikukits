/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ds_btree.c - B-Tree with pool-allocated nodes
 *
 * Platform-independent implementation of a B-Tree. All node storage
 * is statically allocated from a fixed-size pool; no heap usage.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "tiku_kits_ds_btree.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* INTERNAL HELPERS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Allocate a new node from the static pool
 * @param bt B-Tree
 * @return Node index, or TIKU_KITS_DS_BTREE_NONE if pool full
 */
static uint8_t alloc_node(struct tiku_kits_ds_btree *bt)
{
    uint8_t idx;
    struct tiku_kits_ds_btree_node *node;
    uint8_t i;

    if (bt->next_free >= TIKU_KITS_DS_BTREE_MAX_NODES) {
        return TIKU_KITS_DS_BTREE_NONE;
    }

    idx = bt->next_free;
    bt->next_free++;
    bt->n_nodes++;

    node = &bt->nodes[idx];
    node->n_keys = 0;
    node->is_leaf = 1;
    for (i = 0; i < TIKU_KITS_DS_BTREE_MAX_CHILDREN; i++) {
        node->children[i] = TIKU_KITS_DS_BTREE_NONE;
    }
    memset(node->keys, 0, sizeof(node->keys));

    return idx;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Split a full child node
 *
 * The child at position child_pos of the parent must be full
 * (n_keys == MAX_KEYS). A new node is allocated, the upper half
 * of the child's keys move to the new node, and the median key
 * is promoted into the parent.
 *
 * @param bt         B-Tree
 * @param parent_idx Index of parent node
 * @param child_pos  Position of the full child in parent's children
 * @return TIKU_KITS_DS_OK or TIKU_KITS_DS_ERR_FULL
 */
static int split_child(struct tiku_kits_ds_btree *bt,
                        uint8_t parent_idx,
                        uint8_t child_pos)
{
    struct tiku_kits_ds_btree_node *parent;
    struct tiku_kits_ds_btree_node *child;
    struct tiku_kits_ds_btree_node *new_node;
    uint8_t new_idx;
    uint8_t child_idx;
    uint8_t t;
    uint8_t i;

    t = TIKU_KITS_DS_BTREE_ORDER;
    parent = &bt->nodes[parent_idx];
    child_idx = parent->children[child_pos];
    child = &bt->nodes[child_idx];

    /* Allocate new node for upper half */
    new_idx = alloc_node(bt);
    if (new_idx == TIKU_KITS_DS_BTREE_NONE) {
        return TIKU_KITS_DS_ERR_FULL;
    }
    new_node = &bt->nodes[new_idx];
    new_node->is_leaf = child->is_leaf;

    /* Copy upper (t-1) keys from child to new node */
    for (i = 0; i < t - 1; i++) {
        new_node->keys[i] = child->keys[i + t];
    }
    new_node->n_keys = t - 1;

    /* If child is not a leaf, copy upper t children */
    if (!child->is_leaf) {
        for (i = 0; i < t; i++) {
            new_node->children[i] = child->children[i + t];
            child->children[i + t] = TIKU_KITS_DS_BTREE_NONE;
        }
    }

    /* Child now has only t-1 keys (the lower half) */
    child->n_keys = t - 1;

    /* Shift parent's children right to make room for new node */
    for (i = parent->n_keys; i > child_pos; i--) {
        parent->children[i + 1] = parent->children[i];
    }
    parent->children[child_pos + 1] = new_idx;

    /* Shift parent's keys right and insert median */
    for (i = parent->n_keys; i > child_pos; i--) {
        parent->keys[i] = parent->keys[i - 1];
    }
    parent->keys[child_pos] = child->keys[t - 1];
    parent->n_keys++;

    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Insert a key into a non-full node (recursive)
 * @param bt       B-Tree
 * @param node_idx Index of the non-full node
 * @param key      Key to insert
 * @return TIKU_KITS_DS_OK or TIKU_KITS_DS_ERR_FULL
 */
static int insert_nonfull(struct tiku_kits_ds_btree *bt,
                           uint8_t node_idx,
                           tiku_kits_ds_elem_t key)
{
    struct tiku_kits_ds_btree_node *node;
    int i;
    int rc;

    node = &bt->nodes[node_idx];

    if (node->is_leaf) {
        /* Shift keys right and insert at sorted position */
        i = (int)node->n_keys - 1;
        while (i >= 0 && node->keys[i] > key) {
            node->keys[i + 1] = node->keys[i];
            i--;
        }
        node->keys[i + 1] = key;
        node->n_keys++;
        return TIKU_KITS_DS_OK;
    }

    /* Internal node: find child to descend into */
    i = (int)node->n_keys - 1;
    while (i >= 0 && node->keys[i] > key) {
        i--;
    }
    i++;

    /* If the child is full, split it first */
    if (bt->nodes[node->children[i]].n_keys
            == TIKU_KITS_DS_BTREE_MAX_KEYS) {
        rc = split_child(bt, node_idx, (uint8_t)i);
        if (rc != TIKU_KITS_DS_OK) {
            return rc;
        }
        /*
         * After split, re-read node pointer (pool is static so
         * address is stable, but n_keys changed). Decide which
         * of the two children to descend into.
         */
        node = &bt->nodes[node_idx];
        if (key > node->keys[i]) {
            i++;
        }
    }

    return insert_nonfull(bt, node->children[i], key);
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Recursive search within a subtree
 * @param bt       B-Tree
 * @param node_idx Current node index
 * @param key      Key to search for
 * @return TIKU_KITS_DS_OK or TIKU_KITS_DS_ERR_NOTFOUND
 */
static int search_node(const struct tiku_kits_ds_btree *bt,
                        uint8_t node_idx,
                        tiku_kits_ds_elem_t key)
{
    const struct tiku_kits_ds_btree_node *node;
    uint8_t i;

    if (node_idx == TIKU_KITS_DS_BTREE_NONE) {
        return TIKU_KITS_DS_ERR_NOTFOUND;
    }

    node = &bt->nodes[node_idx];

    /* Linear scan through keys */
    i = 0;
    while (i < node->n_keys && key > node->keys[i]) {
        i++;
    }

    /* Check if key is found at position i */
    if (i < node->n_keys && key == node->keys[i]) {
        return TIKU_KITS_DS_OK;
    }

    /* If leaf, key is not in the tree */
    if (node->is_leaf) {
        return TIKU_KITS_DS_ERR_NOTFOUND;
    }

    /* Descend into appropriate child */
    return search_node(bt, node->children[i], key);
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Recursively count keys in a subtree
 * @param bt       B-Tree
 * @param node_idx Current node index
 * @return Total key count in subtree
 */
static uint16_t count_node(const struct tiku_kits_ds_btree *bt,
                            uint8_t node_idx)
{
    const struct tiku_kits_ds_btree_node *node;
    uint16_t total;
    uint8_t i;

    if (node_idx == TIKU_KITS_DS_BTREE_NONE) {
        return 0;
    }

    node = &bt->nodes[node_idx];
    total = node->n_keys;

    if (!node->is_leaf) {
        for (i = 0; i <= node->n_keys; i++) {
            total += count_node(bt, node->children[i]);
        }
    }

    return total;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Recursively compute height of a subtree
 * @param bt       B-Tree
 * @param node_idx Current node index
 * @return Height of subtree (1 for a leaf)
 */
static uint8_t height_node(const struct tiku_kits_ds_btree *bt,
                            uint8_t node_idx)
{
    const struct tiku_kits_ds_btree_node *node;

    if (node_idx == TIKU_KITS_DS_BTREE_NONE) {
        return 0;
    }

    node = &bt->nodes[node_idx];

    if (node->is_leaf) {
        return 1;
    }

    /* All leaves in a B-Tree are at the same depth, so descend
     * into the first child to determine height. */
    return 1 + height_node(bt, node->children[0]);
}

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

int tiku_kits_ds_btree_init(struct tiku_kits_ds_btree *bt)
{
    if (bt == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    bt->root = TIKU_KITS_DS_BTREE_NONE;
    bt->n_nodes = 0;
    bt->next_free = 0;
    memset(bt->nodes, 0, sizeof(bt->nodes));

    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* INSERT / SEARCH                                                           */
/*---------------------------------------------------------------------------*/

int tiku_kits_ds_btree_insert(struct tiku_kits_ds_btree *bt,
                               tiku_kits_ds_elem_t key)
{
    uint8_t new_root_idx;
    uint8_t old_root_idx;
    struct tiku_kits_ds_btree_node *new_root;
    int rc;

    if (bt == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    /* Empty tree: create root leaf and insert */
    if (bt->root == TIKU_KITS_DS_BTREE_NONE) {
        uint8_t idx = alloc_node(bt);
        if (idx == TIKU_KITS_DS_BTREE_NONE) {
            return TIKU_KITS_DS_ERR_FULL;
        }
        bt->root = idx;
        bt->nodes[idx].keys[0] = key;
        bt->nodes[idx].n_keys = 1;
        bt->nodes[idx].is_leaf = 1;
        return TIKU_KITS_DS_OK;
    }

    /* If root is full, split it */
    if (bt->nodes[bt->root].n_keys == TIKU_KITS_DS_BTREE_MAX_KEYS) {
        new_root_idx = alloc_node(bt);
        if (new_root_idx == TIKU_KITS_DS_BTREE_NONE) {
            return TIKU_KITS_DS_ERR_FULL;
        }

        old_root_idx = bt->root;
        new_root = &bt->nodes[new_root_idx];
        new_root->is_leaf = 0;
        new_root->n_keys = 0;
        new_root->children[0] = old_root_idx;

        bt->root = new_root_idx;

        rc = split_child(bt, new_root_idx, 0);
        if (rc != TIKU_KITS_DS_OK) {
            return rc;
        }
    }

    return insert_nonfull(bt, bt->root, key);
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ds_btree_search(const struct tiku_kits_ds_btree *bt,
                               tiku_kits_ds_elem_t key)
{
    if (bt == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    if (bt->root == TIKU_KITS_DS_BTREE_NONE) {
        return TIKU_KITS_DS_ERR_EMPTY;
    }

    return search_node(bt, bt->root, key);
}

/*---------------------------------------------------------------------------*/
/* MIN / MAX                                                                 */
/*---------------------------------------------------------------------------*/

int tiku_kits_ds_btree_min(const struct tiku_kits_ds_btree *bt,
                            tiku_kits_ds_elem_t *value)
{
    const struct tiku_kits_ds_btree_node *node;
    uint8_t idx;

    if (bt == NULL || value == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    if (bt->root == TIKU_KITS_DS_BTREE_NONE) {
        return TIKU_KITS_DS_ERR_EMPTY;
    }

    /* Follow leftmost children down to a leaf */
    idx = bt->root;
    node = &bt->nodes[idx];
    while (!node->is_leaf) {
        idx = node->children[0];
        node = &bt->nodes[idx];
    }

    *value = node->keys[0];
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ds_btree_max(const struct tiku_kits_ds_btree *bt,
                            tiku_kits_ds_elem_t *value)
{
    const struct tiku_kits_ds_btree_node *node;
    uint8_t idx;

    if (bt == NULL || value == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    if (bt->root == TIKU_KITS_DS_BTREE_NONE) {
        return TIKU_KITS_DS_ERR_EMPTY;
    }

    /* Follow rightmost children down to a leaf */
    idx = bt->root;
    node = &bt->nodes[idx];
    while (!node->is_leaf) {
        idx = node->children[node->n_keys];
        node = &bt->nodes[idx];
    }

    *value = node->keys[node->n_keys - 1];
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* QUERY                                                                     */
/*---------------------------------------------------------------------------*/

uint16_t tiku_kits_ds_btree_count(
    const struct tiku_kits_ds_btree *bt)
{
    if (bt == NULL) {
        return 0;
    }

    return count_node(bt, bt->root);
}

/*---------------------------------------------------------------------------*/

uint8_t tiku_kits_ds_btree_height(
    const struct tiku_kits_ds_btree *bt)
{
    if (bt == NULL) {
        return 0;
    }

    return height_node(bt, bt->root);
}

/*---------------------------------------------------------------------------*/
/* CLEAR                                                                     */
/*---------------------------------------------------------------------------*/

int tiku_kits_ds_btree_clear(struct tiku_kits_ds_btree *bt)
{
    if (bt == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    bt->root = TIKU_KITS_DS_BTREE_NONE;
    bt->n_nodes = 0;
    bt->next_free = 0;
    memset(bt->nodes, 0, sizeof(bt->nodes));

    return TIKU_KITS_DS_OK;
}
