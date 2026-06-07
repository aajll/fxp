/**
 * SPDX-License-Identifier: MIT
 *
 * @file: fxp.h
 *
 * @brief
 *    Public API for the fxp library, a portable fixed-point arithmetic kernel
 *    for embedded C.
 *
 * @details
 *    fxp provides the deterministic integer math that fixed-point code
 *    otherwise hand-rolls: saturating and checked add/sub/mul/div, fused
 *    multiply-divide, a wide multiply-accumulate, format rescaling, clamping,
 *    interpolation, and conversion. The format is fixed at build time (default
 *    signed Q16.16); see @ref fxp_conf.h.
 *
 *    Design contract (library-wide):
 *      - **No undefined behaviour** for any in-range `fxp_t` input. Multiplies
 *        and scales use a 2*WORD intermediate and clamp before narrowing.
 *      - **No allocation, no global state.** Values are plain integers; every
 *        function is pure, reentrant, and thread-safe on distinct arguments.
 *      - **No floating point in this header or the core.** Optional `double`
 *        helpers live in @ref fxp_convert.h (a separable translation unit).
 *      - **One build-wide rounding policy** applied to every narrowing path
 *        (see @c FXP_ROUNDING).
 *      - **Determinism.** Results are bit-identical across compilers, targets,
 *        and optimisation levels.
 *
 *    Pointer-argument conventions:
 *      - An `out` result pointer on a `_chk` function is **required**: it must
 *        be non-NULL (stated as a @pre on each function). This matches the
 *        convention of hot-path numeric kernels and avoids a branch per call.
 *      - An optional status/flag pointer (e.g. `st`, `clamped`, `overflow`) is
 *        **NULL-tolerant**: pass NULL to ignore it.
 *
 *    Operation flavours:
 *      - `_sat` clamps to the representable range and returns the value.
 *      - `_chk` writes the result through an out-parameter and returns an
 *        @ref fxp_status.
 *
 * @defgroup fxp_api fxp fixed-point kernel
 * @{
 */
#ifndef FXP_H_
#define FXP_H_

