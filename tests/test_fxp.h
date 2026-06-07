/**
 * SPDX-License-Identifier: MIT
 *
 * @file: test_fxp.h
 *
 * @brief
 *    Minimal assertion / runner harness shared by the fxp unit tests.
 */
#ifndef TEST_FXP_H_
#define TEST_FXP_H_

#include <stdio.h>
#include <stdlib.h>

/** @brief Abort the process with a diagnostic if @p expr is false. */
#define TEST_ASSERT(expr)                                                      \
        do {                                                                   \
                if (!(expr)) {                                                 \
                        (void)fprintf(stderr, "FAIL  %s:%d  %s\n", __FILE__,   \
                                      __LINE__, #expr);                        \
                        exit(EXIT_FAILURE);                                    \
                }                                                              \
        } while (0)

/** @brief Report a passing test case by name. */
#define TEST_PASS(name) (void)fprintf(stdout, "PASS  %s\n", (name))

/** @brief Declare and open a test-case function body. */
#define TEST_CASE(name)                                                        \
        static void name(void);                                                \
        static void name(void)

/** @brief Run a test-case function and report it. */
#define TEST_RUN(fn)                                                           \
        do {                                                                   \
                (fn)();                                                        \
                TEST_PASS(#fn);                                                \
        } while (0)

#endif /* TEST_FXP_H_ */
