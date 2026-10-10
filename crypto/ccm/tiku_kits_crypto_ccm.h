/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_ccm.h - AES-128-CCM* with a 13-byte nonce (RFC 3610, L = 2)
 *
 * The mode IEEE 802.15.4 link security uses: a CBC-MAC tag of 4, 8 or 16
 * bytes over the additional data and the message, and CTR encryption.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_CRYPTO_CCM_H_
#define TIKU_KITS_CRYPTO_CCM_H_

#include <stddef.h>
#include "../tiku_kits_crypto.h"

/**
 * @brief Encrypt (@p decrypt 0) or decrypt @p m_len bytes of @p m into @p out
 *        and compute the @p mic_len-byte tag into @p mic.
 *
 * Encrypt: @p m is the plaintext, @p out the ciphertext, @p mic the tag.
 * Decrypt: @p m is the ciphertext, @p out the plaintext, @p mic the tag
 * recomputed over it, which the caller compares with the received one.
 * @p out may be @p m.
 *
 * @return TIKU_KITS_CRYPTO_OK, _ERR_NULL, _ERR_PARAM for a tag length other
 *         than 4, 8 or 16, or _ERR_SIZE for a message over 65535 bytes or
 *         additional data over 65279 bytes
 */
int tiku_kits_crypto_ccm_star(int decrypt, const uint8_t key[16],
                              const uint8_t nonce[13], const uint8_t *aad,
                              size_t aad_len, const uint8_t *m, size_t m_len,
                              uint8_t mic_len, uint8_t *out, uint8_t *mic);

#endif /* TIKU_KITS_CRYPTO_CCM_H_ */
