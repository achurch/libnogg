/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2016 Andrew Church <achurch@achurch.org>
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
    EXPECT_TRUE(vorbis = vorbis_open_file("tests/data/square.ogg", 0, NULL));

    void **array;
    EXPECT_TRUE(array = alloc_channel_array(vorbis, 2, 123, 64));
    EXPECT_EQ((uintptr_t)array % 64, 0);
    EXPECT_EQ((uintptr_t)array[0], (uintptr_t)array + 64);
    EXPECT_EQ((uintptr_t)array[1], (uintptr_t)array + 64 + 128);
    mem_free(vorbis, array);

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}
