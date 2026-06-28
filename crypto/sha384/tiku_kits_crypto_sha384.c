/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_sha384.c - SHA-384 (FIPS 180-4), SHA-512 core
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied.  See the License for the specific language governing
 * permissions and limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tiku_kits_crypto_sha384.h"
#include <string.h>

#define ROTR(x, n) (((x) >> (n)) | ((x) << (64 - (n))))
#define CH(x, y, z)   (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z)  (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define BSIG0(x) (ROTR(x, 28) ^ ROTR(x, 34) ^ ROTR(x, 39))
#define BSIG1(x) (ROTR(x, 14) ^ ROTR(x, 18) ^ ROTR(x, 41))
#define SSIG0(x) (ROTR(x, 1)  ^ ROTR(x, 8)  ^ ((x) >> 7))
#define SSIG1(x) (ROTR(x, 19) ^ ROTR(x, 61) ^ ((x) >> 6))

static const uint64_t K[80] = {
    0x428a2f98d728ae22ULL,0x7137449123ef65cdULL,0xb5c0fbcfec4d3b2fULL,0xe9b5dba58189dbbcULL,
    0x3956c25bf348b538ULL,0x59f111f1b605d019ULL,0x923f82a4af194f9bULL,0xab1c5ed5da6d8118ULL,
    0xd807aa98a3030242ULL,0x12835b0145706fbeULL,0x243185be4ee4b28cULL,0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL,0x80deb1fe3b1696b1ULL,0x9bdc06a725c71235ULL,0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL,0xefbe4786384f25e3ULL,0x0fc19dc68b8cd5b5ULL,0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL,0x4a7484aa6ea6e483ULL,0x5cb0a9dcbd41fbd4ULL,0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL,0xa831c66d2db43210ULL,0xb00327c898fb213fULL,0xbf597fc7beef0ee4ULL,
    0xc6e00bf33da88fc2ULL,0xd5a79147930aa725ULL,0x06ca6351e003826fULL,0x142929670a0e6e70ULL,
    0x27b70a8546d22ffcULL,0x2e1b21385c26c926ULL,0x4d2c6dfc5ac42aedULL,0x53380d139d95b3dfULL,
    0x650a73548baf63deULL,0x766a0abb3c77b2a8ULL,0x81c2c92e47edaee6ULL,0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL,0xa81a664bbc423001ULL,0xc24b8b70d0f89791ULL,0xc76c51a30654be30ULL,
    0xd192e819d6ef5218ULL,0xd69906245565a910ULL,0xf40e35855771202aULL,0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL,0x1e376c085141ab53ULL,0x2748774cdf8eeb99ULL,0x34b0bcb5e19b48a8ULL,
    0x391c0cb3c5c95a63ULL,0x4ed8aa4ae3418acbULL,0x5b9cca4f7763e373ULL,0x682e6ff3d6b2b8a3ULL,
    0x748f82ee5defb2fcULL,0x78a5636f43172f60ULL,0x84c87814a1f0ab72ULL,0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL,0xa4506cebde82bde9ULL,0xbef9a3f7b2c67915ULL,0xc67178f2e372532bULL,
    0xca273eceea26619cULL,0xd186b8c721c0c207ULL,0xeada7dd6cde0eb1eULL,0xf57d4f7fee6ed178ULL,
    0x06f067aa72176fbaULL,0x0a637dc5a2c898a6ULL,0x113f9804bef90daeULL,0x1b710b35131c471bULL,
    0x28db77f523047d84ULL,0x32caab7b40c72493ULL,0x3c9ebe0a15c9bebcULL,0x431d67c49c100d4cULL,
    0x4cc5d4becb3e42b6ULL,0x597f299cfc657e2aULL,0x5fcb6fab3ad6faecULL,0x6c44198c4a475817ULL };