#ifdef __cplusplus
extern "C" {
#endif

/* ================ INCLUDES ================================================ */

#include <stdbool.h>
#include <stdint.h>

#include "fxp_config.h"
#include "fxp_const.h"
#include "fxp_status.h"

/* ================ TYPEDEFS ================================================ */

/**
 * @typedef fxp_acc_t
 * @brief Wide accumulator for multiply-accumulate sums (FIR taps, biquads, dot
 *        products). Holds many raw products before a single narrowing step, so
 *        intermediate terms are never clamped or rounded mid-sum.
 *
 * @warning The accumulator has **finite headroom**: it is a @c fxp_wide_t
 *          (`2*WORD` bits). A raw product of two full-scale @c fxp_t values is
 *          up to `2^(2*WORD-2)`, so summing many full-scale products can
 *          overflow. The fast @c fxp_acc_mac does **not** check this (signed
 *          overflow is undefined); use @c fxp_acc_mac_chk when operand
 *          magnitudes are not known to be bounded. In Q16.16, normalised inputs
 *          (`|x| < 1`) give products `< 2^32`, leaving room for ~2^31 terms,
 *          whereas full-scale (`±32768`) inputs leave room for only ~2.
 */
typedef fxp_wide_t fxp_acc_t;

/* ================ CONSTRUCTION / READOUT ================================== */

/**
 * @brief Construct an @c fxp_t from a whole integer.
 * @param whole Integer value.
 * @return @p whole as a fixed-point value.
 * @post The result is saturated to [@c FXP_MIN, @c FXP_MAX] if @p whole lies
 *       outside the representable integer range.
 */
fxp_t fxp_from_int(int32_t whole);

/**
 * @brief Construct the value @p num / @p den.
 * @param num Numerator.
 * @param den Denominator.
 * @param[out] st Optional status (NULL to ignore): @c FXP_OK, @c FXP_SATURATED,
 *                or @c FXP_DIV_ZERO.
 * @return @p num / @p den as a fixed-point value, rounded per @c FXP_ROUNDING.
 * @post Returns 0 with @c FXP_DIV_ZERO when @p den == 0; saturates with
 *       @c FXP_SATURATED when the quotient is out of range.
 */
fxp_t fxp_from_ratio(int32_t num, int32_t den, enum fxp_status *st);

/**
 * @brief Convert to the nearest integer using the build rounding policy.
 * @param a Fixed-point value.
 * @return Rounded integer value.
 * @warning Rounding can carry past the integer range: e.g. @c fxp_to_int of a
 *          value just below @c FXP_MAX rounds up to `2^(WORD-1-FRAC)`, one
 * above the largest representable whole. Size the destination accordingly.
 */
int32_t fxp_to_int(fxp_t a);

/** @brief Largest integral value <= @p a, as an @c fxp_t (round toward -inf).
 */
fxp_t fxp_floor(fxp_t a);
/** @brief Smallest integral value >= @p a, as an @c fxp_t (round toward +inf).
 *  @post Saturates to @c FXP_MAX if the ceiling is not representable. */
fxp_t fxp_ceil(fxp_t a);
/** @brief Nearest integral value to @p a, ties to even, as an @c fxp_t. */
fxp_t fxp_round(fxp_t a);

/* ================ ARITHMETIC: SAT + CHK PAIRS ============================= */

/** @brief Saturating add. @return @p a + @p b, clamped to [MIN, MAX]. */
fxp_t fxp_add_sat(fxp_t a, fxp_t b);
/**
 * @brief Checked add.
 * @param[out] out Result (clamped on overflow). @pre @p out != NULL.
 * @retval FXP_OK Result was in range.
 * @retval FXP_SATURATED Result was clamped to [MIN, MAX].
 */
enum fxp_status fxp_add_chk(fxp_t a, fxp_t b, fxp_t *out);

/** @brief Saturating subtract. @return @p a - @p b, clamped to [MIN, MAX]. */
fxp_t fxp_sub_sat(fxp_t a, fxp_t b);
/**
 * @brief Checked subtract.
 * @param[out] out Result (clamped on overflow). @pre @p out != NULL.
 * @retval FXP_OK Result was in range.
 * @retval FXP_SATURATED Result was clamped to [MIN, MAX].
 */
enum fxp_status fxp_sub_chk(fxp_t a, fxp_t b, fxp_t *out);

/** @brief Saturating multiply (2*WORD intermediate, rounded narrow). */
fxp_t fxp_mul_sat(fxp_t a, fxp_t b);
/**
 * @brief Checked multiply.
 * @param[out] out Result (clamped on overflow). @pre @p out != NULL.
 * @retval FXP_OK Result was in range.
 * @retval FXP_SATURATED Result was clamped to [MIN, MAX].
 */
enum fxp_status fxp_mul_chk(fxp_t a, fxp_t b, fxp_t *out);

/**
 * @brief Checked divide @p a / @p b.
 * @param[out] out Result (0 on divide-by-zero, clamped on overflow).
 * @pre @p out != NULL.
 * @retval FXP_OK Result was in range.
 * @retval FXP_DIV_ZERO @p b == 0; @p out set to 0.
 * @retval FXP_SATURATED Result was clamped to [MIN, MAX].
 */
enum fxp_status fxp_div_chk(fxp_t a, fxp_t b, fxp_t *out);

/**
 * @brief Fused @p a * @p b / @p divisor, evaluated in the wide intermediate and
 *        narrowed once, avoiding the overflow / precision loss of a separate
 *        multiply then divide.
 * @param[out] out Result (0 on divide-by-zero, clamped on overflow).
 * @pre @p out != NULL.
 * @retval FXP_OK Result was in range.
 * @retval FXP_DIV_ZERO @p divisor == 0; @p out set to 0.
 * @retval FXP_SATURATED Result was clamped to [MIN, MAX].
 */
enum fxp_status fxp_muldiv_chk(fxp_t a, fxp_t b, fxp_t divisor, fxp_t *out);

/* ================ WIDE ACCUMULATOR ======================================= */

/** @brief A zeroed accumulator. */
fxp_acc_t fxp_acc_zero(void);

/**
 * @brief Fast multiply-accumulate: @p acc + @p a * @p b, kept wide.
 * @warning Does **not** check accumulator overflow (see @ref fxp_acc_t). Use
 *          only when the running sum is known to stay within @c fxp_wide_t.
 */
fxp_acc_t fxp_acc_mac(fxp_acc_t acc, fxp_t a, fxp_t b);

/**
 * @brief Checked multiply-accumulate: @p acc + @p a * @p b with overflow
 *        detection.
 * @param[out] overflow Optional (NULL to ignore): set true iff the addition
 *             would have overflowed the accumulator.
 * @return The new accumulator. On overflow it is clamped to the wide bound
 *         (@c FXP_WIDE_MAX / @c FXP_WIDE_MIN) so a later narrow saturates
 *         correctly rather than wrapping.
 */
fxp_acc_t fxp_acc_mac_chk(fxp_acc_t acc, fxp_t a, fxp_t b, bool *overflow);

/** @brief Narrow an accumulator to @c fxp_t, rounding and saturating. */
fxp_t fxp_acc_narrow_sat(fxp_acc_t acc);
/**
 * @brief Checked accumulator narrow.
 * @param[out] out Result (clamped on overflow). @pre @p out != NULL.
 * @retval FXP_OK Result was in range.
 * @retval FXP_SATURATED Result was clamped to [MIN, MAX].
 */
enum fxp_status fxp_acc_narrow_chk(fxp_acc_t acc, fxp_t *out);

/* ================ CHECKED NARROW ========================================= */

/**
 * @brief Narrow an already-fxp-scaled wide value to @c fxp_t, saturating.
 * @note No implicit `>>FRAC`: use the accumulator narrow for product sums.
 */
fxp_t fxp_narrow_sat(fxp_wide_t wide);
/**
 * @brief Checked narrow of an fxp-scaled wide value.
 * @param[out] out Result (clamped on overflow). @pre @p out != NULL.
 * @retval FXP_OK Result was in range.
 * @retval FXP_SATURATED Result was clamped to [MIN, MAX].
 */
enum fxp_status fxp_narrow_chk(fxp_wide_t wide, fxp_t *out);

/* ================ HELPERS ================================================= */

/** @brief Saturating negate. @post @c FXP_MIN negates to @c FXP_MAX. */
fxp_t fxp_neg_sat(fxp_t a);
/** @brief Saturating absolute value. @post `abs(FXP_MIN)` saturates to MAX. */
fxp_t fxp_abs_sat(fxp_t a);
/**
 * @brief Clamp @p a into [@p lo, @p hi].
 * @pre @p lo <= @p hi.
 * @return @p a clamped to the inclusive range.
 */
fxp_t fxp_clamp(fxp_t a, fxp_t lo, fxp_t hi);

/**
 * @brief Clamp a wide (pre-narrow) value into [@p lo, @p hi] and report whether
 *        clamping occurred (the shape control loops use for anti-windup and
 *        output-limit status).
 * @param[out] clamped Optional (NULL to ignore): set true iff @p a fell outside
 *             [@p lo, @p hi].
 * @pre @p lo <= @p hi.
 */
fxp_t fxp_clamp_wide(fxp_wide_t a, fxp_t lo, fxp_t hi, bool *clamped);

/**
 * @brief Linear interpolation @p a + (@p b - @p a) * @p t.
 * @param t Blend factor, typically in [0, 1] (Q-format); values outside
 *          extrapolate.
 * @note The `(b - a)` term is evaluated wide to avoid overflow; the result is
 *       saturated.
 */
fxp_t fxp_lerp(fxp_t a, fxp_t b, fxp_t t);

/* ================ COMPARISON ============================================== */

/**
 * @brief Three-way compare for the current format.
 * @return <0 if @p a < @p b, 0 if equal, >0 if @p a > @p b.
 * @warning Defined only for two values in the **same** format. To compare
 *          across formats, convert first with @c fxp_rescale.
 */
int fxp_cmp(fxp_t a, fxp_t b);

/* ================ FORMAT CONVERSION ====================================== */

/**
 * @brief Rescale a raw value between fractional-bit counts.
 * @param a Raw value interpreted with @p from_frac fractional bits.
 * @param from_frac Source fractional bit count.
 * @param to_frac Destination fractional bit count.
 * @param[out] out Result interpreted with @p to_frac fractional bits (0 on a
 *             bad parameter). @pre @p out != NULL.
 * @pre @p from_frac, @p to_frac are each < @c FXP_WIDE_BITS.
 * @retval FXP_OK Exact conversion.
 * @retval FXP_BAD_PARAM @p from_frac or @p to_frac >= @c FXP_WIDE_BITS; @p out
 *         set to 0. (A larger shift would be undefined / silently masked.)
 * @retval FXP_SATURATED Up-scaling overflowed; @p out clamped.
 * @retval FXP_INEXACT Down-scaling discarded nonzero low bits.
 */
enum fxp_status fxp_rescale(fxp_t a, uint8_t from_frac, uint8_t to_frac,
                            fxp_t *out);

#ifdef __cplusplus
}
#endif

#endif /* FXP_H_ */

/** @} */
