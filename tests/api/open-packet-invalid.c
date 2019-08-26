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


static int bad_call_count;  // Neither of these should be called.
static void *dummy_malloc(void *opaque, int32_t size, int32_t align)
{
    bad_call_count++;
    return NULL;
}
static void dummy_free(void *opaque, void *ptr)
{
    bad_call_count++;
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

    vorbis_callbacks_t callbacks = {.malloc = NULL, .free = NULL};
    vorbis_error_t error;

    error = (vorbis_error_t)-1;
    EXPECT_FALSE(vorbis_open_packet(NULL, len_id,
                                    data+ofs_setup, len_setup,
                                    callbacks, NULL, 0, &error));
    EXPECT_EQ(error, VORBIS_ERROR_INVALID_ARGUMENT);

    error = (vorbis_error_t)-1;
    EXPECT_FALSE(vorbis_open_packet(data+ofs_id, 0,
                                    data+ofs_setup, len_setup,
                                    callbacks, NULL, 0, &error));
    EXPECT_EQ(error, VORBIS_ERROR_INVALID_ARGUMENT);

    error = (vorbis_error_t)-1;
    EXPECT_FALSE(vorbis_open_packet(data+ofs_id, -1,
                                    data+ofs_setup, len_setup,
                                    callbacks, NULL, 0, &error));
    EXPECT_EQ(error, VORBIS_ERROR_INVALID_ARGUMENT);

    error = (vorbis_error_t)-1;
    EXPECT_FALSE(vorbis_open_packet(data+ofs_id, len_id,
                                    NULL, len_setup,
                                    callbacks, NULL, 0, &error));
    EXPECT_EQ(error, VORBIS_ERROR_INVALID_ARGUMENT);

    error = (vorbis_error_t)-1;
    EXPECT_FALSE(vorbis_open_packet(data+ofs_id, len_id,
                                    data+ofs_setup, 0,
                                    callbacks, NULL, 0, &error));
    EXPECT_EQ(error, VORBIS_ERROR_INVALID_ARGUMENT);

    error = (vorbis_error_t)-1;
    EXPECT_FALSE(vorbis_open_packet(data+ofs_id, len_id,
                                    data+ofs_setup, -1,
                                    callbacks, NULL, 0, &error));
    EXPECT_EQ(error, VORBIS_ERROR_INVALID_ARGUMENT);

    callbacks.malloc = dummy_malloc;
    error = (vorbis_error_t)-1;
    EXPECT_FALSE(vorbis_open_packet(data+ofs_id, len_id,
                                    data+ofs_setup, len_setup,
                                    callbacks, NULL, 0, &error));
    EXPECT_EQ(error, VORBIS_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(bad_call_count, 0);

    callbacks.malloc = NULL;
    callbacks.free = dummy_free;
    error = (vorbis_error_t)-1;
    EXPECT_FALSE(vorbis_open_packet(data+ofs_id, len_id,
                                    data+ofs_setup, len_setup,
                                    callbacks, NULL, 0, &error));
    EXPECT_EQ(error, VORBIS_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(bad_call_count, 0);

    free(data);
    return EXIT_SUCCESS;
}
