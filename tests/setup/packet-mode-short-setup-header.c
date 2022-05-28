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


static const char id_packet[30] =
    "\1vorbis\0\0\0\0\1\xA0\x0F\0\0\0\0\0\0\xFF\xFF\xFF\xFF"
    "\0\0\0\0\x99\x01";
static const char setup_packet[4] =
    "\5vor";

int main(void)
{
    vorbis_error_t error = (vorbis_error_t)-1;
    vorbis_callbacks_t callbacks = {.malloc = NULL, .free = NULL};
    EXPECT_FALSE(vorbis_open_packet(id_packet, sizeof(id_packet),
                                    setup_packet, sizeof(setup_packet),
                                    callbacks, NULL, 0, &error));
    EXPECT_EQ(error, VORBIS_ERROR_STREAM_INVALID);

    return EXIT_SUCCESS;
}
