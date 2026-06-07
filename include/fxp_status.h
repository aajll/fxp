/**
 * SPDX-License-Identifier: MIT
 *
 * @file: fxp_status.h
 *
 * @brief
 *    Status codes returned by the checked (`_chk`) fxp operations.
 *
 * @details
 *    Fixed-point has no NaN/Inf sentinel, so out-of-range and degenerate
 *    operations are reported through this enum rather than through a magic
 *    return value. Saturating (`_sat`) operations clamp silently and do not
 *    return a status; the checked variants write their result through an
 *    out-parameter and return one of these codes.
 */
#ifndef FXP_STATUS_H_
#define FXP_STATUS_H_

/**
 * @enum fxp_status
 * @brief Result of a checked fxp operation.
 */
enum fxp_status {
        FXP_OK = 0,    /**< Exact, in-range result.                          */
        FXP_SATURATED, /**< Result was out of range and clamped to [MIN,MAX].*/
        FXP_OVERFLOW,  /**< Input could not be represented at all (convert). */
        FXP_DIV_ZERO,  /**< Divisor / denominator was zero; result is 0.     */
        FXP_INEXACT,   /**< Nonzero low bits were discarded by rounding.     */
        FXP_BAD_PARAM, /**< An argument was out of its valid domain.         */
};

#endif /* FXP_STATUS_H_ */
