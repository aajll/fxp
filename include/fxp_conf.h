/**
 * SPDX-License-Identifier: MIT
 *
 * @file: fxp_conf.h
 *
 * @brief
 *    Compile-time configuration knobs for the fxp fixed-point kernel.
 *
 * @details
 *    This header selects the fixed-point format and rounding policy at build
 *    time. It is pulled in automatically by @ref fxp_config.h (and therefore by
 *    @ref fxp.h). Every option uses an `#ifndef` guard, so a value may be
 *    overridden either by defining it on the compiler command line
 *    (`-DFXP_FRAC_BITS=24`) or by defining it before the first include of any
 *    fxp header.
 *
 *    The format is fixed for the whole build; there is no runtime-tagged
 *    precision. Consumers mixing formats convert at boundaries with
 *    @c fxp_rescale().
 *
 * @note Defaults: signed Q16.16 (32-bit word, 16 fractional bits), round to
 *       nearest, ties to even (RNE).
 */
#ifndef FXP_CONF_H_
#define FXP_CONF_H_

/* ================ ROUNDING MODE TOKENS ==================================== */

/**
 * @def FXP_ROUND_TRUNCATE
 * @brief Round toward negative infinity (bare arithmetic shift / floor).
 *        Cheapest; carries a systematic toward-`-inf` bias.
 */
#define FXP_ROUND_TRUNCATE     (0)

/**
 * @def FXP_ROUND_NEAREST_UP
 * @brief Round to nearest; ties resolve toward positive infinity.
 *        One add + shift; tiny positive bias on exact-half values.
 */
#define FXP_ROUND_NEAREST_UP   (1)

/**
 * @def FXP_ROUND_NEAREST_EVEN
 * @brief Round to nearest; ties resolve to the even neighbour ("banker's
 *        rounding"). Zero statistical bias; IEEE-754 default. Recommended.
 */
#define FXP_ROUND_NEAREST_EVEN (2)

/* ================ CONFIGURATION =========================================== */

#ifndef FXP_WORD_BITS
/**
 * @def FXP_WORD_BITS
 * @brief Underlying signed integer storage width, in bits. Supported: 16 or 32.
 *
 * @note 64 is not supported: a correct multiply needs a 2*WORD intermediate and
 *       there is no portable 128-bit integer.
 */
#define FXP_WORD_BITS (32)
#endif

#ifndef FXP_FRAC_BITS
/**
 * @def FXP_FRAC_BITS
 * @brief Number of fractional bits. Must satisfy
 *        `0 < FXP_FRAC_BITS < FXP_WORD_BITS - 1`.
 *
 * @note At least one integer bit (beyond the sign bit) is required so that
 *       1.0 (`FXP_ONE`) is representable; pure-fractional Q0.x formats are not
 *       supported. The upper bound is enforced by a `_Static_assert`.
 *
 * @note With the defaults this yields signed Q16.16 (15 integer bits + sign,
 *       16 fractional bits; resolution 2^-16, range approximately +/-32768).
 */
#define FXP_FRAC_BITS (16)
#endif

#ifndef FXP_ROUNDING
/**
 * @def FXP_ROUNDING
 * @brief Rounding policy applied to every narrowing path (multiply, divide,
 *        multiply-divide, accumulator narrow, rescale-down, and `to_int`).
 *
 * @note One policy, build-wide, never per call. See @ref fxp_config.h.
 */
#define FXP_ROUNDING FXP_ROUND_NEAREST_EVEN
#endif

#endif /* FXP_CONF_H_ */
