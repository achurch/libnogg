/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2019 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#include "include/nogg.h"
#include "tests/common.h"


/* Counts of calls to each of the callbacks.  None but malloc() and free()
 * should be called. */
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
    return -1;
}

static int64_t tell(void *opaque)
{
    tell_count++;
    return 0;
}

static void seek(void *opaque, int64_t offset)
{
    seek_count++;
}

static int32_t read(void *opaque, void *buf, int32_t len)
{
    read_count++;
    return 0;
}

static void close(void *opaque)
{
    close_count++;
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
    uint8_t *data;
    long size;
    EXPECT(f = fopen("tests/data/square.ogg", "rb"));
    EXPECT_EQ(fseek(f, 0, SEEK_END), 0);
    EXPECT_GT(size = ftell(f), 0);
    EXPECT_EQ(fseek(f, 0, SEEK_SET), 0);
    EXPECT(data = malloc(size));
    EXPECT_EQ(fread(data, 1, size, f), size);
    fclose(f);

    const int ofs_id = 0x1C;
    const int len_id = 0x1E;
    const int ofs_setup = 0xB9;
    const int len_setup = 0x9AC;
    EXPECT_MEMEQ(data+ofs_id, "\x01vorbis", 7);
    EXPECT_MEMEQ(data+ofs_setup, "\x05vorbis", 7);

    vorbis_t *vorbis;
    vorbis_callbacks_t callbacks = {.length = length,
                                    .tell = tell,
                                    .seek = seek,
                                    .read = read,
                                    .close = close,
                                    .malloc = local_malloc,
                                    .free = local_free};
    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT(vorbis = vorbis_open_packet(data+ofs_id, len_id,
                                       data+ofs_setup, len_setup,
                                       callbacks, NULL, 0, &error));
    EXPECT_EQ(error, VORBIS_NO_ERROR);
    EXPECT_EQ(length_count, 0);
    EXPECT_EQ(tell_count, 0);
    EXPECT_EQ(seek_count, 0);
    EXPECT_EQ(read_count, 0);
    EXPECT_EQ(close_count, 0);
    EXPECT_GT(malloc_count, 0);

    vorbis_close(vorbis);
    EXPECT_EQ(length_count, 0);
    EXPECT_EQ(tell_count, 0);
    EXPECT_EQ(seek_count, 0);
    EXPECT_EQ(read_count, 0);
    EXPECT_EQ(close_count, 0);
    EXPECT_GT(free_count, 0);
    EXPECT_EQ(free_nonnull_count, malloc_count);

    free(data);
    return EXIT_SUCCESS;
}
