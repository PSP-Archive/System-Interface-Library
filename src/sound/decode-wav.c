/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sound/decode-wav.c: Audio "decoder" for RIFF WAVE-encapsulated PCM data.
 */

#include "src/base.h"
#include "src/memory.h"
#include "src/sound.h"
#include "src/sound/decode.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Private data structure. */

struct SoundDecodePrivate {
    int data_offset;  // File offset of PCM data (in bytes).
    int sample_size;  // Size of a single sample (in bytes).
    int len;          // Audio data length (in samples).
    int pos;          // Current decode position (in samples).
};

/*-----------------------------------------------------------------------*/

/* Method implementation declarations. */

static int decode_wav_get_pcm(
    SoundDecodeHandle *this, int16_t *pcm_buffer, int pcm_len,
    int *loop_offset_ret);
static void decode_wav_close(SoundDecodeHandle *this);

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * scan_wav_header:  Scan the header of the audio data, and confirm that
 * it's a RIFF WAVE file with S16LE PCM data.  On success, the stereo and
 * native_freq fields of the decoder instance handle are set from the
 * contents of the "fmt " chunk.
 *
 * [Parameters]
 *     buffer: Buffer containing data to scan.
 *     buflen: Data length, in bytes.
 * [Return value]
 *     True if the data is a valid WAV file, false if not.
 */
static int scan_wav_header(SoundDecodeHandle *this, const uint8_t *buffer,
                           int buflen);

/*************************************************************************/
/************************ Method implementations *************************/
/*************************************************************************/

int decode_wav_open(SoundDecodeHandle *this)
{
    this->get_pcm = decode_wav_get_pcm;
    this->close   = decode_wav_close;

    this->private = mem_alloc(sizeof(*this->private), 0, MEM_ALLOC_TEMP);
    if (!this->private) {
        DLOG("Out of memory");
        return 0;
    }

    /* Check at most the first 2k of the file for a valid WAV header.  If
     * the header extends past the first 2k, it's probably either a broken
     * file or something besides PCM, so treat that as an error. */
    const uint8_t *data;
    const int len = decode_get_data(this, 0, 2048, &data);
    if (!scan_wav_header(this, data, len)) {
        mem_free(this->private);
        return 0;
    }

    this->private->sample_size = this->stereo ? 4 : 2;
    this->private->pos = 0;

    return 1;
}

/*-----------------------------------------------------------------------*/

static int decode_wav_get_pcm(
    SoundDecodeHandle *this, int16_t *pcm_buffer, int pcm_len,
    int *loop_offset_ret)
{
    SoundDecodePrivate *private = this->private;

    const int loopend =
        (this->loop_length > 0
         ? ubound(this->loop_start + this->loop_length, private->len)
         : private->len);
    int endpoint;
    if (this->internal->loop && private->pos < loopend) {
        endpoint = loopend;
    } else {
        endpoint = private->len;
    }

    *loop_offset_ret = 0;
    int copied = 0;
    while (copied < pcm_len) {
        if (private->pos < endpoint) {
            int to_copy = ubound(pcm_len - copied, endpoint - private->pos);
            const uint8_t *data;
            const int len = decode_get_data(
                this,
                private->data_offset + private->pos * private->sample_size,
                to_copy * private->sample_size, &data);
            if (len != to_copy * private->sample_size) {
                DLOG("Short read (wanted %d, got %d)",
                     to_copy * private->sample_size, len);
                to_copy = len / private->sample_size;
                if (!to_copy) {
                    break;
                }
            }
            memcpy((uint8_t *)pcm_buffer + copied * private->sample_size,
                   data, to_copy * private->sample_size);
            copied += to_copy;
            private->pos += to_copy;
        }
        if (private->pos >= endpoint) {
            if (this->internal->loop
             && loopend > 0  // Avoid infinite loop on empty file.
             && private->pos == loopend) {
                *loop_offset_ret += private->pos - this->loop_start;
                private->pos = this->loop_start;
            } else {  // No loop.
                break;
            }
        }
    }

    return copied;
}

/*-----------------------------------------------------------------------*/

