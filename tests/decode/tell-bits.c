/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2016 Andrew Church <achurch@achurch.org>
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
    EXPECT_TRUE(vorbis = vorbis_open_file("tests/data/noise-6ch.ogg",
                                          0, NULL));

    EXPECT_EQ(stb_vorbis_tell_bits(vorbis->decoder), 0x1D11*8);

    EXPECT_TRUE(start_page(vorbis->decoder, false));
    EXPECT_EQ(stb_vorbis_tell_bits(vorbis->decoder), 0x1D40*8);

    EXPECT_TRUE(start_packet(vorbis->decoder));
    EXPECT_EQ(get_bits(vorbis->decoder, 5), 0x14);
    EXPECT_EQ(stb_vorbis_tell_bits(vorbis->decoder), 0x1D40*8+5);

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}
