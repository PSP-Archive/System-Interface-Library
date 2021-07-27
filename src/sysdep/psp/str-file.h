/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/str-file.h: Definition of the custom stream file format
 * used for movie playback on the PSP.
 */

#ifndef SIL_SRC_SYSDEP_PSP_STR_FILE_H
#define SIL_SRC_SYSDEP_PSP_STR_FILE_H

/*************************************************************************/
/*************************************************************************/

/* All integer values are written in native byte order (little-endian). */

/* File header for *.str movie files.  This header is immediately followed
 * by one STRFrameIndex for each frame in the movie. */
typedef struct STRFileHeader {
    char magic[4];              // "STR\0"
    int32_t header_size;        // sizeof(STRFileHeader)
    int32_t num_frames;         // Number of frames
    int16_t width, height;      // Video frame size
    int32_t fps_num;            // Frame rate (frames/second) numerator
    int32_t fps_den;            // Frame rate (frames/second) numerator
    int32_t max_video_size;     // Maximum video data size for a single frmae
    int32_t max_audio_size;     // Maximum audio data size for a single frmae
} STRFileHeader;

/* Index entry for a frame in the file header. */
typedef struct STRFrameIndex {
    uint32_t offset;            // File offset (in bytes)
    int32_t size;               // Frame data size (in bytes, including header)
} STRFrameIndex;

/* Header for each frame. */
typedef struct STRFrameHeader {
    int32_t video_size;         // Size of video data (in bytes)
    int32_t video_padding;      // Bytes of padding after video data
    int32_t audio_size;         // Size of audio data (in bytes)
    int32_t audio_padding;      // Bytes of padding after audio data
} STRFrameHeader;

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_PSP_STR_FILE_H
