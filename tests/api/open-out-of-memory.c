/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2023 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#include "include/nogg.h"
#include "tests/common.h"


/* Number of bytes currently allocated through our malloc() function. */
static long bytes_allocated;
/* Remaining number of malloc() calls to allow. */
static int malloc_calls_left;


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
    return fread(buf, 1, len, (FILE *)opaque);
}

static void close(void *opaque)
{
    fclose((FILE *)opaque);
}

static void *my_malloc(void *opaque, int32_t size, int32_t align)
{
    if (malloc_calls_left == 0) {
        return NULL;
    }
    malloc_calls_left--;
    void *base = malloc(size + 2*sizeof(void *) + align);
    if (!base) {
        return NULL;
    }
    bytes_allocated += size;
    void *ptr = (void *)((uintptr_t)base + 2*sizeof(void *));
    if (align != 0 && (uintptr_t)ptr % align != 0) {
        ptr = (void *)((uintptr_t)ptr + (align - ((uintptr_t)ptr % align)));
    }
    ((void **)ptr)[-1] = base;
    ((intptr_t *)ptr)[-2] = size;
    return ptr;
}

static void my_free(void *opaque, void *ptr)
{
    if (ptr) {
        bytes_allocated -= ((intptr_t *)ptr)[-2];
        free(((void **)ptr)[-1]);
    }
}


int main(void)
{
    FILE *f;
    EXPECT(f = fopen("tests/data/square.ogg", "rb"));

    vorbis_t *vorbis;
    vorbis_error_t error;

    /* First check that an open call fails if no memory can be allocated
     * at all.  If this succeeds, the code is probably not using our
     * allocation functions. */
    bytes_allocated = 0;
    malloc_calls_left = 0;
    error = (vorbis_error_t)-1;
    EXPECT_FALSE(vorbis_open_callbacks(((const vorbis_callbacks_t){
                                           .length = length,
                                           .tell = tell,
                                           .seek = seek,
                                           .read = read,
                                           .close = close,
                                           .malloc = my_malloc,
                                           .free = my_free}),
                                       f, 0, &error));
    EXPECT_EQ(error, VORBIS_ERROR_INSUFFICIENT_RESOURCES);

    /* Now increment the count of allowed malloc() calls until the open
     * succeeds; this should exercise all allocation failure paths. */
    for (int try = 1; ; try++) {
        fseek(f, 0, SEEK_SET);
        bytes_allocated = 0;
        malloc_calls_left = try;
        error = (vorbis_error_t)-1;
        vorbis = vorbis_open_callbacks(((const vorbis_callbacks_t){
                                           .length = length,
                                           .tell = tell,
                                           .seek = seek,
                                           .read = read,
                                           .close = close,
                                           .malloc = my_malloc,
                                           .free = my_free}),
                                       f, 0, &error);
        if (vorbis) {
            break;
        }
        if (error != VORBIS_ERROR_INSUFFICIENT_RESOURCES) {
            fprintf(stderr, "%s:%d: (iteration %d) error was %d but should"
                    " have been %d\n", __FILE__, __LINE__, try,
                    error, VORBIS_ERROR_INSUFFICIENT_RESOURCES);
            return EXIT_FAILURE;
        }
        if (bytes_allocated != 0) {
            fprintf(stderr, "%s:%d: (iteration %d) bytes_allocated was %ld"
                    " but should have been 0\n", __FILE__, __LINE__, try,
                    bytes_allocated);
            return EXIT_FAILURE;
        }
    }
    EXPECT_EQ(error, VORBIS_NO_ERROR);

    /* Check that we can successfully decode the stream (in case some
     * allocations failed but were not checked). */
    float pcm[1];
    EXPECT_EQ(vorbis_read_float(vorbis, pcm, 1, NULL), 1);

    vorbis_close(vorbis);
    EXPECT_EQ(bytes_allocated, 0);

    return EXIT_SUCCESS;
}
