/**
 * SPDX-License-Identifier: MIT
 *
 * @file: test_rounding.c
 *
 * @brief
 *    Rounding-policy tests: verifies the build-wide policy (default round to
 *    nearest, ties to even) is applied consistently to both the shift-based and
 *    divide-based narrowing paths, including on negative values.
 *
 * @note Assumes the default Q16.16, FXP_ROUND_NEAREST_EVEN build.
 */

#include "fxp.h"
#include "test_fxp.h"

#if FXP_ROUNDING == FXP_ROUND_NEAREST_EVEN

/*
 * Multiply narrows via `>>FRAC`. Choosing a raw product whose low FRAC bits are
 * exactly 0x8000 (half an LSB) exercises the tie rule. With b == FXP_RAW(1) the
 * raw product equals a's raw value, so we control the remainder directly.
 */
TEST_CASE(test_mul_ties_to_even)
{
        /* +0.5 LSB tie above an even quotient (0) -> stays 0 */
        TEST_ASSERT(fxp_mul_sat(FXP_RAW(0x8000), FXP_RAW(1)) == 0);
        /* +0.5 LSB tie above an odd quotient (1) -> rounds up to 2 */
        TEST_ASSERT(fxp_mul_sat(FXP_RAW(0x18000), FXP_RAW(1)) == 2);
        /* -0.5 LSB tie below an even quotient (-2 for -1.5) -> stays -2 */
        TEST_ASSERT(fxp_mul_sat(FXP_RAW(-0x18000), FXP_RAW(1)) == -2);
        /* -0.5 LSB tie at -0.5 (odd neighbour -1) -> rounds to even 0 */
        TEST_ASSERT(fxp_mul_sat(FXP_RAW(-0x8000), FXP_RAW(1)) == 0);
}

/*
 * Divide narrows via integer division. The same ties must resolve to even, and
 * negatives must agree with the shift path (not truncate toward zero).
 */
TEST_CASE(test_div_ties_to_even)
{
        fxp_t out = 0;

        /* (2^-16) / 2 = 0.5 LSB tie, even quotient 0 -> 0 */
        TEST_ASSERT(fxp_div_chk(FXP_RAW(1), FXP_INT(2), &out) == FXP_OK);
        TEST_ASSERT(out == 0);
        /* (3 * 2^-16) / 2 = 1.5 LSB tie, odd quotient 1 -> 2 */
        TEST_ASSERT(fxp_div_chk(FXP_RAW(3), FXP_INT(2), &out) == FXP_OK);
        TEST_ASSERT(out == 2);
        /* negative tie resolves to even, NOT toward zero */
        TEST_ASSERT(fxp_div_chk(FXP_RAW(-1), FXP_INT(2), &out) == FXP_OK);
        TEST_ASSERT(out == 0);
        TEST_ASSERT(fxp_div_chk(FXP_RAW(-3), FXP_INT(2), &out) == FXP_OK);
        TEST_ASSERT(out == -2);
}

/*
 * to_int and the accumulator narrow share the same shift rounding.
 */
TEST_CASE(test_to_int_round_even)
{
        TEST_ASSERT(fxp_to_int(FXP_C(1, 2)) == 0);   /* 0.5 -> 0 */
        TEST_ASSERT(fxp_to_int(FXP_C(3, 2)) == 2);   /* 1.5 -> 2 */
        TEST_ASSERT(fxp_to_int(FXP_C(-1, 2)) == 0);  /* -0.5 -> 0 */
        TEST_ASSERT(fxp_to_int(FXP_C(-3, 2)) == -2); /* -1.5 -> -2 */
}

/*
 * The shift path and the divide path must produce identical results for the
 * same exact value: a/1 (divide) vs a*1 (shift) over a swept set including
 * negatives and tie points.
 */
TEST_CASE(test_shift_divide_agree)
{
        const fxp_t vals[] = {
            FXP_RAW(-0x18000), FXP_RAW(-0x8000), FXP_RAW(-1),      0,
            FXP_RAW(1),        FXP_RAW(0x8000),  FXP_RAW(0x18000), FXP_INT(3),
            FXP_INT(-3),
        };
        size_t i;

        for (i = 0; i < sizeof(vals) / sizeof(vals[0]); ++i) {
                fxp_t via_mul = fxp_mul_sat(vals[i], FXP_ONE);
                fxp_t via_div = 0;

                TEST_ASSERT(fxp_div_chk(vals[i], FXP_ONE, &via_div) == FXP_OK);
                TEST_ASSERT(via_mul == vals[i]); /* x * 1 == x */
                TEST_ASSERT(via_div == vals[i]); /* x / 1 == x */
                TEST_ASSERT(via_mul == via_div);
        }
}

#endif /* FXP_ROUND_NEAREST_EVEN */

int
main(void)
{
        (void)fprintf(stdout, "\n=== fxp rounding tests ===\n\n");

#if FXP_ROUNDING == FXP_ROUND_NEAREST_EVEN
        TEST_RUN(test_mul_ties_to_even);
        TEST_RUN(test_div_ties_to_even);
        TEST_RUN(test_to_int_round_even);
        TEST_RUN(test_shift_divide_agree);
#else
        (void)fprintf(stdout, "SKIP  non-RNE build\n");
#endif

        (void)fprintf(stdout, "\n=== all rounding tests passed ===\n\n");
        return EXIT_SUCCESS;
}
