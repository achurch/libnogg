/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2020 Andrew Church <achurch@achurch.org>
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
    EXPECT(vorbis = vorbis_open_file("tests/data/square.ogg", 0, NULL));

    float pcm[40];

    EXPECT_EQ(vorbis_tell(vorbis), 0);

    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 1, NULL), 1);
    EXPECT_EQ(vorbis_tell(vorbis), 1);

    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 39, NULL), 39);
    EXPECT_EQ(vorbis_tell(vorbis), 40);

    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 1, NULL), 0);
    EXPECT_EQ(vorbis_tell(vorbis), 40);

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}
