/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2022 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#include "include/nogg.h"
#include "src/common.h"
#include "src/decode/common.h"
#include "src/decode/packet.h"
#include "tests/common.h"


int main(void)
{
    vorbis_t *vorbis;
    EXPECT(vorbis = TEST___open_file("tests/data/square.ogg", 0, NULL));

    EXPECT_EQ(stb_vorbis_tell_pcm(vorbis->decoder), 0);

    float pcm[1];
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 1, NULL), 1);
    EXPECT_EQ(stb_vorbis_tell_pcm(vorbis->decoder), 40);

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}
