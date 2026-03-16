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
 * @brief Minimum degree (t) of the B-Tree.
 *
 * This compile-time constant controls the branching factor of the tree.
 * Each non-root node stores at least (t - 1) keys and at most (2t - 1)
 * keys, giving internal nodes between t and 2t children.  A larger
 * value yields wider, shallower trees that require fewer node
 * traversals per search but consume more memory per node.
 *
 * Override before including this header to change the order:
 * @code
 *   #define TIKU_KITS_DS_BTREE_ORDER 5
 *   #include "tiku_kits_ds_btree.h"
 * @endcode
 */
#ifndef TIKU_KITS_DS_BTREE_ORDER
#define TIKU_KITS_DS_BTREE_ORDER 3
#endif

/**
 * @brief Maximum number of keys per node (2 * ORDER - 1).
 *
 * Derived from TIKU_KITS_DS_BTREE_ORDER.  A full node holds exactly
 * this many keys and must be split before another key can be inserted.
 */
#define TIKU_KITS_DS_BTREE_MAX_KEYS \
    (2 * TIKU_KITS_DS_BTREE_ORDER - 1)

/**
 * @brief Maximum number of children per node (2 * ORDER).
 *
 * Derived from TIKU_KITS_DS_BTREE_ORDER.  An internal node has one
 * more child pointer than it has keys.
 */
#define TIKU_KITS_DS_BTREE_MAX_CHILDREN \
    (2 * TIKU_KITS_DS_BTREE_ORDER)

/**
 * @brief Maximum number of nodes in the static pool.
 *
 * All B-Tree nodes are allocated sequentially from a fixed-size array
 * inside the tree struct.  This constant defines the upper bound on
 * how many nodes can exist across the entire tree.  Choose a value
 * that balances memory usage against the number of keys you expect
 * to insert.
 *
 * Override before including this header to change the limit:
 * @code
 *   #define TIKU_KITS_DS_BTREE_MAX_NODES 31
 *   #include "tiku_kits_ds_btree.h"
 * @endcode
 */
#ifndef TIKU_KITS_DS_BTREE_MAX_NODES
#define TIKU_KITS_DS_BTREE_MAX_NODES 15
#endif

/**
 * @brief Sentinel value indicating no node (empty child / empty root).
 *
 * Children and the root field use this value to represent "no node".
 * Chosen as 0xFF because the pool index is uint8_t and 255 exceeds
 * any practical pool size on a resource-constrained target.
 */
#define TIKU_KITS_DS_BTREE_NONE 0xFF

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_ds_btree_node
 * @brief Single B-Tree node containing keys and child-pool indices
 *
 * Each node holds up to BTREE_MAX_KEYS keys in sorted order and up
 * to BTREE_MAX_CHILDREN child pointers.  Child pointers are stored
 * as uint8_t pool indices rather than raw pointers so that the tree
 * is position-independent and can reside in FRAM without relocation.
 * An absent child slot is marked with TIKU_KITS_DS_BTREE_NONE.
 *
 * Two categories of nodes exist:
 *   - @c is_leaf == 1 -- leaf nodes store only keys; all child
 *     slots are NONE.
 *   - @c is_leaf == 0 -- internal nodes store keys and have
 *     (n_keys + 1) valid child pointers.
 *
 * @note The key type is controlled by tiku_kits_ds_elem_t (default
 *       int32_t).  Override at compile time with
 *       @c -DTIKU_KITS_DS_ELEM_TYPE=int16_t to change it globally
 *       for all DS sub-modules.
 */
struct tiku_kits_ds_btree_node {
    tiku_kits_ds_elem_t keys[TIKU_KITS_DS_BTREE_MAX_KEYS];
    uint8_t children[TIKU_KITS_DS_BTREE_MAX_CHILDREN];
    uint8_t n_keys;     /**< Number of keys currently stored  */
    uint8_t is_leaf;    /**< 1 if leaf node, 0 if internal    */
};

/**
 * @struct tiku_kits_ds_btree
 * @brief B-Tree backed by a fixed-size static node pool
 *
 * All node storage lives inside the struct itself as a flat array of
 * BTREE_MAX_NODES entries.  Nodes are allocated sequentially (bump
 * allocator) -- @c next_free advances monotonically.  Because there
 * is no node deallocation, clear() is the only way to reclaim pool
 * space, which resets the entire tree.
 *
 * The @c root field stores the pool index of the root node, or
 * TIKU_KITS_DS_BTREE_NONE when the tree is empty.  No heap
 * allocation is needed -- just declare the tree as a static or
 * local variable.
 *
 * @note Because the pool is a bump allocator, deleting individual
 *       keys is intentionally omitted.  This is a design choice for
 *       resource-constrained targets where B-Tree deletion complexity
 *       (merge/redistribute) is rarely worth the code size.
 *
 * Example:
 * @code
 *   struct tiku_kits_ds_btree bt;
 *   tiku_kits_ds_btree_init(&bt);
 *   tiku_kits_ds_btree_insert(&bt, 10);
 *   tiku_kits_ds_btree_insert(&bt, 20);
 *   tiku_kits_ds_btree_insert(&bt, 5);
 *   tiku_kits_ds_btree_search(&bt, 20);  // returns OK
 *   tiku_kits_ds_btree_search(&bt, 99);  // returns ERR_NOTFOUND
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
 *
 * Resets the node pool, sets the root to BTREE_NONE, and zeros all
 * node storage so that the tree starts in a clean, deterministic
 * state.  Must be called before any insert or search operation.
 *
 * @param bt B-Tree to initialize (must not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if bt is NULL
 */
