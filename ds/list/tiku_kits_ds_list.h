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
 * Maximum number of nodes the pool can hold.
 * Override before including this header to change the limit.
 */
#ifndef TIKU_KITS_DS_LIST_MAX_NODES
#define TIKU_KITS_DS_LIST_MAX_NODES 16
#endif

/** Sentinel value indicating no node (end of list / invalid index). */
#define TIKU_KITS_DS_LIST_NONE 0xFF

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_ds_list_node
 * @brief A single node in the pool-allocated linked list
 *
 * Each node stores one element and an index pointing to the next
 * node in either the active list or the free list.
 */
struct tiku_kits_ds_list_node {
    tiku_kits_ds_elem_t data;   /**< Stored element value */
    uint8_t next;               /**< Index into pool, or LIST_NONE */
};

/**
 * @struct tiku_kits_ds_list
 * @brief Pool-allocated singly linked list with static storage
 *
 * The pool array serves as both active and free storage. The free
 * list is threaded through the pool using each node's next field.
 * Allocation pops from the free head; deallocation pushes back.
 *
 * Example:
 * @code
 *   struct tiku_kits_ds_list lst;
 *   tiku_kits_ds_list_init(&lst, 8);
 *   tiku_kits_ds_list_push_front(&lst, 42);
 * @endcode
 */
struct tiku_kits_ds_list {
    struct tiku_kits_ds_list_node pool[TIKU_KITS_DS_LIST_MAX_NODES];
    uint8_t head;       /**< Index of first active node, or LIST_NONE */
    uint8_t free;       /**< Index of first free node */
    uint16_t count;     /**< Number of active nodes */
    uint16_t capacity;  /**< User-requested capacity */
};

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a linked list with the given capacity
 * @param list     List to initialize
 * @param capacity Maximum number of nodes (1..LIST_MAX_NODES)
 * @return TIKU_KITS_DS_OK or error code
 */
int tiku_kits_ds_list_init(struct tiku_kits_ds_list *list,
                            uint16_t capacity);

/*---------------------------------------------------------------------------*/
/* INSERT OPERATIONS                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Insert an element at the front of the list
 * @param list  List
 * @param value Value to insert
 * @return TIKU_KITS_DS_OK or TIKU_KITS_DS_ERR_FULL
 */
int tiku_kits_ds_list_push_front(struct tiku_kits_ds_list *list,
                                  tiku_kits_ds_elem_t value);

/**
 * @brief Append an element at the end of the list
 * @param list  List
 * @param value Value to append
 * @return TIKU_KITS_DS_OK or TIKU_KITS_DS_ERR_FULL
 */
int tiku_kits_ds_list_push_back(struct tiku_kits_ds_list *list,
                                 tiku_kits_ds_elem_t value);

/**
 * @brief Insert a new node after the node at the given index
 * @param list     List
 * @param node_idx Index of the node to insert after
 * @param value    Value for the new node
 * @return TIKU_KITS_DS_OK, TIKU_KITS_DS_ERR_FULL, or
 *         TIKU_KITS_DS_ERR_BOUNDS
 */
int tiku_kits_ds_list_insert_after(struct tiku_kits_ds_list *list,
                                    uint8_t node_idx,
                                    tiku_kits_ds_elem_t value);

/*---------------------------------------------------------------------------*/
/* REMOVE OPERATIONS                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Remove the front element from the list
 * @param list  List
 * @param value Output: removed element
 * @return TIKU_KITS_DS_OK or TIKU_KITS_DS_ERR_EMPTY
 */
int tiku_kits_ds_list_pop_front(struct tiku_kits_ds_list *list,
                                 tiku_kits_ds_elem_t *value);

/**
 * @brief Remove the node at the given pool index
 * @param list     List
 * @param node_idx Pool index of the node to remove
 * @return TIKU_KITS_DS_OK or TIKU_KITS_DS_ERR_BOUNDS
 *
 * Traverses the list to find the predecessor and unlinks the node.
 */
int tiku_kits_ds_list_remove(struct tiku_kits_ds_list *list,
                              uint8_t node_idx);

/*---------------------------------------------------------------------------*/
/* SEARCH AND ACCESS                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Linear search for a value in the list
 * @param list     List
 * @param value    Value to search for
 * @param node_idx Output: pool index of the first matching node
 * @return TIKU_KITS_DS_OK or TIKU_KITS_DS_ERR_NOTFOUND
 */
int tiku_kits_ds_list_find(const struct tiku_kits_ds_list *list,
                            tiku_kits_ds_elem_t value,
                            uint8_t *node_idx);

/**
 * @brief Get the data stored at a given pool index
 * @param list     List
 * @param node_idx Pool index of the node
 * @param value    Output: element value
 * @return TIKU_KITS_DS_OK or TIKU_KITS_DS_ERR_BOUNDS
 */
int tiku_kits_ds_list_get(const struct tiku_kits_ds_list *list,
                           uint8_t node_idx,
                           tiku_kits_ds_elem_t *value);

/*---------------------------------------------------------------------------*/
/* STATE OPERATIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Clear the list, returning all nodes to the free pool
 * @param list List
 * @return TIKU_KITS_DS_OK or error code
 */
int tiku_kits_ds_list_clear(struct tiku_kits_ds_list *list);

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Get the current number of active nodes
 * @param list List
 * @return Current size, or 0 if list is NULL
 */
uint16_t tiku_kits_ds_list_size(const struct tiku_kits_ds_list *list);

/**
 * @brief Get the pool index of the head node
 * @param list List
 * @return Head index, or LIST_NONE if NULL or empty
 */
uint8_t tiku_kits_ds_list_head(const struct tiku_kits_ds_list *list);

/**
 * @brief Get the next-node index for a given node
 * @param list     List
 * @param node_idx Pool index of the current node
 * @return Next-node index, or LIST_NONE if invalid
 */
uint8_t tiku_kits_ds_list_next(const struct tiku_kits_ds_list *list,
                                uint8_t node_idx);

#endif /* TIKU_KITS_DS_LIST_H_ */
