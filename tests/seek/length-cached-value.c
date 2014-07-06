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

/* Include internal structure defs so we can mess with the handle. */
#include "include/internal.h"
#include "src/decode/common.h"


int main(void)
{
    vorbis_t *vorbis;
    EXPECT_TRUE(vorbis = vorbis_open_file("tests/data/square.ogg", 0, NULL));

    EXPECT_EQ(vorbis_length(vorbis), 40);

    vorbis->decoder->total_samples = 42;
    EXPECT_EQ(vorbis_length(vorbis), 42);  // Should use the cached value.

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}
