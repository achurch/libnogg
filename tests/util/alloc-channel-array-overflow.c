/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2024 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#include "include/nogg.h"
#include "src/common.h"
#include "src/util/memory.h"
#include "tests/common.h"


int main(void)
{
    vorbis_t *vorbis;
    /* We don't actually use this file; we just need something to give us
     * a vorbis_t object. */
    EXPECT(vorbis = TEST___open_file("tests/data/square.ogg", 0, NULL));

    /* The data size itself is (just barely) within the signed 32-bit limit,
     * but adding the array pointers will push it into overflow. */
    EXPECT_FALSE(alloc_channel_array(vorbis, 0x10, 0x7FFFFFF, 0));

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}
