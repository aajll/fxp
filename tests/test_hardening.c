/**
 * SPDX-License-Identifier: MIT
 *
 * @file: test_hardening.c
 *
 * @brief
 *    Edge-case tests: out-of-domain and overflowing fxp_rescale arguments, and
 *    checked-accumulator overflow detection. These cover behaviour at the
 *    boundaries of the input domain rather than ordinary in-range values.
 *
 * @note Assumes the default Q16.16 build (FXP_WIDE_BITS == 64, FXP_WORD_BITS
 *       == 32).
 */

#include "fxp.h"
#include "test_fxp.h"

/* ================ fxp_rescale domain / overflow ========================= */

TEST_CASE(test_rescale_bad_param)
{
        fxp_t out = 123;

        /* Shift distance >= wide width would be UB / hardware-masked. */
        TEST_ASSERT(fxp_rescale(FXP_INT(3), 0, FXP_WIDE_BITS, &out)
                    == FXP_BAD_PARAM);
        TEST_ASSERT(out == 0);
        TEST_ASSERT(fxp_rescale(FXP_INT(3), FXP_WIDE_BITS, 0, &out)
                    == FXP_BAD_PARAM);
        TEST_ASSERT(fxp_rescale(FXP_INT(3), 0, 200, &out) == FXP_BAD_PARAM);
}

TEST_CASE(test_rescale_upscale_saturates)
{
        fxp_t out = 0;

        /* In-domain but overflowing up-scale must saturate, not wrap. */
        TEST_ASSERT(fxp_rescale(FXP_MAX, 16, 40, &out) == FXP_SATURATED);
        TEST_ASSERT(out == FXP_MAX);
        TEST_ASSERT(fxp_rescale(FXP_MIN, 16, 40, &out) == FXP_SATURATED);
        TEST_ASSERT(out == FXP_MIN);
        /* zero is always exact */
        TEST_ASSERT(fxp_rescale(0, 0, 40, &out) == FXP_OK);
        TEST_ASSERT(out == 0);
        /* small value, exact up-scale */
        TEST_ASSERT(fxp_rescale(FXP_INT(1), 16, 20, &out) == FXP_OK);
        TEST_ASSERT(out == (fxp_t)((fxp_wide_t)FXP_INT(1) * 16));
}

TEST_CASE(test_rescale_downscale)
{
        fxp_t out = 0;

        /* drop nonzero low bits -> INEXACT */
        TEST_ASSERT(fxp_rescale(0x1F, 16, 12, &out) == FXP_INEXACT);
        /* zero low bits -> exact */
        TEST_ASSERT(fxp_rescale((fxp_t)0x100, 16, 12, &out) == FXP_OK);
        TEST_ASSERT(out == (fxp_t)0x10);
        /* shift past the whole word: everything is lost, rounds to 0 */
        TEST_ASSERT(fxp_rescale(12345, 40, 0, &out) == FXP_INEXACT);
        TEST_ASSERT(out == 0);
        TEST_ASSERT(fxp_rescale(0, 40, 0, &out) == FXP_OK);
        TEST_ASSERT(out == 0);
}

/* ================ checked accumulator =================================== */

TEST_CASE(test_acc_mac_chk_overflow)
{
        fxp_acc_t acc = fxp_acc_zero();
        bool ov = false;
        int hits = 0;
        int i;

        /* Many full-scale products overflow the accumulator; the checked MAC
         * must flag it and clamp so the narrow saturates to MAX (not wrap). */
        for (i = 0; i < 512; ++i) {
                acc = fxp_acc_mac_chk(acc, FXP_MAX, FXP_MAX, &ov);
                if (ov) {
                        ++hits;
                }
        }
        TEST_ASSERT(hits > 0);
        TEST_ASSERT(fxp_acc_narrow_sat(acc) == FXP_MAX);

        /* Negative overflow clamps to MIN. */
        acc = fxp_acc_zero();
        for (i = 0; i < 512; ++i) {
                acc = fxp_acc_mac_chk(acc, FXP_MAX, FXP_MIN, &ov);
        }
        TEST_ASSERT(fxp_acc_narrow_sat(acc) == FXP_MIN);
}

TEST_CASE(test_acc_mac_chk_no_false_positive)
{
        fxp_acc_t acc = fxp_acc_zero();
        bool ov = true;
        fxp_t out = 0;

        /* Normal, bounded accumulation never reports overflow and matches the
         * fast path exactly. */
        acc = fxp_acc_mac_chk(acc, FXP_C(1, 2), FXP_INT(4), &ov); /* 0.5*4 */
        TEST_ASSERT(ov == false);
        acc = fxp_acc_mac_chk(acc, FXP_C(1, 4), FXP_INT(8), &ov); /* 0.25*8 */
        TEST_ASSERT(ov == false);
        TEST_ASSERT(fxp_acc_narrow_chk(acc, &out) == FXP_OK);
        TEST_ASSERT(out == FXP_INT(4)); /* 2 + 2 */

        /* NULL overflow flag is accepted. */
        acc = fxp_acc_mac_chk(fxp_acc_zero(), FXP_INT(1), FXP_INT(1), NULL);
        TEST_ASSERT(fxp_acc_narrow_sat(acc) == FXP_INT(1));
}

/* ================ RUNNER ================================================ */

int
main(void)
{
        (void)fprintf(stdout, "\n=== fxp edge-case tests ===\n\n");

        TEST_RUN(test_rescale_bad_param);
        TEST_RUN(test_rescale_upscale_saturates);
        TEST_RUN(test_rescale_downscale);
        TEST_RUN(test_acc_mac_chk_overflow);
        TEST_RUN(test_acc_mac_chk_no_false_positive);

        (void)fprintf(stdout, "\n=== all edge-case tests passed ===\n\n");
        return EXIT_SUCCESS;
}
