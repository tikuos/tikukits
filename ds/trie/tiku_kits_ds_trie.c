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
 * @brief Initialize a single node to empty state.
 *
 * Sets all child slots to TRIE_NONE and clears the terminal flag
 * and value.  Called by alloc_node() each time a new node is
 * taken from the pool.
 */
static void init_node(struct tiku_kits_ds_trie_node *node)
{
    uint16_t i;

    /* Fill every child slot with the "absent" sentinel so that
     * traversal can distinguish allocated children from empty ones. */
    for (i = 0; i < TIKU_KITS_DS_TRIE_ALPHABET_SIZE; i++) {
        node->children[i] = TIKU_KITS_DS_TRIE_NONE;
    }
    node->is_terminal = 0;
    node->value = 0;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Allocate a new node from the static pool.
 *
 * Uses a simple bump allocator: the next free node is always at
 * nodes[pool_used].  After allocation pool_used is incremented.
 * The newly allocated node is initialized to empty state via
 * init_node().  Returns TRIE_NONE if the pool is exhausted.
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
 * @brief Extract the high nibble (bits 7..4) of a byte.
 *
 * Shifts right by 4 and masks to isolate the upper nibble.
 * Used as the first of two trie-level indices per key byte.
 */
static uint8_t high_nibble(uint8_t b)
{
    return (uint8_t)((b >> 4) & 0x0F);
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Extract the low nibble (bits 3..0) of a byte.
 *
 * Masks to isolate the lower nibble.  Used as the second of two
 * trie-level indices per key byte.
 */
static uint8_t low_nibble(uint8_t b)
{
    return (uint8_t)(b & 0x0F);
}

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a trie to empty state.
 *
 * Zeros the entire node pool for deterministic state, resets the
 * bump allocator and key counter, then allocates the root node
 * (index 0).  The root is always present so that insert/search
 * can assume node 0 exists without an extra check.
 */
int tiku_kits_ds_trie_init(struct tiku_kits_ds_trie *trie)
{
    if (trie == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    memset(trie->nodes, 0, sizeof(trie->nodes));
    trie->pool_used = 0;
    trie->num_keys = 0;

    /* Allocate root node (always index 0) -- this consumes one
     * pool slot so the effective capacity for user keys is
     * MAX_NODES - 1 worth of internal nodes. */
    alloc_node(trie);

    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* INSERT / SEARCH / REMOVE                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Insert a key-value pair into the trie.
 *
 * O(key_len) -- for each key byte, two nibble lookups and
 * potential node allocations are performed.  Existing path nodes
 * are reused (prefix sharing), so keys with common prefixes are
 * stored efficiently.  If the key already exists the value is
 * overwritten and the key count is not incremented.
 *
 * Note: if the pool is exhausted mid-insert, the partially
 * created path remains in the trie.  This is harmless because
 * the terminal flag is only set after the full path succeeds.
 */
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
        /* High nibble first -- descend or allocate */
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

        /* Low nibble second -- descend or allocate */
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

    /* Only increment the key count for genuinely new keys;
     * overwriting an existing key does not change the count. */
    if (!trie->nodes[cur].is_terminal) {
        trie->num_keys++;
    }
    trie->nodes[cur].is_terminal = 1;
    trie->nodes[cur].value = value;

    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Search for a key and retrieve its value.
 *
 * O(key_len) -- walks the nibble path from root to leaf.  Returns
 * NOTFOUND as soon as a missing child is encountered, so searches
 * for absent keys terminate early when prefixes diverge.  The
 * value is copied out through @p value so the caller owns its own
 * copy.
 */
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
        /* High nibble -- early exit if path breaks */
        nibble = high_nibble(key[i]);
        child = trie->nodes[cur].children[nibble];
        if (child == TIKU_KITS_DS_TRIE_NONE) {
            return TIKU_KITS_DS_ERR_NOTFOUND;
        }
        cur = child;

        /* Low nibble -- early exit if path breaks */
        nibble = low_nibble(key[i]);
        child = trie->nodes[cur].children[nibble];
        if (child == TIKU_KITS_DS_TRIE_NONE) {
            return TIKU_KITS_DS_ERR_NOTFOUND;
        }
        cur = child;
    }

    /* The full path exists but may be a prefix of a longer key
     * rather than a stored key itself. */
    if (!trie->nodes[cur].is_terminal) {
        return TIKU_KITS_DS_ERR_NOTFOUND;
    }

    *value = trie->nodes[cur].value;
    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/

/**
 * @brief Check whether a key exists in the trie.
 *
 * O(key_len) -- same traversal as search() but without copying
 * the value.  Returns 0 on any invalid input rather than an error
 * code, so the function can be used directly in conditionals.
 */
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
        /* High nibble -- early exit if path breaks */
        nibble = high_nibble(key[i]);
        child = trie->nodes[cur].children[nibble];
        if (child == TIKU_KITS_DS_TRIE_NONE) {
            return 0;
        }
        cur = child;

        /* Low nibble -- early exit if path breaks */
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

/**
 * @brief Remove a key from the trie (lazy deletion).
 *
 * O(key_len) -- walks the nibble path to locate the terminal node
 * and clears its is_terminal flag.  The node itself is not freed
 * back to the pool because the bump allocator does not support
 * deallocation.  This keeps the implementation simple at the cost
 * of not reclaiming memory for deleted keys.
 */
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
        /* High nibble -- early exit if path breaks */
        nibble = high_nibble(key[i]);
        child = trie->nodes[cur].children[nibble];
        if (child == TIKU_KITS_DS_TRIE_NONE) {
            return TIKU_KITS_DS_ERR_NOTFOUND;
        }
        cur = child;

        /* Low nibble -- early exit if path breaks */
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

    /* Lazy deletion: unmark terminal, decrement count.
     * The node and its path remain allocated in the pool. */
    trie->nodes[cur].is_terminal = 0;
    trie->num_keys--;

    return TIKU_KITS_DS_OK;
}

/*---------------------------------------------------------------------------*/
/* QUERY                                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Return the number of keys stored in the trie.
 *
 * Safe to call with a NULL pointer -- returns 0 rather than
 * dereferencing.  Returns the terminal key count, not the number
 * of allocated pool nodes.
 */
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

/**
 * @brief Reset the trie to empty state.
 *
 * Zeros the entire node pool, resets the bump allocator and key
 * counter, and re-allocates the root node.  This restores the
 * full pool capacity, unlike lazy deletion which cannot reclaim
 * nodes.
 */
int tiku_kits_ds_trie_clear(struct tiku_kits_ds_trie *trie)
{
    if (trie == NULL) {
        return TIKU_KITS_DS_ERR_NULL;
    }

    memset(trie->nodes, 0, sizeof(trie->nodes));
    trie->pool_used = 0;
    trie->num_keys = 0;

    /* Re-allocate root node (index 0) so that subsequent
     * insert/search calls can assume it exists. */
    alloc_node(trie);

    return TIKU_KITS_DS_OK;
}
