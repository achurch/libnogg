/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#include "include/nogg.h"
#include "include/test.h"

#include "tests/data/square_float.h"  // Defines expected_pcm[].


int main(void)
{
    vorbis_t *vorbis;
    EXPECT_TRUE(vorbis = vorbis_open_from_file("tests/data/square.ogg", NULL));

    float pcm[1];
    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_float(vorbis, NULL, 1, &error), 0);
    EXPECT_EQ(error, VORBIS_ERROR_INVALID_ARGUMENT);
    error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, -1, &error), 0);
    EXPECT_EQ(error, VORBIS_ERROR_INVALID_ARGUMENT);

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}
