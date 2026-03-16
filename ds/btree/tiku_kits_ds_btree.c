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
 *
 * Uses a simple bump allocator: next_free advances monotonically.
 * The new node is initialized as an empty leaf with all child slots
 * set to BTREE_NONE and keys zeroed for a deterministic state.
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
    /* New nodes default to leaf; callers promote to internal when needed */
    node->is_leaf = 1;
    for (i = 0; i < TIKU_KITS_DS_BTREE_MAX_CHILDREN; i++) {
        node->children[i] = TIKU_KITS_DS_BTREE_NONE;
    }
    memset(node->keys, 0, sizeof(node->keys));

    return idx;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Split a full child node, promoting the median key into the parent
 *
 * The child at position @p child_pos of the parent must be full
 * (n_keys == MAX_KEYS).  A new node is allocated for the upper half
 * of the child's keys, the median key is promoted into the parent,
 * and the parent's children array is shifted right to accommodate
 * the new sibling.  After the split both the original child and the
 * new node hold exactly (t - 1) keys.
 *
 * The shift loops run backwards from the parent's current key/child
 * count to avoid overwriting data before it has been moved.
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

    /* Allocate new node for the upper half of the split child */
    new_idx = alloc_node(bt);
    if (new_idx == TIKU_KITS_DS_BTREE_NONE) {
        return TIKU_KITS_DS_ERR_FULL;
    }
    new_node = &bt->nodes[new_idx];
    /* The new sibling inherits the child's leaf/internal status */
    new_node->is_leaf = child->is_leaf;

    /* Copy upper (t-1) keys from child to new node.  The median
     * key at index (t-1) is not copied -- it will be promoted. */
    for (i = 0; i < t - 1; i++) {
        new_node->keys[i] = child->keys[i + t];
    }
    new_node->n_keys = t - 1;

    /* If child is internal, move the upper t child pointers to
     * the new node and clear them in the original child. */
    if (!child->is_leaf) {
        for (i = 0; i < t; i++) {
            new_node->children[i] = child->children[i + t];
            child->children[i + t] = TIKU_KITS_DS_BTREE_NONE;
        }
    }

    /* Child retains only the lower (t-1) keys */
    child->n_keys = t - 1;

    /* Shift parent's children right to open a slot for new_node.
     * Loop runs backwards to avoid clobbering unread entries. */
    for (i = parent->n_keys; i > child_pos; i--) {
        parent->children[i + 1] = parent->children[i];
    }
    parent->children[child_pos + 1] = new_idx;

    /* Shift parent's keys right and promote the median key */
    for (i = parent->n_keys; i > child_pos; i--) {
        parent->keys[i] = parent->keys[i - 1];
    }
    parent->keys[child_pos] = child->keys[t - 1];
    parent->n_keys++;

    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Insert a key into a non-full node (recursive descent)
 *
 * Pre-condition: the node at @p node_idx is guaranteed to be non-full
 * because the caller splits full nodes before descending.  In a leaf
 * the key is inserted at its sorted position by shifting existing keys
 * right.  In an internal node the correct child is located, split if
 * full, and then the function recurses into that child.
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
        /* Shift keys right to open the correct sorted position.
         * The loop scans backwards so each key moves into a slot
         * that has already been vacated by the previous iteration. */
        i = (int)node->n_keys - 1;
        while (i >= 0 && node->keys[i] > key) {
            node->keys[i + 1] = node->keys[i];
            i--;
        }
        node->keys[i + 1] = key;
        node->n_keys++;
        return TIKU_KITS_DS_OK;
    }

    /* Internal node: find the child subtree whose key range
     * contains the new key by scanning keys right to left. */
    i = (int)node->n_keys - 1;
    while (i >= 0 && node->keys[i] > key) {
        i--;
    }
    i++;

    /* If the target child is full, split it before descending
     * so we never recurse into a full node. */
    if (bt->nodes[node->children[i]].n_keys
            == TIKU_KITS_DS_BTREE_MAX_KEYS) {
        rc = split_child(bt, node_idx, (uint8_t)i);
        if (rc != TIKU_KITS_DS_OK) {
            return rc;
        }
        /* After split the median key was promoted into this node,
         * so n_keys changed.  Re-read the pointer (pool addresses
         * are stable, but we need the updated key array) and
         * decide which of the two post-split children to descend
         * into based on the promoted key. */
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
 *
 * At each node, keys are scanned linearly to find either a match or
 * the correct child to descend into.  The recursion terminates when
 * the key is found, a leaf is reached without finding it, or a NONE
 * child index is encountered (which should not happen in a valid
 * tree but is handled defensively).
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
 *
 * Accumulates n_keys from this node, then recurses into each child.
 * For internal nodes with k keys there are (k + 1) children to visit.
 * The total cost is O(N) where N is the number of allocated nodes.
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
 *
 * All leaves in a valid B-Tree reside at the same depth, so
 * descending the first child at each level is sufficient to
 * determine the overall height.  O(log_t n).
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

/**
 * @brief Initialize a B-Tree to empty state
 *
 * Zeros the entire node pool so that the struct is in a clean,
 * deterministic state regardless of prior memory contents.
 */
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

/**
 * @brief Insert a key into the B-Tree
 *
 * Handles the special cases of an empty tree and a full root before
 * delegating to insert_nonfull() for the recursive descent.  Root
 * splitting creates a new root node whose sole child is the old root,
 * increasing tree height by one.
 */
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

    /* Empty tree: create root leaf and place the key directly */
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

    /* If the root is full, create a new root and split the old root
     * as a child.  This is the only operation that increases tree
     * height. */
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

/**
 * @brief Search for a key in the B-Tree
 *
 * Delegates to the recursive search_node() helper after checking
 * for NULL and empty-tree edge cases.
 */
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

/**
 * @brief Find the minimum (leftmost) key in the B-Tree
 *
 * Follows the leftmost child pointer at every level until a leaf is
 * reached, then returns keys[0] of that leaf.
 */
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

    /* Follow the leftmost child pointer at each level -- because
     * keys are sorted within each node, the global minimum is
     * always in the leftmost leaf's first key slot. */
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

/**
 * @brief Find the maximum (rightmost) key in the B-Tree
 *
 * Follows the rightmost child pointer (children[n_keys]) at every
 * level until a leaf is reached, then returns the last key in that
 * leaf.
 */
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

    /* Follow the rightmost child pointer at each level -- symmetric
     * to btree_min.  The global maximum is always in the rightmost
     * leaf's last key slot. */
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

/**
 * @brief Count the total number of keys stored in the B-Tree
 *
 * Delegates to the recursive count_node() helper which performs a
 * full traversal.  Returns 0 for a NULL or empty tree.
 */
uint16_t tiku_kits_ds_btree_count(
    const struct tiku_kits_ds_btree *bt)
{
    if (bt == NULL) {
        return 0;
    }

    return count_node(bt, bt->root);
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Return the height of the B-Tree
 *
 * Delegates to the recursive height_node() helper which follows
 * the leftmost path.  Returns 0 for a NULL or empty tree.
 */
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

/**
 * @brief Reset the B-Tree to empty state
 *
 * Rewinds the bump allocator, sets root to BTREE_NONE, and zeros
 * all node storage.  This is functionally identical to init() and
 * is the only way to reclaim pool space.
 */
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
