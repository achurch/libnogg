/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2024 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#include "include/nogg.h"
#include "src/common.h"  // For lenof().
#include "tests/common.h"


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
    int i;
    for (i = 0; i < 10000; i++) {
        FILE *f;
        vorbis_t *vorbis;

        read_count = 10000;
        EXPECT(f = fopen("tests/data/square.ogg", "rb"));
        EXPECT(vorbis = vorbis_open_callbacks(((const vorbis_callbacks_t){
                                                  .length = length,
                                                  .tell = tell,
                                                  .seek = seek,
                                                  .read = read,
                                                  .close = close}),
                                              f, 0, NULL));

        read_count = i;
        const int64_t length = vorbis_length(vorbis);
        vorbis_close(vorbis);
        if (length > 0) {
            break;
        }
    }
    if (i == 10000) {
        printf("%s:%d: Failed to get file length\n", __FILE__, __LINE__);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
