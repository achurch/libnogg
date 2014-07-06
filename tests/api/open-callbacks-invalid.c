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


/* Dummy callback functions for vorbis_open_callbacks(). */
static int64_t dummy_length(void *opaque) {return -1;}
static int64_t dummy_tell(void *opaque) {return 0;}
static void dummy_seek(void *opaque, int64_t offset) {}
static int32_t dummy_read(void *opaque, void *buf, int32_t len) {return 0;}
static void dummy_close(void *opaque) {}
static void *dummy_malloc(void *opaque, int32_t size) {return NULL;}
static void dummy_free(void *opaque, void *ptr) {}

int main(void)
{
    vorbis_error_t error;

    /* Both tell and seek functions are needed if the length callback is
     * provided. */
    error = (vorbis_error_t)-1;
    EXPECT_FALSE(vorbis_open_callbacks(((const vorbis_callbacks_t){
                                           .length = dummy_length,
                                           .seek = dummy_seek,
                                           .read = dummy_read,
                                           .close = dummy_close}),
                                       NULL, 0, &error));
    EXPECT_EQ(error, VORBIS_ERROR_INVALID_ARGUMENT);
    error = (vorbis_error_t)-1;
    EXPECT_FALSE(vorbis_open_callbacks(((const vorbis_callbacks_t){
                                           .length = dummy_length,
                                           .tell = dummy_tell,
                                           .read = dummy_read,
                                           .close = dummy_close}),
                                       NULL, 0, &error));
    EXPECT_EQ(error, VORBIS_ERROR_INVALID_ARGUMENT);

    /* A read callback must be provided. */
    error = (vorbis_error_t)-1;
    EXPECT_FALSE(vorbis_open_callbacks(((const vorbis_callbacks_t){
                                           .length = dummy_length,
                                           .tell = dummy_tell,
                                           .seek = dummy_seek,
                                           .close = dummy_close}),
                                       NULL, 0, &error));
    EXPECT_EQ(error, VORBIS_ERROR_INVALID_ARGUMENT);

    /* Either both or none of the malloc and free callbacks must be given. */
    error = (vorbis_error_t)-1;
    EXPECT_FALSE(vorbis_open_callbacks(((const vorbis_callbacks_t){
                                           .length = dummy_length,
                                           .tell = dummy_tell,
                                           .seek = dummy_seek,
                                           .read = dummy_read,
                                           .close = dummy_close,
                                           .malloc = dummy_malloc}),
                                       NULL, 0, &error));
    EXPECT_EQ(error, VORBIS_ERROR_INVALID_ARGUMENT);
    error = (vorbis_error_t)-1;
    EXPECT_FALSE(vorbis_open_callbacks(((const vorbis_callbacks_t){
                                           .length = dummy_length,
                                           .tell = dummy_tell,
                                           .seek = dummy_seek,
                                           .read = dummy_read,
                                           .close = dummy_close,
                                           .free = dummy_free}),
                                       NULL, 0, &error));
    EXPECT_EQ(error, VORBIS_ERROR_INVALID_ARGUMENT);

    return EXIT_SUCCESS;
}
