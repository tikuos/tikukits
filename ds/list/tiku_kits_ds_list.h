/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ds_list.h - Pool-allocated singly linked list
 *
 * Platform-independent linked list library for embedded systems. All
 * storage is statically allocated using a compile-time maximum node
 * count. Nodes are managed through an index-based pool allocator --
 * no heap required.
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

#ifndef TIKU_KITS_DS_LIST_H_
#define TIKU_KITS_DS_LIST_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_ds.h"

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * @brief Maximum number of nodes the pool can hold.
 *
 * This compile-time constant defines the upper bound on the number of
 * list nodes available in the static pool.  Each list instance reserves
 * this many node slots in its backing array, so choose a value that
 * balances memory usage against the longest list your application needs.
 *
 * Override before including this header to change the limit:
 * @code
 *   #define TIKU_KITS_DS_LIST_MAX_NODES 32
 *   #include "tiku_kits_ds_list.h"
 * @endcode
 */
#ifndef TIKU_KITS_DS_LIST_MAX_NODES
#define TIKU_KITS_DS_LIST_MAX_NODES 16
#endif

/**
 * @brief Sentinel value indicating no node (end of list / invalid index).
 *
 * Used by the pool allocator and list traversal to mark the end of a
 * chain.  Chosen as 0xFF so that it is distinguishable from any valid
 * pool index (which ranges from 0 to MAX_NODES-1).  The uint8_t index
 * type therefore limits the pool to at most 255 nodes.
 */
#define TIKU_KITS_DS_LIST_NONE 0xFF

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_ds_list_node
 * @brief A single node in the pool-allocated linked list
 *
 * Each node stores one element and an index-based link to the next
 * node.  The same node struct is used in both the active chain and
 * the free chain -- only the interpretation of @c next differs:
 *   - In the active list, @c next points to the successor element.
 *   - In the free list, @c next points to the next available slot.
 *
 * Using an index (uint8_t) instead of a pointer saves 2 bytes per
 * node on 16-bit targets and 6 bytes on 32-bit targets.
 *
 * @note Element type is controlled by tiku_kits_ds_elem_t (default
 *       int32_t).  Override at compile time with
 *       @c -DTIKU_KITS_DS_ELEM_TYPE=int16_t to change it globally
 *       for all DS sub-modules.
 */
struct tiku_kits_ds_list_node {
    tiku_kits_ds_elem_t data;   /**< Stored element value */
    uint8_t next;               /**< Index into pool, or LIST_NONE */
};

/**
 * @struct tiku_kits_ds_list
 * @brief Pool-allocated singly linked list with static storage
 *
 * A general-purpose, pool-backed singly linked list that stores
 * elements in statically allocated nodes.  Because all storage lives
 * inside the struct itself, no heap allocation is needed -- just
 * declare the list as a static or local variable.
 *
 * The pool array serves double duty as both active and free storage.
 * Two independent chains are threaded through the same backing array:
 *   - @c head -- the active list, containing user-inserted elements
 *     in insertion order.
 *   - @c free -- the free list, linking all unused node slots so
 *     that allocation is O(1) (pop from free head) and deallocation
 *     is O(1) (push to free head).
 *
 * Two sizes are tracked independently:
 *   - @c capacity -- the runtime limit passed to init (must be
 *     <= TIKU_KITS_DS_LIST_MAX_NODES).  This lets different list
 *     instances use different logical sizes while sharing the same
 *     compile-time backing pool.
 *   - @c count -- the number of nodes currently in the active chain.
 *     Traversal and bounds-checking use this value.
 *
 * @note Element type is controlled by tiku_kits_ds_elem_t (default
 *       int32_t).  Override at compile time with
 *       @c -DTIKU_KITS_DS_ELEM_TYPE=int16_t to change it globally
 *       for all DS sub-modules.
 *
 * Example:
 * @code
 *   struct tiku_kits_ds_list lst;
 *   tiku_kits_ds_list_init(&lst, 8);      // use 8 of 16 pool slots
 *   tiku_kits_ds_list_push_front(&lst, 42);
 *   tiku_kits_ds_list_push_back(&lst, 7);
 *   // lst now contains: [42] -> [7], count == 2
 * @endcode
 */
