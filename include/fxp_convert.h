/**
 * SPDX-License-Identifier: MIT
 *
 * @file: fxp_convert.h
 *
 * @brief
 *    Optional floating-point conversion helpers for fxp.
 *
 * @details
 *    These are the ONLY fxp functions that touch `double`. They are
 *    deliberately split out of @ref fxp.h and built as a separate translation
 *    unit (fxp_convert.c) so the core library links no soft-float ABI symbols
 *    on targets that forbid them. Include this header only where host-side or
 *    bring-up code genuinely needs float interop (e.g. tests, tooling).
 */
#ifndef FXP_CONVERT_H_
#define FXP_CONVERT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "fxp.h"

/**
 * @brief Convert a @c double to the build fixed-point format (round to nearest,
 *        ties away from zero).
 * @param x Value to convert.
 * @param[out] st Optional status: @c FXP_OVERFLOW if @p x is out of range (the
 *                result is then saturated), else @c FXP_OK. May be NULL.
 * @return @p x as an @c fxp_t.
 */
fxp_t fxp_from_double(double x, enum fxp_status *st);

/**
 * @brief Convert an @c fxp_t to @c double (exact).
 * @param a Fixed-point value.
 * @return @p a as a @c double.
 */
double fxp_to_double(fxp_t a);

#ifdef __cplusplus
}
#endif

#endif /* FXP_CONVERT_H_ */
