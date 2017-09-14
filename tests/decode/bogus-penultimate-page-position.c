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
    FILE *f;
    uint8_t *data;
    long size;
    EXPECT(f = fopen("tests/data/6-mode-bits-multipage.ogg", "rb"));
    EXPECT_EQ(fseek(f, 0, SEEK_END), 0);
    EXPECT_GT(size = ftell(f), 0);
    EXPECT_EQ(fseek(f, 0, SEEK_SET), 0);
    EXPECT(data = malloc(size));
    EXPECT_EQ(fread(data, 1, size, f), size);
    fclose(f);
    /* Set the second-to-last page's granule position to a ridiculously
     * high value.  The value has the high bit set, which will cause the
     * final page to look like wraparound instead of moving backward, and
     * also results in a negative 32-bit value when subtracted from the
     * position reported in the final packet. */
    MODIFY(data[0xE00], 0x05, 0x15);
    MODIFY(data[0xE06], 0x00, 0x80);

    vorbis_t *vorbis;
    EXPECT(vorbis = vorbis_open_buffer(data, size, 0, NULL));

    float pcm[0x641];
    vorbis_error_t error = (vorbis_error_t)-1;
    /* The decoder should see what looks like an oversized final frame
     * and truncate it to the right edge of the frame's window. */
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 0x641, &error), 0x640);
    EXPECT_EQ(error, VORBIS_ERROR_STREAM_END);

    vorbis_close(vorbis);
    free(data);
    return EXIT_SUCCESS;
}
