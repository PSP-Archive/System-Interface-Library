/*
 * libwebmdec: a decoder library for WebM audio/video streams
 * Copyright (c) 2014-2019 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#ifndef WEBMDEC_H
#define WEBMDEC_H

#ifdef __cplusplus
extern "C" {
#endif

/*************************************************************************/
/****************************** Data types *******************************/
/*************************************************************************/

/**
 * webmdec_t:  Type of a WebM stream handle.  This is the object through
 * which all operations on a particular stream are performed.
 */
typedef struct webmdec_t webmdec_t;

/**
 * webmdec_callbacks_t:  Structure containing callbacks for reading from a
 * stream, used with webmdec_open_from_callbacks().  The "opaque" parameter
 * to each of these functions receives the argument passed to the "opaque"
 * parameter to webmdec_open_with_callbacks(), and it may thus be used to
 * point to a file handle, state block, or similar data structure for the
 * stream data.
 */
typedef struct webmdec_callbacks_t {
    /* Return the length of the stream, or -1 if the stream is not seekable.
     * This value is assumed to be constant for any given stream.  If this
     * function pointer is NULL, the stream is assumed to be unseekable,
     * and the tell() and seek() function pointers will be ignored. */
    long (*length)(void *opaque);

    /* Return the current byte offset in the stream, where offset 0
     * indicates the first byte of stream data.  This function will only be
     * called on seekable streams. */
    long (*tell)(void *opaque);

    /* Seek to the given byte offset in the stream.  This function will
     * only be called on seekable streams, and the value of offset will
     * always satisfy 0 <= offset <= length().  The operation is assumed to
     * succeed (though this does not imply that a subsequent read operation
     * must succeed); for streams on which a seek operation could fail, the
     * stream must be reported as unseekable. */
    void (*seek)(void *opaque, long offset);

    /* Read data from the stream, returning the number of bytes
     * successfully read.  For seekable streams, the caller will never
     * attempt to read beyond the end of the stream.  A return value less
     * than the requested length is interpreted as a fatal error and will
     * cause all subsequent operations on the associated handle to fail. */
    long (*read)(void *opaque, void *buffer, long length);

    /* Close the stream.  This function will be called exactly once for a
     * successfully opened stream, and no other functions will be called on
     * the stream once it has been closed.  If the open operation fails,
     * this function will not be called at all.  This function pointer may
     * be NULL if no close operation is required. */
    void (*close)(void *opaque);
} webmdec_callbacks_t;

/**
 * webmdec_error_t:  Type of error codes returned from libwebmdec functions.
 */
typedef enum webmdec_error_t {
    /* No error has occurred. */
    WEBMDEC_NO_ERROR = 0,

    /* An invalid argument was passed to a function. */
    WEBMDEC_ERROR_INVALID_ARGUMENT = 1,
    /* The requested function is not supported in this build. */
    WEBMDEC_ERROR_DISABLED_FUNCTION = 2,
    /* Insufficient system resources were available for the operation. */
    WEBMDEC_ERROR_INSUFFICIENT_RESOURCES = 3,
    /* An attempt to open a file failed.  The global errno variable
     * indicates the specific error that occurred. */
    WEBMDEC_ERROR_FILE_OPEN_FAILED = 4,

    /* The stream is not a WebM stream or is corrupt. */
    WEBMDEC_ERROR_STREAM_INVALID = 101,
    /* A seek operation was attempted on an unseekable stream. */
    WEBMDEC_ERROR_STREAM_NOT_SEEKABLE = 102,
    /* A read operation attempted to read past the end of the stream. */
    WEBMDEC_ERROR_STREAM_END = 103,
    /* The stream does not have any tracks of the requested type(s). */
    WEBMDEC_ERROR_STREAM_NO_TRACKS = 104,
    /* An error occurred while reading stream data. */
    WEBMDEC_ERROR_STREAM_READ_FAILURE = 105,

    /* An error occurred while initializing the video or audio decoder. */
    WEBMDEC_ERROR_DECODE_SETUP_FAILURE = 201,
    /* An error occurred while decoding video or audio data. */
    WEBMDEC_ERROR_DECODE_FAILURE = 202,
    /* The video data was decoded into an unsupported pixel format. */
    WEBMDEC_ERROR_UNSUPPORTED_PIXEL_FORMAT = 203,
} webmdec_error_t;

