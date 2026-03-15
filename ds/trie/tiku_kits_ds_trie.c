/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ds_trie.c - Nibble trie (prefix tree) with pool-allocated nodes
 *
 * Platform-independent implementation of a nibble trie. All node storage
 * is statically allocated from a fixed-size pool; no heap usage. Each
 * byte of a key is split into high and low 4-bit nibbles, giving a
 * branching factor of 16 per level.
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

#include "tiku_kits_ds_trie.h"
#include <string.h>

/*---------------------------------------------------------------------------*/
/* INTERNAL HELPERS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a single node to empty state
 * @param node Node to initialize
 */
static void init_node(struct tiku_kits_ds_trie_node *node)
{
    uint16_t i;

    for (i = 0; i < TIKU_KITS_DS_TRIE_ALPHABET_SIZE; i++) {
        node->children[i] = TIKU_KITS_DS_TRIE_NONE;
    }
    node->is_terminal = 0;
    node->value = 0;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Allocate a new node from the static pool
 * @param trie Trie
 * @return Node index, or TIKU_KITS_DS_TRIE_NONE if pool full
 */
static uint16_t alloc_node(struct tiku_kits_ds_trie *trie)
{
    uint16_t idx;

    if (trie->pool_used >= TIKU_KITS_DS_TRIE_MAX_NODES) {
        return TIKU_KITS_DS_TRIE_NONE;
    }

    idx = trie->pool_used;
    trie->pool_used++;

    init_node(&trie->nodes[idx]);

    return idx;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Extract the high nibble (bits 7..4) of a byte
 * @param b Input byte
 * @return High nibble (0..15)
 */
static uint8_t high_nibble(uint8_t b)
{
    return (uint8_t)((b >> 4) & 0x0F);
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Extract the low nibble (bits 3..0) of a byte
 * @param b Input byte
 * @return Low nibble (0..15)
 */
static uint8_t low_nibble(uint8_t b)
{
    return (uint8_t)(b & 0x0F);
}

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

int tiku_kits_ds_trie_init(struct tiku_kits_ds_trie *trie)
{
    if (trie == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    memset(trie->nodes, 0, sizeof(trie->nodes));
    trie->pool_used = 0;
    trie->num_keys = 0;

    /* Allocate root node (always index 0) */
    alloc_node(trie);

    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* INSERT / SEARCH / REMOVE                                                  */
/*---------------------------------------------------------------------------*/

int tiku_kits_ds_trie_insert(struct tiku_kits_ds_trie *trie,
                              const uint8_t *key,
                              uint16_t key_len,
                              tiku_kits_ds_elem_t value)
{
    uint16_t cur;
    uint16_t i;
    uint8_t nibble;
    uint16_t child;

    if (trie == NULL || key == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    if (key_len == 0) {
        return TIKU_KITS_DS_ERR_PARAM;
    }

    /* Start at root (node 0) */
    cur = 0;

    for (i = 0; i < key_len; i++) {
        /* High nibble first */
        nibble = high_nibble(key[i]);
        child = trie->nodes[cur].children[nibble];
        if (child == TIKU_KITS_DS_TRIE_NONE) {
            child = alloc_node(trie);
            if (child == TIKU_KITS_DS_TRIE_NONE) {
                return TIKU_KITS_DS_ERR_FULL;
            }
            trie->nodes[cur].children[nibble] = child;
        }
        cur = child;

        /* Low nibble second */
        nibble = low_nibble(key[i]);
        child = trie->nodes[cur].children[nibble];
        if (child == TIKU_KITS_DS_TRIE_NONE) {
            child = alloc_node(trie);
            if (child == TIKU_KITS_DS_TRIE_NONE) {
                return TIKU_KITS_DS_ERR_FULL;
            }
            trie->nodes[cur].children[nibble] = child;
        }
        cur = child;
    }

    /* Mark terminal and store value */
    if (!trie->nodes[cur].is_terminal) {
        trie->num_keys++;
    }
    trie->nodes[cur].is_terminal = 1;
    trie->nodes[cur].value = value;

    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ds_trie_search(
    const struct tiku_kits_ds_trie *trie,
    const uint8_t *key,
    uint16_t key_len,
    tiku_kits_ds_elem_t *value)
{
    uint16_t cur;
    uint16_t i;
    uint8_t nibble;
    uint16_t child;

    if (trie == NULL || key == NULL || value == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    if (key_len == 0) {
        return TIKU_KITS_DS_ERR_PARAM;
    }

    /* Start at root (node 0) */
    cur = 0;

    for (i = 0; i < key_len; i++) {
        /* High nibble */
        nibble = high_nibble(key[i]);
        child = trie->nodes[cur].children[nibble];
        if (child == TIKU_KITS_DS_TRIE_NONE) {
            return TIKU_KITS_DS_ERR_NOTFOUND;
        }
        cur = child;

        /* Low nibble */
        nibble = low_nibble(key[i]);
        child = trie->nodes[cur].children[nibble];
        if (child == TIKU_KITS_DS_TRIE_NONE) {
            return TIKU_KITS_DS_ERR_NOTFOUND;
        }
        cur = child;
    }

    if (!trie->nodes[cur].is_terminal) {
        return TIKU_KITS_DS_ERR_NOTFOUND;
    }

    *value = trie->nodes[cur].value;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ds_trie_contains(
    const struct tiku_kits_ds_trie *trie,
    const uint8_t *key,
    uint16_t key_len)
{
    uint16_t cur;
    uint16_t i;
    uint8_t nibble;
    uint16_t child;

    if (trie == NULL || key == NULL || key_len == 0) {
        return 0;
    }

    /* Start at root (node 0) */
    cur = 0;

    for (i = 0; i < key_len; i++) {
        /* High nibble */
        nibble = high_nibble(key[i]);
        child = trie->nodes[cur].children[nibble];
        if (child == TIKU_KITS_DS_TRIE_NONE) {
            return 0;
        }
        cur = child;

        /* Low nibble */
        nibble = low_nibble(key[i]);
        child = trie->nodes[cur].children[nibble];
        if (child == TIKU_KITS_DS_TRIE_NONE) {
            return 0;
        }
        cur = child;
    }

    return trie->nodes[cur].is_terminal ? 1 : 0;
}

/*---------------------------------------------------------------------------*/

int tiku_kits_ds_trie_remove(struct tiku_kits_ds_trie *trie,
                              const uint8_t *key,
                              uint16_t key_len)
{
    uint16_t cur;
    uint16_t i;
    uint8_t nibble;
    uint16_t child;

    if (trie == NULL || key == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    if (key_len == 0) {
        return TIKU_KITS_DS_ERR_PARAM;
    }

    /* Start at root (node 0) */
    cur = 0;

    for (i = 0; i < key_len; i++) {
        /* High nibble */
        nibble = high_nibble(key[i]);
        child = trie->nodes[cur].children[nibble];
        if (child == TIKU_KITS_DS_TRIE_NONE) {
            return TIKU_KITS_DS_ERR_NOTFOUND;
        }
        cur = child;

        /* Low nibble */
        nibble = low_nibble(key[i]);
        child = trie->nodes[cur].children[nibble];
        if (child == TIKU_KITS_DS_TRIE_NONE) {
            return TIKU_KITS_DS_ERR_NOTFOUND;
        }
        cur = child;
    }

    if (!trie->nodes[cur].is_terminal) {
        return TIKU_KITS_DS_ERR_NOTFOUND;
    }

    /* Lazy deletion: unmark terminal, decrement count */
    trie->nodes[cur].is_terminal = 0;
    trie->num_keys--;

    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* QUERY                                                                     */
/*---------------------------------------------------------------------------*/

uint16_t tiku_kits_ds_trie_count(
    const struct tiku_kits_ds_trie *trie)
{
    if (trie == NULL) {
        return 0;
    }

    return trie->num_keys;
}

/*---------------------------------------------------------------------------*/
/* CLEAR                                                                     */
/*---------------------------------------------------------------------------*/

int tiku_kits_ds_trie_clear(struct tiku_kits_ds_trie *trie)
{
    if (trie == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    memset(trie->nodes, 0, sizeof(trie->nodes));
    trie->pool_used = 0;
    trie->num_keys = 0;

    /* Re-allocate root node (index 0) */
    alloc_node(trie);

    return TIKU_KITS_DS_OK;
}
