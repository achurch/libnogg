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


int main(void)
{
    vorbis_t *vorbis;
    EXPECT_TRUE(vorbis = vorbis_open_file("tests/data/square.ogg", NULL));
    vorbis_close(vorbis);

    EXPECT_FALSE(vorbis_open_file(NULL, NULL));
    EXPECT_FALSE(vorbis_open_file("tests/data/nonexistent", NULL));

    return EXIT_SUCCESS;
}
