/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ds_btree.h - B-Tree with pool-allocated nodes
 *
 * Platform-independent B-Tree for embedded systems. All node storage
 * is statically allocated from a fixed-size pool. The element type is
 * inherited from the DS common header. Only insert and search are
 * provided (no delete) as B-Tree deletion is rarely needed on
 * resource-constrained embedded targets.
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

#ifndef TIKU_KITS_DS_BTREE_H_
#define TIKU_KITS_DS_BTREE_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_ds.h"

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * Minimum degree of the B-Tree (t). Each non-root node has at least
 * t-1 keys and at most 2t-1 keys.
 * Override before including this header to change the order.
 */
#ifndef TIKU_KITS_DS_BTREE_ORDER
#define TIKU_KITS_DS_BTREE_ORDER 3
#endif

/** Maximum number of keys per node = 2*ORDER - 1 */
#define TIKU_KITS_DS_BTREE_MAX_KEYS \
    (2 * TIKU_KITS_DS_BTREE_ORDER - 1)

/** Maximum number of children per node = 2*ORDER */
#define TIKU_KITS_DS_BTREE_MAX_CHILDREN \
    (2 * TIKU_KITS_DS_BTREE_ORDER)

/**
 * Maximum number of nodes in the static pool.
 * Override before including this header to change the limit.
 */
#ifndef TIKU_KITS_DS_BTREE_MAX_NODES
#define TIKU_KITS_DS_BTREE_MAX_NODES 15
#endif

/** Sentinel value indicating no node (empty child / empty root) */
#define TIKU_KITS_DS_BTREE_NONE 0xFF

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_ds_btree_node
 * @brief Single B-Tree node with keys and child indices
 *
 * Children are stored as uint8_t indices into the parent tree's
 * node pool. TIKU_KITS_DS_BTREE_NONE indicates an absent child.
 */
struct tiku_kits_ds_btree_node {
    tiku_kits_ds_elem_t keys[TIKU_KITS_DS_BTREE_MAX_KEYS];
    uint8_t children[TIKU_KITS_DS_BTREE_MAX_CHILDREN];
    uint8_t n_keys;     /**< Number of keys currently stored  */
    uint8_t is_leaf;    /**< 1 if leaf node, 0 if internal    */
};

/**
 * @struct tiku_kits_ds_btree
 * @brief B-Tree with static node pool
 *
 * Nodes are allocated sequentially from the pool. The root field
 * stores the index of the root node, or BTREE_NONE if the tree
 * is empty.
 *
 * Example:
 * @code
 *   struct tiku_kits_ds_btree bt;
 *   tiku_kits_ds_btree_init(&bt);
 *   tiku_kits_ds_btree_insert(&bt, 42);
 *   tiku_kits_ds_btree_search(&bt, 42);  // returns OK
 * @endcode
 */
struct tiku_kits_ds_btree {
    struct tiku_kits_ds_btree_node
        nodes[TIKU_KITS_DS_BTREE_MAX_NODES];
    uint8_t root;       /**< Index of root node, or NONE      */
    uint8_t n_nodes;    /**< Number of allocated nodes         */
    uint8_t next_free;  /**< Index of next free slot in pool   */
};

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a B-Tree to empty state
 * @param bt B-Tree to initialize
 * @return TIKU_KITS_DS_OK or TIKU_KITS_DS_ERR_NULL
 */
int tiku_kits_ds_btree_init(struct tiku_kits_ds_btree *bt);

/*---------------------------------------------------------------------------*/
/* INSERT / SEARCH                                                           */
/*---------------------------------------------------------------------------*/

/**
 * @brief Insert a key into the B-Tree
 *
 * Handles root splitting when the root is full. Duplicate keys
 * are allowed (inserted to the right of existing equal keys).
 *
 * @param bt  B-Tree
 * @param key Key to insert
 * @return TIKU_KITS_DS_OK or TIKU_KITS_DS_ERR_FULL (node pool
 *         exhausted) or TIKU_KITS_DS_ERR_NULL
 */
int tiku_kits_ds_btree_insert(struct tiku_kits_ds_btree *bt,
                               tiku_kits_ds_elem_t key);

/**
 * @brief Search for a key in the B-Tree
 * @param bt  B-Tree
 * @param key Key to find
 * @return TIKU_KITS_DS_OK if found,
 *         TIKU_KITS_DS_ERR_NOTFOUND if not found,
 *         TIKU_KITS_DS_ERR_EMPTY if tree is empty, or
 *         TIKU_KITS_DS_ERR_NULL
 */
int tiku_kits_ds_btree_search(const struct tiku_kits_ds_btree *bt,
                               tiku_kits_ds_elem_t key);

/*---------------------------------------------------------------------------*/
/* MIN / MAX                                                                 */
/*---------------------------------------------------------------------------*/

/**
 * @brief Find the minimum (leftmost) key in the B-Tree
 * @param bt    B-Tree
 * @param value Output: minimum key
 * @return TIKU_KITS_DS_OK, TIKU_KITS_DS_ERR_EMPTY, or
 *         TIKU_KITS_DS_ERR_NULL
 */
int tiku_kits_ds_btree_min(const struct tiku_kits_ds_btree *bt,
                            tiku_kits_ds_elem_t *value);

/**
 * @brief Find the maximum (rightmost) key in the B-Tree
 * @param bt    B-Tree
 * @param value Output: maximum key
 * @return TIKU_KITS_DS_OK, TIKU_KITS_DS_ERR_EMPTY, or
 *         TIKU_KITS_DS_ERR_NULL
 */
int tiku_kits_ds_btree_max(const struct tiku_kits_ds_btree *bt,
                            tiku_kits_ds_elem_t *value);

/*---------------------------------------------------------------------------*/
/* QUERY                                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Count the total number of keys stored in the B-Tree
 * @param bt B-Tree
 * @return Total key count, or 0 if bt is NULL or tree is empty
 */
uint16_t tiku_kits_ds_btree_count(
    const struct tiku_kits_ds_btree *bt);

/**
 * @brief Return the height of the B-Tree
 * @param bt B-Tree
 * @return Height (0 if empty, 1 for a single root node, etc.),
 *         or 0 if bt is NULL
 */
uint8_t tiku_kits_ds_btree_height(
    const struct tiku_kits_ds_btree *bt);

/*---------------------------------------------------------------------------*/
/* CLEAR                                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Reset the B-Tree to empty state
 * @param bt B-Tree to clear
 * @return TIKU_KITS_DS_OK or TIKU_KITS_DS_ERR_NULL
 */
int tiku_kits_ds_btree_clear(struct tiku_kits_ds_btree *bt);

#endif /* TIKU_KITS_DS_BTREE_H_ */
