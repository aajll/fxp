/**
 * SPDX-License-Identifier: MIT
 *
 * @file: test_convert.c
 *
 * @brief
 *    Tests for the optional double<->fxp helpers, plus a differential oracle
 *    that cross-checks the integer arithmetic against double over a swept grid
 *    of operands (host-only; lives here because it needs floating point).
 */

#include "fxp.h"
#include "fxp_convert.h"
#include "test_fxp.h"

/* Worst-case rounding error of one operation is half an LSB; allow a couple of
 * LSBs of slack so accumulated conversion error never trips a false failure. */
#define TOL (2.0 / (double)FXP_ONE)

static int
approx(double a, double b)
{
        double d = a - b;

        return (d < 0.0 ? -d : d) <= TOL;
}

TEST_CASE(test_double_roundtrip)
{
        const double xs[] = {0.0, 1.0,   -1.0,  3.14,  -3.14,
                             0.5, -0.25, 100.0, -100.0};
        size_t i;

        for (i = 0; i < sizeof(xs) / sizeof(xs[0]); ++i) {
                fxp_t v = fxp_from_double(xs[i], NULL);

                TEST_ASSERT(approx(fxp_to_double(v), xs[i]));
        }
}

TEST_CASE(test_double_overflow)
{
        enum fxp_status st = FXP_OK;

        TEST_ASSERT(fxp_from_double(1.0e9, &st) == FXP_MAX);
        TEST_ASSERT(st == FXP_OVERFLOW);
        TEST_ASSERT(fxp_from_double(-1.0e9, &st) == FXP_MIN);
        TEST_ASSERT(st == FXP_OVERFLOW);
}

/*
 * Differential oracle: sweep a grid of operands, compute each op in fxp and in
 * double, and require agreement within rounding tolerance (skipping cases that
 * saturate, where the two intentionally diverge).
 */
TEST_CASE(test_oracle_arithmetic)
{
        int ia;
        int ib;

        for (ia = -50; ia <= 50; ++ia) {
                for (ib = -50; ib <= 50; ++ib) {
                        double da = (double)ia / 4.0;
                        double db = (double)ib / 4.0;
                        fxp_t a = fxp_from_double(da, NULL);
                        fxp_t b = fxp_from_double(db, NULL);
                        fxp_t out = 0;

                        TEST_ASSERT(
                            approx(fxp_to_double(fxp_add_sat(a, b)), da + db));
                        TEST_ASSERT(
                            approx(fxp_to_double(fxp_sub_sat(a, b)), da - db));
                        TEST_ASSERT(
                            approx(fxp_to_double(fxp_mul_sat(a, b)), da * db));

                        if (ib != 0) {
                                TEST_ASSERT(fxp_div_chk(a, b, &out) == FXP_OK);
                                TEST_ASSERT(
                                    approx(fxp_to_double(out), da / db));
                        }
                }
        }
}

int
main(void)
{
        (void)fprintf(stdout, "\n=== fxp convert / oracle tests ===\n\n");

        TEST_RUN(test_double_roundtrip);
        TEST_RUN(test_double_overflow);
        TEST_RUN(test_oracle_arithmetic);

        (void)fprintf(stdout, "\n=== all convert tests passed ===\n\n");
        return EXIT_SUCCESS;
}
