/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#include "include/nogg.h"
#include "include/internal.h"

#ifdef USE_STDIO
# include <stdio.h>
#endif

/*************************************************************************/
/************************** File I/O callbacks ***************************/
/*************************************************************************/

#ifdef USE_STDIO

static long file_length(void *opaque)
{
    FILE *f = (FILE *)opaque;
    const long saved_offset = ftell(f);
    if (fseek(f, 0, SEEK_END) != 0) {
        return -1;
    }
    const long length = ftell(f);
    if (fseek(f, saved_offset, SEEK_SET) != 0) {
        return -1;
    }
    return length;
}

static long file_tell(void *opaque)
{
    FILE *f = (FILE *)opaque;
    return ftell(f);
}

static void file_seek(void *opaque, long offset)
{
    FILE *f = (FILE *)opaque;
    fseek(f, offset, SEEK_SET);
}

static long file_read(void *opaque, void *buffer, long length)
{
    FILE *f = (FILE *)opaque;
    return (long)fread(buffer, 1, (size_t)length, f);
}

static void file_close(void *opaque)
{
    FILE *f = (FILE *)opaque;
    fclose(f);
}

static const vorbis_callbacks_t file_callbacks = {
    .length = file_length,
    .tell   = file_tell,
    .seek   = file_seek,
    .read   = file_read,
    .close  = file_close,
};

#endif  /* USE_STDIO */

/*************************************************************************/
/*************************** Interface routine ***************************/
/*************************************************************************/

vorbis_t *vorbis_open_from_file(
    const char *path, vorbis_error_t *error_ret)
{
#ifdef USE_STDIO

    if (!path) {
        if (error_ret) {
            *error_ret = VORBIS_ERROR_INVALID_ARGUMENT;
        }
        return NULL;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        if (error_ret) {
            *error_ret = VORBIS_ERROR_FILE_OPEN_FAILED;
        }
        return NULL;
    }

    vorbis_t *handle =
        vorbis_open_from_callbacks(file_callbacks, f, error_ret);
    if (!handle) {
        fclose(f);
    }
    return handle;

#else  /* !USE_STDIO */

    if (error_ret) {
        *error_ret = VORBIS_ERROR_DISABLED_FUNCTION;
    }
    return NULL;

#endif
}

/*************************************************************************/
/*************************************************************************/
