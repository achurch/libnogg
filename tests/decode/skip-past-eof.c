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


static const char truncated_comment_page[90] =
    "OggS\0\2\0\0\0\0\0\0\0\0\1\0\0\0\0\0\0\0\0\0\0\0\1\x1E"
    "\1vorbis\0\0\0\0\1\xA0\x0F\0\0\0\0\0\0\xFF\xFF\xFF\xFF"
    "\0\0\0\0\x99\x01"
    "OggS\0\0\0\0\0\0\0\0\0\0\1\0\0\0\0\0\0\0\0\0\0\0\1\x1E"
    "\3vor";

static int32_t read(void *opaque, void *buf, int32_t len)
{
    int *ppos = (int *)opaque;
    if (len > (int)sizeof(truncated_comment_page) - *ppos) {
        len = (int)sizeof(truncated_comment_page) - *ppos;
    }
    memcpy(buf, truncated_comment_page + *ppos, len);
    *ppos += len;
    return len;
}

int main(void)
{
    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_FALSE(vorbis_open_from_buffer(
                     truncated_comment_page, sizeof(truncated_comment_page),
                     &error));
    EXPECT_EQ(error, VORBIS_ERROR_STREAM_INVALID);

    error = (vorbis_error_t)-1;
    int pos = 0;
    EXPECT_FALSE(vorbis_open_from_callbacks(
                     ((const vorbis_callbacks_t){.read = read}),
                     &pos, &error));
    EXPECT_EQ(error, VORBIS_ERROR_STREAM_INVALID);

    return EXIT_SUCCESS;
}
