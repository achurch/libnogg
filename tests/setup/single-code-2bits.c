/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2016 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

/* Tests handling of a codebook containing a single symbol encoded in
 * non-ordered sparse mode, but with a code length of 2 (which is invalid). */

#include "include/nogg.h"
#include "tests/common.h"


int main(void)
{
    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_FALSE(vorbis_open_file("tests/data/single-code-2bits.ogg", 0,
                                  &error));
    EXPECT_EQ(error, VORBIS_ERROR_DECODE_SETUP_FAILED);
    return EXIT_SUCCESS;
}
