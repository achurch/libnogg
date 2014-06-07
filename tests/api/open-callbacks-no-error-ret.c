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


static int32_t read(void *opaque, void *buf, int32_t len)
{
    return fread(buf, 1, len, (FILE *)opaque);
}


int main(void)
{
    FILE *f;
    vorbis_t *vorbis;
    EXPECT_TRUE(f = fopen("tests/data/square.ogg", "rb"));
    EXPECT_TRUE(vorbis = vorbis_open_from_callbacks(
                    ((const vorbis_callbacks_t){.read = read}), f, NULL));
    vorbis_close(vorbis);
    fclose(f);

    EXPECT_FALSE(vorbis_open_from_callbacks(
                    ((const vorbis_callbacks_t){.read = NULL}), NULL, NULL));

    return EXIT_SUCCESS;
}