struct tiku_kits_ds_list {
    struct tiku_kits_ds_list_node pool[TIKU_KITS_DS_LIST_MAX_NODES];
    uint8_t head;       /**< Index of first active node, or LIST_NONE */
    uint8_t free;       /**< Index of first free node in the pool */
    uint16_t count;     /**< Number of nodes in the active chain */
    uint16_t capacity;  /**< Runtime capacity set by init (<= MAX_NODES) */
};

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a linked list with the given capacity
 *
 * Resets the list to an empty state and builds the free chain by
 * linking all pool slots in index order.  The backing pool is zeroed
 * so that uninitialised reads return a deterministic value.
 *
 * @param list     List to initialize (must not be NULL)
 * @param capacity Maximum number of nodes (1..LIST_MAX_NODES)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if list is NULL,
 *         TIKU_KITS_DS_ERR_PARAM if capacity is 0 or exceeds MAX_NODES
 */
int tiku_kits_ds_list_init(struct tiku_kits_ds_list *list,
                            uint16_t capacity);

/*---------------------------------------------------------------------------*/
/* INSERT OPERATIONS                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Insert an element at the front of the list
 *
 * Allocates a node from the free pool and links it as the new head.
 * This is an O(1) operation since no traversal is required.
 *
 * @param list  List (must not be NULL)
 * @param value Value to insert
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if list is NULL,
 *         TIKU_KITS_DS_ERR_FULL if the free pool is exhausted
 */
int tiku_kits_ds_list_push_front(struct tiku_kits_ds_list *list,
                                  tiku_kits_ds_elem_t value);

/**
 * @brief Append an element at the end of the list
 *
 * Allocates a node from the free pool and appends it after the
 * current tail.  This is an O(n) operation because the singly linked
 * list must be traversed to locate the tail (no tail pointer is
 * maintained, keeping the struct smaller).
 *
 * @param list  List (must not be NULL)
 * @param value Value to append
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if list is NULL,
 *         TIKU_KITS_DS_ERR_FULL if the free pool is exhausted
 */
int tiku_kits_ds_list_push_back(struct tiku_kits_ds_list *list,
                                 tiku_kits_ds_elem_t value);

/**
 * @brief Insert a new node after the node at the given pool index
 *
 * Allocates a node from the free pool and splices it into the active
 * chain immediately after @p node_idx.  The target node is validated
 * by walking the active chain to confirm it is reachable from head,
 * making the validation step O(n) in the worst case.
 *
 * @param list     List (must not be NULL)
 * @param node_idx Pool index of the node to insert after (must be an
 *                 active node reachable from head)
 * @param value    Value for the new node
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if list is NULL,
 *         TIKU_KITS_DS_ERR_BOUNDS if node_idx is not an active node,
 *         TIKU_KITS_DS_ERR_FULL if the free pool is exhausted
 */
int tiku_kits_ds_list_insert_after(struct tiku_kits_ds_list *list,
                                    uint8_t node_idx,
                                    tiku_kits_ds_elem_t value);

/*---------------------------------------------------------------------------*/
/* REMOVE OPERATIONS                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Remove and return the front element from the list
 *
 * Unlinks the head node and returns it to the free pool.  This is
 * an O(1) operation since no traversal is required.  The removed
 * element's value is copied to @p value before the node is freed.
 *
 * @param list  List (must not be NULL)
 * @param value Output pointer where the removed element is written
 *              (must not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if list or value is NULL,
 *         TIKU_KITS_DS_ERR_EMPTY if the list has no active nodes
 */
int tiku_kits_ds_list_pop_front(struct tiku_kits_ds_list *list,
                                 tiku_kits_ds_elem_t *value);

/**
 * @brief Remove the node at the given pool index
 *
 * Traverses the active chain to locate the predecessor of
 * @p node_idx, unlinks the target node, and returns it to the free
 * pool.  This is O(n) in the worst case because a singly linked
 * list requires a scan to find the predecessor.  Removing the head
 * node is a special case that avoids the traversal.
 *
 * @param list     List (must not be NULL)
 * @param node_idx Pool index of the node to remove (must be an
 *                 active node reachable from head)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if list is NULL,
 *         TIKU_KITS_DS_ERR_BOUNDS if node_idx is out of range or
 *         not found in the active chain
 */
