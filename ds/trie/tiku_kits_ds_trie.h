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
 * @brief Maximum number of nodes in the static pool.
 *
 * This compile-time constant defines the upper bound on the total
 * number of trie nodes that can be allocated.  Each key byte
 * consumes up to two nodes (one per nibble), so a key of length N
 * bytes may need up to 2*N new nodes in the worst case (no shared
 * prefixes).  Choose a value that balances memory usage against
 * the number and length of keys your application needs.
 *
 * Override before including this header to change the limit:
 * @code
 *   #define TIKU_KITS_DS_TRIE_MAX_NODES 128
 *   #include "tiku_kits_ds_trie.h"
 * @endcode
 */
#ifndef TIKU_KITS_DS_TRIE_MAX_NODES
#define TIKU_KITS_DS_TRIE_MAX_NODES 32
#endif

/**
 * @brief Alphabet size for the nibble trie.
 *
 * Each byte of a key is split into two 4-bit nibbles, so the
 * branching factor is 16.  This design trades slightly higher
 * per-node memory (16 child slots) for shallower depth compared
 * to a binary trie: a K-byte key requires 2*K levels rather than
 * 8*K.  This constant should not normally need overriding.
 *
 * Override before including this header to change the limit:
 * @code
 *   #define TIKU_KITS_DS_TRIE_ALPHABET_SIZE 16
 *   #include "tiku_kits_ds_trie.h"
 * @endcode
 */
#ifndef TIKU_KITS_DS_TRIE_ALPHABET_SIZE
#define TIKU_KITS_DS_TRIE_ALPHABET_SIZE 16
#endif

/**
 * @brief Sentinel value indicating no child node.
 *
 * Stored in the children[] array of a trie node to indicate that
 * no child exists for that nibble.  Chosen as 0xFFFF so that it
 * can never collide with a valid pool index (which is bounded by
 * TIKU_KITS_DS_TRIE_MAX_NODES).
 */
#define TIKU_KITS_DS_TRIE_NONE ((uint16_t)0xFFFF)

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/**
 * @struct tiku_kits_ds_trie_node
 * @brief Single trie node with child indices and optional value.
 *
 * Each node holds an array of child indices (one per possible
 * nibble value, 0..15) that point into the parent trie's node
 * pool.  TIKU_KITS_DS_TRIE_NONE indicates an absent child.
 * Terminal nodes (where a complete key ends) have @c is_terminal
 * set to 1 and store the associated value in @c value.
 *
 * @note Element type is controlled by tiku_kits_ds_elem_t (default
 *       int32_t).  Override at compile time with
 *       @c -DTIKU_KITS_DS_ELEM_TYPE=int16_t to change it globally
 *       for all DS sub-modules.
 */
struct tiku_kits_ds_trie_node {
    uint16_t children[TIKU_KITS_DS_TRIE_ALPHABET_SIZE];
                                /**< Child pool indices, or
                                     TRIE_NONE if absent          */
    uint8_t is_terminal;        /**< 1 if a complete key ends at
                                     this node                    */
    tiku_kits_ds_elem_t value;  /**< Value stored at terminal
                                     nodes (undefined if not
                                     terminal)                    */
};

/**
 * @struct tiku_kits_ds_trie
 * @brief Nibble trie with statically allocated node pool.
 *
 * A prefix tree that maps arbitrary byte-array keys to values.
 * Because all node storage lives inside the struct itself (a
 * fixed-size pool), no heap allocation is needed -- just declare
 * the trie as a static or local variable.
 *
 * Nodes are allocated sequentially from the pool via a simple
 * bump allocator.  Node 0 is always the root, allocated during
 * init.  Two counters are tracked:
 *   - @c pool_used -- number of nodes allocated so far.  New
 *     nodes are taken from nodes[pool_used] and pool_used is
 *     incremented.  Once the pool is exhausted, inserts that
 *     require new nodes fail with TIKU_KITS_DS_ERR_FULL.
 *   - @c num_keys -- number of distinct keys currently stored
 *     (i.e. terminal nodes).  Incremented on new inserts,
 *     decremented on removes.
 *
 * Deletion is lazy: the terminal flag is cleared but the node
 * itself is not reclaimed.  This keeps the implementation simple
 * and avoids the complexity of node recycling in a static pool.
 *
 * @note Each key byte maps to two trie levels (high nibble, then
 *       low nibble), so a K-byte key traverses 2*K levels and may
 *       allocate up to 2*K new nodes if no prefix is shared.
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
 *
 *   tiku_kits_ds_trie_remove(&t, key, 2);
 *   // key is removed; val is no longer retrievable
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
 * @brief Initialize a trie to empty state.
 *
 * Zeros the entire node pool, resets the pool allocator and key
 * counter, then allocates the root node (always index 0) from the
 * pool.  After this call the trie is ready for inserts.
 *
 * @param trie Trie to initialize (must not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if trie is NULL
 */
int tiku_kits_ds_trie_init(struct tiku_kits_ds_trie *trie);

/*---------------------------------------------------------------------------*/
/* INSERT / SEARCH / REMOVE                                                  */
/*---------------------------------------------------------------------------*/

