/*
 * Tiku Operating System
 * http://tiku-os.org
 *
 * Authors: Ambuj Varshney <ambuj@tiku-os.org>
 *
 * tiku_kits_crypto_bn.h - the multiply-accumulate every bignum loop runs.
 *
 * Shared by the Montgomery CIOS inner loops in p256, p384 and rsa, which is
 * where those algorithms spend most of their time.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TIKU_KITS_CRYPTO_BN_H_
#define TIKU_KITS_CRYPTO_BN_H_

#include <stdint.h>

/*
 * UMAAL is Thumb-2 mainline only: every ARM part in this tree has it (M4 is
 * ARMv7E-M, M33/M55/M85 are ARMv8-M mainline), but ARMv6-M and ARMv8-M
 * baseline do not, so the portable form below stays the definition of record.
 */
#if !defined(TIKU_KITS_CRYPTO_BN_PORTABLE) &&                     \
    (defined(__ARM_ARCH_7M__)      || defined(__ARM_ARCH_7EM__) || \
     defined(__ARM_ARCH_8M_MAIN__) || defined(__ARM_ARCH_8_1M_MAIN__))
#define TIKU_KITS_CRYPTO_BN_UMAAL 1
#endif

/*
 * Define TIKU_KITS_CRYPTO_BN_PORTABLE to force the C form on a part that has
 * the instruction: it is the differential oracle for the assembly, and the
 * control an A/B measurement needs.
 */

/**
 * @brief Accumulate a*b into the pair, carrying: hi:lo = a*b + hi + lo.
 *
 * Both addends land in one instruction where UMAAL exists; the compiler
 * otherwise emits a multiply plus a separate carry chain for the second.
 *
 * @param lo  Low word, read and written
 * @param hi  Carry word, read and written; never overflows for 32-bit inputs
 * @param a   Multiplicand
 * @param b   Multiplier
 */
static inline void
tiku_kits_crypto_bn_mac(uint32_t *lo, uint32_t *hi, uint32_t a, uint32_t b)
{
#if defined(TIKU_KITS_CRYPTO_BN_UMAAL)
    __asm__ ("umaal %0, %1, %2, %3"
             : "+r" (*lo), "+r" (*hi)
             : "r" (a), "r" (b));
#else
    uint64_t v = (uint64_t)a * b + *hi + *lo;

    *lo = (uint32_t)v;
    *hi = (uint32_t)(v >> 32);
#endif
}

#endif /* TIKU_KITS_CRYPTO_BN_H_ */
