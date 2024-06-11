/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2024 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#include "include/nogg.h"
#include "tests/common.h"

#include "tests/data/square_float.h"  // Defines expected_pcm[].


static bool failed_read = false;

static int64_t length(void *opaque)
{
    FILE *f = (FILE *)opaque;
    const long saved_offset = ftell(f);
    if (fseek(f, 0, SEEK_END) != 0) {
        return -1;
    }
    const int64_t len = ftell(f);
    if (fseek(f, saved_offset, SEEK_SET) != 0) {
        return -1;
    }
    return len;
}

static int64_t tell(void *opaque)
{
    return ftell((FILE *)opaque);
}

static void seek(void *opaque, int64_t offset)
{
    fseek((FILE *)opaque, offset, SEEK_SET);
}

static int32_t read(void *opaque, void *buf, int32_t len)
{
    FILE *f = (FILE *)opaque;
    if (!failed_read) {
        /* Simulate a transient read error on the last byte of the file. */
        const int64_t size = length(opaque);
        const int64_t pos = ftell(f);
        const int64_t error_pos = size - 1;
        if (pos + len > error_pos) {
            len = error_pos - pos;
            failed_read = true;
            if (len == 0) {
                return 0;
            }
        }
    }
    return fread(buf, 1, len, f);
}

static void close(void *opaque)
{
    fclose((FILE *)opaque);
}


int main(void)
{
    FILE *f;
    EXPECT(f = fopen("tests/data/square.ogg", "rb"));
    vorbis_t *vorbis;
    EXPECT(vorbis = vorbis_open_callbacks(((const vorbis_callbacks_t){
                                              .length = length,
                                              .tell = tell,
                                              .seek = seek,
                                              .read = read,
                                              .close = close}),
                                          f, 0, NULL));

    float pcm[41];

    /* The transient read error should cause a decode failure. */
    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 1, &error), 0);
    EXPECT_EQ(error, VORBIS_ERROR_DECODE_FAILED);
    EXPECT(failed_read);

    /* The error should not prevent a subsequent seek and re-read. */
    EXPECT(vorbis_seek(vorbis, 0));
    error = (vorbis_error_t)-1;
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 41, &error), 40);
    EXPECT_EQ(error, VORBIS_ERROR_STREAM_END);
    COMPARE_PCM_FLOAT(pcm, expected_pcm, 40);

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}
