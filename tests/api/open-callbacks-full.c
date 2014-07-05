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


/* Counts of calls to each of the callbacks. */
static int length_count;
static int tell_count;
static int seek_count;
static int read_count;
static int close_count;

static int64_t length(void *opaque)
{
    length_count++;
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
    tell_count++;
    return ftell((FILE *)opaque);
}

static void seek(void *opaque, int64_t offset)
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

    EXPECT_TRUE(f = fopen("tests/data/square.ogg", "rb"));
    EXPECT_TRUE(vorbis = vorbis_open_callbacks(((const vorbis_callbacks_t){
                                                   .length = length,
                                                   .tell = tell,
                                                   .seek = seek,
                                                   .read = read,
                                                   .close = close}),
                                               f, &error));
    EXPECT_EQ(error, VORBIS_NO_ERROR);
    EXPECT_TRUE(length_count > 0);
    EXPECT_TRUE(tell_count > 0);
    EXPECT_TRUE(seek_count == 0);
    EXPECT_TRUE(read_count > 0);
    EXPECT_EQ(close_count, 0);

    EXPECT_EQ(vorbis_length(vorbis), 40);
    EXPECT_TRUE(seek_count > 0);

    vorbis_close(vorbis);
    EXPECT_EQ(close_count, 1);

    return EXIT_SUCCESS;
}