static void compress(uint64_t s[8], const uint8_t blk[128])
{
    uint64_t w[80], a, b, c, d, e, f, g, h, t1, t2;
    int t;
    for (t = 0; t < 16; t++) {
        const uint8_t *p = blk + t * 8;
        w[t] = ((uint64_t)p[0] << 56) | ((uint64_t)p[1] << 48) |
               ((uint64_t)p[2] << 40) | ((uint64_t)p[3] << 32) |
               ((uint64_t)p[4] << 24) | ((uint64_t)p[5] << 16) |
               ((uint64_t)p[6] << 8)  |  (uint64_t)p[7];
    }
    for (t = 16; t < 80; t++)
        w[t] = SSIG1(w[t - 2]) + w[t - 7] + SSIG0(w[t - 15]) + w[t - 16];

    a = s[0]; b = s[1]; c = s[2]; d = s[3];
    e = s[4]; f = s[5]; g = s[6]; h = s[7];
    for (t = 0; t < 80; t++) {
        t1 = h + BSIG1(e) + CH(e, f, g) + K[t] + w[t];
        t2 = BSIG0(a) + MAJ(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    s[0] += a; s[1] += b; s[2] += c; s[3] += d;
    s[4] += e; s[5] += f; s[6] += g; s[7] += h;
}

int tiku_kits_crypto_sha384_init(tiku_kits_crypto_sha384_ctx_t *c)
{
    if (c == 0) return -1;
    c->state[0] = 0xcbbb9d5dc1059ed8ULL; c->state[1] = 0x629a292a367cd507ULL;
    c->state[2] = 0x9159015a3070dd17ULL; c->state[3] = 0x152fecd8f70e5939ULL;
    c->state[4] = 0x67332667ffc00b31ULL; c->state[5] = 0x8eb44a8768581511ULL;
    c->state[6] = 0xdb0c2e0d64f98fa7ULL; c->state[7] = 0x47b5481dbefa4fa4ULL;
    c->count = 0; c->buf_len = 0;
    return 0;
}

int tiku_kits_crypto_sha384_update(tiku_kits_crypto_sha384_ctx_t *c,
                                   const uint8_t *data, size_t len)
{
    if (c == 0 || (data == 0 && len)) return -1;
    c->count += (uint64_t)len * 8;
    while (len) {
        size_t take = 128 - c->buf_len;
        if (take > len) take = len;
        memcpy(c->buffer + c->buf_len, data, take);
        c->buf_len += (uint8_t)take;
        data += take; len -= take;
        if (c->buf_len == 128) { compress(c->state, c->buffer); c->buf_len = 0; }
    }
    return 0;
}

int tiku_kits_crypto_sha384_final(tiku_kits_crypto_sha384_ctx_t *c, uint8_t *digest)
{
    uint64_t bits = c->count;
    int i;
    if (c == 0 || digest == 0) return -1;

    c->buffer[c->buf_len++] = 0x80;
    if (c->buf_len > 112) {
        while (c->buf_len < 128) c->buffer[c->buf_len++] = 0;
        compress(c->state, c->buffer); c->buf_len = 0;
    }
    while (c->buf_len < 112) c->buffer[c->buf_len++] = 0;
    /* 128-bit length, big-endian; high 64 bits are 0 for any practical input */
    for (i = 0; i < 8; i++) c->buffer[112 + i] = 0;
    for (i = 0; i < 8; i++) c->buffer[120 + i] = (uint8_t)(bits >> (56 - 8 * i));
    compress(c->state, c->buffer);

    /* output the first 6 state words = 384 bits */
    for (i = 0; i < 6; i++) {
        uint64_t v = c->state[i];
        digest[i * 8]     = (uint8_t)(v >> 56); digest[i * 8 + 1] = (uint8_t)(v >> 48);
        digest[i * 8 + 2] = (uint8_t)(v >> 40); digest[i * 8 + 3] = (uint8_t)(v >> 32);
        digest[i * 8 + 4] = (uint8_t)(v >> 24); digest[i * 8 + 5] = (uint8_t)(v >> 16);
        digest[i * 8 + 6] = (uint8_t)(v >> 8);  digest[i * 8 + 7] = (uint8_t)v;
    }
    return 0;
}
