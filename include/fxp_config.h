/**
 * SPDX-License-Identifier: MIT
 *
 * @file: fxp_config.h
 *
 * @brief
 *    Resolves the fxp configuration into concrete types and derived constants.
 *
 * @details
 *    Includes @ref fxp_conf.h (the user-overridable knobs), then defines the
 *    fixed-point storage type @c fxp_t, the wide intermediate type used by all
 *    multiply/divide/accumulate paths, and the format-derived constants
 *    (@c FXP_ONE, @c FXP_MIN, @c FXP_MAX, @c FXP_FRAC_MASK). Configuration
 *    invariants are enforced here with `_Static_assert` so an invalid
 *    `fxp_conf.h` fails the build rather than miscomputing at runtime.
 */
#ifndef FXP_CONFIG_H_
#define FXP_CONFIG_H_

#include <stdint.h>

#include "fxp_conf.h"

/* ================ STORAGE AND INTERMEDIATE TYPES ========================== */

#if FXP_WORD_BITS == 32
/** @brief Signed fixed-point storage type (current build: 32-bit). */
typedef int32_t fxp_t;
/** @brief Wide intermediate: holds the 2*WORD product of two @c fxp_t. */
typedef int64_t fxp_wide_t;
#elif FXP_WORD_BITS == 16
/** @brief Signed fixed-point storage type (current build: 16-bit). */
typedef int16_t fxp_t;
/** @brief Wide intermediate: holds the 2*WORD product of two @c fxp_t. */
typedef int32_t fxp_wide_t;
#else
#error "FXP_WORD_BITS must be 16 or 32 (no portable 128-bit for WORD=64)."
#endif

/**
 * @brief Always-64-bit calculation type for construction/division helpers.
 *
 * @details The constructors @c fxp_from_int / @c fxp_from_ratio accept full
 *          @c int32_t arguments regardless of @c FXP_WORD_BITS. On a 16-bit
 *          build @c fxp_wide_t is only 32-bit, so scaling those arguments needs
 *          a wider type to avoid overflow before saturation. This type is at
 *          least 64-bit on every conforming platform.
 */
typedef int_least64_t fxp_calc_t;

/** @brief Bit width of @c fxp_wide_t (2 * FXP_WORD_BITS). */
#define FXP_WIDE_BITS (2 * FXP_WORD_BITS)

/* ================ DERIVED CONSTANTS ======================================= */

/** @brief Value of 1.0 in the current format (`1 << FXP_FRAC_BITS`). */
#define FXP_ONE       ((fxp_t)((fxp_t)1 << FXP_FRAC_BITS))

/** @brief Mask selecting the fractional bits of a raw @c fxp_t. */
#define FXP_FRAC_MASK ((fxp_wide_t)(((fxp_wide_t)1 << FXP_FRAC_BITS) - 1))

/** @brief Largest representable @c fxp_t (raw `2^(WORD-1) - 1`). */
#define FXP_MAX       ((fxp_t)((((fxp_wide_t)1) << (FXP_WORD_BITS - 1)) - 1))

/** @brief Most negative representable @c fxp_t (raw `-2^(WORD-1)`). */
#define FXP_MIN       ((fxp_t)(-FXP_MAX - 1))

#if FXP_WORD_BITS == 32
/** @brief Largest value the wide intermediate / accumulator can hold. */
#define FXP_WIDE_MAX INT64_MAX
/** @brief Most negative value the wide intermediate / accumulator can hold. */
#define FXP_WIDE_MIN INT64_MIN
#else
#define FXP_WIDE_MAX INT32_MAX
#define FXP_WIDE_MIN INT32_MIN
#endif

/* ================ CONFIGURATION INVARIANTS ================================ */

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert((FXP_WORD_BITS == 16) || (FXP_WORD_BITS == 32),
               "FXP_WORD_BITS must be 16 or 32");
/* Require at least one integer bit (beyond the sign bit) so that 1.0
 * (== FXP_ONE) is representable; the library and the FXP_INT/FXP_C macros
 * depend on it. This rules out pure-fractional Q0.x formats by design. */
_Static_assert((FXP_FRAC_BITS > 0) && (FXP_FRAC_BITS < (FXP_WORD_BITS - 1)),
               "FXP_FRAC_BITS must satisfy 0 < FXP_FRAC_BITS < FXP_WORD_BITS-1 "
               "(1.0 must be representable; pure-fractional Q0.x unsupported)");
_Static_assert((FXP_ROUNDING == FXP_ROUND_TRUNCATE)
                   || (FXP_ROUNDING == FXP_ROUND_NEAREST_UP)
                   || (FXP_ROUNDING == FXP_ROUND_NEAREST_EVEN),
               "FXP_ROUNDING must be one of the FXP_ROUND_* tokens");
/* Right shift of a negative value must be arithmetic (floors toward -inf);
 * the rounding helpers depend on it. Universal in practice, mandated by C23. */
_Static_assert((-1 >> 1) == -1, "arithmetic right shift of negatives required");
#endif

#endif /* FXP_CONFIG_H_ */
