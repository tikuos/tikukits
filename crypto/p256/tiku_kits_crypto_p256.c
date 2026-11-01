/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_p256.c - NIST P-256 ECDSA and ECDH
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * 256-bit bignum over uint32_t[8] limbs (little-endian) with generic
 * Montgomery multiplication; the per-modulus Montgomery constants (n0, R^2,
 * modulus-2) are derived at runtime to avoid hardcoded-constant transcription
 * errors.  Field additions and the Montgomery reduction select their result
 * by mask, never by branch.  Verification works on public data in Jacobian
 * coordinates.  A secret scalar (a signing nonce, a private key) multiplies
 * through the complete projective formulas of Renes, Costello and Batina
 * (eprint 2015/1060, a = -3), which have no exceptional case, so every bit
 * costs one doubling and one addition.
 */

#include "tiku_kits_crypto_p256.h"
#include "../tiku_kits_crypto_bn.h"
#include <string.h>

#define NL  8   /* number of 32-bit limbs in a 256-bit number */

typedef uint32_t bn[NL];

/* ---- P-256 domain constants (big-endian byte form) ---------------------- */

/* field prime p */
static const uint8_t P256_P[32] = {
    0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff };
/* group order n */
static const uint8_t P256_N[32] = {
    0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
    0xbc,0xe6,0xfa,0xad,0xa7,0x17,0x9e,0x84,0xf3,0xb9,0xca,0xc2,0xfc,0x63,0x25,0x51 };
/* base point G */
static const uint8_t P256_GX[32] = {
    0x6b,0x17,0xd1,0xf2,0xe1,0x2c,0x42,0x47,0xf8,0xbc,0xe6,0xe5,0x63,0xa4,0x40,0xf2,
    0x77,0x03,0x7d,0x81,0x2d,0xeb,0x33,0xa0,0xf4,0xa1,0x39,0x45,0xd8,0x98,0xc2,0x96 };
static const uint8_t P256_GY[32] = {
    0x4f,0xe3,0x42,0xe2,0xfe,0x1a,0x7f,0x9b,0x8e,0xe7,0xeb,0x4a,0x7c,0x0f,0x9e,0x16,
    0x2b,0xce,0x33,0x57,0x6b,0x31,0x5e,0xce,0xcb,0xb6,0x40,0x68,0x37,0xbf,0x51,0xf5 };

/* curve coefficient b (big-endian) */
static const uint8_t P256_B[32] = {
    0x5a,0xc6,0x35,0xd8,0xaa,0x3a,0x93,0xe7,0xb3,0xeb,0xbd,0x55,0x76,0x98,0x86,0xbc,
    0x65,0x1d,0x06,0xb0,0xcc,0x53,0xb0,0xf6,0x3b,0xce,0x3c,0x3e,0x27,0xd2,0x60,0x4b };

/* ---- bignum primitives -------------------------------------------------- */

static void bn_zero(bn a)        { int i; for (i=0;i<NL;i++) a[i]=0; }
static void bn_copy(bn d,const bn s){ int i; for(i=0;i<NL;i++) d[i]=s[i]; }
static int  bn_is_zero(const bn a){ int i; uint32_t x=0; for(i=0;i<NL;i++) x|=a[i]; return x==0; }
static int  bn_eq(const bn a,const bn b){ int i; for(i=0;i<NL;i++) if(a[i]!=b[i]) return 0; return 1; }

/* Parse 32 big-endian bytes into a little-endian limb array. */
static void bn_from_be(bn a, const uint8_t b[32])
{
    int i;
    for (i = 0; i < NL; i++) {
        const uint8_t *p = b + (NL - 1 - i) * 4;
        a[i] = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
               ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
    }
}

/* a >= b ? 1 : 0  (unsigned) */
static int bn_geq(const bn a, const bn b)
{
    int i;
    for (i = NL - 1; i >= 0; i--) {
        if (a[i] != b[i]) return a[i] > b[i];
    }
    return 1;
}

