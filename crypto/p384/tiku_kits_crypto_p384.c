/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_p384.c - NIST P-384 ECDSA signature verification
 *
 * Adapted from the P-256 kit: 256-bit -> 384-bit (NL=12 limbs), P-384 curve
 * constants.  Generic CIOS Montgomery (runtime n0/R^2/m-2), Jacobian a=-3
 * doubling.  Verify-only, public-data-only -> not constant-time.  Big working
 * buffers are file-scope static (deep, non-reentrant verify path -> keeps it
 * off the small caller stack, same as P-256/RSA).
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

#include "tiku_kits_crypto_p384.h"
#include <string.h>

#define NL  12   /* 384-bit numbers as 12 x 32-bit limbs */

typedef uint32_t bn[NL];

/* ---- P-384 domain constants (big-endian byte form) ---------------------- */

static const uint8_t P384_P[48] = {
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfe,
    0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff };
static const uint8_t P384_N[48] = {
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xc7,0x63,0x4d,0x81,0xf4,0x37,0x2d,0xdf,
    0x58,0x1a,0x0d,0xb2,0x48,0xb0,0xa7,0x7a,0xec,0xec,0x19,0x6a,0xcc,0xc5,0x29,0x73 };
static const uint8_t P384_GX[48] = {
    0xaa,0x87,0xca,0x22,0xbe,0x8b,0x05,0x37,0x8e,0xb1,0xc7,0x1e,0xf3,0x20,0xad,0x74,
    0x6e,0x1d,0x3b,0x62,0x8b,0xa7,0x9b,0x98,0x59,0xf7,0x41,0xe0,0x82,0x54,0x2a,0x38,
    0x55,0x02,0xf2,0x5d,0xbf,0x55,0x29,0x6c,0x3a,0x54,0x5e,0x38,0x72,0x76,0x0a,0xb7 };
static const uint8_t P384_GY[48] = {
    0x36,0x17,0xde,0x4a,0x96,0x26,0x2c,0x6f,0x5d,0x9e,0x98,0xbf,0x92,0x92,0xdc,0x29,
    0xf8,0xf4,0x1d,0xbd,0x28,0x9a,0x14,0x7c,0xe9,0xda,0x31,0x13,0xb5,0xf0,0xb8,0xc0,
    0x0a,0x60,0xb1,0xce,0x1d,0x7e,0x81,0x9d,0x7a,0x43,0x1d,0x7c,0x90,0xea,0x0e,0x5f };

/* ---- bignum primitives -------------------------------------------------- */

static void bn_zero(bn a)        { int i; for (i=0;i<NL;i++) a[i]=0; }
static void bn_copy(bn d,const bn s){ int i; for(i=0;i<NL;i++) d[i]=s[i]; }
static int  bn_is_zero(const bn a){ int i; uint32_t x=0; for(i=0;i<NL;i++) x|=a[i]; return x==0; }
static int  bn_eq(const bn a,const bn b){ int i; for(i=0;i<NL;i++) if(a[i]!=b[i]) return 0; return 1; }

static void bn_from_be(bn a, const uint8_t b[48])
{
    int i;
    for (i = 0; i < NL; i++) {
        const uint8_t *p = b + (NL - 1 - i) * 4;
        a[i] = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
               ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
    }
}

static int bn_geq(const bn a, const bn b)
{
    int i;
    for (i = NL - 1; i >= 0; i--) if (a[i] != b[i]) return a[i] > b[i];
    return 1;
}

static uint32_t bn_add(bn d, const bn a, const bn b)
{
    uint64_t c = 0; int i;
    for (i = 0; i < NL; i++) { c += (uint64_t)a[i] + b[i]; d[i] = (uint32_t)c; c >>= 32; }
    return (uint32_t)c;
}

static uint32_t bn_sub(bn d, const bn a, const bn b)
{
    uint64_t c = 0; int i;
    for (i = 0; i < NL; i++) { uint64_t t = (uint64_t)a[i] - b[i] - c; d[i] = (uint32_t)t; c = (t >> 32) & 1; }
    return (uint32_t)c;
}

