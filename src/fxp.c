/**
 * SPDX-License-Identifier: MIT
 *
 * @file: fxp.c
 *
 * @brief
 *    Implementation of the fxp fixed-point kernel.
 *
 * @details
 *    Integer-only. Every narrowing path (multiply, divide, multiply-divide,
 *    accumulator narrow, rescale-down, and to_int) routes through the same two
 *    rounding helpers (@c fxp_shr_round and @c fxp_div_round), so the build's
 *    @c FXP_ROUNDING policy is applied uniformly across every operation. No
 *    floating point appears in this translation unit; optional `double` helpers
 *    live in fxp_convert.c.
 */

/* ================ INCLUDES ================================================ */

#include <stddef.h>

#include "fxp.h"

/* ================ INTERNAL: ROUNDING ===================================== */

/**
 * @internal
 * @brief Arithmetic right shift of @p v by @p s bits, applying @c FXP_ROUNDING.
 *
 * Reference point is the floor (arithmetic `>>`), so the fractional remainder
 * is always in [0, 2^s). Shared by every shift-based narrowing.
 */
static fxp_wide_t
fxp_shr_round(fxp_wide_t v, unsigned int s)
{
        fxp_wide_t q;
        fxp_wide_t rem;
#if FXP_ROUNDING != FXP_ROUND_TRUNCATE
        fxp_wide_t half;
#endif

        if (s == 0U) {
                return v;
        }

        q = v >> s;
        /* Mask, not `q << s`: left-shifting a negative signed value is UB. For
         * an arithmetic right shift this yields the floor remainder in
         * [0, 2^s). */
        rem = v & (((fxp_wide_t)1 << s) - 1);

#if FXP_ROUNDING == FXP_ROUND_TRUNCATE
        (void)rem;
        return q; /* floor toward -inf */
#else
        half = (fxp_wide_t)1 << (s - 1U);
        if (rem == 0) {
                return q;
        }
#if FXP_ROUNDING == FXP_ROUND_NEAREST_UP
        return (rem >= half) ? (q + 1) : q;
#else /* FXP_ROUND_NEAREST_EVEN */
        if (rem > half) {
                return q + 1;
        }
        if (rem < half) {
                return q;
        }
        return q + (q & 1); /* tie: round to even */
#endif
#endif
}

/**
 * @internal
 * @brief Divide @p num by @p den (den != 0) applying @c FXP_ROUNDING.
 *
 * Normalised to a positive divisor and a floor quotient so the same tie logic
 * as @c fxp_shr_round applies. Caller guarantees @p den != 0 and that neither
 * negation overflows (true for all fxp operand ranges).
 */
static fxp_calc_t
fxp_div_round(fxp_calc_t num, fxp_calc_t den)
{
        fxp_calc_t fq;
        fxp_calc_t fr;
#if FXP_ROUNDING != FXP_ROUND_TRUNCATE
        fxp_calc_t twice;
#endif

        if (den < 0) {
                num = -num;
                den = -den;
        }

        fq = num / den; /* toward zero */
        fr = num % den; /* sign of num, |fr| < den */
        if (fr < 0) {   /* convert to floor: 0 <= fr < den */
                fq -= 1;
                fr += den;
        }

#if FXP_ROUNDING == FXP_ROUND_TRUNCATE
        return fq; /* floor toward -inf */
#else
        if (fr == 0) {
                return fq;
        }
        twice = 2 * fr; /* 0 < twice < 2*den */
#if FXP_ROUNDING == FXP_ROUND_NEAREST_UP
        return (twice >= den) ? (fq + 1) : fq;
#else /* FXP_ROUND_NEAREST_EVEN */
        if (twice > den) {
                return fq + 1;
        }
        if (twice < den) {
                return fq;
        }
        return fq + (fq & 1); /* tie: round to even */
#endif
#endif
}

/* ================ INTERNAL: SATURATION =================================== */

/**
 * @internal
 * @brief Clamp a wide value to [@c FXP_MIN, @c FXP_MAX].
 * @param[out] clamped Set true iff clamping occurred (may be NULL).
 */
