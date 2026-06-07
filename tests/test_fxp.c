/**
 * SPDX-License-Identifier: MIT
 *
 * @file: test_fxp.c
 *
 * @brief
 *    Core unit tests for the fxp kernel: construction/readout, saturating and
 *    checked arithmetic, the wide accumulator, narrow, helpers, comparison, and
 *    rescale.
 *
 * @note Tests assume the default Q16.16 build format.
 */

#include "test_fxp.h"
#include "fxp.h"

/* ================ CONSTRUCTION / READOUT ================================= */

TEST_CASE(test_from_int_and_to_int)
{
        TEST_ASSERT(fxp_from_int(0) == 0);
        TEST_ASSERT(fxp_from_int(1) == FXP_ONE);
        TEST_ASSERT(fxp_from_int(-3) == -3 * FXP_ONE);
        TEST_ASSERT(fxp_to_int(FXP_INT(5)) == 5);
        TEST_ASSERT(fxp_to_int(FXP_INT(-5)) == -5);
        /* out-of-range integer saturates */
        TEST_ASSERT(fxp_from_int(1000000) == FXP_MAX);
        TEST_ASSERT(fxp_from_int(-1000000) == FXP_MIN);
}

TEST_CASE(test_constants)
{
        TEST_ASSERT(FXP_INT(2) == 2 * FXP_ONE);
        TEST_ASSERT(FXP_RAW(12345) == (fxp_t)12345);
        /* FXP_C(num, den) rounds half away from zero */
        TEST_ASSERT(FXP_C(1, 2) == FXP_ONE / 2);
        TEST_ASSERT(FXP_C(-1, 2) == -FXP_ONE / 2);
        TEST_ASSERT(FXP_C(7, 2) == (7 * FXP_ONE) / 2);
        /* 3.14 in Q16.16 == round(3.14 * 65536) == 205783 */
        TEST_ASSERT(FXP_C(314, 100) == 205783);
}

TEST_CASE(test_from_ratio)
{
        enum fxp_status st = FXP_OK;

        TEST_ASSERT(fxp_from_ratio(1, 2, &st) == FXP_ONE / 2);
        TEST_ASSERT(st == FXP_OK);
        TEST_ASSERT(fxp_from_ratio(-7, 2, &st) == -(7 * FXP_ONE) / 2);
        TEST_ASSERT(st == FXP_OK);
        /* divide by zero */
        TEST_ASSERT(fxp_from_ratio(1, 0, &st) == 0);
        TEST_ASSERT(st == FXP_DIV_ZERO);
        /* NULL status accepted */
        TEST_ASSERT(fxp_from_ratio(3, 1, NULL) == FXP_INT(3));
}

TEST_CASE(test_floor_ceil_round)
{
        fxp_t v = FXP_C(314, 100);  /* 3.14 */
        fxp_t n = FXP_C(-314, 100); /* -3.14 */

        TEST_ASSERT(fxp_floor(v) == FXP_INT(3));
        TEST_ASSERT(fxp_ceil(v) == FXP_INT(4));
        TEST_ASSERT(fxp_round(v) == FXP_INT(3));
        TEST_ASSERT(fxp_floor(n) == FXP_INT(-4));
        TEST_ASSERT(fxp_ceil(n) == FXP_INT(-3));
        TEST_ASSERT(fxp_round(n) == FXP_INT(-3));
        /* exact integers are unchanged */
        TEST_ASSERT(fxp_floor(FXP_INT(5)) == FXP_INT(5));
        TEST_ASSERT(fxp_ceil(FXP_INT(5)) == FXP_INT(5));
        /* round-half-to-even */
        TEST_ASSERT(fxp_round(FXP_C(1, 2)) == FXP_INT(0)); /* 0.5 -> 0 */
        TEST_ASSERT(fxp_round(FXP_C(3, 2)) == FXP_INT(2)); /* 1.5 -> 2 */
        TEST_ASSERT(fxp_round(FXP_C(5, 2)) == FXP_INT(2)); /* 2.5 -> 2 */
}