/* ---- Montgomery context ------------------------------------------------- */

typedef struct { bn m; bn rr; bn m2; uint32_t n0; } mont_ctx;

static void dbl_mod(bn x, const bn m)
{
    uint32_t c = bn_add(x, x, x);
    bn t;
    if (c || bn_geq(x, m)) { bn_sub(t, x, m); bn_copy(x, t); }
}

static void mont_init(mont_ctx *c, const uint8_t m_be[48])
{
    uint32_t inv; int i;
    bn two;
    bn_from_be(c->m, m_be);
    inv = 1;
    for (i = 0; i < 5; i++) inv *= 2u - c->m[0] * inv;
    c->n0 = (uint32_t)(0u - inv);
    /* rr = R^2 mod m = 2^(2*32*NL) mod m, via 2*32*NL doublings of 1 */
    bn_zero(c->rr); c->rr[0] = 1;
    for (i = 0; i < 64 * NL; i++) dbl_mod(c->rr, c->m);
    bn_zero(two); two[0] = 2;
    bn_sub(c->m2, c->m, two);
}

static void mont_mul(bn out, const bn a, const bn b, const mont_ctx *ctx)
{
    uint32_t t[NL + 1];
    int i, j;
    for (i = 0; i <= NL; i++) t[i] = 0;
    for (i = 0; i < NL; i++) {
        uint64_t c = 0, v; uint32_t m, hi;
        for (j = 0; j < NL; j++) { v = (uint64_t)a[j] * b[i] + t[j] + c; t[j] = (uint32_t)v; c = v >> 32; }
        v = (uint64_t)t[NL] + c; t[NL] = (uint32_t)v; hi = (uint32_t)(v >> 32);
        m = (uint32_t)((uint64_t)t[0] * ctx->n0);
        v = (uint64_t)m * ctx->m[0] + t[0]; c = v >> 32;
        for (j = 1; j < NL; j++) { v = (uint64_t)m * ctx->m[j] + t[j] + c; t[j - 1] = (uint32_t)v; c = v >> 32; }
        v = (uint64_t)t[NL] + c; t[NL - 1] = (uint32_t)v; t[NL] = hi + (uint32_t)(v >> 32);
    }
    {
        bn lo, d; uint32_t bw;
        for (i = 0; i < NL; i++) lo[i] = t[i];
        bw = bn_sub(d, lo, ctx->m);
        if (t[NL] != 0 || bw == 0) bn_copy(out, d);
        else                       bn_copy(out, lo);
    }
}

static void to_mont(bn out, const bn a, const mont_ctx *c)  { mont_mul(out, a, c->rr, c); }
static void from_mont(bn out, const bn a, const mont_ctx *c){ bn one; bn_zero(one); one[0]=1; mont_mul(out, a, one, c); }

static void mont_inv(bn out, const bn a, const mont_ctx *c)
{
    bn am, r; int i, bit;
    to_mont(am, a, c);
    bn_zero(r); r[0] = 1; to_mont(r, r, c);
    for (i = NL * 32 - 1; i >= 0; i--) {
        mont_mul(r, r, r, c);
        bit = (c->m2[i >> 5] >> (i & 31)) & 1;
        if (bit) mont_mul(r, r, am, c);
    }
    from_mont(out, r, c);
}

/* ---- field (mod p) helpers + points ------------------------------------- */

static mont_ctx FP, FN;
static bn       MG_X, MG_Y;
static int      p384_ready;

static void fp_add(bn o,const bn a,const bn b){ uint32_t c=bn_add(o,a,b); bn t; if(c||bn_geq(o,FP.m)){bn_sub(t,o,FP.m);bn_copy(o,t);} }
static void fp_sub(bn o,const bn a,const bn b){ uint32_t bw=bn_sub(o,a,b); bn t; if(bw){bn_add(t,o,FP.m);bn_copy(o,t);} }
static void fp_mul(bn o,const bn a,const bn b){ mont_mul(o,a,b,&FP); }
static void fp_sqr(bn o,const bn a){ mont_mul(o,a,a,&FP); }

