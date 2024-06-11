/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2024 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#include "include/nogg.h"
#include "tests/common.h"


int main(void)
{
#ifndef USE_STDIO
    LOG("Skipping test because stdio support is disabled.");
    return EXIT_SUCCESS;
#endif

    vorbis_t *vorbis;
    EXPECT(vorbis = vorbis_open_file("tests/data/square.ogg", 0, NULL));
    vorbis_close(vorbis);

    EXPECT_FALSE(vorbis_open_file(NULL, 0, NULL));
    EXPECT_FALSE(vorbis_open_file("tests/data/nonexistent", 0, NULL));

    return EXIT_SUCCESS;
}
