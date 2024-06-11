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


/* Counts of calls to each of the callbacks. */
static int length_count;
static int tell_count;
static int seek_count;
static int read_count;
static int close_count;

static int64_t length(void *opaque)
{
    length_count++;
    return -1;
}

static int64_t tell(void *opaque)  // Should never be called.
{
    tell_count++;
    return ftell((FILE *)opaque);
}

static void seek(void *opaque, int64_t offset)  // Should never be called.
{
    seek_count++;
    fseek((FILE *)opaque, offset, SEEK_SET);
}

static int32_t read(void *opaque, void *buf, int32_t len)
{
    read_count++;
    return fread(buf, 1, len, (FILE *)opaque);
}

static void close(void *opaque)
{
    close_count++;
    fclose((FILE *)opaque);
}


int main(void)
{
    FILE *f;
    vorbis_t *vorbis;
    vorbis_error_t error = (vorbis_error_t)-1;

    EXPECT(f = fopen("tests/data/square.ogg", "rb"));
    EXPECT(vorbis = vorbis_open_callbacks(((const vorbis_callbacks_t){
                                              .length = length,
                                              .tell = tell,
                                              .seek = seek,
                                              .read = read,
                                              .close = close}),
                                          f, 0, &error));
    EXPECT_EQ(error, VORBIS_NO_ERROR);
    EXPECT_GT(length_count, 0);
    EXPECT_EQ(tell_count, 0);
    EXPECT_EQ(seek_count, 0);
    EXPECT_GT(read_count, 0);
    EXPECT_EQ(close_count, 0);

    vorbis_close(vorbis);
    EXPECT_EQ(close_count, 1);

    return EXIT_SUCCESS;
}
