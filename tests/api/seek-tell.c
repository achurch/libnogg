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
    EXPECT(vorbis = vorbis_open_file("tests/data/6-mode-bits-multipage.ogg",
                                     0, NULL));

    float pcm[1];
    EXPECT(vorbis_seek(vorbis, 1));
    EXPECT_EQ(vorbis_tell(vorbis), 1);
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 1, NULL), 1);

    EXPECT(vorbis_seek(vorbis, 1492));
    EXPECT_EQ(vorbis_tell(vorbis), 1492);
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 1, NULL), 0);

    EXPECT(vorbis_seek(vorbis, 1024));
    EXPECT_EQ(vorbis_tell(vorbis), 1024);
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 1, NULL), 1);

    EXPECT(vorbis_seek(vorbis, 1493));
    EXPECT_EQ(vorbis_tell(vorbis), 1492);
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 1, NULL), 0);

    EXPECT(vorbis_seek(vorbis, 0));
    EXPECT_EQ(vorbis_tell(vorbis), 0);
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 1, NULL), 1);

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}
