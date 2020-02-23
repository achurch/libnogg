/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2020 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#include "include/nogg.h"
#include "src/common.h"  // For min().
#include "tests/common.h"


struct buffer_info {
    char *buffer;
    int64_t size;        // Size of actual data.
    int64_t break_pos;   // Point at which to insert null data.
    int64_t break_size;  // Amount of null data to insert.
    int64_t offset;      // Current read offset.
};

static int64_t length(void *opaque)
{
    struct buffer_info *bi = opaque;
    return bi->size + bi->break_size;
}

static int64_t tell(void *opaque)
{
    struct buffer_info *bi = opaque;
    return bi->offset;
}

static void seek(void *opaque, int64_t offset)
{
    struct buffer_info *bi = opaque;
    bi->offset = offset;
}

static int32_t read(void *opaque, void *buf, int32_t len)
{
    struct buffer_info *bi = opaque;
    int32_t result = 0;
    if (bi->offset < bi->break_pos) {
        const int32_t count = min(len, bi->break_pos - bi->offset);
        memcpy(buf, bi->buffer + bi->offset, count);
        buf = (char *)buf + count;
        bi->offset += count;
        len -= count;
        result += count;
    }
    if (bi->offset >= bi->break_pos
     && bi->offset < bi->break_pos + bi->break_size) {
        const int32_t count =
            min(len, (bi->break_pos + bi->break_size) - bi->offset);
        memset(buf, 0, count);
        buf = (char *)buf + count;
        bi->offset += count;
        len -= count;
        result += count;
    }
    if (bi->offset >= bi->break_pos + bi->break_size
     && bi->offset < bi->size + bi->break_size) {
        const int32_t count =
            min(len, (bi->size + bi->break_size) - bi->offset);
        memcpy(buf, bi->buffer + (bi->offset - bi->break_size), count);
        buf = (char *)buf + count;
        bi->offset += count;
        len -= count;
        result += count;
    }
    return result;
}


int main(void)
{
    vorbis_t *vorbis;
    vorbis_error_t error = (vorbis_error_t)-1;
    struct buffer_info bi;

    FILE *f;
    EXPECT(f = fopen("tests/data/sample-rate-max.ogg", "rb"));
    EXPECT_EQ(fseek(f, 0, SEEK_END), 0);
    EXPECT_GT(bi.size = ftell(f), 0);
    EXPECT_EQ(fseek(f, 0, SEEK_SET), 0);
    EXPECT(bi.buffer = malloc(bi.size));
    EXPECT_EQ(fread(bi.buffer, 1, bi.size, f), bi.size);
    fclose(f);

    /* In order to trigger overflow in the bitrate numerator, we need
     * (size*8) * rate + samples/2 to be at least 2^63.  The test stream
     * has 40 samples and is set to sampling rate 2^32-1, so:
     *    size*8 >= (2^63 - 20) / 0xFFFFFFFF
     *    size*8 > 2^31
     *    size > 2^28
     */
    bi.break_pos = 0xA65;
    bi.break_size = 0x10000001 - bi.size;
    bi.offset = 0;
    EXPECT(vorbis = vorbis_open_callbacks(
               ((const vorbis_callbacks_t){.length = length, .tell = tell,
                                           .seek = seek, .read = read}),
               &bi, VORBIS_OPTION_SCAN_FOR_NEXT_PAGE, &error));
    EXPECT_EQ(error, VORBIS_NO_ERROR);
    EXPECT_EQ(vorbis_channels(vorbis), 1);
    EXPECT_EQ(vorbis_rate(vorbis), UINT32_MAX);
    EXPECT_EQ(vorbis_length(vorbis), 40);
    EXPECT_EQ(vorbis_bitrate(vorbis), INT32_MAX);
    vorbis_close(vorbis);

    /* Also check the ordinary overflow case.  In this case, we don't need
     * to insert any padding at all since the 2^32-1 sampling rate will
     * push the result above 2^31 for us. */
    bi.break_size = 0;
    bi.offset = 0;
    EXPECT(vorbis = vorbis_open_callbacks(
               ((const vorbis_callbacks_t){.length = length, .tell = tell,
                                           .seek = seek, .read = read}),
               &bi, VORBIS_OPTION_SCAN_FOR_NEXT_PAGE, &error));
    EXPECT_EQ(error, VORBIS_NO_ERROR);
    EXPECT_EQ(vorbis_channels(vorbis), 1);
    EXPECT_EQ(vorbis_rate(vorbis), UINT32_MAX);
    EXPECT_EQ(vorbis_length(vorbis), 40);
    EXPECT_EQ(vorbis_bitrate(vorbis), INT32_MAX);
    vorbis_close(vorbis);

    free(bi.buffer);
    return EXIT_SUCCESS;
}
