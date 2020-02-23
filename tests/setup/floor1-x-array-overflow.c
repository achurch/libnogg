/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2020 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

/* Tests handling of a type 1 floor definition containing more X values
 * (66) than the spec permits (65).  If FLOOR1_X_LIST_MAX (defined in
 * src/decode/common.h) was increased to 66, the test file would decode
 * to the same data as "6-mode-bits.ogg". */

#include "include/nogg.h"
#include "tests/common.h"


int main(void)
{
    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_FALSE(vorbis_open_file("tests/data/floor1-x-array-overflow.ogg",
                                  0, &error));
    EXPECT_EQ(error, VORBIS_ERROR_DECODE_SETUP_FAILED);

    return EXIT_SUCCESS;
}
