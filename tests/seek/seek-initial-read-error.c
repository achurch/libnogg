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

#include "tests/data/6ch_float.h"  // Defines expected_pcm[].


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

static int read_count;
static int32_t read(void *opaque, void *buf, int32_t len)
{
    if (read_count > 0) {
        read_count--;
        return fread(buf, 1, len, (FILE *)opaque);
    } else {
        return 0;
    }
}

static void close(void *opaque)
{
    fclose((FILE *)opaque);
}


int main(void)
{
    FILE *f;
    vorbis_t *vorbis;

    read_count = 10000;
    EXPECT_TRUE(f = fopen("tests/data/6ch-all-page-types.ogg", "rb"));
    EXPECT_TRUE(vorbis = vorbis_open_from_callbacks(
                    ((const vorbis_callbacks_t){
                        .length = length,
                        .tell = tell,
                        .seek = seek,
                        .read = read,
                        .close = close}),
                    f, NULL));

    read_count = 0;
    EXPECT_FALSE(vorbis_seek(vorbis, 0));

    read_count = 10000;
    /* Should still fail because the initial failure was cached. */
    EXPECT_FALSE(vorbis_seek(vorbis, 0));

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}
