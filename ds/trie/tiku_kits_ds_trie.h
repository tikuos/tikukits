/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ds_trie.h - Nibble trie (prefix tree) with pool-allocated nodes
 *
 * Platform-independent nibble trie for embedded byte-key lookups. Each
 * byte of a key is split into two 4-bit nibbles, yielding a branching
 * factor of 16 per level. All node storage is statically allocated from
 * a fixed-size pool. The element type is inherited from the DS common
 * header.
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

#ifndef TIKU_KITS_DS_TRIE_H_
#define TIKU_KITS_DS_TRIE_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_ds.h"

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/**
 * Maximum number of nodes in the static pool.
 * Override before including this header to change the limit.
 */
#ifndef TIKU_KITS_DS_TRIE_MAX_NODES
#define TIKU_KITS_DS_TRIE_MAX_NODES 32
#endif

/**
 * Alphabet size for the nibble trie. Each byte is split into two
 * 4-bit nibbles, so the branching factor is 16.
 */
#ifndef TIKU_KITS_DS_TRIE_ALPHABET_SIZE
#define TIKU_KITS_DS_TRIE_ALPHABET_SIZE 16
#endif

/** Sentinel value indicating no child node */
#define TIKU_KITS_DS_TRIE_NONE ((uint16_t)0xFFFF)

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_ds_trie_node
 * @brief Single trie node with child indices and optional value
 *
 * Children are stored as uint16_t indices into the parent trie's
 * node pool. TIKU_KITS_DS_TRIE_NONE indicates an absent child.
 */
struct tiku_kits_ds_trie_node {
    uint16_t children[TIKU_KITS_DS_TRIE_ALPHABET_SIZE];
    uint8_t is_terminal;        /**< 1 if a key ends here         */
    tiku_kits_ds_elem_t value;  /**< Stored value for terminals   */
};

/**
 * @struct tiku_kits_ds_trie
 * @brief Nibble trie with static node pool
 *
 * Nodes are allocated sequentially from the pool. Node 0 is
 * always the root, allocated during init.
 *
 * Example:
 * @code
 *   struct tiku_kits_ds_trie t;
 *   uint8_t key[] = {0xAB, 0xCD};
 *   tiku_kits_ds_elem_t val;
 *
 *   tiku_kits_ds_trie_init(&t);
 *   tiku_kits_ds_trie_insert(&t, key, 2, 42);
 *   tiku_kits_ds_trie_search(&t, key, 2, &val); // val == 42
 * @endcode
 */
struct tiku_kits_ds_trie {
    struct tiku_kits_ds_trie_node
        nodes[TIKU_KITS_DS_TRIE_MAX_NODES];
    uint16_t pool_used;  /**< Number of allocated nodes          */
    uint16_t num_keys;   /**< Number of terminal keys stored     */
};

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/**
 * @brief Initialize a trie to empty state
 *
 * Allocates the root node (index 0) from the pool.
 *
 * @param trie Trie to initialize
 * @return TIKU_KITS_DS_OK or TIKU_KITS_DS_ERR_NULL
 */
int tiku_kits_ds_trie_init(struct tiku_kits_ds_trie *trie);

/*---------------------------------------------------------------------------*/
/* INSERT / SEARCH / REMOVE                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Insert a key-value pair into the trie
 *
 * The key is an arbitrary byte array of length key_len. Each byte
 * is split into two 4-bit nibbles for traversal. If the key
 * already exists, its value is overwritten.
 *
 * @param trie    Trie
 * @param key     Byte array key
 * @param key_len Length of key in bytes
 * @param value   Value to associate with the key
 * @return TIKU_KITS_DS_OK, TIKU_KITS_DS_ERR_NULL,
 *         TIKU_KITS_DS_ERR_PARAM (key_len == 0), or
 *         TIKU_KITS_DS_ERR_FULL (node pool exhausted)
 */
int tiku_kits_ds_trie_insert(struct tiku_kits_ds_trie *trie,
                              const uint8_t *key,
                              uint16_t key_len,
                              tiku_kits_ds_elem_t value);

/**
 * @brief Search for a key and retrieve its value
 *
 * @param trie    Trie
 * @param key     Byte array key
 * @param key_len Length of key in bytes
 * @param value   Output: value associated with the key
 * @return TIKU_KITS_DS_OK if found,
 *         TIKU_KITS_DS_ERR_NOTFOUND if not found,
 *         TIKU_KITS_DS_ERR_NULL, or
 *         TIKU_KITS_DS_ERR_PARAM (key_len == 0)
 */
int tiku_kits_ds_trie_search(
    const struct tiku_kits_ds_trie *trie,
    const uint8_t *key,
    uint16_t key_len,
    tiku_kits_ds_elem_t *value);

/**
 * @brief Check whether a key exists in the trie
 *
 * @param trie    Trie
 * @param key     Byte array key
 * @param key_len Length of key in bytes
 * @return 1 if key exists, 0 otherwise
 */
int tiku_kits_ds_trie_contains(
    const struct tiku_kits_ds_trie *trie,
    const uint8_t *key,
    uint16_t key_len);

/**
 * @brief Remove a key from the trie (lazy deletion)
 *
 * Marks the terminal node as non-terminal and decrements the
 * key count. Nodes are not reclaimed.
 *
 * @param trie    Trie
 * @param key     Byte array key
 * @param key_len Length of key in bytes
 * @return TIKU_KITS_DS_OK,
 *         TIKU_KITS_DS_ERR_NOTFOUND if key does not exist,
 *         TIKU_KITS_DS_ERR_NULL, or
 *         TIKU_KITS_DS_ERR_PARAM (key_len == 0)
 */
int tiku_kits_ds_trie_remove(struct tiku_kits_ds_trie *trie,
                              const uint8_t *key,
                              uint16_t key_len);

/*---------------------------------------------------------------------------*/
/* QUERY                                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Return the number of keys stored in the trie
 * @param trie Trie
 * @return Key count, or 0 if trie is NULL
 */
uint16_t tiku_kits_ds_trie_count(
    const struct tiku_kits_ds_trie *trie);

/*---------------------------------------------------------------------------*/
/* CLEAR                                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Reset the trie to empty state
 *
 * Clears all nodes and re-allocates the root.
 *
 * @param trie Trie to clear
 * @return TIKU_KITS_DS_OK or TIKU_KITS_DS_ERR_NULL
 */
int tiku_kits_ds_trie_clear(struct tiku_kits_ds_trie *trie);

#endif /* TIKU_KITS_DS_TRIE_H_ */
