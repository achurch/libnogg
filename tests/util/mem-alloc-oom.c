/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2020 Andrew Church <achurch@achurch.org>
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

    /* Attempt to run out of memory (or process space).  This is trivial
     * in a 32-bit environment, but depends on the OS in a 64-bit
     * environment.  On Linux, disabling memory overcommit (for example,
     * with "sudo sysctl vm.overcommit_memory=2") can help ensure this
     * test works as intended. */
    void *allocs[1000];
    int num_allocs;
    for (num_allocs = 0; num_allocs < (int)(sizeof(allocs)/sizeof(*allocs));
         num_allocs++)
    {
        allocs[num_allocs] = mem_alloc(vorbis, 0x7FFFFFFF, 0);
        if (!allocs[num_allocs]) {
            break;
        }
    }
    if (num_allocs == (int)(sizeof(allocs)/sizeof(*allocs))) {
        printf("tests/util/mem-alloc-oom:%d: warning: Failed to exhaust"
               " system memory\n", __LINE__);
    }
    while (num_allocs > 0) {
        num_allocs--;
        mem_free(vorbis, allocs[num_allocs]);
    }

    vorbis_close(vorbis);
    return EXIT_SUCCESS;
}