/* ================ ARITHMETIC ============================================= */

TEST_CASE(test_add_sub)
{
        fxp_t out = 0;

        TEST_ASSERT(fxp_add_sat(FXP_INT(2), FXP_INT(3)) == FXP_INT(5));
        TEST_ASSERT(fxp_sub_sat(FXP_INT(2), FXP_INT(3)) == FXP_INT(-1));

        TEST_ASSERT(fxp_add_chk(FXP_INT(2), FXP_INT(3), &out) == FXP_OK);
        TEST_ASSERT(out == FXP_INT(5));

        /* saturation */
        TEST_ASSERT(fxp_add_sat(FXP_MAX, FXP_MAX) == FXP_MAX);
        TEST_ASSERT(fxp_add_chk(FXP_MAX, FXP_INT(1), &out) == FXP_SATURATED);
        TEST_ASSERT(out == FXP_MAX);
        TEST_ASSERT(fxp_sub_chk(FXP_MIN, FXP_INT(1), &out) == FXP_SATURATED);
        TEST_ASSERT(out == FXP_MIN);
}

TEST_CASE(test_mul)
{
        fxp_t out = 0;

        TEST_ASSERT(fxp_mul_sat(FXP_INT(3), FXP_INT(4)) == FXP_INT(12));
        TEST_ASSERT(fxp_mul_sat(FXP_C(1, 2), FXP_C(1, 2)) == FXP_C(1, 4));
        TEST_ASSERT(fxp_mul_sat(FXP_INT(-3), FXP_INT(4)) == FXP_INT(-12));

        TEST_ASSERT(fxp_mul_chk(FXP_INT(3), FXP_INT(4), &out) == FXP_OK);
        TEST_ASSERT(out == FXP_INT(12));

        /* saturation */
        TEST_ASSERT(fxp_mul_sat(FXP_MAX, FXP_INT(2)) == FXP_MAX);
        TEST_ASSERT(fxp_mul_chk(FXP_MIN, FXP_INT(2), &out) == FXP_SATURATED);
        TEST_ASSERT(out == FXP_MIN);
}

TEST_CASE(test_div)
{
        fxp_t out = 0;

        TEST_ASSERT(fxp_div_chk(FXP_INT(7), FXP_INT(2), &out) == FXP_OK);
        TEST_ASSERT(out == FXP_C(7, 2));
        TEST_ASSERT(fxp_div_chk(FXP_INT(-7), FXP_INT(2), &out) == FXP_OK);
        TEST_ASSERT(out == FXP_C(-7, 2));

        /* divide by zero */
        TEST_ASSERT(fxp_div_chk(FXP_INT(1), 0, &out) == FXP_DIV_ZERO);
        TEST_ASSERT(out == 0);

        /* saturation: large / tiny */
        TEST_ASSERT(fxp_div_chk(FXP_MAX, FXP_C(1, 4), &out) == FXP_SATURATED);
        TEST_ASSERT(out == FXP_MAX);
}

TEST_CASE(test_muldiv)
{
        fxp_t out = 0;

        TEST_ASSERT(fxp_muldiv_chk(FXP_INT(2), FXP_INT(3), FXP_INT(4), &out)
                    == FXP_OK);
        TEST_ASSERT(out == FXP_C(3, 2)); /* 6/4 = 1.5 */

        /* muldiv keeps the product wide: a*b exceeds the fxp range but the
         * fused result is representable. */
        TEST_ASSERT(
            fxp_muldiv_chk(FXP_INT(1000), FXP_INT(1000), FXP_INT(2000), &out)
            == FXP_OK);
        TEST_ASSERT(out == FXP_INT(500)); /* 1e6 / 2000 = 500 */

        /* divide by zero */
        TEST_ASSERT(fxp_muldiv_chk(FXP_INT(2), FXP_INT(3), 0, &out)
                    == FXP_DIV_ZERO);
        TEST_ASSERT(out == 0);
}

/* ================ WIDE ACCUMULATOR ====================================== */

