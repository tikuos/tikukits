/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_ds_bloom.h - Fixed-size Bloom filter for set membership
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Platform-independent Bloom filter library for embedded systems.
 * All storage is statically allocated using a compile-time maximum
 * bit count. Multiple hash functions are derived from FNV-1a with
 * seed variation. Suited for probabilistic set membership queries
 * on resource-constrained microcontrollers.
 */

#ifndef TIKU_KITS_DS_BLOOM_H_
#define TIKU_KITS_DS_BLOOM_H_

/*---------------------------------------------------------------------------*/
/* INCLUDES                                                                  */
/*---------------------------------------------------------------------------*/

#include "../tiku_kits_ds.h"

/*---------------------------------------------------------------------------*/
/* CONFIGURATION                                                             */
/*---------------------------------------------------------------------------*/

/*
 * This compile-time constant defines the upper bound on the filter's
 * bit-array size.  Each Bloom filter instance reserves
 * (MAX_BITS + 7) / 8 bytes of static storage, so choose a value
 * that balances memory usage against the desired false-positive
 * rate.  A larger bit array reduces collisions for a given number
 * of inserted keys.
 * Override before including this header to change the limit:
 */

/**
 * @brief Maximum number of bits the Bloom filter can hold.
 *
 * @code
 * #define TIKU_KITS_DS_BLOOM_MAX_BITS 512
 * #include "tiku_kits_ds_bloom.h"
 * @endcode
 */
#ifndef TIKU_KITS_DS_BLOOM_MAX_BITS
#define TIKU_KITS_DS_BLOOM_MAX_BITS 256
#endif

/**
 * @brief Number of bytes needed to store MAX_BITS bits.
 *
 * Computed at compile time using ceiling division.  This determines
 * the size of the static backing byte array inside every Bloom
 * filter instance.
 */
#define TIKU_KITS_DS_BLOOM_MAX_BYTES \
    ((TIKU_KITS_DS_BLOOM_MAX_BITS + 7) / 8)

/*
 * Each hash function maps a key to a different bit position in the
 * filter.  More hashes reduce the false-positive rate up to a point,
 * but also increase the cost of add/check operations (each is O(k)
 * where k is the number of hashes).  The optimal k depends on the
 * ratio of bit-array size to expected item count.
 * Override before including this header to change the limit:
 */

/**
 * @brief Maximum number of independent hash functions.
 *
 * @code
 * #define TIKU_KITS_DS_BLOOM_MAX_HASHES 5
 * #include "tiku_kits_ds_bloom.h"
 * @endcode
 */
#ifndef TIKU_KITS_DS_BLOOM_MAX_HASHES
#define TIKU_KITS_DS_BLOOM_MAX_HASHES 3
#endif

/*---------------------------------------------------------------------------*/
/* TYPE DEFINITIONS                                                          */
/*---------------------------------------------------------------------------*/

/*
 * A probabilistic set-membership container.  Keys are hashed by
 * multiple independent hash functions (FNV-1a with per-hash seed
 * variation) and the resulting bit positions are set in a compact
 * byte array.  Membership queries return "possibly present" or
 * "definitely absent" -- false positives are possible but false
 * negatives are not.
 * Because all storage lives inside the struct itself, no heap
 * allocation is needed -- just declare the filter as a static or
 * local variable.
 * Three parameters control filter behaviour:
 * - @c num_bits -- the runtime bit-array size passed to init
 * (must be <= TIKU_KITS_DS_BLOOM_MAX_BITS).  A larger value
 * reduces the false-positive rate for a given item count.
 * - @c num_hashes -- the number of independent hash functions
 * (must be <= TIKU_KITS_DS_BLOOM_MAX_HASHES).  More hashes
 * improve accuracy up to a point, but increase per-operation
 * cost linearly.
 * - @c count -- the number of items inserted so far.  Exposed
 * for diagnostic use; the filter itself does not use it for
 * add/check logic.
 * Example:
 */

/**
 * @brief Fixed-capacity Bloom filter with statically allocated byte storage
 *
 * @struct tiku_kits_ds_bloom
 * @note Bit N is stored in @c bits[N/8] at bit position @c (N%8).
 * Byte-level packing is used (instead of 32-bit words as in
 * the bitmap module) because Bloom filters are typically
 * accessed one bit at a time, making byte addressing simpler
 * and equally efficient.
 * @code
 * struct tiku_kits_ds_bloom bf;
 * tiku_kits_ds_bloom_init(&bf, 128, 3);   // 128 bits, 3 hashes
 * tiku_kits_ds_bloom_add(&bf, "hello", 5);
 * tiku_kits_ds_bloom_add(&bf, "world", 5);
 * int r = tiku_kits_ds_bloom_check(&bf, "hello", 5); // r == 1
 * int s = tiku_kits_ds_bloom_check(&bf, "miss",  4); // s == 0 (likely)
 * @endcode
 */
