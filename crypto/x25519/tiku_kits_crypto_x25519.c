/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_x25519.c - X25519 (Curve25519 ECDH) key agreement
 *
 * Curve25519 scalar multiplication via the Montgomery ladder, using the
 * canonical 16-limb (radix-2^16) field representation over GF(2^255-19).
 * Algorithm per RFC 7748; the compact limb representation and ladder are
 * the widely-used reference form.  Constant-time in the scalar (the only
 * branch on secret data is the constant-time conditional swap).
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

#include "tiku_kits_crypto_x25519.h"

/* A field element: 16 limbs of nominally 16 bits, held in int64_t so that
 * products and carries have head-room before reduction. */
typedef int64_t gf[16];

/* The constant (a24) = (486662 - 2) / 4 = 121665, as a field element. */
static const gf c121665 = {0xDB41, 1};

/* Carry-propagate and partially reduce a field element modulo 2^255-19. */
static void
x25519_carry(gf o)
{
    int   i;
    int64_t c;

    for (i = 0; i < 16; i++) {
        o[i] += (int64_t)1 << 16;
        c = o[i] >> 16;
        /* For i<15 fold the carry into the next limb; for i==15 fold the
         * top carry back into limb 0 scaled by 38 (= 2*19), since
         * 2^256 == 38 (mod 2^255-19). */
        o[(i + 1) * (i < 15)] += c - 1 + 37 * (c - 1) * (i == 15);
        o[i] -= c << 16;
    }
}

/* Constant-time conditional swap of p and q when b == 1. */
static void
x25519_cswap(gf p, gf q, int b)
{
    int64_t t, c = ~((int64_t)b - 1);
    int     i;

    for (i = 0; i < 16; i++) {
        t = c & (p[i] ^ q[i]);
        p[i] ^= t;
        q[i] ^= t;
    }
}

/* Serialise a field element to 32 little-endian bytes (fully reduced). */
static void
x25519_pack(uint8_t *o, const gf n)
{
    int  i, j, b;
    gf   m, t;

    for (i = 0; i < 16; i++) {
        t[i] = n[i];
    }
    x25519_carry(t);
    x25519_carry(t);
    x25519_carry(t);
    /* Conditionally subtract the prime twice to bring into [0, p). */
    for (j = 0; j < 2; j++) {
        m[0] = t[0] - 0xFFED;
        for (i = 1; i < 15; i++) {
            m[i] = t[i] - 0xFFFF - ((m[i - 1] >> 16) & 1);
            m[i - 1] &= 0xFFFF;
        }
        m[15] = t[15] - 0x7FFF - ((m[14] >> 16) & 1);
        b = (int)((m[15] >> 16) & 1);
        m[14] &= 0xFFFF;
        x25519_cswap(t, m, 1 - b);
    }
    for (i = 0; i < 16; i++) {
        o[2 * i]     = (uint8_t)(t[i] & 0xFF);
        o[2 * i + 1] = (uint8_t)(t[i] >> 8);
    }
}

/* Parse 32 little-endian bytes into a field element (clears the top bit). */
static void
x25519_unpack(gf o, const uint8_t *n)
{
    int i;

    for (i = 0; i < 16; i++) {
        o[i] = n[2 * i] + ((int64_t)n[2 * i + 1] << 8);
    }
    o[15] &= 0x7FFF;
}

static void
x25519_add(gf o, const gf a, const gf b)
{
    int i;
    for (i = 0; i < 16; i++) {
        o[i] = a[i] + b[i];
    }
}

static void
x25519_sub(gf o, const gf a, const gf b)
{
    int i;
    for (i = 0; i < 16; i++) {
        o[i] = a[i] - b[i];
    }
}

/* Field multiply: schoolbook 16x16 then fold 2^256 == 38 (mod p). */
static void
x25519_mul(gf o, const gf a, const gf b)
{
    int64_t t[31];
    int     i, j;

    for (i = 0; i < 31; i++) {
        t[i] = 0;
    }
    for (i = 0; i < 16; i++) {
        for (j = 0; j < 16; j++) {
            t[i + j] += a[i] * b[j];
        }
    }
    for (i = 0; i < 15; i++) {
        t[i] += 38 * t[i + 16];
    }
    for (i = 0; i < 16; i++) {
        o[i] = t[i];
    }
    x25519_carry(o);
    x25519_carry(o);
}

static void
x25519_sq(gf o, const gf a)
{
    x25519_mul(o, a, a);
}

/* Field inverse via Fermat: o = i^(p-2) mod p, p = 2^255-19. */
static void
x25519_inv(gf o, const gf i)
{
    gf  c;
    int a;

    for (a = 0; a < 16; a++) {
        c[a] = i[a];
    }
    for (a = 253; a >= 0; a--) {
        x25519_sq(c, c);
        if (a != 2 && a != 4) {
            x25519_mul(c, c, i);
        }
    }
    for (a = 0; a < 16; a++) {
        o[a] = c[a];
    }
}

void
tiku_kits_crypto_x25519_scalarmult(uint8_t out[TIKU_KITS_CRYPTO_X25519_LEN],
                                   const uint8_t scalar[TIKU_KITS_CRYPTO_X25519_LEN],
                                   const uint8_t point[TIKU_KITS_CRYPTO_X25519_LEN])
{
    uint8_t z[32];
    gf      x, a, b, c, d, e, f;
    int64_t r;
    int     i;

    /* Clamp the scalar per RFC 7748. */
    for (i = 0; i < 31; i++) {
        z[i] = scalar[i];
    }
    z[31] = (uint8_t)((scalar[31] & 127) | 64);
    z[0]  = (uint8_t)(z[0] & 248);

    x25519_unpack(x, point);
    for (i = 0; i < 16; i++) {
        b[i] = x[i];
        d[i] = a[i] = c[i] = 0;
    }
    a[0] = d[0] = 1;

    for (i = 254; i >= 0; i--) {
        r = (z[i >> 3] >> (i & 7)) & 1;
        x25519_cswap(a, b, (int)r);
        x25519_cswap(c, d, (int)r);
        x25519_add(e, a, c);
        x25519_sub(a, a, c);
        x25519_add(c, b, d);
        x25519_sub(b, b, d);
        x25519_sq(d, e);
        x25519_sq(f, a);
        x25519_mul(a, c, a);
        x25519_mul(c, b, e);
        x25519_add(e, a, c);
        x25519_sub(a, a, c);
        x25519_sq(b, a);
        x25519_sub(c, d, f);
        x25519_mul(a, c, c121665);
        x25519_add(a, a, d);
        x25519_mul(c, c, a);
        x25519_mul(a, d, f);
        x25519_mul(d, b, x);
        x25519_sq(b, e);
        x25519_cswap(a, b, (int)r);
        x25519_cswap(c, d, (int)r);
    }
    x25519_inv(c, c);
    x25519_mul(a, a, c);
    x25519_pack(out, a);
}

void
tiku_kits_crypto_x25519_base(uint8_t out[TIKU_KITS_CRYPTO_X25519_LEN],
                             const uint8_t scalar[TIKU_KITS_CRYPTO_X25519_LEN])
{
    /* Curve25519 base point: u = 9, little-endian. */
    static const uint8_t basepoint[32] = { 9 };

    tiku_kits_crypto_x25519_scalarmult(out, scalar, basepoint);
}