/* d = a + b, returns carry out. */
static uint32_t bn_add(bn d, const bn a, const bn b)
{
    uint64_t c = 0; int i;
    for (i = 0; i < NL; i++) { c += (uint64_t)a[i] + b[i]; d[i] = (uint32_t)c; c >>= 32; }
    return (uint32_t)c;
}

/* d = a - b, returns borrow out. */
static uint32_t bn_sub(bn d, const bn a, const bn b)
{
    uint64_t c = 0; int i;
    for (i = 0; i < NL; i++) { uint64_t t = (uint64_t)a[i] - b[i] - c; d[i] = (uint32_t)t; c = (t >> 32) & 1; }
    return (uint32_t)c;
}

/* d = flag ? a : b, for flag 0 or 1, without a branch. */
static void bn_select(bn d, const bn a, const bn b, uint32_t flag)
{
    uint32_t m = (uint32_t)0 - (flag & 1u); int i;
    for (i = 0; i < NL; i++) d[i] = (a[i] & m) | (b[i] & ~m);
}

/* ---- Montgomery context ------------------------------------------------- */

typedef struct {
    bn       m;        /* modulus                            */
    bn       rr;       /* R^2 mod m   (R = 2^256)            */
    bn       m2;       /* m - 2  (Fermat-inverse exponent)  */
    uint32_t n0;       /* -m^-1 mod 2^32                     */
} mont_ctx;

/* x = 2x mod m */
static void dbl_mod(bn x, const bn m)
{
    uint32_t c = bn_add(x, x, x);
    bn t;
    if (c || bn_geq(x, m)) { bn_sub(t, x, m); bn_copy(x, t); }
}

static void mont_init(mont_ctx *c, const uint8_t m_be[32])
{
    uint32_t inv; int i;
    bn one, two;

    bn_from_be(c->m, m_be);

    /* n0 = -m[0]^-1 mod 2^32  (Newton's iteration, m[0] odd) */
    inv = 1;
    for (i = 0; i < 5; i++) inv *= 2u - c->m[0] * inv;
    c->n0 = (uint32_t)(0u - inv);

    /* rr = 2^512 mod m : start at 1 and double 512 times mod m */
    bn_zero(c->rr); c->rr[0] = 1;
    for (i = 0; i < 512; i++) dbl_mod(c->rr, c->m);

    /* m2 = m - 2 */
    bn_zero(one); one[0] = 1;
    bn_zero(two); two[0] = 2;
    bn_sub(c->m2, c->m, two);
    (void)one;
}

/* CIOS Montgomery multiply: out = a*b*R^-1 mod m. */
static void mont_mul(bn out, const bn a, const bn b, const mont_ctx *ctx)
{
    uint32_t t[NL + 1];
    int i, j;
    for (i = 0; i <= NL; i++) t[i] = 0;

    for (i = 0; i < NL; i++) {
        uint32_t c = 0, lo;
        uint32_t m, hi;
        uint64_t v;
        for (j = 0; j < NL; j++) {
            lo = t[j];
            tiku_kits_crypto_bn_mac(&lo, &c, a[j], b[i]);
            t[j] = lo;
        }
        v = (uint64_t)t[NL] + c;
        t[NL] = (uint32_t)v;
        hi = (uint32_t)(v >> 32);              /* 0 or 1 */

        m = (uint32_t)((uint64_t)t[0] * ctx->n0);
        /* The low limb this produces is zero by construction; only its
         * carry into the shifted-down loop below is wanted. */
        lo = t[0]; c = 0;
        tiku_kits_crypto_bn_mac(&lo, &c, m, ctx->m[0]);
        for (j = 1; j < NL; j++) {
            lo = t[j];
            tiku_kits_crypto_bn_mac(&lo, &c, m, ctx->m[j]);
            t[j - 1] = lo;
        }
        v = (uint64_t)t[NL] + c;
        t[NL - 1] = (uint32_t)v;
        t[NL] = hi + (uint32_t)(v >> 32);
    }

    /* final reduce: subtract m if t >= m (accounting for the t[NL] high word) */
    {
        bn lo, d; uint32_t bw;
        for (i = 0; i < NL; i++) lo[i] = t[i];
        bw = bn_sub(d, lo, ctx->m);
        bn_select(out, d, lo, t[NL] | (bw ^ 1u));
    }
}