struct tiku_kits_ds_bloom {
    uint8_t  bits[TIKU_KITS_DS_BLOOM_MAX_BYTES];
    uint16_t num_bits;      /**< Runtime bit count set by init (<= MAX_BITS) */
    uint8_t  num_hashes;    /**< Number of hash functions (1..MAX_HASHES)    */
    uint16_t count;         /**< Number of items added (diagnostic only)     */
};

/*---------------------------------------------------------------------------*/
/* INITIALIZATION                                                            */
/*---------------------------------------------------------------------------*/

/*
 * Resets the filter to an empty state: clears all bits, stores the
 * runtime bit count and hash count, and zeroes the item counter.
 * The backing byte array is zeroed so that every bit starts in a
 * deterministic (clear) state regardless of prior memory contents.
 */

/**
 * @brief Initialize a Bloom filter with the given dimensions
 *
 * @param bloom      Bloom filter to initialize (must not be NULL)
 * @param num_bits   Number of bits to use (1..BLOOM_MAX_BITS)
 * @param num_hashes Number of hash functions (1..BLOOM_MAX_HASHES)
 * @return TIKU_KITS_DS_OK on success,
 * TIKU_KITS_DS_ERR_NULL if bloom is NULL,
 * TIKU_KITS_DS_ERR_PARAM if num_bits is 0 or exceeds
 * MAX_BITS, or if num_hashes is 0 or exceeds MAX_HASHES
 */
int tiku_kits_ds_bloom_init(struct tiku_kits_ds_bloom *bloom,
                            uint16_t num_bits,
                            uint8_t num_hashes);

/*---------------------------------------------------------------------------*/
/* INSERT                                                                    */
/*---------------------------------------------------------------------------*/

/*
 * Computes @c num_hashes bit positions by running FNV-1a with a
 * different seed for each hash index, then sets each corresponding
 * bit in the byte array.  O(k * key_len) where k is num_hashes.
 * Duplicate inserts are harmless -- they set the same bits again
 * and increment the item counter, but do not change the filter's
 * membership semantics.
 */

/**
 * @brief Add a key to the Bloom filter
 *
 * @param bloom   Bloom filter (must not be NULL)
 * @param key     Pointer to key data (must not be NULL)
 * @param key_len Length of key in bytes (must be > 0)
 * @return TIKU_KITS_DS_OK on success,
 * TIKU_KITS_DS_ERR_NULL if bloom or key is NULL,
 * TIKU_KITS_DS_ERR_PARAM if key_len is 0
 */
int tiku_kits_ds_bloom_add(struct tiku_kits_ds_bloom *bloom,
                           const void *key,
                           uint16_t key_len);

/*---------------------------------------------------------------------------*/
/* QUERY                                                                     */
/*---------------------------------------------------------------------------*/

/*
 * Recomputes the same @c num_hashes bit positions that add() would
 * set and checks whether all of them are already set.  If any bit
 * is clear the key is *definitely* absent (return 0).  If all bits
 * are set the key is *possibly* present (return 1) -- a false
 * positive can occur when other keys have coincidentally set the
 * same bit positions.  False negatives are impossible by design.
 * O(k * key_len) where k is num_hashes.
 */

/**
 * @brief Check whether a key may be in the Bloom filter
 *
 * @param bloom   Bloom filter (must not be NULL)
 * @param key     Pointer to key data (must not be NULL)
 * @param key_len Length of key in bytes (must be > 0)
 * @return 1 if possibly present (all hash bits set),
 * 0 if definitely absent (at least one hash bit clear),
 * TIKU_KITS_DS_ERR_NULL (negative) if bloom or key is NULL,
 * TIKU_KITS_DS_ERR_PARAM (negative) if key_len is 0
 */
int tiku_kits_ds_bloom_check(const struct tiku_kits_ds_bloom *bloom,
                             const void *key,
                             uint16_t key_len);

/*---------------------------------------------------------------------------*/
/* UTILITY                                                                   */
/*---------------------------------------------------------------------------*/

/*
 * Resets the filter to its initial empty state without changing
 * num_bits or num_hashes.  Only the bytes covering [0, num_bits)
 * are zeroed, not the full MAX_BYTES backing buffer.  After
 * clearing, all subsequent check() calls will return 0 (absent).
 */

/**
 * @brief Clear all bits and reset the item count
 *
 * @param bloom Bloom filter (must not be NULL)
 * @return TIKU_KITS_DS_OK on success,
 * TIKU_KITS_DS_ERR_NULL if bloom is NULL
 */
int tiku_kits_ds_bloom_clear(struct tiku_kits_ds_bloom *bloom);

/*
 * Returns the cumulative insert count (including duplicates), not
 * the number of distinct items.  Safe to call with a NULL pointer
 * -- returns 0.  This is a diagnostic counter; the filter itself
 * does not use it for add/check logic.
 */

/**
 * @brief Return the number of items added to the filter
 *
 * @param bloom Bloom filter, or NULL
 * @return Number of items added, or 0 if bloom is NULL
 */
uint16_t tiku_kits_ds_bloom_count(
    const struct tiku_kits_ds_bloom *bloom);

#endif /* TIKU_KITS_DS_BLOOM_H_ */
