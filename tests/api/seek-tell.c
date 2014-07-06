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

#include "tests/data/square_float.h"  // Defines expected_pcm[].


int main(void)
{
    vorbis_t *vorbis;
    EXPECT_TRUE(vorbis = vorbis_open_file("tests/data/square.ogg", 0, NULL));

    EXPECT_TRUE(vorbis_seek(vorbis, 1));
    EXPECT_EQ(vorbis_tell(vorbis), 1);

    EXPECT_TRUE(vorbis_seek(vorbis, 40));
    EXPECT_EQ(vorbis_tell(vorbis), 40);

    EXPECT_TRUE(vorbis_seek(vorbis, 41));
    EXPECT_EQ(vorbis_tell(vorbis), 41);  // Does not take length into account.

    EXPECT_TRUE(vorbis_seek(vorbis, 0));
    EXPECT_EQ(vorbis_tell(vorbis), 0);

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}
