/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2019 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#include "include/nogg.h"
#include "src/common.h"  // For lenof().
#include "tests/common.h"


typedef struct Buffer {
    const uint8_t *data;
    int size;
    int pos;
} Buffer;

static int64_t length(void *opaque)
{
    Buffer *b = (Buffer *)opaque;
    return b->size;
}

static int64_t tell(void *opaque)
{
    Buffer *b = (Buffer *)opaque;
    return b->pos;
}

static void seek(void *opaque, int64_t offset)
{
    Buffer *b = (Buffer *)opaque;
    b->pos = offset;
}

static int read_count;
static int32_t read(void *opaque, void *buf, int32_t len)
{
    Buffer *b = (Buffer *)opaque;
    if (read_count > 0) {
        read_count--;
        if (len > b->size - b->pos) {
            len = b->size - b->pos;
        }
        memcpy(buf, b->data+b->pos, len);
        b->pos += len;
        return len;
    } else {
        return 0;
    }
}

static void close(UNUSED void *opaque)
{
    /* Nothing to do. */
}


int main(void)
{
    FILE *f;
    uint8_t *data;
    long size;
    EXPECT(f = fopen("tests/data/split-packet.ogg", "rb"));
    EXPECT_EQ(fseek(f, 0, SEEK_END), 0);
    EXPECT_EQ(size = ftell(f), 0xEF4);
    EXPECT_EQ(fseek(f, 0, SEEK_SET), 0);
    EXPECT(data = malloc(size+0x2000));
    EXPECT_EQ(fread(data, 1, size, f), size);
    fclose(f);
    memmove(data+0x2E71, data+0xE71, 0xEF4-0xE71);
    memset(data+0xE71, 0, 0x2000);

    Buffer b = {.data = data, .size = size+0x2000};

    int i;
    for (i = 0; i < 10000; i++) {
        vorbis_t *vorbis;

        read_count = 10000;
        b.pos = 0;
        EXPECT(vorbis = vorbis_open_callbacks(
                   ((const vorbis_callbacks_t){.length = length,
                                               .tell = tell,
                                               .seek = seek,
                                               .read = read,
                                               .close = close}),
                   &b, VORBIS_OPTION_SCAN_FOR_NEXT_PAGE, NULL));

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
