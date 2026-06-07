/**
 * SPDX-License-Identifier: MIT
 *
 * @file: fxp_const.h
 *
 * @brief
 *    Compile-time constant constructors for fxp values (no floating point).
 *
 * @details
 *    Each macro expands to an integer constant expression in the current build
 *    format, usable in static initializers (e.g. coefficient / gain tables)
 *    without dragging a soft-float runtime onto the target.
 *
 *    @c FXP_C uses a single signed numerator (rather than a `whole + num/den`
 *    split) so the sign is unambiguous, and rounds half away from zero at
 *    compile time. That compile-time rounding is a one-off on a constant and is
 *    independent of the runtime @c FXP_ROUNDING policy.
 *
 * @warning These are macros and may evaluate their arguments more than once
 *          (`FXP_C` expands @c num twice and @c den three times). Pass only
 *          constant, side-effect-free expressions. They perform **no**
 *          range check: a value too large for the format silently wraps on the
 *          final cast to @c fxp_t (use the runtime constructors for untrusted
 *          input).
 */
#ifndef FXP_CONST_H_
#define FXP_CONST_H_

#include <stdint.h>

#include "fxp_config.h"

/**
 * @def FXP_RAW
 * @brief Wrap an already-scaled raw fixed-point integer as an @c fxp_t.
 *        Escape hatch for precomputed constants or a foreign Q-format the
 *        caller has already converted to the build format.
 */
#define FXP_RAW(bits)  ((fxp_t)(bits))

/**
 * @def FXP_INT
 * @brief Exact integer @p whole as an @c fxp_t.
 * @note Uses multiplication (not a left shift) so negative @p whole is
 *       well-defined; left-shifting a negative signed value is UB in C.
 */
#define FXP_INT(whole) ((fxp_t)((fxp_wide_t)(whole) * (fxp_wide_t)FXP_ONE))

/**
 * @def FXP_C
 * @brief The value @p num / @p den in the build format, e.g.
 *        `FXP_C(314, 100)` ~= 3.14, `FXP_C(-7, 2)` == -3.5.
 *
 * @details Integer constant expression; rounds half away from zero. The result
 *          sign is positive iff @p num and @p den share a sign, which selects
 *          the direction of the rounding bias added before the truncating
 *          divide.
 */
#define FXP_C(num, den)                                                        \
        ((fxp_t)((((fxp_wide_t)(num) * (fxp_wide_t)FXP_ONE)                    \
                  + ((((num) >= 0) == ((den) >= 0))                            \
                         ? ((fxp_wide_t)(den) / 2)                             \
                         : -((fxp_wide_t)(den) / 2)))                          \
                 / (fxp_wide_t)(den)))

#endif /* FXP_CONST_H_ */