TEST_CASE(test_accumulator)
{
        /* A dot product: sum coeff[i]*x[i], narrowed once. */
        const fxp_t coeff[4] = {FXP_C(1, 2), FXP_C(1, 4), FXP_C(1, 8),
                                FXP_INT(1)};
        const fxp_t x[4] = {FXP_INT(2), FXP_INT(4), FXP_INT(8), FXP_INT(1)};
        fxp_acc_t acc = fxp_acc_zero();
        fxp_t out = 0;
        int i;

        for (i = 0; i < 4; ++i) {
                acc = fxp_acc_mac(acc, coeff[i], x[i]);
        }
        /* 0.5*2 + 0.25*4 + 0.125*8 + 1*1 = 1 + 1 + 1 + 1 = 4 */
        TEST_ASSERT(fxp_acc_narrow_sat(acc) == FXP_INT(4));
        TEST_ASSERT(fxp_acc_narrow_chk(acc, &out) == FXP_OK);
        TEST_ASSERT(out == FXP_INT(4));

        /* Narrowing once: a sum that exceeds range saturates only at the end.
         */
        acc = fxp_acc_zero();
        acc = fxp_acc_mac(acc, FXP_MAX, FXP_INT(1));
        acc = fxp_acc_mac(acc, FXP_MAX, FXP_INT(1));
        TEST_ASSERT(fxp_acc_narrow_chk(acc, &out) == FXP_SATURATED);
        TEST_ASSERT(out == FXP_MAX);
}

/* ================ NARROW ================================================ */

TEST_CASE(test_narrow)
{
        fxp_t out = 0;

        TEST_ASSERT(fxp_narrow_sat((fxp_wide_t)FXP_INT(3)) == FXP_INT(3));
        TEST_ASSERT(fxp_narrow_chk((fxp_wide_t)FXP_INT(3), &out) == FXP_OK);
        TEST_ASSERT(out == FXP_INT(3));

        TEST_ASSERT(fxp_narrow_sat((fxp_wide_t)FXP_MAX + 1) == FXP_MAX);
        TEST_ASSERT(fxp_narrow_chk((fxp_wide_t)FXP_MIN - 1, &out)
                    == FXP_SATURATED);
        TEST_ASSERT(out == FXP_MIN);
}

/* ================ HELPERS =============================================== */

TEST_CASE(test_neg_abs)
{
        TEST_ASSERT(fxp_neg_sat(FXP_INT(3)) == FXP_INT(-3));
        TEST_ASSERT(fxp_abs_sat(FXP_INT(-3)) == FXP_INT(3));
        TEST_ASSERT(fxp_abs_sat(FXP_INT(3)) == FXP_INT(3));
        /* MIN has no positive counterpart -> saturates to MAX */
        TEST_ASSERT(fxp_neg_sat(FXP_MIN) == FXP_MAX);
        TEST_ASSERT(fxp_abs_sat(FXP_MIN) == FXP_MAX);
}

TEST_CASE(test_clamp)
{
        TEST_ASSERT(fxp_clamp(FXP_INT(5), FXP_INT(0), FXP_INT(10))
                    == FXP_INT(5));
        TEST_ASSERT(fxp_clamp(FXP_INT(-1), FXP_INT(0), FXP_INT(10))
                    == FXP_INT(0));
        TEST_ASSERT(fxp_clamp(FXP_INT(11), FXP_INT(0), FXP_INT(10))
                    == FXP_INT(10));
}

TEST_CASE(test_clamp_wide)
{
        bool clamped = true;

        TEST_ASSERT(fxp_clamp_wide((fxp_wide_t)FXP_INT(5), FXP_INT(0),
                                   FXP_INT(10), &clamped)
                    == FXP_INT(5));
        TEST_ASSERT(clamped == false);

        TEST_ASSERT(
            fxp_clamp_wide((fxp_wide_t)FXP_MAX * 4, FXP_MIN, FXP_MAX, &clamped)
            == FXP_MAX);
        TEST_ASSERT(clamped == true);

        clamped = false;
        TEST_ASSERT(
            fxp_clamp_wide((fxp_wide_t)FXP_MIN * 4, FXP_MIN, FXP_MAX, &clamped)
            == FXP_MIN);
        TEST_ASSERT(clamped == true);

        /* NULL flag accepted */
        TEST_ASSERT(fxp_clamp_wide((fxp_wide_t)FXP_INT(5), FXP_INT(0),
                                   FXP_INT(10), NULL)
                    == FXP_INT(5));
}

