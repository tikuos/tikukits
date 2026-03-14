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
 * @param list List
 * @return Pool index of allocated node, or LIST_NONE if pool is empty
 */
static uint8_t pool_alloc(struct tiku_kits_ds_list *list)
{
    uint8_t idx;

    if (list->free == TIKU_KITS_DS_LIST_NONE) {
        return TIKU_KITS_DS_LIST_NONE;
    }

    idx = list->free;
    list->free = list->pool[idx].next;
    list->pool[idx].next = TIKU_KITS_DS_LIST_NONE;
    return idx;
}

/**
 * @brief Return a node to the free list
 * @param list List
 * @param idx  Pool index of the node to free
 */
static void pool_free(struct tiku_kits_ds_list *list, uint8_t idx)
{
    list->pool[idx].data = 0;
    list->pool[idx].next = list->free;
    list->free = idx;
}

/**
 * @brief Check whether a node index is a valid active node
 * @param list     List
 * @param node_idx Pool index to validate
 * @return 1 if the index is reachable from head, 0 otherwise
 */
static int is_active_node(const struct tiku_kits_ds_list *list,
                           uint8_t node_idx)
{
    uint8_t cur;

    if (node_idx >= list->capacity) {
        return 0;
    }

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

    memset(list->pool, 0, sizeof(list->pool));

    /* Build the free list: each node points to the next index */
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
    list->pool[idx].next = list->head;
    list->head = idx;
    list->count++;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

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
        /* Traverse to find the tail */
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

int tiku_kits_ds_list_insert_after(struct tiku_kits_ds_list *list,
                                    uint8_t node_idx,
                                    tiku_kits_ds_elem_t value)
{
    uint8_t idx;

    if (list == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (!is_active_node(list, node_idx)) {
        return TIKU_KITS_DS_ERR_BOUNDS;
    }

    idx = pool_alloc(list);
    if (idx == TIKU_KITS_DS_LIST_NONE) {
        return TIKU_KITS_DS_ERR_FULL;
    }

    list->pool[idx].data = value;
    list->pool[idx].next = list->pool[node_idx].next;
    list->pool[node_idx].next = idx;
    list->count++;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* REMOVE OPERATIONS                                                         */
/*---------------------------------------------------------------------------*/

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
    list->head = list->pool[idx].next;
    pool_free(list, idx);
    list->count--;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

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

    /* Special case: removing the head node */
    if (list->head == node_idx) {
        list->head = list->pool[node_idx].next;
        pool_free(list, node_idx);
        list->count--;
        return TIKU_KITS_DS_OK;
    }

    /* Traverse to find the predecessor */
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

    /* node_idx is not in the active list */
    return TIKU_KITS_DS_ERR_BOUNDS;
}

/*---------------------------------------------------------------------------*/
/* SEARCH AND ACCESS                                                         */
/*---------------------------------------------------------------------------*/

int tiku_kits_ds_list_find(const struct tiku_kits_ds_list *list,
                            tiku_kits_ds_elem_t value,
                            uint8_t *node_idx)
{
    uint8_t cur;

    if (list == NULL || node_idx == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

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

int tiku_kits_ds_list_get(const struct tiku_kits_ds_list *list,
                           uint8_t node_idx,
                           tiku_kits_ds_elem_t *value)
{
    if (list == NULL || value == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }
    if (!is_active_node(list, node_idx)) {
        return TIKU_KITS_DS_ERR_BOUNDS;
    }

    *value = list->pool[node_idx].data;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* STATE OPERATIONS                                                          */
/*---------------------------------------------------------------------------*/

int tiku_kits_ds_list_clear(struct tiku_kits_ds_list *list)
{
    if (list == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    /* Re-initialize to return all nodes to the free pool */
    return tiku_kits_ds_list_init(list, list->capacity);
}

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

uint16_t tiku_kits_ds_list_size(const struct tiku_kits_ds_list *list)
{
    if (list == NULL) {
        return 0;
    }
    return list->count;
}

/*---------------------------------------------------------------------------*/

uint8_t tiku_kits_ds_list_head(const struct tiku_kits_ds_list *list)
{
    if (list == NULL) {
        return TIKU_KITS_DS_LIST_NONE;
    }
    return list->head;
}

/*---------------------------------------------------------------------------*/

uint8_t tiku_kits_ds_list_next(const struct tiku_kits_ds_list *list,
                                uint8_t node_idx)
{
    if (list == NULL) {
        return TIKU_KITS_DS_LIST_NONE;
    }
    if (node_idx >= list->capacity) {
        return TIKU_KITS_DS_LIST_NONE;
    }
    if (!is_active_node(list, node_idx)) {
        return TIKU_KITS_DS_LIST_NONE;
    }
    return list->pool[node_idx].next;
}