typedef struct { bn X, Y, Z; } jpt;

static int  pt_is_inf(const jpt *p){ return bn_is_zero(p->Z); }
static void pt_set_inf(jpt *p){ bn_zero(p->X); bn_zero(p->Y); bn_zero(p->Z); }

static void pt_double(jpt *R, const jpt *P)
{
    static bn delta, gamma, beta, alpha, t1, t2, x3, y3, z3;
    if (pt_is_inf(P)) { *R = *P; return; }
    fp_sqr(delta, P->Z);
    fp_sqr(gamma, P->Y);
    fp_mul(beta, P->X, gamma);
    fp_sub(t1, P->X, delta);
    fp_add(t2, P->X, delta);
    fp_mul(alpha, t1, t2);
    fp_add(t1, alpha, alpha); fp_add(alpha, t1, alpha);
    fp_sqr(x3, alpha);
    fp_add(t1, beta, beta); fp_add(t1, t1, t1); fp_add(t1, t1, t1);
    fp_sub(x3, x3, t1);
    fp_add(t1, P->Y, P->Z); fp_sqr(z3, t1);
    fp_sub(z3, z3, gamma); fp_sub(z3, z3, delta);
    fp_add(t1, beta, beta); fp_add(t1, t1, t1);
    fp_sub(t1, t1, x3);
    fp_mul(y3, alpha, t1);
    fp_sqr(t2, gamma); fp_add(t2, t2, t2); fp_add(t2, t2, t2); fp_add(t2, t2, t2);
    fp_sub(y3, y3, t2);
    bn_copy(R->X, x3); bn_copy(R->Y, y3); bn_copy(R->Z, z3);
}

static void pt_add(jpt *R, const jpt *P, const jpt *Q)
{
    static bn z1z1, z2z2, u1, u2, s1, s2, h, i_, j_, r_, v, t1, t2, x3, y3, z3;
    if (pt_is_inf(P)) { *R = *Q; return; }
    if (pt_is_inf(Q)) { *R = *P; return; }
    fp_sqr(z1z1, P->Z);
    fp_sqr(z2z2, Q->Z);
    fp_mul(u1, P->X, z2z2);
    fp_mul(u2, Q->X, z1z1);
    fp_mul(s1, P->Y, Q->Z); fp_mul(s1, s1, z2z2);
    fp_mul(s2, Q->Y, P->Z); fp_mul(s2, s2, z1z1);
    fp_sub(h, u2, u1);
    fp_sub(r_, s2, s1);
    if (bn_is_zero(h)) {
        if (bn_is_zero(r_)) { pt_double(R, P); return; }
        pt_set_inf(R); return;
    }
    fp_add(t1, h, h); fp_sqr(i_, t1);
    fp_mul(j_, h, i_);
    fp_add(r_, r_, r_);
    fp_mul(v, u1, i_);
    fp_sqr(x3, r_); fp_sub(x3, x3, j_);
    fp_add(t1, v, v); fp_sub(x3, x3, t1);
    fp_sub(t1, v, x3); fp_mul(y3, r_, t1);
    fp_mul(t2, s1, j_); fp_add(t2, t2, t2);
    fp_sub(y3, y3, t2);
    fp_add(t1, P->Z, Q->Z); fp_sqr(t1, t1);
    fp_sub(t1, t1, z1z1); fp_sub(t1, t1, z2z2);
    fp_mul(z3, t1, h);
    bn_copy(R->X, x3); bn_copy(R->Y, y3); bn_copy(R->Z, z3);
}

static void pt_mul(jpt *R, const bn k, const jpt *P)
{
    int i, bit;
    pt_set_inf(R);
    for (i = NL * 32 - 1; i >= 0; i--) {
        pt_double(R, R);
        bit = (k[i >> 5] >> (i & 31)) & 1;
        if (bit) pt_add(R, R, P);
    }
}

