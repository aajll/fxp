/**
 * SPDX-License-Identifier: MIT
 *
 * @file: fxp_convert.c
 *
 * @brief
 *    Floating-point conversion helpers for fxp (optional, separable).
 *
 * @details
 *    This is the only translation unit in the library that references `double`.
 *    Keeping it separate lets the core (fxp.c) link with no soft-float ABI on
 *    targets that forbid floating point. Rounding here is round-to-nearest,
 *    ties away from zero, implemented without <math.h> so no extra runtime is
 *    pulled in.
 */

/* ================ INCLUDES ================================================ */

#include <stddef.h>

#include "fxp_convert.h"

/* ================ GLOBAL FUNCTIONS ======================================= */

fxp_t
fxp_from_double(double x, enum fxp_status *st)
{
        double scaled = x * (double)FXP_ONE;
        double rounded;

        /* Round to nearest, ties away from zero, without <math.h>: a cast to an
         * integer type truncates toward zero, so bias by +/- 0.5 first. */
        rounded = (scaled >= 0.0) ? (scaled + 0.5) : (scaled - 0.5);

        if (rounded > (double)FXP_MAX) {
                if (st != NULL) {
                        *st = FXP_OVERFLOW;
                }
                return FXP_MAX;
        }
        if (rounded < (double)FXP_MIN) {
                if (st != NULL) {
                        *st = FXP_OVERFLOW;
                }
                return FXP_MIN;
        }

        if (st != NULL) {
                *st = FXP_OK;
        }
        return (fxp_t)rounded;
}

double
fxp_to_double(fxp_t a)
{
        return (double)a / (double)FXP_ONE;
}
