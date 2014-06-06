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


static const float expected_pcm[40] = {
     0.29796642,
     0.28825402,
    -0.29268929,
    -0.29751733,
     0.29216015,
     0.30115831,
    -0.29719138,
    -0.30642563,
     0.30050674,
     0.30787474,
    -0.30737656,
    -0.31176230,
     0.31152803,
     0.31318176,
    -0.31650603,
    -0.31528369,
     0.31903532,
     0.31498858,
    -0.32139820,
    -0.31334731,
     0.32383591,
     0.31142461,
    -0.32516962,
    -0.30818537,
     0.32661662,
     0.30529839,
    -0.32614839,
    -0.30038923,
     0.32665163,
     0.29660469,
    -0.32541728,
    -0.29181275,
     0.32483107,
     0.28928989,
    -0.32133257,
    -0.28613579,
     0.31750277,
     0.28576025,
    -0.30977806,
    -0.28817573,
};

int main(void)
{
    vorbis_t *decoder = vorbis_open_from_file("tests/data/square.ogg", NULL);
    EXPECT_TRUE(decoder);
    EXPECT_EQ(vorbis_channels(decoder), 1);
    EXPECT_EQ(vorbis_rate(decoder), 4000);

    float pcm[41];
    EXPECT_EQ(vorbis_read_float(decoder, pcm, 41, NULL), 40);
    COMPARE_PCM_FLOAT(pcm, expected_pcm, 40);

    vorbis_close(decoder);
    return EXIT_SUCCESS;
}
