/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2023 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#include "include/nogg.h"
#include "src/common.h"
#include "tests/common.h"


static int32_t read(void *opaque, void *buf, int32_t len)
{
    return fread(buf, 1, len, (FILE *)opaque);
}


int main(void)
{
    FILE *f;
    EXPECT(f = fopen("tests/data/noise-6ch.ogg", "rb"));
    vorbis_t *vorbis;
    EXPECT(vorbis = vorbis_open_callbacks(
               ((const vorbis_callbacks_t){.read = read}), f, 0, NULL));

    EXPECT_EQ(stb_vorbis_tell_bits(vorbis->decoder), 0);

    vorbis_close(vorbis);
    fclose(f);
    return EXIT_SUCCESS;
}
