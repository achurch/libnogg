/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#ifndef NOGG_H
#define NOGG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*************************************************************************/
/****************************** Data types *******************************/
/*************************************************************************/

/**
 * vorbis_t:  Type of a Vorbis decoder handle.  This is the object through
 * which all operations on a particular stream are performed.
 */
typedef struct vorbis_t vorbis_t;

/**
 * vorbis_callbacks_t:  Structure containing callbacks for reading from a
 * stream, used with vorbis_open_from_callbacks().  The "opaque" parameter
 * to each of these functions receives the argument passed to the "opaque"
 * parameter to vorbis_open_with_callbacks(), and it may thus be used to
 * point to a file handle, state block, or similar data structure for the
 * stream data.
 */
typedef struct vorbis_callbacks_t {
    /* Return the length of the stream, or -1 if the stream is not seekable.
     * This value is assumed to be constant for any given stream.  If this
     * function pointer is NULL, the stream is assumed to be unseekable,
     * and the tell() and seek() function pointers will be ignored. */
    int64_t (*length)(void *opaque);

    /* Return the current byte offset in the stream, where offset 0
     * indicates the first byte of stream data.  This function will only be
     * called on seekable streams. */
    int64_t (*tell)(void *opaque);

    /* Seek to the given byte offset in the stream.  This function will
     * only be called on seekable streams, and the value of offset will
     * always satisfy 0 <= offset <= length().  The operation is assumed to
     * succeed (though this does not imply that a subsequent read operation
     * must succeed); for streams on which a seek operation could fail, the
     * stream must be reported as unseekable. */
    void (*seek)(void *opaque, int64_t offset);

    /* Read data from the stream, returning the number of bytes
     * successfully read.  For seekable streams, the caller will never
     * attempt to read beyond the end of the stream.  A return value less
     * than the requested length is interpreted as a fatal error and will
     * cause all subsequent operations on the associated handle to fail. */
    int64_t (*read)(void *opaque, void *buffer, int64_t length);

    /* Close the stream.  This function will be called exactly once for a
     * successfully opened stream, and no other functions will be called on
     * the stream once it has been closed.  If the open operation fails,
     * this function will not be called at all.  This function pointer may
     * be NULL if no close operation is required. */
    void (*close)(void *opaque);
} vorbis_callbacks_t;

/**
 * vorbis_error_t:  Type of error codes returned from vorbis_*() functions.
 */
typedef enum vorbis_error_t {
    /* No error has occurred. */
    VORBIS_NO_ERROR = 0,

    /* An invalid argument was passed to a function. */
    VORBIS_ERROR_INVALID_ARGUMENT = 1,
    /* The requested function is not supported in this build. */
    VORBIS_ERROR_DISABLED_FUNCTION = 2,
    /* Insufficient system resources were available for the operation. */
    VORBIS_ERROR_INSUFFICIENT_RESOURCES = 3,
    /* An attempt to open a file failed.  The global errno variable
     * indicates the specific error that occurred. */
    VORBIS_ERROR_FILE_OPEN_FAILED = 4,

    /* The stream is not a Vorbis stream or is corrupt. */
    VORBIS_ERROR_STREAM_INVALID = 101,
    /* A seek operation was attempted on an unseekable stream. */
    VORBIS_ERROR_STREAM_NOT_SEEKABLE = 102,
    /* A read operation attempted to read past the end of the stream. */
    VORBIS_ERROR_STREAM_END = 103,
    /* An error occurred while reading stream data. */
    VORBIS_ERROR_STREAM_READ_FAILURE = 104,

    /* An error occurred while initializing the Vorbis decoder. */
    VORBIS_ERROR_DECODE_SETUP_FAILURE = 201,
    /* An unrecoverable error occurred while decoding audio data. */
    VORBIS_ERROR_DECODE_FAILURE = 202,
    /* An error was detected in the stream, but the decoder was able to
     * recover and subsequent read operations may be attempted. */
    VORBIS_ERROR_DECODE_RECOVERED = 203,
} vorbis_error_t;

/*************************************************************************/
/**************** Interface: Library version information *****************/
/*************************************************************************/

/**
 * nogg_version:  Return the version number of the library as a string
 * (for example, "1.2.3").
 *
 * [Return value]
 *     Library version number.
 */
extern const char *nogg_version(void);

/*************************************************************************/
/**************** Interface: Opening and closing streams *****************/
/*************************************************************************/

/**
 * vorbis_open_from_buffer:  Create a new stream handle for a stream whose
 * contents are stored in memory.
 *
 * [Parameters]
 *     buffer: Pointer to the buffer containing the stream data.
 *     length: Length of the stream data, in bytes.
 *     error_ret: Pointer to variable to receive the error code from the
 *         operation (always VORBIS_NO_ERROR on success).  May be NULL if
 *         the error code is not needed.
 * [Return value]
 *     Newly-created handle, or NULL on error.
 */
extern vorbis_t *vorbis_open_from_buffer(
    const void *buffer, int64_t length, vorbis_error_t *error_ret);

/**
 * vorbis_open_from_callbacks:  Create a new stream handle for a stream
 * whose contents are accessed through a set of callbacks.
 *
 * This function will fail with VORBIS_ERROR_INVALID_ARGUMENT if the
 * callback set is incorrectly specified (no read function, or a length
 * function but no tell or seek function).
 *
 * [Parameters]
 *     callbacks: Set of callbacks to be used to access the stream data.
 *     opaque: Opaque pointer value passed through to the callbacks.
 *     error_ret: Pointer to variable to receive the error code from the
 *         operation (always VORBIS_NO_ERROR on success).  May be NULL if
 *         the error code is not needed.
 * [Return value]
 *     Newly-created handle, or NULL on error.
 */