static void to_mont(bn out, const bn a, const mont_ctx *c)  { mont_mul(out, a, c->rr, c); }
static void from_mont(bn out, const bn a, const mont_ctx *c){ bn one; bn_zero(one); one[0]=1; mont_mul(out, a, one, c); }

/* out = a^-1 mod m, via Fermat: a^(m-2).  Inputs/outputs are plain (not Mont). */
static void mont_inv(bn out, const bn a, const mont_ctx *c)
{
    bn am, r; int i, bit;
    to_mont(am, a, c);
    bn_zero(r); r[0] = 1; to_mont(r, r, c);            /* r = Mont(1) */
    for (i = NL * 32 - 1; i >= 0; i--) {
        mont_mul(r, r, r, c);
        bit = (c->m2[i >> 5] >> (i & 31)) & 1;
        if (bit) mont_mul(r, r, am, c);
    }
    from_mont(out, r, c);
}

/* ---- field (mod p) helpers --------------------------------------------- */

static mont_ctx FP, FN;   /* field and scalar contexts */
static bn       MG_X, MG_Y;  /* base point G in Montgomery (field) form */
static bn       MB, MONE;    /* b and 1 in Montgomery (field) form */
static int      p256_ready;

/* o = a + b mod m, for a, b < m: the sum less m kept unless that borrowed
 * from a sum that did not carry. */
static void mod_add(bn o, const bn a, const bn b, const bn m)
{
    bn sum, t;
    uint32_t c = bn_add(sum, a, b);
    uint32_t bw = bn_sub(t, sum, m);
    bn_select(o, t, sum, c | (bw ^ 1u));
}
/* o = a - b mod m, for a, b < m. */
static void mod_sub(bn o, const bn a, const bn b, const bn m)
{
    bn d, t;
    uint32_t bw = bn_sub(d, a, b);
    (void)bn_add(t, d, m);
    bn_select(o, t, d, bw);
}
static void fp_add(bn o,const bn a,const bn b){ mod_add(o, a, b, FP.m); }
static void fp_sub(bn o,const bn a,const bn b){ mod_sub(o, a, b, FP.m); }
static void fp_mul(bn o,const bn a,const bn b){ mont_mul(o,a,b,&FP); }
static void fp_sqr(bn o,const bn a){ mont_mul(o,a,a,&FP); }

/* ---- Jacobian point arithmetic (coords in field-Montgomery form) -------- */

typedef struct { bn X, Y, Z; } jpt;

static int  pt_is_inf(const jpt *p){ return bn_is_zero(p->Z); }
static void pt_set_inf(jpt *p){ bn_zero(p->X); bn_zero(p->Y); bn_zero(p->Z); }

/* R = 2P  (a = -3 doubling). */
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
    fp_add(t1, alpha, alpha); fp_add(alpha, t1, alpha);   /* alpha *= 3 */

    /* x3 = alpha^2 - 8*beta */
    fp_sqr(x3, alpha);
    fp_add(t1, beta, beta); fp_add(t1, t1, t1); fp_add(t1, t1, t1); /* 8*beta */
    fp_sub(x3, x3, t1);

    /* z3 = (Y+Z)^2 - gamma - delta */
    fp_add(t1, P->Y, P->Z); fp_sqr(z3, t1);
    fp_sub(z3, z3, gamma); fp_sub(z3, z3, delta);

    /* y3 = alpha*(4*beta - x3) - 8*gamma^2 */
    fp_add(t1, beta, beta); fp_add(t1, t1, t1);           /* 4*beta */
    fp_sub(t1, t1, x3);
    fp_mul(y3, alpha, t1);
    fp_sqr(t2, gamma); fp_add(t2, t2, t2); fp_add(t2, t2, t2); fp_add(t2, t2, t2); /* 8*gamma^2 */
    fp_sub(y3, y3, t2);

    bn_copy(R->X, x3); bn_copy(R->Y, y3); bn_copy(R->Z, z3);
}