static void decode_wav_close(SoundDecodeHandle *this)
{
    mem_free(this->private);
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static int scan_wav_header(SoundDecodeHandle *this, const uint8_t *buffer,
                           int buflen)
{
    /* First check the file header. */

    if (memcmp(buffer, "RIFF", 4) != 0 || memcmp(buffer+8, "WAVE", 4) != 0) {
        DLOG("Data is not a RIFF WAVE");
        return 0;
    }

    /* Look for the "fmt ", "smpl", and "data" chunks.  To simplify
     * processing, we ignore all chunks after the "data" chunk, and thus
     * don't handle the case where "fmt " or "smpl" comes after "data".
     * (In practice, everybody puts "data" last, so this shouldn't be a
     * problem.) */

    int fmt_offset = 0, smpl_offset = 0, data_offset = 0;
    int fmt_size = 0, smpl_size = 0, data_size = 0;
    int pos = 12;
    while (!data_offset && pos < buflen) {
        const uint32_t chunk_size = buffer[pos+4] <<  0
                                  | buffer[pos+5] <<  8
                                  | buffer[pos+6] << 16
                                  | buffer[pos+7] << 24;
        if (chunk_size >= 0x80000000U) {
            DLOG("Chunk at 0x%X too large", chunk_size);
            return 0;
        }
        if (memcmp(buffer + pos, "fmt ", 4) == 0) {
            fmt_offset = pos + 8;
            fmt_size = chunk_size;
        } else if (memcmp(buffer + pos, "smpl", 4) == 0) {
            smpl_offset = pos + 8;
            smpl_size = chunk_size;
        } else if (memcmp(buffer + pos, "data", 4) == 0) {
            data_offset = pos + 8;
            data_size = chunk_size;
        }
        pos += align_up(8 + chunk_size, 2);
    }
    if (!fmt_offset || !data_offset) {
        DLOG("'%s' chunk not found in data", !fmt_offset ? "fmt " : "data");
        return 0;
    }

    /* Process the "fmt " chunk. */

    if (fmt_size < 16) {
        DLOG("'fmt ' chunk too small (%d, must be at least 16)", fmt_size);
        return 0;
    }
    const int format        = buffer[fmt_offset+ 0] | buffer[fmt_offset+ 1]<<8;
    const int channels      = buffer[fmt_offset+ 2] | buffer[fmt_offset+ 3]<<8;
    const int bits          = buffer[fmt_offset+14] | buffer[fmt_offset+15]<<8;
    const uint32_t freq     = (uint32_t)buffer[fmt_offset+ 4] <<  0
                            | (uint32_t)buffer[fmt_offset+ 5] <<  8
                            | (uint32_t)buffer[fmt_offset+ 6] << 16
                            | (uint32_t)buffer[fmt_offset+ 7] << 24;
    const uint32_t byterate = (uint32_t)buffer[fmt_offset+ 8] <<  0
                            | (uint32_t)buffer[fmt_offset+ 9] <<  8
                            | (uint32_t)buffer[fmt_offset+10] << 16
                            | (uint32_t)buffer[fmt_offset+11] << 24;
    if (format != 0x0001) {
        DLOG("Audio format 0x%X not supported", format);
        return 0;
    }
    if (channels != 1 && channels != 2) {
        DLOG("%d channels not supported", channels);
        return 0;
    }
    if (bits != 16) {
        DLOG("%d-bit samples not supported", bits);
        return 0;
    }
    if (freq >= 0x80000000U) {
        DLOG("Invalid frequency %u", freq);
        return 0;
    }
    if (byterate >= 0x10000000U) {  // Avoid overflow when converting to bits.
        DLOG("Invalid data rate %u", byterate);
        return 0;
    }

    /* Process the "smpl" chunk if it was found. */

    this->loop_start = 0;
    this->loop_length = 0;
    if (smpl_offset) {
        if (smpl_size < 60) {
            DLOG("'smpl' chunk too small (%d, must be at least 60), ignoring",
                 smpl_size);
        } else {
            const int num_loops = buffer[smpl_offset+28] <<  0
                                | buffer[smpl_offset+29] <<  8
                                | buffer[smpl_offset+30] << 16
                                | buffer[smpl_offset+31] << 24;
            if (num_loops > 0) {
                this->loop_start = buffer[smpl_offset+44] <<  0
                                 | buffer[smpl_offset+45] <<  8
                                 | buffer[smpl_offset+46] << 16
                                 | buffer[smpl_offset+47] << 24;
                const int loop_end = buffer[smpl_offset+48] <<  0
                                   | buffer[smpl_offset+49] <<  8
                                   | buffer[smpl_offset+50] << 16
                                   | buffer[smpl_offset+51] << 24;
                /* The endpoint in the smpl chunk is the index of the last
                 * sample in the loop, _not_ the index of the first sample
                 * after the loop, so we need to add 1 when computing the
                 * loop length. */
                this->loop_length = (loop_end + 1) - this->loop_start;
                if (this->loop_length <= 0) {
                    DLOG("Bad loop endpoints %d - %d in smpl chunk, ignoring",
                         this->loop_start, loop_end);
                    this->loop_start = 0;
                    this->loop_length = 0;
                }
            }
        }
    }

    /* Success!  Update the instance handle. */

    this->stereo = (channels == 2);
    this->native_freq = (int)freq;
    this->bitrate = (int)byterate * 8;
    this->private->data_offset = data_offset;
    if (data_size > 0 && data_size < (this->internal->datalen - data_offset)) {
        this->private->len = data_size / (2*channels);
    } else {
        this->private->len =
            (this->internal->datalen - data_offset) / (2*channels);
    }
    if (this->loop_start + this->loop_length > this->private->len) {
        DLOG("Loop endpoints %d - %d in smpl chunk are out of range for"
             " stream length %d, ignoring loop", this->loop_start,
             this->loop_start + this->loop_length - 1, this->private->len);
        this->loop_start = 0;
        this->loop_length = 0;
    }

    return 1;
}

/*************************************************************************/
/*************************************************************************/
