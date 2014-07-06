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
# include <limits.h>
# include <stdio.h>
#else
# include <stddef.h>
#endif

/*************************************************************************/
/************************** File I/O callbacks ***************************/
/*************************************************************************/

#ifdef USE_STDIO

static int64_t file_length(void *opaque)
{
    FILE *f = (FILE *)opaque;
    const long saved_offset = ftell(f);
    if (fseek(f, 0, SEEK_END) != 0) {
        return -1;
    }
    const int64_t length = ftell(f);
    /* This seek will always succeed if the previous one succeeded, so we
     * don't bother checking its return value (since there's no way to test
     * the failure path). */
    fseek(f, saved_offset, SEEK_SET);
    return length;
}

static int64_t file_tell(void *opaque)
{
    FILE *f = (FILE *)opaque;
    return ftell(f);
}

static void file_seek(void *opaque, int64_t offset)
{
    FILE *f = (FILE *)opaque;
    /* We assume that ftell() won't succeed if the file size wouldn't fit
     * in a long, so the offset here is guaranteed to fit. */
    fseek(f, (long)offset, SEEK_SET);
}

static int32_t file_read(void *opaque, void *buffer, int32_t length)
{
    FILE *f = (FILE *)opaque;
    return (int32_t)fread(buffer, 1, (size_t)length, f);
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

vorbis_t *vorbis_open_file(
    const char *path, unsigned int options, vorbis_error_t *error_ret)
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

    vorbis_t *handle = vorbis_open_callbacks(file_callbacks, f, options,
                                             error_ret);
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