/* R = P + Q  (general Jacobian add-2007-bl). */
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
        if (bn_is_zero(r_)) { pt_double(R, P); return; }   /* P == Q */
        pt_set_inf(R); return;                             /* P == -Q */
    }
    fp_add(t1, h, h); fp_sqr(i_, t1);          /* I = (2H)^2 */
    fp_mul(j_, h, i_);                          /* J = H*I    */
    fp_add(r_, r_, r_);                         /* r = 2(S2-S1) */
    fp_mul(v, u1, i_);                          /* V = U1*I   */

    fp_sqr(x3, r_); fp_sub(x3, x3, j_);
    fp_add(t1, v, v); fp_sub(x3, x3, t1);      /* X3 = r^2 - J - 2V */

    fp_sub(t1, v, x3); fp_mul(y3, r_, t1);
    fp_mul(t2, s1, j_); fp_add(t2, t2, t2);
    fp_sub(y3, y3, t2);                         /* Y3 = r*(V-X3) - 2*S1*J */

    fp_add(t1, P->Z, Q->Z); fp_sqr(t1, t1);
    fp_sub(t1, t1, z1z1); fp_sub(t1, t1, z2z2);
    fp_mul(z3, t1, h);                          /* Z3 = ((Z1+Z2)^2-Z1Z1-Z2Z2)*H */

    bn_copy(R->X, x3); bn_copy(R->Y, y3); bn_copy(R->Z, z3);
}

/* R = k*P  (double-and-add; k is a plain scalar). */
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

static void p256_setup(void)
{
    bn gx, gy;
    if (p256_ready) return;
    mont_init(&FP, P256_P);
    mont_init(&FN, P256_N);
    bn_from_be(gx, P256_GX); to_mont(MG_X, gx, &FP);
    bn_from_be(gy, P256_GY); to_mont(MG_Y, gy, &FP);
    bn_from_be(gx, P256_B); to_mont(MB, gx, &FP);
    bn_zero(gx); gx[0] = 1; to_mont(MONE, gx, &FP);
    p256_ready = 1;
}

/* ---- ECDSA verify ------------------------------------------------------- */