int tiku_kits_ds_list_remove(struct tiku_kits_ds_list *list,
                              uint8_t node_idx);

/*---------------------------------------------------------------------------*/
/* SEARCH AND ACCESS                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Linear search for the first occurrence of a value
 *
 * Walks the active chain from head to tail and returns the pool
 * index of the first node whose data matches @p value.  Because
 * the list is unsorted, a linear scan is the only option -- O(n)
 * worst case.
 *
 * @param list     List (must not be NULL)
 * @param value    Value to search for
 * @param node_idx Output pointer where the pool index of the first
 *                 matching node is written (must not be NULL)
 * @return TIKU_KITS_DS_OK if found,
 *         TIKU_KITS_DS_ERR_NULL if list or node_idx is NULL,
 *         TIKU_KITS_DS_ERR_NOTFOUND if value is not present
 */
int tiku_kits_ds_list_find(const struct tiku_kits_ds_list *list,
                            tiku_kits_ds_elem_t value,
                            uint8_t *node_idx);

/**
 * @brief Read the data stored at a given pool index
 *
 * Copies the element from the node at @p node_idx into the
 * caller-provided location pointed to by @p value.  The node is
 * validated by walking the active chain to confirm it is reachable,
 * preventing reads from freed or out-of-range pool slots.
 *
 * @param list     List (must not be NULL)
 * @param node_idx Pool index of the node to read (must be an active
 *                 node reachable from head)
 * @param value    Output pointer where the element is written (must
 *                 not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if list or value is NULL,
 *         TIKU_KITS_DS_ERR_BOUNDS if node_idx is not an active node
 */
int tiku_kits_ds_list_get(const struct tiku_kits_ds_list *list,
                           uint8_t node_idx,
                           tiku_kits_ds_elem_t *value);

/*---------------------------------------------------------------------------*/
/* STATE OPERATIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Clear the list, returning all nodes to the free pool
 *
 * Re-initializes the list by rebuilding the free chain and resetting
 * the active head to LIST_NONE.  This is equivalent to calling init
 * again with the same capacity.  The backing pool is zeroed.
 *
 * @param list List (must not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if list is NULL
 */
int tiku_kits_ds_list_clear(struct tiku_kits_ds_list *list);

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Return the current number of active nodes
 *
 * Returns the logical size (number of nodes in the active chain),
 * not the pool capacity.  Safe to call with a NULL pointer --
 * returns 0.
 *
 * @param list List, or NULL
 * @return Number of active nodes, or 0 if list is NULL
 */
uint16_t tiku_kits_ds_list_size(const struct tiku_kits_ds_list *list);

/**
 * @brief Return the pool index of the head node
 *
 * Returns the pool index of the first node in the active chain.
 * If the list is empty or NULL, returns LIST_NONE.  Use this as
 * the starting point for manual iteration with list_next().
 *
 * @param list List, or NULL
 * @return Pool index of the head node, or LIST_NONE if the list is
 *         NULL or empty
 */
uint8_t tiku_kits_ds_list_head(const struct tiku_kits_ds_list *list);

/**
 * @brief Return the next-node index for a given node
 *
 * Returns the pool index of the successor of @p node_idx in the
 * active chain.  The node is validated by walking the active chain
 * to confirm it is reachable, preventing traversal through freed
 * slots.  Returns LIST_NONE if the node is the tail, if the list
 * is NULL, or if node_idx is invalid.
 *
 * @param list     List, or NULL
 * @param node_idx Pool index of the current node
 * @return Pool index of the next active node, or LIST_NONE if
 *         node_idx is invalid, is the tail, or list is NULL
 */
uint8_t tiku_kits_ds_list_next(const struct tiku_kits_ds_list *list,
                                uint8_t node_idx);

#endif /* TIKU_KITS_DS_LIST_H_ */