/**
 * webmdec_open_mode_t:  Mode constants for the webmdec_open_*() functions.
 * Note that opening a stream with OPEN_VIDEO or OPEN_AUDIO only affects
 * the behavior of webmdec_decode_frame(); webmdec_read_frame() and the
 * webmdec_video_*() and webmdec_audio_*() informational functions will
 * work on both video and audio tracks if the stream contains such tracks.
 */
typedef enum webmdec_open_mode_t {
    /* Open the stream for decoding both video and audio.  If the stream
     * is a video-only or audio-only stream, the open will still succeed
     * but attempting to decode the nonexistent track will result in a
     * DECODE_SETUP_FAILURE error. */
    WEBMDEC_OPEN_ANY = 1,

    /* Open the stream for decoding video only.  If the stream does not
     * contain a video track, the open will fail. */
    WEBMDEC_OPEN_VIDEO = 2,

    /* Open the stream for decoding audio only.  If the stream does not
     * contain a audio track, the open will fail. */
    WEBMDEC_OPEN_AUDIO = 3,
} webmdec_open_mode_t;

/*************************************************************************/
/**************** Interface: Library version information *****************/
/*************************************************************************/

/**
 * webmdec_version:  Return the version number of the library as a string
 * (for example, "1.2.3").
 *
 * [Return value]
 *     Library version number.
 */
extern const char *webmdec_version(void);

/*************************************************************************/
/**************** Interface: Opening and closing streams *****************/
/*************************************************************************/

/**
 * webmdec_open_from_buffer:  Create a new stream handle for a stream whose
 * contents are stored in memory.
 *
 * [Parameters]
 *     buffer: Pointer to the buffer containing the stream data.
 *     length: Length of the stream data, in bytes.
 *     open_mode: Decoding mode to use (a WEBMDEC_OPEN_* constant).
 *     error_ret: Pointer to variable to receive the error code from the
 *         operation (always WEBMDEC_NO_ERROR on success).  May be NULL if
 *         the error code is not needed.
 * [Return value]
 *     Newly-created handle, or NULL on error.
 */
extern webmdec_t *webmdec_open_from_buffer(
    const void *buffer, long length, webmdec_open_mode_t open_mode,
    webmdec_error_t *error_ret);

/**
 * webmdec_open_from_callbacks:  Create a new stream handle for a stream
 * whose contents are accessed through a set of callbacks.
 *
 * This function will fail with WEBMDEC_ERROR_INVALID_ARGUMENT if the
 * callback set is incorrectly specified (no read function, or a length
 * function but no tell or seek function).
 *
 * [Parameters]
 *     callbacks: Set of callbacks to be used to access the stream data.
 *     opaque: Opaque pointer value passed through to the callbacks.
 *     open_mode: Decoding mode to use (a WEBMDEC_OPEN_* constant).
 *     error_ret: Pointer to variable to receive the error code from the
 *         operation (always WEBMDEC_NO_ERROR on success).  May be NULL if
 *         the error code is not needed.
 * [Return value]
 *     Newly-created handle, or NULL on error.
 */
extern webmdec_t *webmdec_open_from_callbacks(
    webmdec_callbacks_t callbacks, void *opaque, webmdec_open_mode_t open_mode,
    webmdec_error_t *error_ret);

/**
 * webmdec_open_from_file:  Create a new stream handle for a stream whose
 * contents will be read from a file on the filesystem.
 *
 * If stdio support was disabled when the library was built, this function
 * will always fail with WEBMDEC_ERROR_DISABLED_FUNCTION.
 *
 * [Parameters]
 *     path: Pathname of the file from which the stream is to be read.
 *     open_mode: Decoding mode to use (a WEBMDEC_OPEN_* constant).
 *     error_ret: Pointer to variable to receive the error code from the
 *         operation (always WEBMDEC_NO_ERROR on success).  May be NULL if
 *         the error code is not needed.
 * [Return value]
 *     Newly-created handle, or NULL on error.
 */