static fxp_t
fxp_saturate(fxp_calc_t v, bool *clamped)
{
        if (v > (fxp_calc_t)FXP_MAX) {
                if (clamped != NULL) {
                        *clamped = true;
                }
                return FXP_MAX;
        }
        if (v < (fxp_calc_t)FXP_MIN) {
                if (clamped != NULL) {
                        *clamped = true;
                }
                return FXP_MIN;
        }
        if (clamped != NULL) {
                *clamped = false;
        }
        return (fxp_t)v;
}

/* ================ CONSTRUCTION / READOUT ================================= */

fxp_t
fxp_from_int(int32_t whole)
{
        /* Clamp to the representable integer range BEFORE scaling so the
         * multiply cannot overflow the wide intermediate (matters on 16-bit
         * builds, where fxp_wide_t is only 32-bit). */
        const int32_t hi = (int32_t)(FXP_MAX / FXP_ONE);
        const int32_t lo = (int32_t)(FXP_MIN / FXP_ONE);

        if (whole > hi) {
                return FXP_MAX;
        }
        if (whole < lo) {
                return FXP_MIN;
        }
        return (fxp_t)((fxp_wide_t)whole * (fxp_wide_t)FXP_ONE);
}

fxp_t
fxp_from_ratio(int32_t num, int32_t den, enum fxp_status *st)
{
        bool clamped = false;
        fxp_t result;

        if (den == 0) {
                if (st != NULL) {
                        *st = FXP_DIV_ZERO;
                }
                return 0;
        }

        /* Scale in the always-64-bit calc type: `num << FRAC` would overflow a
         * 32-bit wide intermediate on 16-bit builds. */
        result =
            fxp_saturate(fxp_div_round((fxp_calc_t)num * (fxp_calc_t)FXP_ONE,
                                       (fxp_calc_t)den),
                         &clamped);

        if (st != NULL) {
                *st = clamped ? FXP_SATURATED : FXP_OK;
        }
        return result;
}

int32_t
fxp_to_int(fxp_t a)
{
        return (int32_t)fxp_shr_round((fxp_wide_t)a, FXP_FRAC_BITS);
}

fxp_t
fxp_floor(fxp_t a)
{
        fxp_wide_t rem = (fxp_wide_t)a & FXP_FRAC_MASK;

        return (fxp_t)((fxp_wide_t)a - rem);
}

fxp_t
fxp_ceil(fxp_t a)
{
        fxp_wide_t rem = (fxp_wide_t)a & FXP_FRAC_MASK;

        if (rem == 0) {
                return a;
        }
        return fxp_saturate(((fxp_wide_t)a - rem) + FXP_ONE, NULL);
}

fxp_t
fxp_round(fxp_t a)
{
        fxp_wide_t rem = (fxp_wide_t)a & FXP_FRAC_MASK; /* 0 <= rem < 2^FRAC */
        fxp_wide_t floor_raw = (fxp_wide_t)a - rem; /* multiple of FXP_ONE */
        fxp_wide_t half = (fxp_wide_t)1 << (FXP_FRAC_BITS - 1U);
        fxp_wide_t result = floor_raw;

        if (rem > half) {
                result = floor_raw + FXP_ONE;
        } else if (rem == half) {
                /* tie to even: round up only if the floor quotient is odd */
                if (((floor_raw >> FXP_FRAC_BITS) & 1) != 0) {
                        result = floor_raw + FXP_ONE;
                }
        } else {
                /* keep floor */
        }
        return fxp_saturate(result, NULL);
}

/* ================ ARITHMETIC: SAT + CHK PAIRS =========================== */

fxp_t
fxp_add_sat(fxp_t a, fxp_t b)
{
        return fxp_saturate((fxp_wide_t)a + (fxp_wide_t)b, NULL);
}

enum fxp_status
fxp_add_chk(fxp_t a, fxp_t b, fxp_t *out)
{
        bool clamped = false;

        *out = fxp_saturate((fxp_wide_t)a + (fxp_wide_t)b, &clamped);
        return clamped ? FXP_SATURATED : FXP_OK;
}

fxp_t
fxp_sub_sat(fxp_t a, fxp_t b)
{
        return fxp_saturate((fxp_wide_t)a - (fxp_wide_t)b, NULL);
}