int tiku_kits_crypto_p256_ecdsa_verify(const uint8_t qx[32], const uint8_t qy[32],
                                       const uint8_t *hash, size_t hashlen,
                                       const uint8_t r[32], const uint8_t s[32])
{
    /* static (not stack): verify is non-reentrant; keeps the deep EC point
     * math off the (small) caller stack on 32-bit targets -- same rationale
     * as the RSA buffers. */
    static bn  rr, ss, e, w, u1, u2, qxn, qyn;
    static jpt G, Q, T1, T2, Rp;
    static bn  zinv, z2, x_aff, xmod;
    uint8_t hb[32];
    size_t i;

    p256_setup();

    bn_from_be(rr, r);
    bn_from_be(ss, s);
    /* r, s must be in [1, n-1] */
    if (bn_is_zero(rr) || bn_is_zero(ss)) return TIKU_KITS_CRYPTO_P256_BAD;
    if (bn_geq(rr, FN.m) || bn_geq(ss, FN.m)) return TIKU_KITS_CRYPTO_P256_BAD;

    /* e = leftmost 256 bits of hash, reduced mod n */
    memset(hb, 0, sizeof hb);
    if (hashlen > 32) hashlen = 32;
    for (i = 0; i < hashlen; i++) hb[i] = hash[i];   /* leftmost bytes */
    bn_from_be(e, hb);
    if (bn_geq(e, FN.m)) { bn t; bn_sub(t, e, FN.m); bn_copy(e, t); }

    /* w = s^-1 mod n; u1 = e*w mod n; u2 = r*w mod n  (all mod n) */
    mont_inv(w, ss, &FN);
    {
        bn em, rm;
        to_mont(em, e, &FN);  mont_mul(u1, em, w, &FN);   /* (e*R)*w*R^-1 = e*w */
        to_mont(rm, rr, &FN); mont_mul(u2, rm, w, &FN);
    }

    /* G and Q as Jacobian points (Z = 1 in Montgomery form) */
    bn_copy(G.X, MG_X); bn_copy(G.Y, MG_Y);
    bn_zero(G.Z); G.Z[0] = 1; to_mont(G.Z, G.Z, &FP);

    bn_from_be(qxn, qx); to_mont(Q.X, qxn, &FP);
    bn_from_be(qyn, qy); to_mont(Q.Y, qyn, &FP);
    bn_zero(Q.Z); Q.Z[0] = 1; to_mont(Q.Z, Q.Z, &FP);

    /* R = u1*G + u2*Q */
    pt_mul(&T1, u1, &G);
    pt_mul(&T2, u2, &Q);
    pt_add(&Rp, &T1, &T2);
    if (pt_is_inf(&Rp)) return TIKU_KITS_CRYPTO_P256_BAD;

    /* affine x = X / Z^2  (convert out of Montgomery) */
    {
        bn zfm, z2m, xfm;
        from_mont(zfm, Rp.Z, &FP);     /* Z (plain) */
        mont_inv(zinv, zfm, &FP);      /* Z^-1 (plain) */
        to_mont(zfm, zinv, &FP);
        fp_sqr(z2m, zfm);              /* (Z^-1)^2 in Mont */
        fp_mul(xfm, Rp.X, z2m);        /* X * (Z^-1)^2 in Mont */
        from_mont(x_aff, xfm, &FP);    /* affine x, plain */
        (void)z2;
    }

    /* valid iff (x mod n) == r */
    bn_copy(xmod, x_aff);
    if (bn_geq(xmod, FN.m)) { bn t; bn_sub(t, xmod, FN.m); bn_copy(xmod, t); }
    return bn_eq(xmod, rr) ? TIKU_KITS_CRYPTO_P256_OK : TIKU_KITS_CRYPTO_P256_BAD;
}

/* ---- secret scalars ----------------------------------------------------- */

static void bn_to_be(uint8_t out[32], const bn a)
{
    int i;
    for (i = 0; i < NL; i++) {
        uint32_t w = a[NL - 1 - i];
        out[i*4] = (uint8_t)(w >> 24); out[i*4+1] = (uint8_t)(w >> 16);
        out[i*4+2] = (uint8_t)(w >> 8); out[i*4+3] = (uint8_t)w;
    }
}

/* Clears @p n bytes in a way the compiler keeps. */
static void wipe(void *p, size_t n)
{
    volatile uint8_t *v = (volatile uint8_t *)p;
    while (n-- > 0u) *v++ = 0u;
}

/* 1 when 0 < d < n, else 0. */
static uint32_t scalar_ok(const bn d)
{
    bn t; uint32_t z = 0; int i;
    for (i = 0; i < NL; i++) z |= d[i];
    return bn_sub(t, d, FN.m) & (uint32_t)(z != 0u);
}

/* a mod n for a < 2^256, which is below 2n. */
static void reduce_n(bn a)
{
    bn t;
    uint32_t bw = bn_sub(t, a, FN.m);
    bn_select(a, a, t, bw);
}

