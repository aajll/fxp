/**
 * SPDX-License-Identifier: MIT
 *
 * @file: test_format16.c
 *
 * @brief
 *    Exercises the 16-bit configuration (WORD=16, Q8.8). Confirms the
 *    constructors saturate out-of-range inputs and that core arithmetic is
 *    correct in a non-default format.
 *
 * @note This file and the library it links are compiled with
 *       `-DFXP_WORD_BITS=16 -DFXP_FRAC_BITS=8`.
 */

#include "fxp.h"
#include "test_fxp.h"

TEST_CASE(test_format16_assumptions)
{
        /* Q8.8: 8 fractional bits, integer range [-128, 127]. */
        TEST_ASSERT(FXP_WORD_BITS == 16);
        TEST_ASSERT(FXP_ONE == 256);
        TEST_ASSERT(FXP_MAX == 32767);
        TEST_ASSERT(FXP_MIN == -32768);
        TEST_ASSERT(sizeof(fxp_t) == 2);
}

TEST_CASE(test_format16_from_int_saturates)
{
        /* Inputs beyond the integer range saturate rather than wrapping. */
        TEST_ASSERT(fxp_from_int(127) == 127 * FXP_ONE);
        TEST_ASSERT(fxp_from_int(128) == FXP_MAX); /* 128 not representable */
        TEST_ASSERT(fxp_from_int(10000000) == FXP_MAX);
        TEST_ASSERT(fxp_from_int(-128) == FXP_MIN);
        TEST_ASSERT(fxp_from_int(-10000000) == FXP_MIN);
        TEST_ASSERT(fxp_from_int(0) == 0);
}

TEST_CASE(test_format16_from_ratio_saturates)
{
        enum fxp_status st = FXP_OK;
        fxp_t v;

        v = fxp_from_ratio(7, 2, &st); /* 3.5 */
        TEST_ASSERT(st == FXP_OK);
        TEST_ASSERT(v == 3 * FXP_ONE + FXP_ONE / 2);

        v = fxp_from_ratio(2000000, 1, &st); /* far out of range */
        TEST_ASSERT(st == FXP_SATURATED);
        TEST_ASSERT(v == FXP_MAX);

        v = fxp_from_ratio(1, 0, &st);
        TEST_ASSERT(st == FXP_DIV_ZERO);
        TEST_ASSERT(v == 0);
}

TEST_CASE(test_format16_arithmetic)
{
        fxp_t out = 0;

        TEST_ASSERT(fxp_mul_sat(FXP_INT(2), FXP_INT(3)) == FXP_INT(6));
        TEST_ASSERT(fxp_add_sat(FXP_MAX, FXP_INT(1)) == FXP_MAX);
        TEST_ASSERT(fxp_div_chk(FXP_INT(7), FXP_INT(2), &out) == FXP_OK);
        TEST_ASSERT(out == 3 * FXP_ONE + FXP_ONE / 2); /* 3.5 */
        TEST_ASSERT(fxp_muldiv_chk(FXP_INT(2), FXP_INT(3), FXP_INT(4), &out)
                    == FXP_OK);
        TEST_ASSERT(out == FXP_ONE + FXP_ONE / 2); /* 1.5 */
}

int
main(void)
{
        (void)fprintf(stdout, "\n=== fxp 16-bit (Q8.8) tests ===\n\n");

        TEST_RUN(test_format16_assumptions);
        TEST_RUN(test_format16_from_int_saturates);
        TEST_RUN(test_format16_from_ratio_saturates);
        TEST_RUN(test_format16_arithmetic);

        (void)fprintf(stdout, "\n=== all 16-bit tests passed ===\n\n");
        return EXIT_SUCCESS;
}
