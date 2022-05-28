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


int main(void)
{
#ifndef USE_STDIO
    LOG("Skipping test because stdio support is disabled.");
    return EXIT_SUCCESS;
#endif

    vorbis_error_t error = (vorbis_error_t)-1;
    EXPECT_FALSE(vorbis_open_file("tests/data/nonexistent", 0, &error));
    EXPECT_EQ(error, VORBIS_ERROR_FILE_OPEN_FAILED);

    return EXIT_SUCCESS;
}
