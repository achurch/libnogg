/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2015 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#include "include/nogg.h"
#include "src/common.h"
#include "tests/common.h"


int main(void)
{
    vorbis_t *vorbis;
    EXPECT_TRUE(vorbis = vorbis_open_file("tests/data/noise-6ch.ogg",
                                          0, NULL));

    EXPECT_EQ(stb_vorbis_tell_bits(vorbis->decoder), 0x1D11*8);

    float pcm[6];
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 1, NULL), 1);
    EXPECT_EQ(stb_vorbis_tell_bits(vorbis->decoder), 0x1E94*8);

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}