TEST_CASE(test_lerp)
{
        TEST_ASSERT(fxp_lerp(FXP_INT(0), FXP_INT(10), 0) == FXP_INT(0));
        TEST_ASSERT(fxp_lerp(FXP_INT(0), FXP_INT(10), FXP_ONE) == FXP_INT(10));
        TEST_ASSERT(fxp_lerp(FXP_INT(0), FXP_INT(10), FXP_C(1, 2))
                    == FXP_INT(5));
        TEST_ASSERT(fxp_lerp(FXP_INT(0), FXP_INT(10), FXP_C(1, 4))
                    == FXP_C(5, 2));
        /* (b - a) is evaluated wide: extreme endpoints do not overflow */
        TEST_ASSERT(fxp_lerp(FXP_MIN, FXP_MAX, 0) == FXP_MIN);
        TEST_ASSERT(fxp_lerp(FXP_MIN, FXP_MAX, FXP_ONE) == FXP_MAX);
}

TEST_CASE(test_cmp)
{
        TEST_ASSERT(fxp_cmp(FXP_INT(1), FXP_INT(2)) < 0);
        TEST_ASSERT(fxp_cmp(FXP_INT(2), FXP_INT(1)) > 0);
        TEST_ASSERT(fxp_cmp(FXP_INT(2), FXP_INT(2)) == 0);
        TEST_ASSERT(fxp_cmp(FXP_MIN, FXP_MAX) < 0);
}

/* ================ FORMAT CONVERSION ===================================== */

TEST_CASE(test_rescale)
{
        fxp_t out = 0;

        /* identity */
        TEST_ASSERT(fxp_rescale(12345, 16, 16, &out) == FXP_OK);
        TEST_ASSERT(out == 12345);

        /* up-scale by 4 bits: exact, may saturate */
        TEST_ASSERT(fxp_rescale(FXP_INT(1), 16, 20, &out) == FXP_OK);
        TEST_ASSERT(out == (fxp_t)((fxp_wide_t)FXP_INT(1) * 16));
        TEST_ASSERT(fxp_rescale(FXP_MAX, 16, 20, &out) == FXP_SATURATED);
        TEST_ASSERT(out == FXP_MAX);

        /* down-scale dropping nonzero low bits -> INEXACT */
        TEST_ASSERT(fxp_rescale(0x1F, 16, 12, &out) == FXP_INEXACT);
        /* down-scale with zero low bits -> exact */
        TEST_ASSERT(fxp_rescale((fxp_t)(0x10 << 4), 16, 12, &out) == FXP_OK);
}

/* ================ RUNNER ================================================ */

int
main(void)
{
        (void)fprintf(stdout, "\n=== fxp core tests ===\n\n");

        TEST_RUN(test_from_int_and_to_int);
        TEST_RUN(test_constants);
        TEST_RUN(test_from_ratio);
        TEST_RUN(test_floor_ceil_round);
        TEST_RUN(test_add_sub);
        TEST_RUN(test_mul);
        TEST_RUN(test_div);
        TEST_RUN(test_muldiv);
        TEST_RUN(test_accumulator);
        TEST_RUN(test_narrow);
        TEST_RUN(test_neg_abs);
        TEST_RUN(test_clamp);
        TEST_RUN(test_clamp_wide);
        TEST_RUN(test_lerp);
        TEST_RUN(test_cmp);
        TEST_RUN(test_rescale);

        (void)fprintf(stdout, "\n=== all core tests passed ===\n\n");
        return EXIT_SUCCESS;
}