/* Is the affine point (x,y), given as plain limbs, on y^2 = x^3 - 3x + b? */
static int pt_on_curve(const bn x, const bn y)
{
    bn xm, ym, lhs, rhs, t;
    to_mont(xm, x, &FP); to_mont(ym, y, &FP);
    fp_sqr(lhs, ym);                         /* y^2            */
    fp_sqr(rhs, xm); fp_mul(rhs, rhs, xm);   /* x^3            */
    fp_add(t, xm, xm); fp_add(t, t, xm);     /* 3x             */
    fp_sub(rhs, rhs, t);                     /* x^3 - 3x       */
    fp_add(rhs, rhs, MB);                    /* + b            */
    return bn_eq(lhs, rhs);
}

/* Homogeneous projective point (X:Y:Z), x = X/Z and y = Y/Z, coordinates
 * in field-Montgomery form; the identity is (0:1:0). */
typedef struct { bn X, Y, Z; } ppt;

/* R = P + Q for any two points, equal ones and the identity included
 * (eprint 2015/1060, algorithm 4).  R may be P or Q. */
static void cp_add(ppt *R, const ppt *P, const ppt *Q)
{
    bn t0, t1, t2, t3, t4, x3, y3, z3;

    fp_mul(t0, P->X, Q->X);
    fp_mul(t1, P->Y, Q->Y);
    fp_mul(t2, P->Z, Q->Z);
    fp_add(t3, P->X, P->Y);
    fp_add(t4, Q->X, Q->Y);
    fp_mul(t3, t3, t4);
    fp_add(t4, t0, t1);
    fp_sub(t3, t3, t4);
    fp_add(t4, P->Y, P->Z);
    fp_add(x3, Q->Y, Q->Z);
    fp_mul(t4, t4, x3);
    fp_add(x3, t1, t2);
    fp_sub(t4, t4, x3);
    fp_add(x3, P->X, P->Z);
    fp_add(y3, Q->X, Q->Z);
    fp_mul(x3, x3, y3);
    fp_add(y3, t0, t2);
    fp_sub(y3, x3, y3);
    fp_mul(z3, MB, t2);
    fp_sub(x3, y3, z3);
    fp_add(z3, x3, x3);
    fp_add(x3, x3, z3);
    fp_sub(z3, t1, x3);
    fp_add(x3, t1, x3);
    fp_mul(y3, MB, y3);
    fp_add(t1, t2, t2);
    fp_add(t2, t1, t2);
    fp_sub(y3, y3, t2);
    fp_sub(y3, y3, t0);
    fp_add(t1, y3, y3);
    fp_add(y3, t1, y3);
    fp_add(t1, t0, t0);
    fp_add(t0, t1, t0);
    fp_sub(t0, t0, t2);
    fp_mul(t1, t4, y3);
    fp_mul(t2, t0, y3);
    fp_mul(y3, x3, z3);
    fp_add(y3, y3, t2);
    fp_mul(x3, t3, x3);
    fp_sub(x3, x3, t1);
    fp_mul(z3, t4, z3);
    fp_mul(t1, t3, t0);
    fp_add(z3, z3, t1);

    bn_copy(R->X, x3); bn_copy(R->Y, y3); bn_copy(R->Z, z3);
}

