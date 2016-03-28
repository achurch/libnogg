/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2016 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#include "include/nogg.h"
#include "tests/common.h"


int main(void)
{
    vorbis_t *vorbis;
    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_TRUE(vorbis = vorbis_open_file("tests/data/square.ogg", 0, &error));
    EXPECT_EQ(error, VORBIS_NO_ERROR);

    vorbis_close(vorbis);

    return EXIT_SUCCESS;
}