enum fxp_status
fxp_sub_chk(fxp_t a, fxp_t b, fxp_t *out)
{
        bool clamped = false;

        *out = fxp_saturate((fxp_wide_t)a - (fxp_wide_t)b, &clamped);
        return clamped ? FXP_SATURATED : FXP_OK;
}

fxp_t
fxp_mul_sat(fxp_t a, fxp_t b)
{
        fxp_wide_t prod = (fxp_wide_t)a * (fxp_wide_t)b;

        return fxp_saturate(fxp_shr_round(prod, FXP_FRAC_BITS), NULL);
}

enum fxp_status
fxp_mul_chk(fxp_t a, fxp_t b, fxp_t *out)
{
        fxp_wide_t prod = (fxp_wide_t)a * (fxp_wide_t)b;
        bool clamped = false;

        *out = fxp_saturate(fxp_shr_round(prod, FXP_FRAC_BITS), &clamped);
        return clamped ? FXP_SATURATED : FXP_OK;
}

enum fxp_status
fxp_div_chk(fxp_t a, fxp_t b, fxp_t *out)
{
        bool clamped = false;

        if (b == 0) {
                *out = 0;
                return FXP_DIV_ZERO;
        }

        *out = fxp_saturate(
            fxp_div_round((fxp_wide_t)a * (fxp_wide_t)FXP_ONE, (fxp_wide_t)b),
            &clamped);
        return clamped ? FXP_SATURATED : FXP_OK;
}

enum fxp_status
fxp_muldiv_chk(fxp_t a, fxp_t b, fxp_t divisor, fxp_t *out)
{
        fxp_wide_t prod;
        bool clamped = false;

        if (divisor == 0) {
                *out = 0;
                return FXP_DIV_ZERO;
        }

        /* value (a*b/divisor) in raw units reduces to a_raw*b_raw/divisor_raw.
         */
        prod = (fxp_wide_t)a * (fxp_wide_t)b;
        *out = fxp_saturate(fxp_div_round(prod, (fxp_wide_t)divisor), &clamped);
        return clamped ? FXP_SATURATED : FXP_OK;
}

/* ================ WIDE ACCUMULATOR ====================================== */

fxp_acc_t
fxp_acc_zero(void)
{
        return (fxp_acc_t)0;
}

fxp_acc_t
fxp_acc_mac(fxp_acc_t acc, fxp_t a, fxp_t b)
{
        return acc + ((fxp_wide_t)a * (fxp_wide_t)b);
}

fxp_acc_t
fxp_acc_mac_chk(fxp_acc_t acc, fxp_t a, fxp_t b, bool *overflow)
{
        fxp_wide_t prod = (fxp_wide_t)a * (fxp_wide_t)b;
        bool ovf = false;

        /* Detect signed overflow of acc + prod before it happens, and clamp the
         * accumulator to its wide bound so a later narrow saturates correctly.
         */
        if ((prod > 0) && (acc > (FXP_WIDE_MAX - prod))) {
                acc = FXP_WIDE_MAX;
                ovf = true;
        } else if ((prod < 0) && (acc < (FXP_WIDE_MIN - prod))) {
                acc = FXP_WIDE_MIN;
                ovf = true;
        } else {
                acc = acc + prod;
        }

        if (overflow != NULL) {
                *overflow = ovf;
        }
        return acc;
}

fxp_t
fxp_acc_narrow_sat(fxp_acc_t acc)
{
        return fxp_saturate(fxp_shr_round(acc, FXP_FRAC_BITS), NULL);
}

enum fxp_status
fxp_acc_narrow_chk(fxp_acc_t acc, fxp_t *out)
{
        bool clamped = false;

        *out = fxp_saturate(fxp_shr_round(acc, FXP_FRAC_BITS), &clamped);
        return clamped ? FXP_SATURATED : FXP_OK;
}

/* ================ CHECKED NARROW ======================================== */

fxp_t
fxp_narrow_sat(fxp_wide_t wide)
{
        return fxp_saturate(wide, NULL);
}

enum fxp_status
fxp_narrow_chk(fxp_wide_t wide, fxp_t *out)
{
        bool clamped = false;

        *out = fxp_saturate(wide, &clamped);
        return clamped ? FXP_SATURATED : FXP_OK;
}

/* ================ HELPERS =============================================== */

