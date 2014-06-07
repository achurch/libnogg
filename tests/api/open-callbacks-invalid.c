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


/* Dummy callback functions for vorbis_open_from_callbacks(). */
static int64_t dummy_length(void *opaque) {return -1;}
static int64_t dummy_tell(void *opaque) {return 0;}
static void dummy_seek(void *opaque, int64_t offset) {}
static int32_t dummy_read(void *opaque, void *buf, int32_t len) {return 0;}
static void dummy_close(void *opaque) {}

int main(void)
{
    vorbis_error_t error;

    /* Both tell and seek functions are needed if the length callback is
     * provided. */
    EXPECT_FALSE(vorbis_open_from_callbacks(
                     ((const vorbis_callbacks_t){
                         .length = dummy_length,
                         .seek = dummy_seek,
                         .read = dummy_read,
                         .close = dummy_close}),
                     NULL, &error));
    EXPECT_EQ(error, VORBIS_ERROR_INVALID_ARGUMENT);
    EXPECT_FALSE(vorbis_open_from_callbacks(
                     ((const vorbis_callbacks_t){
                         .length = dummy_length,
                         .tell = dummy_tell,
                         .read = dummy_read,
                         .close = dummy_close}),
                     NULL, &error));
    EXPECT_EQ(error, VORBIS_ERROR_INVALID_ARGUMENT);

    /* A read callback must be provided. */
    EXPECT_FALSE(vorbis_open_from_callbacks(
                     ((const vorbis_callbacks_t){
                         .length = dummy_length,
                         .tell = dummy_tell,
                         .seek = dummy_seek,
                         .close = dummy_close}),
                     NULL, &error));
    EXPECT_EQ(error, VORBIS_ERROR_INVALID_ARGUMENT);

    return EXIT_SUCCESS;
}
