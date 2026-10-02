/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_aeskw.h - AES key wrap (RFC 3394)
 *
 * Keys carried under a key: WPA2's handshake delivers the group key
 * wrapped under the KEK.  A 16- or 32-byte KEK; data in 8-byte blocks,
 * at least two; the wrapped form is one block longer.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_CRYPTO_AESKW_H_
#define TIKU_KITS_CRYPTO_AESKW_H_

#include <stddef.h>
#include "../tiku_kits_crypto.h"

/** @brief Wrap @p len bytes of @p in into @p out (@p len + 8 bytes).
 *         @return TIKU_KITS_CRYPTO_OK, _ERR_NULL, or _ERR_SIZE for a KEK or
 *         length the scheme does not take */
int tiku_kits_crypto_aeskw_wrap(const uint8_t *kek, size_t kek_len,
                                const uint8_t *in, size_t len, uint8_t *out);

/** @brief Unwrap @p len bytes of @p in into @p out (@p len - 8 bytes).
 *         @return TIKU_KITS_CRYPTO_OK, _ERR_NULL, _ERR_SIZE, or _ERR_CORRUPT
 *         when the integrity check fails: wrong KEK or altered data */
int tiku_kits_crypto_aeskw_unwrap(const uint8_t *kek, size_t kek_len,
                                  const uint8_t *in, size_t len, uint8_t *out);

#endif /* TIKU_KITS_CRYPTO_AESKW_H_ */
