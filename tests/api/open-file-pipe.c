/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#if (defined(__APPLE__) && TARGET_OS_MAC) || defined(__unix) || defined(__posix)
# undef _POSIX_C_SOURCE
# define _POSIX_C_SOURCE  200809L
# include <errno.h>
# include <stdlib.h>
# include <sys/stat.h>
# include <unistd.h>
#endif

#include "include/nogg.h"
#include "include/test.h"


int main(void)
{
#if defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200809L

    FILE *f;
    uint8_t *data;
    long size;
    EXPECT_TRUE(f = fopen("tests/data/square.ogg", "rb"));
    EXPECT_EQ(fseek(f, 0, SEEK_END), 0);
    EXPECT_GT(size = ftell(f), 0);
    EXPECT_EQ(fseek(f, 0, SEEK_SET), 0);
    EXPECT_TRUE(data = malloc(size));
    EXPECT_EQ(fread(data, 1, size, f), size);
    fclose(f);

    char tempdir[] = "/tmp/testXXXXXX";
    char tempfifo[] = "/tmp/testXXXXXX/pipe";
    if (!mkdtemp(tempdir)) {
        fprintf(stderr, "mkdtemp(%s): %s\n", tempdir, strerror(errno));
        return EXIT_FAILURE;
    }
    memcpy(tempfifo, tempdir, strlen(tempdir));
    if (mkfifo(tempfifo, S_IRUSR | S_IWUSR) != 0) {
        fprintf(stderr, "mkfifo(%s): %s\n", tempfifo, strerror(errno));
        if (rmdir(tempdir) != 0) {
            fprintf(stderr, "rmdir(%s): %s\n", tempdir, strerror(errno));
        }
        return EXIT_FAILURE;
    }
    switch (fork()) {
      case -1:
        perror("fork()");
        if (unlink(tempfifo) != 0) {
            fprintf(stderr, "unlink(%s): %s\n", tempfifo, strerror(errno));
        } else if (rmdir(tempdir) != 0) {
            fprintf(stderr, "rmdir(%s): %s\n", tempdir, strerror(errno));
        }
        return EXIT_FAILURE;
      case 0:
        f = fopen(tempfifo, "w");
        if (!f) {
            fprintf(stderr, "fopen(%s) for write: %s\n", tempfifo,
                    strerror(errno));
        } else {
            if ((long)fwrite(data, 1, size, f) != size) {
                fprintf(stderr, "fwrite(%s): %s\n", tempfifo, strerror(errno));
            }
            fclose(f);
        }
        _exit(0);
    }

    vorbis_error_t error = (vorbis_error_t)-1;
    vorbis_t *vorbis = vorbis_open_file(tempfifo, &error);
    if (unlink(tempfifo) != 0) {
        fprintf(stderr, "unlink(%s): %s\n", tempfifo, strerror(errno));
    } else if (rmdir(tempdir) != 0) {
        fprintf(stderr, "rmdir(%s): %s\n", tempdir, strerror(errno));
    }
    EXPECT_TRUE(vorbis);
    EXPECT_EQ(error, VORBIS_NO_ERROR);

    vorbis_close(vorbis);

    free(data);
    return EXIT_SUCCESS;

#else  // not POSIX

    fprintf(stderr, "Skipping test because mkdtemp() is not available.\n");
    return EXIT_SUCCESS;

#endif
}
