/**
 * SPDX-License-Identifier: MIT
 *
 * @file: test_rounding_modes.c
 *
 * @brief
 *    Checks every narrowing path against the expected result of the build's
 *    rounding mode, for ties, non-ties and exact values of both signs, with
 *    positive and negative divisors.
 *
 * @note Compiled once per rounding mode: against the default library
 *       (FXP_ROUND_NEAREST_EVEN) and against core builds with
 *       `-DFXP_ROUNDING=FXP_ROUND_TRUNCATE` and
 *       `-DFXP_ROUNDING=FXP_ROUND_NEAREST_UP`.
 */

#include <stddef.h>

#include "fxp.h"
#include "test_fxp.h"

/* The exact value num / 2^sh and its rounding to an integer in each mode. */
struct rounding_case {
        int32_t num;
        unsigned int sh;
        int32_t truncate;
        int32_t nearest_up;
        int32_t nearest_even;
};

static const struct rounding_case cases[] = {
    /* Ties. */
    {1, 1U, 0, 1, 0},     /*  0.5  */
    {3, 1U, 1, 2, 2},     /*  1.5  */
    {5, 1U, 2, 3, 2},     /*  2.5  */
    {-1, 1U, -1, 0, 0},   /* -0.5  */
    {-3, 1U, -2, -1, -2}, /* -1.5  */
    {-5, 1U, -3, -2, -2}, /* -2.5  */
    /* Below and above half. */
    {1, 2U, 0, 0, 0},     /*  0.25 */
    {3, 2U, 0, 1, 1},     /*  0.75 */
    {-1, 2U, -1, 0, 0},   /* -0.25 */
    {-3, 2U, -1, -1, -1}, /* -0.75 */
    /* Exact. */
    {4, 1U, 2, 2, 2},
    {-4, 1U, -2, -2, -2},
    {0, 1U, 0, 0, 0},
};

static int32_t
expected(const struct rounding_case *c)
{
#if FXP_ROUNDING == FXP_ROUND_TRUNCATE
        return c->truncate;
#elif FXP_ROUNDING == FXP_ROUND_NEAREST_UP
        return c->nearest_up;
#else
        return c->nearest_even;
#endif
}

/* Raw value of num / 2^sh in the build's format. */
static fxp_t
scaled(const struct rounding_case *c)
{
        return FXP_RAW(c->num * (FXP_ONE >> c->sh));
}

/* Shift-based narrowing: multiply, accumulator, to_int and rescale. */
TEST_CASE(test_shift_paths)
{
        size_t i;

        for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
                const struct rounding_case *c = &cases[i];
                const fxp_t want = FXP_RAW(expected(c));
                const enum fxp_status exact =
                    ((c->num % 2) == 0) ? FXP_OK : FXP_INEXACT;
                fxp_t out = 0;

                /* raw * 1 shifts the raw value down by FXP_FRAC_BITS. */
                TEST_ASSERT(fxp_mul_sat(scaled(c), FXP_RAW(1)) == want);
                TEST_ASSERT(fxp_mul_chk(scaled(c), FXP_RAW(1), &out) == FXP_OK);
                TEST_ASSERT(out == want);
                TEST_ASSERT(fxp_acc_narrow_sat((fxp_acc_t)scaled(c)) == want);
                TEST_ASSERT(fxp_to_int(scaled(c)) == expected(c));

                /* Rescale num from sh fractional bits to an integer. */
                TEST_ASSERT(
                    fxp_rescale(FXP_RAW(c->num), (uint_least8_t)c->sh, 0U, &out)
                    == exact);
                TEST_ASSERT(out == want);
        }
}

/* Divide-based narrowing, with the divisor of each sign. */
TEST_CASE(test_divide_paths)
{
        size_t i;

        for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
                const struct rounding_case *c = &cases[i];
                const fxp_t want = FXP_RAW(expected(c));
                const int32_t den = (int32_t)1 << c->sh;
                enum fxp_status st = FXP_BAD_PARAM;
                fxp_t out = 0;

                /* raw num / den, as a fixed-point division by den. */
                TEST_ASSERT(fxp_div_chk(FXP_RAW(c->num), FXP_INT(den), &out)
                            == FXP_OK);
                TEST_ASSERT(out == want);
                TEST_ASSERT(fxp_div_chk(FXP_RAW(-c->num), FXP_INT(-den), &out)
                            == FXP_OK);
                TEST_ASSERT(out == want);

                /* raw num * raw 1 / raw den. */
                TEST_ASSERT(fxp_muldiv_chk(FXP_RAW(c->num), FXP_RAW(1),
                                           FXP_RAW(den), &out)
                            == FXP_OK);
                TEST_ASSERT(out == want);
                TEST_ASSERT(fxp_muldiv_chk(FXP_RAW(-c->num), FXP_RAW(1),
                                           FXP_RAW(-den), &out)
                            == FXP_OK);
                TEST_ASSERT(out == want);

                /* num / (den * FXP_ONE) as a ratio. */
                TEST_ASSERT(fxp_from_ratio(c->num, den * FXP_ONE, &st) == want);
                TEST_ASSERT(st == FXP_OK);
                TEST_ASSERT(fxp_from_ratio(-c->num, -den * FXP_ONE, &st)
                            == want);
                TEST_ASSERT(st == FXP_OK);
        }
}

int
main(void)
{
        (void)fprintf(stdout, "\n=== fxp rounding mode %d tests ===\n\n",
                      FXP_ROUNDING);

        TEST_RUN(test_shift_paths);
        TEST_RUN(test_divide_paths);

        (void)fprintf(stdout, "\n=== all rounding mode tests passed ===\n\n");
        return EXIT_SUCCESS;
}
