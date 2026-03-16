/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ds_list.c - Pool-allocated singly linked list implementation
 *
 * Platform-independent implementation of a pool-allocated singly linked
 * list. The free list is a singly linked chain through the pool array.
 * Allocate = pop from free head; deallocate = push to free head.
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

#include "tiku_kits_ds_list.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* INTERNAL HELPERS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Allocate a node from the free list
 *
 * Pops the head of the free chain and returns its pool index.
 * O(1) -- no traversal needed because the free head is tracked
 * explicitly.  The allocated node's next pointer is reset to
 * LIST_NONE so it does not carry stale free-chain links into
 * the active chain.
 */
static uint8_t pool_alloc(struct tiku_kits_ds_list *list)
{
    uint8_t idx;

    if (list->free == TIKU_KITS_DS_LIST_NONE) {
        return TIKU_KITS_DS_LIST_NONE;
    }

    idx = list->free;
    list->free = list->pool[idx].next;
    /* Clear the free-chain link so the node enters the active chain
     * without a stale successor reference. */
    list->pool[idx].next = TIKU_KITS_DS_LIST_NONE;
    return idx;
}

/**
 * @brief Return a node to the free list
 *
 * Pushes @p idx onto the head of the free chain.  The node's data
 * field is zeroed for safety so that stale element values do not
 * persist in freed slots.  O(1) -- no traversal needed.
 */
static void pool_free(struct tiku_kits_ds_list *list, uint8_t idx)
{
    list->pool[idx].data = 0;
    /* Push onto free head -- the freed node becomes the new free head
     * and its next pointer links to the previous free head. */
    list->pool[idx].next = list->free;
    list->free = idx;
}

/**
 * @brief Check whether a node index is a valid active node
 *
 * Walks the active chain from head to tail and returns 1 if
 * @p node_idx is encountered.  This O(n) validation prevents
 * callers from accidentally reading or modifying freed pool slots
 * by supplying a stale index.  A simple bounds check is performed
 * first to reject obviously invalid indices without traversal.
 */
static int is_active_node(const struct tiku_kits_ds_list *list,
                           uint8_t node_idx)
{
    uint8_t cur;

    /* Quick reject: index beyond the runtime capacity is never valid */
    if (node_idx >= list->capacity) {
        return 0;
    }

    /* Walk the active chain to confirm reachability */
    cur = list->head;
    while (cur != TIKU_KITS_DS_LIST_NONE) {
        if (cur == node_idx) {
            return 1;
        }
        cur = list->pool[cur].next;
    }
    return 0;
}

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a linked list with the given capacity
 *
 * Zeros the entire pool and threads a free chain through the first
 * @p capacity slots.  The active head is set to LIST_NONE (empty).
 * The runtime capacity is clamped to LIST_MAX_NODES at compile time
 * so the static pool is never overrun.
 */