static void p384_setup(void)
{
    bn gx, gy;
    if (p384_ready) return;
    mont_init(&FP, P384_P);
    mont_init(&FN, P384_N);
    bn_from_be(gx, P384_GX); to_mont(MG_X, gx, &FP);
    bn_from_be(gy, P384_GY); to_mont(MG_Y, gy, &FP);
    p384_ready = 1;
}

/* ---- ECDSA verify ------------------------------------------------------- */

int tiku_kits_crypto_p384_ecdsa_verify(const uint8_t qx[48], const uint8_t qy[48],
                                       const uint8_t *hash, size_t hashlen,
                                       const uint8_t r[48], const uint8_t s[48])
{
    static bn  rr, ss, e, w, u1, u2, qxn, qyn;
    static jpt G, Q, T1, T2, Rp;
    static bn  zinv, x_aff, xmod;
    uint8_t hb[48];
    size_t i;

    p384_setup();

    bn_from_be(rr, r);
    bn_from_be(ss, s);
    if (bn_is_zero(rr) || bn_is_zero(ss)) return TIKU_KITS_CRYPTO_P384_BAD;
    if (bn_geq(rr, FN.m) || bn_geq(ss, FN.m)) return TIKU_KITS_CRYPTO_P384_BAD;

    /* e = bits2int(hash) (FIPS 186-4 2.3.2): the leftmost min(hashlen,48)
     * bytes of the hash, valued as a big-endian integer.  Right-align into
     * the 48-byte field width so a hash SHORTER than the order (e.g. a
     * SHA-256 digest under a P-384 issuer key -- DigiCert Global Root G3
     * signing an ecdsa-with-SHA256 intermediate) keeps its value instead of
     * being scaled up by the zero padding.  A longer hash is left-truncated. */
    memset(hb, 0, sizeof hb);
    if (hashlen > 48) hashlen = 48;
    for (i = 0; i < hashlen; i++) hb[48 - hashlen + i] = hash[i];
    bn_from_be(e, hb);
    if (bn_geq(e, FN.m)) { bn t; bn_sub(t, e, FN.m); bn_copy(e, t); }

    mont_inv(w, ss, &FN);
    {
        bn em, rm;
        to_mont(em, e, &FN);  mont_mul(u1, em, w, &FN);
        to_mont(rm, rr, &FN); mont_mul(u2, rm, w, &FN);
    }

    bn_copy(G.X, MG_X); bn_copy(G.Y, MG_Y);
    bn_zero(G.Z); G.Z[0] = 1; to_mont(G.Z, G.Z, &FP);

    bn_from_be(qxn, qx); to_mont(Q.X, qxn, &FP);
    bn_from_be(qyn, qy); to_mont(Q.Y, qyn, &FP);
    bn_zero(Q.Z); Q.Z[0] = 1; to_mont(Q.Z, Q.Z, &FP);

    pt_mul(&T1, u1, &G);
    pt_mul(&T2, u2, &Q);
    pt_add(&Rp, &T1, &T2);
    if (pt_is_inf(&Rp)) return TIKU_KITS_CRYPTO_P384_BAD;

    {
        bn zfm, z2m, xfm;
        from_mont(zfm, Rp.Z, &FP);
        mont_inv(zinv, zfm, &FP);
        to_mont(zfm, zinv, &FP);
        fp_sqr(z2m, zfm);
        fp_mul(xfm, Rp.X, z2m);
        from_mont(x_aff, xfm, &FP);
    }

    bn_copy(xmod, x_aff);
    if (bn_geq(xmod, FN.m)) { bn t; bn_sub(t, xmod, FN.m); bn_copy(xmod, t); }
    return bn_eq(xmod, rr) ? TIKU_KITS_CRYPTO_P384_OK : TIKU_KITS_CRYPTO_P384_BAD;
}
