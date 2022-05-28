/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2022 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#ifndef NOGG_TESTS_COMMON_H
#define NOGG_TESTS_COMMON_H

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
 * LOG:  Log the given message along with the source file and line.  Pass
 * a format string and format arguments as for printf().
 */
#define LOG(...)  do {                                                  \
    fprintf(stderr, "%s:%d: ", __FILE__, __LINE__);                     \
    fprintf(stderr, __VA_ARGS__);                                       \
    fputc('\n', stderr);                                                \
} while (0)

/*-----------------------------------------------------------------------*/

/**
 * FAIL:  Log the given error message along with the source file and line,
 * and fail the current test.  Pass a format string and format arguments as
 * for printf().
 */
#define FAIL(...)  do {                                                 \
    LOG(__VA_ARGS__);                                                   \
    return EXIT_FAILURE;                                                \
} while (0)

/*-----------------------------------------------------------------------*/

/**
 * EXPECT:  Check that the given expression is true, and fail the test if not.
 */
#define EXPECT(expr)  do {                                              \
    if (!(expr)) {                                                      \
        FAIL("%s was not true as expected", #expr);                     \
    }                                                                   \
} while (0)

/*-----------------------------------------------------------------------*/

/**
 * EXPECT_FALSE:  Check that the given expression is false, and fail the
 * test if not.
 */
#define EXPECT_FALSE(expr)  do {                                        \
    if ((expr)) {                                                       \
        FAIL("%s was not false as expected", #expr);                    \
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
        FAIL("%s was %jd but should have been %jd", #expr, _expr, _value); \
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
        FAIL("%s was %jd but should have been greater than %jd",      \
             #expr, _expr, _value);                                     \
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
        FAIL("%s was %Lg but should have been %Lg", #expr, _expr, _value); \
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
        FAIL("%s was \"%s\" but should have been NULL", #expr, _expr);  \
    } else if (!_expr && _value) {                                      \
        FAIL("%s was NULL but should have been \"%s\"", #expr, _value); \
    } else if (_expr && _value && strcmp(_expr, _value) != 0) {         \
        FAIL("%s was \"%s\" but should have been \"%s\"",               \
             #expr, _expr, _value);                                     \
    }                                                                   \
} while (0)

/*-----------------------------------------------------------------------*/

/**
 * EXPECT_MEMEQ:  Check that the given memory buffer has the expected
 * contents, and fail the test if not.
 */
#define EXPECT_MEMEQ(expr, value, size)  do {                           \
    const void * const _expr = (expr);                                  \
    const void * const _value = (value);                                \
    const int _size = (size);                                           \
    if (!_expr) {                                                       \
        FAIL("%s was NULL but should not have been", #expr);            \
    } else if (memcmp(_expr, _value, _size) != 0) {                     \
        FAIL("%s did not match the expected data", #expr);              \
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
            FAIL("Sample %d was %d but should have been %d",            \
                 _i, _buf[_i], _expected[_i]);                          \
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
            FAIL("Sample %d was %.8g but should have been near %.8g",   \
                 _i, _buf[_i], _expected[_i]);                          \
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

/*-----------------------------------------------------------------------*/

/**
 * TEST___open_file:  Wrapper for vorbis_open_file() which substitutes
 * a call to vorbis_open_callbacks() with locally defined callbacks if
 * USE_STDIO is disabled.  The file specified by "path" must exist.
 */

#ifdef USE_STDIO
    #define TEST___open_file  vorbis_open_file
#else

static int64_t file_length(void *opaque)
{
    FILE *f = (FILE *)opaque;
    const long saved_offset = ftell(f);
    if (saved_offset < 0) {
        return -1;
    }
    fseek(f, 0, SEEK_END);
    const int64_t length = ftell(f);
    fseek(f, saved_offset, SEEK_SET);
    return length;
}

static int64_t file_tell(void *opaque)
{
    FILE *f = (FILE *)opaque;
    return ftell(f);
}

static void file_seek(void *opaque, int64_t offset)
{
    FILE *f = (FILE *)opaque;
    fseek(f, (long)offset, SEEK_SET);
}

static int32_t file_read(void *opaque, void *buffer, int32_t length)
{
    FILE *f = (FILE *)opaque;
    return (int32_t)fread(buffer, 1, (size_t)length, f);
}

static void file_close(void *opaque)
{
    FILE *f = (FILE *)opaque;
    fclose(f);
}

static const vorbis_callbacks_t file_callbacks = {
    .length = file_length,
    .tell   = file_tell,
    .seek   = file_seek,
    .read   = file_read,
    .close  = file_close,
};

#ifndef UNUSED
    #ifdef __GNUC__
        #define UNUSED  __attribute__((unused))
    #else
        #define UNUSED  /*nothing*/
    #endif
#endif
static UNUSED vorbis_t *TEST___open_file(
    const char *path, unsigned int options, vorbis_error_t *error_ret)
{
    FILE *f = fopen(path, "rb");
    vorbis_t *handle =
        vorbis_open_callbacks(file_callbacks, f, options, error_ret);
    if (!handle) {
        fclose(f);
    }
    return handle;
}

#endif  // USE_STDIO

/*************************************************************************/
/*************************************************************************/

#endif  // NOGG_TESTS_COMMON_H