extern webmdec_t *webmdec_open_from_file(
    const char *path, webmdec_open_mode_t open_mode,
    webmdec_error_t *error_ret);

/**
 * webmdec_close:  Close a handle, freeing all associated resources.
 * After calling this function, the handle is no longer valid.
 *
 * [Parameters]
 *     handle: Handle to close.  If NULL, this function does nothing.
 */
extern void webmdec_close(webmdec_t *handle);

/*************************************************************************/
/***************** Interface: Getting stream information *****************/
/*************************************************************************/

/**
 * webmdec_last_error:  Return the error code from the most recent failed
 * operation on the given handle.
 *
 * The value returned from this function is only valid if the function is
 * called immediately after a failed operation on the handle.  Successful
 * operations may arbitrarily change the saved error code.
 *
 * [Parameters]
 *     handle: Handle to operate on.
 * [Return value]
 *     Error code from the most recent failed operation.
 */
extern webmdec_error_t webmdec_last_error(const webmdec_t *handle);

/**
 * webmdec_video_width:  Return the width of the video frame in the given
 * stream.
 *
 * [Parameters]
 *     handle: Handle to operate on.
 * [Return value]
 *     Video frame width, or zero if the stream has no video.
 */
extern int webmdec_video_width(const webmdec_t *handle);

/**
 * webmdec_video_height:  Return the height of the video frame in the given
 * stream.
 *
 * [Parameters]
 *     handle: Handle to operate on.
 * [Return value]
 *     Video frame height, or zero if the stream has no video.
 */
extern int webmdec_video_height(const webmdec_t *handle);

/**
 * webmdec_video_rate:  Return the video frame rate of the given stream.
 * A value of zero indicates that the frame rate is either unknown or
 * not constant.
 *
 * [Parameters]
 *     handle: Handle to operate on.
 * [Return value]
 *     Video frame rate, in frames per second.
 */
extern double webmdec_video_rate(const webmdec_t *handle);

/**
 * webmdec_audio_channels:  Return the number of audio channels in the
 * given stream.
 *
 * [Parameters]
 *     handle: Handle to operate on.
 * [Return value]
 *     Number of audio channels, or zero if the stream has no audio.
 */
extern int webmdec_audio_channels(const webmdec_t *handle);

/**
 * webmdec_audio_rate:  Return the audio sampling rate of the given stream.
 *
 * [Parameters]
 *     handle: Handle to operate on.
 * [Return value]
 *     Audio sampling rate, or zero if the stream has no audio.
 */
extern int webmdec_audio_rate(const webmdec_t *handle);

/*************************************************************************/
/********************* Interface: Seeking in streams *********************/
/*************************************************************************/

/**
 * webmdec_rewind:  Seek to the beginning of the given stream.  Equivalent
 * to webmdec_seek(handle, 0.0).
 *
 * [Parameters]
 *     handle: Handle to operate on.
 * [Return value]
 *     True (nonzero) on success, false (zero) on failure.
 */
extern int webmdec_rewind(webmdec_t *handle);

/**
 * webmdec_seek_to_time:  Seek to the given timestamp in the stream, so
 * that the next read or decode operation starts from (approximately) that
 * timestamp.  A timestamp of 0.0 indicates the beginning of the stream.
 *
 * [Parameters]
 *     handle: Handle to operate on.
 *     timestamp: Timestamp to seek to, in seconds.
 * [Return value]
 *     True (nonzero) on success, false (zero) on failure.
 */
extern int webmdec_seek(webmdec_t *handle, double timestamp);

/**
 * webmdec_tell:  Return the current stream timestamp, which is the
 * timestamp at which the next read or decode operation will start.
 * A timestamp of 0.0 indicates the beginning of the stream.
 *
 * [Parameters]
 *     handle: Handle to operate on.
 * [Return value]
 *     Current stream timestamp, in seconds.
 */