/* R = 2P for any point (eprint 2015/1060, algorithm 6).  R may be P. */
static void cp_dbl(ppt *R, const ppt *P)
{
    bn t0, t1, t2, t3, x3, y3, z3;

    fp_sqr(t0, P->X);
    fp_sqr(t1, P->Y);
    fp_sqr(t2, P->Z);
    fp_mul(t3, P->X, P->Y);
    fp_add(t3, t3, t3);
    fp_mul(z3, P->X, P->Z);
    fp_add(z3, z3, z3);
    fp_mul(y3, MB, t2);
    fp_sub(y3, y3, z3);
    fp_add(x3, y3, y3);
    fp_add(y3, x3, y3);
    fp_sub(x3, t1, y3);
    fp_add(y3, t1, y3);
    fp_mul(y3, x3, y3);
    fp_mul(x3, x3, t3);
    fp_add(t3, t2, t2);
    fp_add(t2, t2, t3);
    fp_mul(z3, MB, z3);
    fp_sub(z3, z3, t2);
    fp_sub(z3, z3, t0);
    fp_add(t3, z3, z3);
    fp_add(z3, z3, t3);
    fp_add(t3, t0, t0);
    fp_add(t0, t3, t0);
    fp_sub(t0, t0, t2);
    fp_mul(t0, t0, z3);
    fp_add(y3, y3, t0);
    fp_mul(t0, P->Y, P->Z);
    fp_add(t0, t0, t0);
    fp_mul(z3, t0, z3);
    fp_sub(x3, x3, z3);
    fp_mul(z3, t0, t1);
    fp_add(z3, z3, z3);
    fp_add(z3, z3, z3);

    bn_copy(R->X, x3); bn_copy(R->Y, y3); bn_copy(R->Z, z3);
}

/* R = k*P: a doubling and an addition for each of the 256 bits of k, the
 * sum kept or dropped by mask, so k drives no branch and no address. */
static void cp_mul(ppt *R, const bn k, const ppt *P)
{
    ppt A, T; int i;

    bn_zero(A.X); bn_copy(A.Y, MONE); bn_zero(A.Z);
    for (i = NL * 32 - 1; i >= 0; i--) {
        uint32_t bit = (k[i >> 5] >> (i & 31)) & 1u;
        cp_dbl(&A, &A);
        cp_add(&T, &A, P);
        bn_select(A.X, T.X, A.X, bit);
        bn_select(A.Y, T.Y, A.Y, bit);
        bn_select(A.Z, T.Z, A.Z, bit);
    }
    *R = A;
    wipe(&A, sizeof A);
    wipe(&T, sizeof T);
}

/* P from affine plain limbs. */
static void cp_from_affine(ppt *P, const bn x, const bn y)
{
    to_mont(P->X, x, &FP); to_mont(P->Y, y, &FP); bn_copy(P->Z, MONE);
}

/* x and y of P, big-endian; @p y may be NULL.  -1 for the identity. */
static int cp_to_affine(const ppt *P, uint8_t x[32], uint8_t *y)
{
    bn z, zi, zim, t, p;
    from_mont(z, P->Z, &FP);
    if (bn_is_zero(z)) return -1;
    mont_inv(zi, z, &FP);
    to_mont(zim, zi, &FP);
    fp_mul(t, P->X, zim); from_mont(p, t, &FP); bn_to_be(x, p);
    if (y != NULL) { fp_mul(t, P->Y, zim); from_mont(p, t, &FP); bn_to_be(y, p); }
    return 0;
}

int tiku_kits_crypto_p256_public(const uint8_t priv[32], uint8_t pub[65])
{
    static bn  d, gx, gy;
    static ppt G, Q;
    int rc;
    p256_setup();
    bn_from_be(d, priv);
    if (!scalar_ok(d)) return TIKU_KITS_CRYPTO_P256_BAD;
    bn_from_be(gx, P256_GX); bn_from_be(gy, P256_GY);
    cp_from_affine(&G, gx, gy);
    cp_mul(&Q, d, &G);
    pub[0] = 0x04;
    rc = cp_to_affine(&Q, pub + 1, pub + 33);
    wipe(d, sizeof d);
    return rc == 0 ? TIKU_KITS_CRYPTO_P256_OK : TIKU_KITS_CRYPTO_P256_BAD;
}

int tiku_kits_crypto_p256_ecdh_keypair(const uint8_t seed[32],
                                       uint8_t priv[32], uint8_t pub[65])
{
    bn d;
    p256_setup();
    bn_from_be(d, seed);                      /* d = seed mod n */
    reduce_n(d);
    bn_to_be(priv, d);
    wipe(d, sizeof d);
    return tiku_kits_crypto_p256_public(priv, pub);  /* _BAD for d = 0 */
}