fxp_t
fxp_neg_sat(fxp_t a)
{
        if (a == FXP_MIN) {
                return FXP_MAX;
        }
        return (fxp_t)(-a);
}

fxp_t
fxp_abs_sat(fxp_t a)
{
        return (a < 0) ? fxp_neg_sat(a) : a;
}

fxp_t
fxp_clamp(fxp_t a, fxp_t lo, fxp_t hi)
{
        if (a < lo) {
                return lo;
        }
        if (a > hi) {
                return hi;
        }
        return a;
}

fxp_t
fxp_clamp_wide(fxp_wide_t a, fxp_t lo, fxp_t hi, bool *clamped)
{
        if (a < (fxp_wide_t)lo) {
                if (clamped != NULL) {
                        *clamped = true;
                }
                return lo;
        }
        if (a > (fxp_wide_t)hi) {
                if (clamped != NULL) {
                        *clamped = true;
                }
                return hi;
        }
        if (clamped != NULL) {
                *clamped = false;
        }
        return (fxp_t)a;
}

fxp_t
fxp_lerp(fxp_t a, fxp_t b, fxp_t t)
{
        fxp_wide_t diff = (fxp_wide_t)b - (fxp_wide_t)a;
        fxp_wide_t scaled = fxp_shr_round(diff * (fxp_wide_t)t, FXP_FRAC_BITS);

        return fxp_saturate((fxp_wide_t)a + scaled, NULL);
}

/* ================ COMPARISON =========================================== */

int
fxp_cmp(fxp_t a, fxp_t b)
{
        return (a > b) - (a < b);
}

/* ================ FORMAT CONVERSION ==================================== */

enum fxp_status
fxp_rescale(fxp_t a, uint8_t from_frac, uint8_t to_frac, fxp_t *out)
{
        /* Reject out-of-domain shift distances up front: a shift >= the wide
         * type width is undefined behaviour, and on common hardware is silently
         * masked (count mod width), which would corrupt the result. */
        if ((from_frac >= FXP_WIDE_BITS) || (to_frac >= FXP_WIDE_BITS)) {
                *out = 0;
                return FXP_BAD_PARAM;
        }

        if (to_frac >= from_frac) {
                unsigned int sh = (unsigned int)(to_frac - from_frac);

                if (a == 0) {
                        *out = 0;
                        return FXP_OK;
                }
                /* A shift by >= the storage width overflows for any nonzero a;
                 * saturate by sign without forming the (overflowing) product.
                 */
                if (sh >= (unsigned int)FXP_WORD_BITS) {
                        *out = (a > 0) ? FXP_MAX : FXP_MIN;
                        return FXP_SATURATED;
                }
                /* Detect overflow against the bound before multiplying, so the
                 * intermediate never overflows. FXP_MIN/FXP_MAX are exact
                 * multiples of 2^sh for sh < FXP_WORD_BITS, so these bounds are
                 * exact. */
                if ((a > 0) && ((fxp_calc_t)a > ((fxp_calc_t)FXP_MAX >> sh))) {
                        *out = FXP_MAX;
                        return FXP_SATURATED;
                }
                if ((a < 0) && ((fxp_calc_t)a < ((fxp_calc_t)FXP_MIN >> sh))) {
                        *out = FXP_MIN;
                        return FXP_SATURATED;
                }
                *out = (fxp_t)((fxp_calc_t)a * ((fxp_calc_t)1 << sh));
                return FXP_OK;
        } else {
                unsigned int sh = (unsigned int)(from_frac - to_frac);
                fxp_wide_t lost;

                /* A shift by >= the storage width drops every significant bit;
                 * round-to-nearest then yields 0 (magnitude < 0.5 LSB). */
                if (sh >= (unsigned int)FXP_WORD_BITS) {
                        *out = 0;
                        return (a != 0) ? FXP_INEXACT : FXP_OK;
                }
                lost = (fxp_wide_t)a & (((fxp_wide_t)1 << sh) - 1);
                /* Down-scaling only shrinks magnitude, so the result always
                 * fits fxp_t; no saturation is possible. */
                *out = (fxp_t)fxp_shr_round((fxp_wide_t)a, sh);
                return (lost != 0) ? FXP_INEXACT : FXP_OK;
        }
}
