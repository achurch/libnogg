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
    vorbis_t *vorbis;
    vorbis_error_t error = (vorbis_error_t)-1;
#ifdef USE_STDIO
    EXPECT(vorbis = vorbis_open_file("tests/data/square.ogg", 0, &error));
    EXPECT_EQ(error, VORBIS_NO_ERROR);
#else
    EXPECT_FALSE(vorbis = vorbis_open_file("tests/data/square.ogg", 0, &error));
    EXPECT_EQ(error, VORBIS_ERROR_DISABLED_FUNCTION);
    /* Also test handling of a NULL error_ret. */
    EXPECT_FALSE(vorbis_open_file("tests/data/square.ogg", 0, NULL));
#endif

    vorbis_close(vorbis);

    return EXIT_SUCCESS;
}
