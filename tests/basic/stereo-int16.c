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


static const int16_t expected_pcm[40] = {
      9401,
      9401,
     -9313,
     -9313,
      9059,
      9059,
     -9043,
     -9043,
      9128,
      9128,
     -9275,
     -9275,
      9346,
      9346,
     -9419,
     -9419,
      9324,
      9324,
     -9309,
     -9309,
      9331,
      9331,
     -9417,
     -9417,
      9602,
      9602,
    -10022,
    -10022,
     10418,
     10418,
    -10852,
    -10852,
     11004,
     11004,
    -10997,
    -10997,
     10873,
     10873,
    -10661,
    -10661,
};

int main(void)
{
    vorbis_t *decoder = vorbis_open_from_file("tests/data/square-stereo.ogg", NULL);
    EXPECT_TRUE(decoder);
    EXPECT_EQ(vorbis_channels(decoder), 2);
    EXPECT_EQ(vorbis_rate(decoder), 4000);

    int16_t pcm[42];
    EXPECT_EQ(vorbis_read_int16(decoder, pcm, 21, NULL), 20);
    COMPARE_PCM_INT16(pcm, expected_pcm, 40);

    vorbis_close(decoder);
    return EXIT_SUCCESS;
}