/**
 * @brief Insert a key-value pair into the trie.
 *
 * The key is an arbitrary byte array of length @p key_len.  Each
 * byte is split into high and low 4-bit nibbles, yielding two
 * trie levels per byte.  The function walks existing nodes where
 * possible and allocates new ones from the pool as needed.  If the
 * key already exists (terminal node found), its value is silently
 * overwritten and the key count is not incremented.
 *
 * Complexity: O(key_len) -- two node lookups per byte, each O(1).
 *
 * @param trie    Trie (must not be NULL)
 * @param key     Byte array key (must not be NULL)
 * @param key_len Length of key in bytes (must be > 0)
 * @param value   Value to associate with the key
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if trie or key is NULL,
 *         TIKU_KITS_DS_ERR_PARAM if key_len is 0,
 *         TIKU_KITS_DS_ERR_FULL if the node pool is exhausted
 *         before the full key path could be created
 */
int tiku_kits_ds_trie_insert(struct tiku_kits_ds_trie *trie,
                              const uint8_t *key,
                              uint16_t key_len,
                              tiku_kits_ds_elem_t value);

/**
 * @brief Search for a key and retrieve its value.
 *
 * Walks the trie along the nibble path defined by @p key.  If
 * the traversal reaches a terminal node at the end of the key,
 * the stored value is copied into @p value.  If any nibble has
 * no child or the final node is not a terminal, the key is not
 * found.
 *
 * Complexity: O(key_len) -- two lookups per byte, each O(1).
 *
 * @param trie    Trie (must not be NULL)
 * @param key     Byte array key (must not be NULL)
 * @param key_len Length of key in bytes (must be > 0)
 * @param value   Output pointer where the associated value is
 *                written (must not be NULL)
 * @return TIKU_KITS_DS_OK if the key is found,
 *         TIKU_KITS_DS_ERR_NULL if trie, key, or value is NULL,
 *         TIKU_KITS_DS_ERR_PARAM if key_len is 0,
 *         TIKU_KITS_DS_ERR_NOTFOUND if the key does not exist
 */
int tiku_kits_ds_trie_search(
    const struct tiku_kits_ds_trie *trie,
    const uint8_t *key,
    uint16_t key_len,
    tiku_kits_ds_elem_t *value);

/**
 * @brief Check whether a key exists in the trie.
 *
 * Walks the nibble path and returns 1 if a terminal node is found
 * at the end of the key, 0 otherwise.  Unlike search(), this
 * function does not copy the value out, making it suitable for
 * existence checks where the value is not needed.  Returns 0 on
 * any invalid input (NULL pointers or zero-length key) rather
 * than an error code, for convenient use in conditionals.
 *
 * Complexity: O(key_len) -- two lookups per byte, each O(1).
 *
 * @param trie    Trie (may be NULL; returns 0)
 * @param key     Byte array key (may be NULL; returns 0)
 * @param key_len Length of key in bytes (returns 0 if 0)
 * @return 1 if the key exists, 0 otherwise
 */
int tiku_kits_ds_trie_contains(
    const struct tiku_kits_ds_trie *trie,
    const uint8_t *key,
    uint16_t key_len);

/**
 * @brief Remove a key from the trie (lazy deletion).
 *
 * Walks the nibble path to locate the key's terminal node.  If
 * found, the terminal flag is cleared and the key count is
 * decremented.  The node and its ancestors are *not* reclaimed
 * from the pool -- this is a deliberate trade-off to keep the
 * implementation simple and avoid the complexity of node recycling
 * in a bump-allocated pool.  Subsequent inserts that share the
 * same prefix will reuse the existing path.
 *
 * Complexity: O(key_len) -- two lookups per byte, each O(1).
 *
 * @param trie    Trie (must not be NULL)
 * @param key     Byte array key (must not be NULL)
 * @param key_len Length of key in bytes (must be > 0)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if trie or key is NULL,
 *         TIKU_KITS_DS_ERR_PARAM if key_len is 0,
 *         TIKU_KITS_DS_ERR_NOTFOUND if the key does not exist
 */
int tiku_kits_ds_trie_remove(struct tiku_kits_ds_trie *trie,
                              const uint8_t *key,
                              uint16_t key_len);

/*---------------------------------------------------------------------------*/
/* QUERY                                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Return the number of keys stored in the trie.
 *
 * Returns the count of distinct terminal keys, not the number of
 * allocated nodes.  Safe to call with a NULL pointer -- returns 0.
 *
 * @param trie Trie, or NULL
 * @return Number of keys currently stored, or 0 if trie is NULL
 */
uint16_t tiku_kits_ds_trie_count(
    const struct tiku_kits_ds_trie *trie);

/*---------------------------------------------------------------------------*/
/* CLEAR                                                                     */
/*---------------------------------------------------------------------------*/

/**
 * @brief Reset the trie to empty state.
 *
 * Zeros the entire node pool, resets the bump allocator and key
 * counter, and re-allocates the root node.  After this call the
 * trie is in the same state as immediately after init -- all
 * previously inserted keys are gone and the full pool capacity is
 * available again.
 *
 * @param trie Trie to clear (must not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 *         TIKU_KITS_DS_ERR_NULL if trie is NULL
 */
int tiku_kits_ds_trie_clear(struct tiku_kits_ds_trie *trie);

#endif /* TIKU_KITS_DS_TRIE_H_ */
