/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2022 Andrew Church <achurch@achurch.org>
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

    /* Since we can't control what address alignment will be returned by
     * malloc(), this test is unreliable in that it may pass on broken
     * code simply by coincidence of the returned pointer alignment.
     * However, we test a wide enough range of alignments that we should
     * normally get at least one unaligned allocation. */
    for (int align = 1; align <= 4096; align *= 2) {
        void *ptr = mem_alloc(vorbis, 1, align);
        if ((uintptr_t)ptr % align != 0) {
            FAIL("mem_alloc(1, %d) returned unaligned pointer %p", align, ptr);
        }
        mem_free(vorbis, ptr);
    }

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}