extern vorbis_t *vorbis_open_from_callbacks(
    vorbis_callbacks_t callbacks, void *opaque, vorbis_error_t *error_ret);

/**
 * vorbis_open_from_file:  Create a new stream handle for a stream whose
 * contents will be read from a file on the filesystem.
 *
 * If stdio support was disabled when the library was built, this function
 * will always fail with VORBIS_ERROR_DISABLED_FUNCTION.
 *
 * [Parameters]
 *     path: Pathname of the file from which the stream is to be read.
 *     error_ret: Pointer to variable to receive the error code from the
 *         operation (always VORBIS_NO_ERROR on success).  May be NULL if
 *         the error code is not needed.
 * [Return value]
 *     Newly-created handle, or NULL on error.
 */
extern vorbis_t *vorbis_open_from_file(
    const char *path, vorbis_error_t *error_ret);

/**
 * vorbis_close:  Close a handle, freeing all associated resources.
 * After calling this function, the handle is no longer valid.
 *
 * [Parameters]
 *     handle: Handle to close.  If NULL, this function does nothing.
 */
extern void vorbis_close(vorbis_t *handle);

/*************************************************************************/
/***************** Interface: Getting stream information *****************/
/*************************************************************************/

/**
 * vorbis_channels:  Return the number of channels in the given stream.
 *
 * [Parameters]
 *     handle: Handle to operate on.
 * [Return value]
 *     Number of audio channels, or zero if the stream has no audio.
 */
extern int vorbis_channels(const vorbis_t *handle);

/**
 * vorbis_rate:  Return the sampling rate of the given stream.
 *
 * [Parameters]
 *     handle: Handle to operate on.
 * [Return value]
 *     Audio sampling rate, or zero if the stream has no audio.
 */
extern int32_t vorbis_rate(const vorbis_t *handle);

/**
 * vorbis_length:  Return number of samples in the given stream, if known.
 *
 * [Parameters]
 *     handle: Handle to operate on.
 * [Return value]
 *     Length of stream in samples, or -1 if the length is not known.
 */
extern int64_t vorbis_length(const vorbis_t *handle);

/*************************************************************************/
/********************* Interface: Seeking in streams *********************/
/*************************************************************************/

/**
 * vorbis_seek:  Seek to the given sample index in the stream.  An index
 * of 0 indicates the first sample in the stream.
 *
 * [Parameters]
 *     handle: Handle to operate on.
 *     position: Position to seek to, in samples.
 * [Return value]
 *     True (nonzero) on success, false (zero) on failure.
 */
extern int vorbis_seek(vorbis_t *handle, int64_t position);

/**
 * vorbis_seek_to_time:  Seek to the given timestamp in the stream.
 * Equivalent to vorbis_seek(handle, timestamp / vorbis_rate(handle)).
 *
 * [Parameters]
 *     handle: Handle to operate on.
 *     timestamp: Timestamp to seek to, in seconds.
 * [Return value]
 *     True (nonzero) on success, false (zero) on failure.
 */
extern int vorbis_seek_to_time(vorbis_t *handle, double timestamp);

/**
 * vorbis_tell:  Return the current decode position, which is the index of
 * the next sample to be returned by one of the vorbis_read_*() functions.
 * An index of 0 indicates the first sample in the stream.
 *
 * [Parameters]
 *     handle: Handle to operate on.
 * [Return value]
 *     Current decode position, in samples.
 */
extern int64_t vorbis_tell(const vorbis_t *handle);

/*************************************************************************/
/*********************** Interface: Reading frames ***********************/
/*************************************************************************/

/**
 * vorbis_read_int16:  Decode and return up to the given number of PCM
 * samples as 16-bit signed integers in the range [-32767,+32767].
 * Multichannel audio data is stored with channels interleaved.
 *
 * If an error occurs, the error code will be stored in *error_ret (if
 * error_ret is not NULL).  In the case of a recoverable error, the
 * return value indicates the number of samples decoded before the error
 * was encountered, and the next read call will return samples starting
 * from the point at which the decoder recovered from the error.
 *
 * [Parameters]
 *     handle: Handle to operate on.
 *     buf: Buffer into which to store decoded audio data.
 *     len: Number of samples (per channel) to read.
 *     error_ret: Pointer to variable to receive the error code from the
 *         operation (or VORBIS_NO_ERROR if no error was encountered).
 *         May be NULL if the error code is not needed.
 * [Return value]
 *     Number of samples successfully read.
 */
extern int64_t vorbis_read_int16(
    vorbis_t *handle, int16_t *buf, int64_t len, vorbis_error_t *error_ret);

/**
 * vorbis_read_float:  Decode and return up to the given number of PCM
 * samples as single-precision floating point values in the range
 * [-1.0,+1.0].  Multichannel audio data is stored with channels
 * interleaved.
 *
 * If an error occurs, the error code will be stored in *error_ret (if
 * error_ret is not NULL).  In the case of a recoverable error, the
 * return value indicates the number of samples decoded before the error
 * was encountered, and the next read call will return samples starting
 * from the point at which the decoder recovered from the error.
 *
 * [Parameters]
 *     handle: Handle to operate on.
 *     buf: Buffer into which to store decoded audio data.
 *     len: Number of samples (per channel) to read.
 *     error_ret: Pointer to variable to receive the error code from the
 *         operation (or VORBIS_NO_ERROR if no error was encountered).
 *         May be NULL if the error code is not needed.
 * [Return value]
 *     Number of samples successfully read.
 */
extern int64_t vorbis_read_float(
    vorbis_t *handle, float *buf, int64_t len, vorbis_error_t *error_ret);

/*************************************************************************/
/*************************************************************************/

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* NOGG_H */
