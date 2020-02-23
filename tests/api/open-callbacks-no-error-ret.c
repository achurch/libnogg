/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2020 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#include "include/nogg.h"
#include "tests/common.h"


static int32_t read(void *opaque, void *buf, int32_t len)
{
    return fread(buf, 1, len, (FILE *)opaque);
}


int main(void)
{
    FILE *f;
    vorbis_t *vorbis;
    EXPECT(f = fopen("tests/data/square.ogg", "rb"));
    EXPECT(vorbis = vorbis_open_callbacks(
               ((const vorbis_callbacks_t){.read = read}), f, 0, NULL));
    vorbis_close(vorbis);
    fclose(f);

    EXPECT_FALSE(vorbis_open_callbacks(
                    ((const vorbis_callbacks_t){.read = NULL}), NULL, 0, NULL));

    return EXIT_SUCCESS;
}
