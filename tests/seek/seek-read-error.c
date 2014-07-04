/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#include "include/nogg.h"
#include "include/internal.h"  // For lenof().
#include "include/test.h"

#include "tests/data/noise-6ch_float.h"  // Defines expected_pcm[].


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

    /* Make sure the first and last page data is cached before we start
     * failing read operations, or seeks won't work at all. */
    EXPECT_TRUE(vorbis_seek(vorbis, 0));

    static const int seek_offsets[] = {0, 2000, 8499};
    for (int i = 0; i < lenof(seek_offsets); i++) {
        const int offset = seek_offsets[i];
        int32_t j;
        for (j = 0; j < 10000; j++) {
            read_count = j;
            if (vorbis_seek(vorbis, offset)) {
                break;
            }
        }
        if (j == 10000) {
            printf("%s:%d: Seek to %d failed\n", __FILE__, __LINE__, offset);
            return EXIT_FAILURE;
        }
        read_count = 10000;

        float pcm[6];
        vorbis_error_t error = (vorbis_error_t)-1;
        const int num_read = vorbis_read_float(vorbis, pcm, 1, &error);
        if (num_read == 0 && error == VORBIS_ERROR_STREAM_END) {
            /* The decode_frame() call in vorbis_seek() hit a read error
             * at a page boundary, which gets interpreted as end-of-stream.
             * There's not much we can do about this case, so just let it
             * slide. */
            continue;
        }
        if (num_read != 1) {
            fprintf(stderr, "%s:%d: Failed to read sample %d (error %d)\n",
                    __FILE__, __LINE__, offset, error);
            return EXIT_FAILURE;
        }
        if (error != VORBIS_NO_ERROR) {
            fprintf(stderr, "%s:%d: error was %d but should have been %d"
                    " for sample %d\n", __FILE__, __LINE__, error,
                    VORBIS_NO_ERROR, offset);
            return EXIT_FAILURE;
        }
        for (j = 0; j < 6; j++) {
            if (fabsf(pcm[j] - expected_pcm[offset*6+j]) > 1.0e-7f) {
                fprintf(stderr, "%s:%d: Sample %d+%d was %.8g but should have"
                        " been near %.8g\n", __FILE__, __LINE__, offset, j,
                        pcm[j], expected_pcm[offset*6+j]);
                return EXIT_FAILURE;
            }
        }
    }

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}