int tiku_kits_ds_btree_init(struct tiku_kits_ds_btree *bt);

/*---------------------------------------------------------------------------*/
/* INSERT / SEARCH                                                           */
/*---------------------------------------------------------------------------*/

/**
 * @brief Insert a key into the B-Tree
 *
 * Uses the standard top-down B-Tree insertion algorithm: if the root
 * is full it is split first so that the recursive descent always
 * enters a non-full node.  This guarantees that at most one split
 * occurs at each level, keeping the worst-case cost at O(t * log_t n).
 *
 * Duplicate keys are permitted and are inserted to the right of any
 * existing equal keys, preserving insertion order among duplicates.
 *
 * @param bt  B-Tree (must not be NULL)
 * @param key Key to insert
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if bt is NULL,
 *         TIKU_KITS_DS_ERR_FULL if the static node pool is exhausted
 */
int tiku_kits_ds_btree_insert(struct tiku_kits_ds_btree *bt,
                               tiku_kits_ds_elem_t key);

/**
 * @brief Search for a key in the B-Tree
 *
 * Performs a top-down search starting at the root.  At each node the
 * keys are scanned linearly (O(t) per level) to find the correct
 * child to descend into, giving an overall O(t * log_t n) search
 * cost.  For the default ORDER of 3, each node holds at most 5 keys,
 * so the per-node scan is trivially fast.
 *
 * @param bt  B-Tree (must not be NULL)
 * @param key Key to find
 * @return TIKU_KITS_DS_OK if the key is found,
 *         TIKU_KITS_DS_ERR_NULL if bt is NULL,
 *         TIKU_KITS_DS_ERR_EMPTY if the tree contains no keys,
 *         TIKU_KITS_DS_ERR_NOTFOUND if the key is not present
 */
int tiku_kits_ds_btree_search(const struct tiku_kits_ds_btree *bt,
                               tiku_kits_ds_elem_t key);

/*---------------------------------------------------------------------------*/
/* MIN / MAX                                                                 */
/*---------------------------------------------------------------------------*/

/**
 * @brief Find the minimum (leftmost) key in the B-Tree
 *
 * Follows the leftmost child pointer at every level until a leaf is
 * reached, then returns the first key in that leaf.  Because all
 * leaves are at the same depth in a B-Tree, the cost is O(log_t n).
 *
 * @param bt    B-Tree (must not be NULL)
 * @param value Output pointer where the minimum key is written
 *              (must not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if bt or value is NULL,
 *         TIKU_KITS_DS_ERR_EMPTY if the tree contains no keys
 */
int tiku_kits_ds_btree_min(const struct tiku_kits_ds_btree *bt,
                            tiku_kits_ds_elem_t *value);

/**
 * @brief Find the maximum (rightmost) key in the B-Tree
 *
 * Follows the rightmost child pointer at every level until a leaf is
 * reached, then returns the last key in that leaf.  Symmetric to
 * btree_min, with the same O(log_t n) cost.
 *
 * @param bt    B-Tree (must not be NULL)
 * @param value Output pointer where the maximum key is written
 *              (must not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if bt or value is NULL,
 *         TIKU_KITS_DS_ERR_EMPTY if the tree contains no keys
 */
int tiku_kits_ds_btree_max(const struct tiku_kits_ds_btree *bt,
                            tiku_kits_ds_elem_t *value);

/*---------------------------------------------------------------------------*/
/* QUERY                                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Count the total number of keys stored in the B-Tree
 *
 * Performs a full recursive traversal of every node, summing the
 * n_keys fields.  O(N) where N is the number of allocated nodes.
 * Safe to call with a NULL pointer -- returns 0.
 *
 * @param bt B-Tree, or NULL
 * @return Total key count, or 0 if bt is NULL or tree is empty
 */
uint16_t tiku_kits_ds_btree_count(
    const struct tiku_kits_ds_btree *bt);

/**
 * @brief Return the height of the B-Tree
 *
 * Descends the leftmost path from root to leaf.  Because all leaves
 * in a B-Tree are at the same depth, following any single path is
 * sufficient.  O(log_t n).  Safe to call with a NULL pointer --
 * returns 0.
 *
 * @param bt B-Tree, or NULL
 * @return Height (0 if empty or NULL, 1 for a single root node,
 *         etc.)
 */
uint8_t tiku_kits_ds_btree_height(
    const struct tiku_kits_ds_btree *bt);

/*---------------------------------------------------------------------------*/
/* CLEAR                                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Reset the B-Tree to empty state
 *
 * Resets root to BTREE_NONE, rewinds the bump allocator, and zeros
 * all node storage.  This is the only way to reclaim pool space
 * because individual node deallocation is not supported.
 *
 * @param bt B-Tree to clear (must not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if bt is NULL
 */
int tiku_kits_ds_btree_clear(struct tiku_kits_ds_btree *bt);

#endif /* TIKU_KITS_DS_BTREE_H_ */