int tiku_kits_crypto_p256_ecdh_shared(const uint8_t priv[32],
                                      const uint8_t peer[65], uint8_t out_x[32])
{
    static bn  d, px, py;
    static ppt P, S;
    int rc;
    p256_setup();
    if (peer[0] != 0x04) return TIKU_KITS_CRYPTO_P256_BAD;  /* uncompressed only */
    bn_from_be(px, peer + 1); bn_from_be(py, peer + 33);
    if (bn_geq(px, FP.m) || bn_geq(py, FP.m)) return TIKU_KITS_CRYPTO_P256_BAD;
    if (!pt_on_curve(px, py)) return TIKU_KITS_CRYPTO_P256_BAD;
    bn_from_be(d, priv);
    if (!scalar_ok(d)) return TIKU_KITS_CRYPTO_P256_BAD;
    cp_from_affine(&P, px, py);
    cp_mul(&S, d, &P);
    rc = cp_to_affine(&S, out_x, NULL);       /* shared secret = S.x */
    wipe(d, sizeof d);
    wipe(&S, sizeof S);
    return rc == 0 ? TIKU_KITS_CRYPTO_P256_OK : TIKU_KITS_CRYPTO_P256_BAD;
}

/* ---- ECDSA sign ---------------------------------------------------------- */

#define NONCE_TRIES 64   /* candidates before a nonce source counts as broken */

int tiku_kits_crypto_p256_ecdsa_sign_nonce(const uint8_t priv[32],
                                           const uint8_t *hash, size_t hashlen,
                                           tiku_kits_crypto_p256_nonce_fn nonce,
                                           void *ctx, uint8_t r[32], uint8_t s[32])
{
    static bn  d, e, k, ki, rr, ss, t, u, gx, gy;
    static ppt G, R;
    uint8_t hb[32], kb[32], xb[32];
    int rc = TIKU_KITS_CRYPTO_P256_BAD, tries;

    p256_setup();
    bn_from_be(d, priv);
    if (nonce == NULL || !scalar_ok(d)) return rc;

    /* e = bits2octets(h1): the leftmost 256 bits of the hash, mod n */
    memset(hb, 0, sizeof hb);
    if (hashlen >= 32u) memcpy(hb, hash, 32);
    else memcpy(hb + 32 - hashlen, hash, hashlen);
    bn_from_be(e, hb);
    reduce_n(e);
    bn_to_be(hb, e);

    bn_from_be(gx, P256_GX); bn_from_be(gy, P256_GY);
    cp_from_affine(&G, gx, gy);
    for (tries = 0; tries < NONCE_TRIES; tries++) {
        nonce(ctx, hb, tries > 0, kb);
        bn_from_be(k, kb);
        if (!scalar_ok(k)) continue;
        cp_mul(&R, k, &G);
        if (cp_to_affine(&R, xb, NULL) != 0) continue;
        bn_from_be(rr, xb);
        reduce_n(rr);                         /* r = x mod n */
        if (bn_is_zero(rr)) continue;
        /* s = k^-1 (e + r d) mod n, each product (a R)(b) R^-1 = a b */
        to_mont(t, rr, &FN); mont_mul(u, t, d, &FN);      /* u = r d     */
        mod_add(u, e, u, FN.m);                           /* u = e + r d */
        mont_inv(ki, k, &FN);
        to_mont(t, ki, &FN); mont_mul(ss, t, u, &FN);     /* s = k^-1 u  */
        if (bn_is_zero(ss)) continue;
        bn_to_be(r, rr);
        bn_to_be(s, ss);
        rc = TIKU_KITS_CRYPTO_P256_OK;
        break;
    }
    wipe(d, sizeof d); wipe(k, sizeof k); wipe(ki, sizeof ki);
    wipe(t, sizeof t); wipe(u, sizeof u); wipe(kb, sizeof kb);
    wipe(&R, sizeof R);
    return rc;
}