int tiku_kits_ds_list_init(struct tiku_kits_ds_list *list,
                            uint16_t capacity)
{
    uint16_t i;

    if (list == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (capacity == 0 || capacity > TIKU_KITS_DS_LIST_MAX_NODES) {
        return TIKU_KITS_DS_ERR_PARAM;
    }

    /* Zero the full static pool so that uninitialised reads return
     * deterministic values regardless of prior contents. */
    memset(list->pool, 0, sizeof(list->pool));

    /* Build the free chain: each node points to the next index,
     * forming a singly linked list of available slots. */
    for (i = 0; i < capacity - 1; i++) {
        list->pool[i].next = (uint8_t)(i + 1);
    }
    list->pool[capacity - 1].next = TIKU_KITS_DS_LIST_NONE;

    list->head = TIKU_KITS_DS_LIST_NONE;
    list->free = 0;
    list->count = 0;
    list->capacity = capacity;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* INSERT OPERATIONS                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Insert an element at the front of the list
 *
 * O(1) -- allocates a free node, writes the value, and links it as
 * the new head by pointing its next to the previous head.
 */
int tiku_kits_ds_list_push_front(struct tiku_kits_ds_list *list,
                                  tiku_kits_ds_elem_t value)
{
    uint8_t idx;

    if (list == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    idx = pool_alloc(list);
    if (idx == TIKU_KITS_DS_LIST_NONE) {
        return TIKU_KITS_DS_ERR_FULL;
    }

    list->pool[idx].data = value;
    /* Link new node before the current head */
    list->pool[idx].next = list->head;
    list->head = idx;
    list->count++;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Append an element at the end of the list
 *
 * O(n) -- must traverse the entire active chain to locate the tail
 * because no tail index is maintained (saving 1 byte of struct
 * overhead).  If the list is empty, the new node becomes the head.
 */
int tiku_kits_ds_list_push_back(struct tiku_kits_ds_list *list,
                                 tiku_kits_ds_elem_t value)
{
    uint8_t idx;
    uint8_t cur;

    if (list == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    idx = pool_alloc(list);
    if (idx == TIKU_KITS_DS_LIST_NONE) {
        return TIKU_KITS_DS_ERR_FULL;
    }

    list->pool[idx].data = value;
    list->pool[idx].next = TIKU_KITS_DS_LIST_NONE;

    if (list->head == TIKU_KITS_DS_LIST_NONE) {
        /* Empty list: new node becomes head */
        list->head = idx;
    } else {
        /* Traverse to find the tail -- the last node whose next is
         * LIST_NONE -- then link the new node after it. */
        cur = list->head;
        while (list->pool[cur].next != TIKU_KITS_DS_LIST_NONE) {
            cur = list->pool[cur].next;
        }
        list->pool[cur].next = idx;
    }

    list->count++;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Insert a new node after the node at the given pool index
 *
 * Validates that @p node_idx is in the active chain (O(n) walk),
 * then allocates a free node and splices it in between node_idx
 * and its current successor.  The splice itself is O(1).
 */
int tiku_kits_ds_list_insert_after(struct tiku_kits_ds_list *list,
                                    uint8_t node_idx,
                                    tiku_kits_ds_elem_t value)
{
    uint8_t idx;

    if (list == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    /* Validate that node_idx is reachable from head before modifying
     * the chain -- prevents splicing into the free chain. */
    if (!is_active_node(list, node_idx)) {
        return TIKU_KITS_DS_ERR_BOUNDS;
    }

    idx = pool_alloc(list);
    if (idx == TIKU_KITS_DS_LIST_NONE) {
        return TIKU_KITS_DS_ERR_FULL;
    }

    /* Splice: new node inherits node_idx's successor, then node_idx
     * points to the new node. */
    list->pool[idx].data = value;
    list->pool[idx].next = list->pool[node_idx].next;
    list->pool[node_idx].next = idx;
    list->count++;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* REMOVE OPERATIONS                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Remove and return the front element from the list
 *
 * O(1) -- unlinks the head node, copies its data to the caller,
 * and returns the node to the free pool.  The new head becomes the
 * old head's successor.
 */
int tiku_kits_ds_list_pop_front(struct tiku_kits_ds_list *list,
                                 tiku_kits_ds_elem_t *value)
{
    uint8_t idx;

    if (list == NULL || value == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (list->head == TIKU_KITS_DS_LIST_NONE) {
        return TIKU_KITS_DS_ERR_EMPTY;
    }

    idx = list->head;
    *value = list->pool[idx].data;
    /* Advance head before freeing the node so the active chain
     * remains consistent. */
    list->head = list->pool[idx].next;
    pool_free(list, idx);
    list->count--;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Remove the node at the given pool index
 *
 * O(n) worst case -- must traverse the active chain to find the
 * predecessor so the predecessor's next pointer can be patched.
 * Head removal is a special O(1) case handled first to avoid the
 * traversal when possible.  The removed node is returned to the
 * free pool.
 */
int tiku_kits_ds_list_remove(struct tiku_kits_ds_list *list,
                              uint8_t node_idx)
{
    uint8_t cur;

    if (list == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (list->head == TIKU_KITS_DS_LIST_NONE) {
        return TIKU_KITS_DS_ERR_BOUNDS;
    }
    if (node_idx >= list->capacity) {
        return TIKU_KITS_DS_ERR_BOUNDS;
    }

    /* Special case: removing the head node -- no predecessor search */
    if (list->head == node_idx) {
        list->head = list->pool[node_idx].next;
        pool_free(list, node_idx);
        list->count--;
        return TIKU_KITS_DS_OK;
    }

    /* Traverse to find the predecessor of node_idx so its next
     * pointer can be patched to skip the removed node. */
    cur = list->head;
    while (cur != TIKU_KITS_DS_LIST_NONE) {
        if (list->pool[cur].next == node_idx) {
            list->pool[cur].next = list->pool[node_idx].next;
            pool_free(list, node_idx);
            list->count--;
            return TIKU_KITS_DS_OK;
        }
        cur = list->pool[cur].next;
    }

    /* node_idx is not in the active chain */
    return TIKU_KITS_DS_ERR_BOUNDS;
}

/*---------------------------------------------------------------------------*/
/* SEARCH AND ACCESS                                                         */
/*---------------------------------------------------------------------------*/

/**
 * @brief Linear search for the first occurrence of a value
 *
 * Walks the active chain from head to tail and returns the pool
 * index of the first node whose data matches @p value.  O(n) worst
 * case.  The search is inherently linear because the list is
 * unsorted.
 */
int tiku_kits_ds_list_find(const struct tiku_kits_ds_list *list,
                            tiku_kits_ds_elem_t value,
                            uint8_t *node_idx)
{
    uint8_t cur;

    if (list == NULL || node_idx == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    /* Forward walk through the active chain -- stop at first match */
    cur = list->head;
    while (cur != TIKU_KITS_DS_LIST_NONE) {
        if (list->pool[cur].data == value) {
            *node_idx = cur;
            return TIKU_KITS_DS_OK;
        }
        cur = list->pool[cur].next;
    }

    return TIKU_KITS_DS_ERR_NOTFOUND;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Read the data stored at a given pool index
 *
 * Validates that @p node_idx is in the active chain (O(n) walk),
 * then copies the element to the caller.  The active-chain check
 * prevents reads from freed slots that still contain stale data.
 */
int tiku_kits_ds_list_get(const struct tiku_kits_ds_list *list,
                           uint8_t node_idx,
                           tiku_kits_ds_elem_t *value)
{
    if (list == NULL || value == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    /* Reject reads from freed or out-of-range pool slots */
    if (!is_active_node(list, node_idx)) {
        return TIKU_KITS_DS_ERR_BOUNDS;
    }

    *value = list->pool[node_idx].data;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* STATE OPERATIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Clear the list, returning all nodes to the free pool
 *
 * Delegates to init() with the existing capacity, which rebuilds
 * the free chain and zeros the pool.  This is simpler and safer
 * than walking the active chain and freeing nodes one by one.
 */
int tiku_kits_ds_list_clear(struct tiku_kits_ds_list *list)
{
    if (list == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    /* Re-initialize to return all nodes to the free pool --
     * reuses the validated init logic rather than duplicating it. */
    return tiku_kits_ds_list_init(list, list->capacity);
}

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

/**
 * @brief Return the current number of active nodes
 *
 * Safe to call with a NULL pointer -- returns 0 rather than
 * dereferencing.  Returns the active count, not the pool capacity.
 */
uint16_t tiku_kits_ds_list_size(const struct tiku_kits_ds_list *list)
{
    if (list == NULL) {
        return 0;
    }
    return list->count;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Return the pool index of the head node
 *
 * Safe to call with a NULL pointer -- returns LIST_NONE.  Use this
 * as the starting point for manual iteration with list_next().
 */
uint8_t tiku_kits_ds_list_head(const struct tiku_kits_ds_list *list)
{
    if (list == NULL) {
        return TIKU_KITS_DS_LIST_NONE;
    }
    return list->head;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Return the next-node index for a given node
 *
 * Validates that @p node_idx is in the active chain before
 * returning its successor.  This O(n) validation prevents callers
 * from accidentally following freed-chain links by supplying a
 * stale index.
 */
uint8_t tiku_kits_ds_list_next(const struct tiku_kits_ds_list *list,
                                uint8_t node_idx)
{
    if (list == NULL) {
        return TIKU_KITS_DS_LIST_NONE;
    }
    if (node_idx >= list->capacity) {
        return TIKU_KITS_DS_LIST_NONE;
    }
    /* Confirm the node is in the active chain before exposing its
     * next pointer -- a freed node's next leads into the free chain. */
    if (!is_active_node(list, node_idx)) {
        return TIKU_KITS_DS_LIST_NONE;
    }
    return list->pool[node_idx].next;
}