extern double webmdec_tell(const webmdec_t *handle);

/*************************************************************************/
/*********************** Interface: Reading frames ***********************/
/*************************************************************************/

/**
 * webmdec_read_frame:  Read the raw video or audio data for the next frame
 * in the stream.
 *
 * This function reads exactly one video or audio frame from the stream;
 * the return variables for the type of frame not seen will be set to
 * NULL, length 0, and timestamp -1.  If the data return variable pointer
 * for a frame type is NULL, the function will skip that frame type and
 * return only frames of the other type.  It is an error to pass NULL for
 * both video_data_ret and audio_data_ret.
 *
 * Buffer pointers returned by this function point to internal memory
 * which only remains valid until the next read or decode operation.
 *
 * If the read operation fails, no return variables are modified.
 *
 * [Parameters]
 *     handle: Handle to operate on.
 *     video_data_ret: Pointer to variable to receive a pointer to the raw
 *         video data for the frame, or NULL if the video data is not
 *         needed.
 *     video_length_ret: Pointer to variable to receive the length of the
 *         raw video data for the frame, in bytes.  Ignored if
 *         video_data_ret is NULL.
 *     video_time_ret: Pointer to variable to receive the video frame's
 *         timestamp, in seconds, or NULL if the video timestamp is not
 *         needed.  Ignored if video_data_ret is NULL.
 *     audio_data_ret: Pointer to variable to receive a pointer to the raw
 *         audio data for the frame, or NULL if the audio data is not
 *         needed.
 *     audio_length_ret: Pointer to variable to receive the length of the
 *         raw audio data for the frame, in bytes.  Ignored if
 *         audio_data_ret is NULL.
 *     audio_time_ret: Pointer to variable to receive the audio frame's
 *         timestamp, in seconds, or NULL if the audio timestamp is not
 *         needed.  Ignored if audio_data_ret is NULL.
 * [Return value]
 *     True (nonzero) on success, false (zero) on failure.
 */
extern int webmdec_read_frame(
    webmdec_t *handle,
    const void **video_data_ret, int *video_length_ret, double *video_time_ret,
    const void **audio_data_ret, int *audio_length_ret, double *audio_time_ret);

/**
 * webmdec_decode_frame:  Read and decode the video or audio data for the
 * the next frame in the stream.  Video is decoded into planar YUV 4:2:0
 * pixel data (plane order Y, U, V), and audio is decoded into single-
 * precision floating point interleaved linear PCM data.
 *
 * The behavior of this function is identical to that of webmdec_read_frame(),
 * except that this function also decodes the raw data.
 *
 * If video or audio decoding is requested and support for the associated
 * decoder was disabled when the library was built, this function will fail
 * with WEBMDEC_ERROR_DISABLED_FUNCTION.
 *
 * [Parameters]
 *     handle: Handle to operate on.
 *     video_data_ret: Pointer to variable to receive a pointer to the
 *         decoded video data for the frame, or NULL if the video data is
 *         not needed.
 *     video_time_ret: Pointer to variable to receive the video frame's
 *         timestamp, in seconds, or NULL if the video timestamp is not
 *         needed.  Ignored if video_data_ret is NULL.
 *     audio_data_ret: Pointer to variable to receive a pointer to the
 *         decoded audio data for the frame, or NULL if the audio data is
 *         not needed.
 *     audio_samples_ret: Pointer to variable to receive the length of the
 *         audio data for the frame, in samples.  Ignored if
 *         audio_Data_ret is NULL.
 *     audio_time_ret: Pointer to variable to receive the audio frame's
 *         timestamp, in seconds, or NULL if the audio timestamp is not
 *         needed.  Ignored if audio_data_ret is NULL.
 * [Return value]
 *     True (nonzero) on success, false (zero) on failure.
 */
extern int webmdec_decode_frame(
    webmdec_t *handle, const void **video_data_ret, double *video_time_ret,
    const float **audio_data_ret, int *audio_samples_ret,
    double *audio_time_ret);

/*************************************************************************/
/*************************************************************************/

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* WEBMDEC_H */
