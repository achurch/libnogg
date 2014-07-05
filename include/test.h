/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

/*
 * This header defines various macros useful in test code.  This is not a
 * public header.
 */

#ifndef NOGG_TEST_H
#define NOGG_TEST_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Maximum allowable error in a floating-point PCM sample.  Define before
 * including this file to use a value other than the default. */
#ifndef PCM_FLOAT_ERROR
# define PCM_FLOAT_ERROR  (1.0e-6f)
#endif

/*************************************************************************/
/*************************************************************************/

/**
 * EXPECT_TRUE:  Check that the given expression is true, and fail the
 * test if not.
 */
#define EXPECT_TRUE(expr)  do {                                         \
    if (!(expr)) {                                                      \
        fprintf(stderr, "%s:%d: %s was not true as expected\n",         \
                __FILE__, __LINE__, #expr);                             \
        return EXIT_FAILURE;                                            \
    }                                                                   \
} while (0)

/*-----------------------------------------------------------------------*/

/**
 * EXPECT_FALSE:  Check that the given expression is false, and fail the
 * test if not.
 */
#define EXPECT_FALSE(expr)  do {                                        \
    if ((expr)) {                                                       \
        fprintf(stderr, "%s:%d: %s was not false as expected\n",        \
                __FILE__, __LINE__, #expr);                             \
        return EXIT_FAILURE;                                            \
    }                                                                   \
} while (0)

/*-----------------------------------------------------------------------*/

/**
 * EXPECT_EQ:  Check that the given integer expression is equal to an
 * expected value, and fail the test if not.
 */
#define EXPECT_EQ(expr, value)  do {                                    \
    const intmax_t _expr = (expr);                                      \
    const intmax_t _value = (value);                                    \
    if (!(_expr == _value)) {                                           \
        fprintf(stderr, "%s:%d: %s was %jd but should have been %jd\n", \
                __FILE__, __LINE__, #expr, _expr, _value);              \
        return EXIT_FAILURE;                                            \
    }                                                                   \
} while (0)

/*-----------------------------------------------------------------------*/

/**
 * EXPECT_GT:  Check that the given integer expression is greather than an
 * expected value, and fail the test if not.
 */
#define EXPECT_GT(expr, value)  do {                                    \
    const intmax_t _expr = (expr);                                      \
    const intmax_t _value = (value);                                    \
    if (!(_expr > _value)) {                                            \
        fprintf(stderr, "%s:%d: %s was %jd but should have been"        \
                " greater than %jd\n",                                  \
                __FILE__, __LINE__, #expr, _expr, _value);              \
        return EXIT_FAILURE;                                            \
    }                                                                   \
} while (0)

/*-----------------------------------------------------------------------*/

/**
 * EXPECT_FLTEQ:  Check that the given floating-point expression is equal
 * to an expected value, and fail the test if not.
 */
#define EXPECT_FLTEQ(expr, value)  do {                                 \
    const long double _expr = (expr);                                   \
    const long double _value = (value);                                 \
    if (!(_expr == _value)) {                                           \
        fprintf(stderr, "%s:%d: %s was %Lg but should have been %Lg\n", \
                __FILE__, __LINE__, #expr, _expr, _value);              \
        return EXIT_FAILURE;                                            \
    }                                                                   \
} while (0)

/*-----------------------------------------------------------------------*/

/**
 * EXPECT_STREQ:  Check that the given string expression is equal to an
 * expected value, and fail the test if not.  A value of NULL is permitted.
 */
#define EXPECT_STREQ(expr, value)  do {                                 \
    const char * const _expr = (expr);                                  \
    const char * const _value = (value);                                \
    if (_expr && !_value) {                                             \
        fprintf(stderr, "%s:%d: %s was \"%s\" but should have been NULL\n", \
                __FILE__, __LINE__, #expr, _expr);                      \
        return EXIT_FAILURE;                                            \
    } else if (!_expr && _value) {                                      \
        fprintf(stderr, "%s:%d: %s was NULL but should have been \"%s\"\n", \
                __FILE__, __LINE__, #expr, _value);                     \
        return EXIT_FAILURE;                                            \
    } else if (_expr && _value && strcmp(_expr, _value) != 0) {         \
        fprintf(stderr, "%s:%d: %s was \"%s\" but should have been \"%s\"\n", \
                __FILE__, __LINE__, #expr, _expr, _value);              \
        return EXIT_FAILURE;                                            \
    }                                                                   \
} while (0)

/*-----------------------------------------------------------------------*/

/**
 * COMPARE_PCM_INT16:  Compare 'len' PCM samples of type int16_t in 'buf'
 * to expected values in 'expected', and fail the test if any do not match.
 */
#define COMPARE_PCM_INT16(buf, expected, len)  do {                     \
    const int16_t * const _buf = (buf);                                 \
    const int16_t * const _expected = (expected);                       \
    const int _len = (len);                                             \
    for (int _i = 0; _i < _len; _i++) {                                 \
        if (_buf[_i] != _expected[_i]) {                                \
            fprintf(stderr, "%s:%d: Sample %d was %d but should have"   \
                    " been %d\n", __FILE__, __LINE__, _i, _buf[_i],     \
                    _expected[_i]);                                     \
            return EXIT_FAILURE;                                        \
        }                                                               \
    }                                                                   \
} while (0)

/*-----------------------------------------------------------------------*/

/**
 * COMPARE_PCM_FLOAT:  Compare 'len' PCM samples of type float in 'buf'
 * to expected values in 'expected', and fail the test if any do not match.
 * Values in 'expected' outside the range (-1.1,+1.1) are assumed to
 * indicate corrupt source data and are ignored.
 */
#define COMPARE_PCM_FLOAT(buf, expected, len)  do {                     \
    const float * const _buf = (buf);                                   \
    const float * const _expected = (expected);                         \
    const int _len = (len);                                             \
    for (int _i = 0; _i < _len; _i++) {                                 \
        if (fabsf(_expected[_i]) > 1.1f) {                              \
            continue;                                                   \
        }                                                               \
        const float _epsilon_base = fabsf(floorf(_expected[_i])) + 1;   \
        const float _epsilon = _epsilon_base * PCM_FLOAT_ERROR;         \
        if (fabsf(_buf[_i] - _expected[_i]) > _epsilon) {               \
            fprintf(stderr, "%s:%d: Sample %d was %.8g but should have" \
                    " been near %.8g\n", __FILE__, __LINE__, _i,        \
                    _buf[_i], _expected[_i]);                           \
            return EXIT_FAILURE;                                        \
        }                                                               \
    }                                                                   \
} while (0)

/*-----------------------------------------------------------------------*/

/**
 * MODIFY:  Check that 'lvalue' has the value 'expected', then store 'new'
 * in 'lvalue'.
 */
#define MODIFY(lvalue, expected, new)  do {     \
    EXPECT_EQ(lvalue, (expected));              \
    lvalue = (new);                             \
} while (0)

/*************************************************************************/
/*************************************************************************/

#endif  // NOGG_TEST_H
