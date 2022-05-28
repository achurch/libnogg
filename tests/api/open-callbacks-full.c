/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2022 Andrew Church <achurch@achurch.org>
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
static int malloc_count;
static int free_count;
/* Count of calls to free() with a non-NULL pointer (should match the count
 * of calls to malloc()). */
static int free_nonnull_count;

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

static void *local_malloc(void *opaque, int32_t size, int32_t align)
{
    malloc_count++;
    /* We ignore the align parameter here since it doesn't matter for
     * the purposes of this particular test. */
    return malloc(size);
}

static void local_free(void *opaque, void *ptr)
{
    free_count++;
    if (ptr) {
        free_nonnull_count++;
    }
    free(ptr);
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
                                              .close = close,
                                              .malloc = local_malloc,
                                              .free = local_free}),
                                          f, 0, &error));
    EXPECT_EQ(error, VORBIS_NO_ERROR);
    EXPECT_GT(length_count, 0);
    EXPECT_GT(tell_count, 0);
    EXPECT_EQ(seek_count, 0);
    EXPECT_GT(read_count, 0);
    EXPECT_EQ(close_count, 0);
    EXPECT_GT(malloc_count, 0);

    EXPECT_EQ(vorbis_length(vorbis), 40);
    EXPECT_GT(seek_count, 0);

    vorbis_close(vorbis);
    EXPECT_EQ(close_count, 1);
    EXPECT_GT(free_count, 0);
    EXPECT_EQ(free_nonnull_count, malloc_count);

    return EXIT_SUCCESS;
}
