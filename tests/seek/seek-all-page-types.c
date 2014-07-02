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

#include "tests/data/6ch-all-page-types_float.h"  // Defines expected_pcm[].


int main(void)
{
    vorbis_t *vorbis;
    EXPECT_TRUE(vorbis = vorbis_open_from_file(
                    "tests/data/6ch-all-page-types.ogg", NULL));

    for (int i = 0; i < 8500; i++) {
        if (!vorbis_seek(vorbis, i)) {
            printf("%s:%d: Seek to %d failed\n", __FILE__, __LINE__, i);
            return EXIT_FAILURE;
        }
        float pcm[6];
        vorbis_error_t error = (vorbis_error_t)-1;
        EXPECT_EQ(vorbis_read_float(vorbis, pcm, 1, &error), 1);
        EXPECT_EQ(error, VORBIS_NO_ERROR);
        for (int j = 0; j < 6; j++) {
            if (fabsf(pcm[j] - expected_pcm[i*6+j]) > 1.0e-7f) {
                fprintf(stderr, "%s:%d: Sample %d+%d was %.8g but should have"
                        " been near %.8g\n", __FILE__, __LINE__, i, j,
                        pcm[j], expected_pcm[i+j]);
                return EXIT_FAILURE;
            }
        }
    }

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}
