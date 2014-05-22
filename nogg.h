// FIXME: just a quick first attempt at an interface

typedef struct vorbis_t vorbis_t;  // opaque to users

// open a data source for reading (three forms, like libwebmdec)
// stream can be either Vorbis-in-Ogg or WebM-style unwrapped Vorbis
extern vorbis_t *vorbis_open_from_buffer(
    const void *buffer, long length, vorbis_error_t *error_ret);
extern vorbis_t *vorbis_open_from_callbacks(
    vorbis_callbacks_t callbacks, void *opaque, vorbis_error_t *error_ret);
extern vorbis_t *vorbis_open_from_file(
    const char *path, vorbis_error_t *error_ret);

// close a stream
extern void vorbis_close(vorbis_t *handle);

// return stream info
extern int vorbis_channels(vorbis_t *handle);
extern int32_t vorbis_rate(vorbis_t *handle);
extern int64_t vorbis_length(vorbis_t *handle);  // in samples, -1 if unknown

// seek in stream
extern int vorbis_seek(vorbis_t *handle, int64_t pos);  // returns OK/NG
extern int64_t vorbis_tell(vorbis_t *handle);

// read PCM samples (channels are interleaved)
extern int vorbis_read_int16(vorbis_t *handle, int16_t *buf, int64_t len);
extern int vorbis_read_float(vorbis_t *handle, float *buf, int64_t len);
