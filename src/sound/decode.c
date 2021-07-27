/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sound/decode.c: Audio data decoding interface.
 */

#include "src/base.h"
#include "src/memory.h"
#include "src/sound.h"
#include "src/sound/decode.h"
#include "src/sysdep.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Mapping from supported audio data formats to open() method
 * implementations.  These can be changed at runtime by calling
 * sound_decode_set_handler(). */

static struct {
    const SoundFormat format;
    SoundDecodeOpenFunc *open;
} decode_handlers[] = {
    {SOUND_FORMAT_WAV, decode_wav_open},
    {SOUND_FORMAT_MP3, NULL},  // No cross-platform MP3 decoder available.
#ifdef SIL_SOUND_INCLUDE_OGG
    {SOUND_FORMAT_OGG, decode_ogg_open},
#else
    {SOUND_FORMAT_OGG, NULL},
#endif
};

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * open_common:  Common functionality for sound_decode_open() and
 * sound_decode_open_from_file().
 *
 * [Parameters]
 *     this: Audio decoder instance handle.
 *     format: Audio data format constant (SOUND_FORMAT_*).
 * [Return value]
 *     True on success, false on error.
 */
static int open_common(SoundDecodeHandle *this, SoundFormat format);

/**
 * init_resample:  Initialize resampling data for a decoder.  If an
 * out-of-memory error occurs, the resample_eof flag is set to terminate
 * decoding.
 *
 * [Parameters]
 *     this: Audio decoder instance handle.
 */
static void init_resample(SoundDecodeHandle *this);

/**
 * fill_resample_buffer:  Fill the resample buffer with PCM data read from
 * the stream, updating this->resample_len and this->resample_eof
 * appropriately.
 *
 * [Parameters]
 *     this: Audio decoder instance handle.
 */
static void fill_resample_buffer(SoundDecodeHandle *this);

/*************************************************************************/
/************* Interface: Audio format decoder configuration *************/
/*************************************************************************/

void sound_decode_set_handler(SoundFormat format,
                              SoundDecodeOpenFunc *open_func)
{
    for (int i = 0; i < lenof(decode_handlers); i++) {
        if (decode_handlers[i].format == format) {
            decode_handlers[i].open = open_func;
        }
    }
}

/*-----------------------------------------------------------------------*/

int sound_decode_has_handler(SoundFormat format)
{
    for (int i = 0; i < lenof(decode_handlers); i++) {
        if (decode_handlers[i].format == format) {
            return decode_handlers[i].open != NULL;
        }
    }
    return 0;
}

/*************************************************************************/
/***************** Interface: Decoder instance creation ******************/
/*************************************************************************/

SoundDecodeHandle *sound_decode_open(
    SoundFormat format, const void *data, int datalen, int loop,
    int interpolate)
{
    if (UNLIKELY(!data) || UNLIKELY(datalen == 0)) {
        DLOG("Invalid parameters: 0x%X %p %d %d %d",
             format, data, datalen, loop, interpolate);
        goto error_return;
    }

    SoundDecodeHandle *this = mem_alloc(sizeof(*this), 0,
                                        MEM_ALLOC_TEMP | MEM_ALLOC_CLEAR);
    if (UNLIKELY(!this)) {
        DLOG("No memory for decoder handle");
        goto error_return;
    }
    this->internal = mem_alloc(sizeof(*(this->internal)), 0,
                                      MEM_ALLOC_TEMP | MEM_ALLOC_CLEAR);
    if (UNLIKELY(!this->internal)) {
        DLOG("No memory for decoder handle");
        goto error_free_this;
    }
    this->internal->data_type      = SOUND_DECODE_BUFFER;
    this->internal->data           = data;
    this->internal->datalen        = datalen;
    this->internal->loop           = (loop != 0);
    this->internal->do_interpolate = (interpolate != 0);

    if (!open_common(this, format)) {
        goto error_free_internal;
    }

    return this;

  error_free_internal:
    mem_free(this->internal);
  error_free_this:
    mem_free(this);
  error_return:
    return NULL;
}

/*-----------------------------------------------------------------------*/

SoundDecodeHandle *sound_decode_open_from_file(
    SoundFormat format, struct SysFile *fh, int64_t dataofs, int datalen,
    int loop, int interpolate)
{
    if (UNLIKELY(!fh) || UNLIKELY(datalen == 0)) {
        DLOG("Invalid parameters: 0x%X %p %lld %lld %d %d", format, fh,
             (long long)dataofs, (long long)datalen, loop, interpolate);
        goto error_return;
    }

    fh = sys_file_dup(fh);
    if (UNLIKELY(!fh)) {
        DLOG("Failed to dup file handle");
        goto error_return;
    }

    SoundDecodeHandle *this = mem_alloc(sizeof(*this), 0,
                                        MEM_ALLOC_TEMP | MEM_ALLOC_CLEAR);
    if (UNLIKELY(!this)) {
        DLOG("No memory for decoder handle");
        goto error_close_file;
    }
    this->internal = mem_alloc(sizeof(*(this->internal)), 0,
                                      MEM_ALLOC_TEMP | MEM_ALLOC_CLEAR);
    if (UNLIKELY(!this->internal)) {
        DLOG("No memory for decoder handle");
        goto error_free_this;
    }
    this->internal->data_type      = SOUND_DECODE_FILE;
    this->internal->fh             = fh;
    this->internal->dataofs        = dataofs;
    this->internal->datalen        = datalen;
    this->internal->loop           = (loop != 0);
    this->internal->do_interpolate = (interpolate != 0);

    /* For files, we need a read buffer. */
    this->internal->read_buffer =
        mem_alloc(READ_BUFFER_SIZE, 0, MEM_ALLOC_TEMP);
    if (!this->internal->read_buffer) {
        DLOG("Out of memory for read buffer");
        goto error_free_internal;
    }
    this->internal->read_async_req = sys_file_read_async(
        this->internal->fh, this->internal->read_buffer,
        ubound(READ_BUFFER_SIZE, this->internal->datalen),
        this->internal->dataofs, 0);  // Immediate deadline for high priority.
    if (!this->internal->read_async_req
     && sys_last_error() != SYSERR_TRANSIENT_FAILURE
     && sys_last_error() != SYSERR_FILE_ASYNC_FULL) {
        DLOG("Async read failed: %s", sys_last_errstr());
        goto error_free_read_buffer;
    }

    if (!open_common(this, format)) {
        goto error_cancel_async_req;
    }

    return this;

  error_cancel_async_req:
    if (this->internal->read_async_req) {
        sys_file_abort_async(this->internal->read_async_req);
        sys_file_wait_async(this->internal->read_async_req);
    }
  error_free_read_buffer:
    mem_free(this->internal->read_buffer);
  error_free_internal:
    mem_free(this->internal);
  error_free_this:
    mem_free(this);
  error_close_file:
    sys_file_close(fh);
  error_return:
    return NULL;
}

/*-----------------------------------------------------------------------*/

SoundDecodeHandle *sound_decode_open_custom(
    SoundDecodeOpenFunc *open_func, void *data, int interpolate)
{
    if (UNLIKELY(!open_func)) {
        DLOG("Invalid parameters: %p %p", open_func, data);
        goto error_return;
    }

    SoundDecodeHandle *this = mem_alloc(sizeof(*this), 0,
                                        MEM_ALLOC_TEMP | MEM_ALLOC_CLEAR);
    if (UNLIKELY(!this)) {
        DLOG("No memory for decoder handle");
        goto error_return;
    }
    this->internal = mem_alloc(sizeof(*(this->internal)), 0,
                                      MEM_ALLOC_TEMP | MEM_ALLOC_CLEAR);
    if (UNLIKELY(!this->internal)) {
        DLOG("No memory for decoder handle");
        goto error_free_this;
    }
    this->custom_data              = data;
    this->internal->data_type      = SOUND_DECODE_CUSTOM;
    this->internal->datalen        = 0; // Avoid problems in decode_get_data().
    this->internal->do_interpolate = (interpolate != 0);

    if (!(*open_func)(this)) {
        goto error_free_internal;
    }

    this->internal->decode_freq = this->native_freq;
    this->internal->output_freq = this->native_freq;

    return this;

  error_free_internal:
    mem_free(this->internal);
  error_free_this:
    mem_free(this);
  error_return:
    return NULL;
}

/*************************************************************************/
/******************** Interface: Decoding operations *********************/
/*************************************************************************/

int sound_decode_is_stereo(SoundDecodeHandle *this)
{
    PRECOND(this != NULL, return 0);
    return this->stereo;
}

/*-----------------------------------------------------------------------*/

int sound_decode_native_freq(SoundDecodeHandle *this)
{
    PRECOND(this != NULL, return 0);
    return this->native_freq;
}

/*-----------------------------------------------------------------------*/

void sound_decode_set_loop_points(SoundDecodeHandle *this,
                                  int start, int length)
{
    PRECOND(this != NULL, return);
    if (UNLIKELY(start < 0) || UNLIKELY(length < 0)) {
        DLOG("Invalid parameters: (%p) %d %d", this, start, length);
        return;
    }

    this->loop_start = start;
    this->loop_length = length;
}

/*-----------------------------------------------------------------------*/

void sound_decode_enable_loop(SoundDecodeHandle *this, int loop)
{
    PRECOND(this != NULL, return);
    this->internal->loop = (loop != 0);
}

/*-----------------------------------------------------------------------*/

void sound_decode_set_decode_freq(SoundDecodeHandle *this, int freq)
{
    PRECOND(this != NULL, return);
    if (UNLIKELY(freq < 0)) {
        DLOG("Invalid frequency: %d", freq);
        return;
    }
    SoundDecodeInternal *internal = this->internal;

    internal->decode_freq = freq;
    /* Enable resampling unconditionally (even if the requested frequency
     * matches the output frequency) on the assumption that if the decode
     * frequency is changed once, it will be changed again later. */
    internal->need_resample = 1;
}

/*-----------------------------------------------------------------------*/

void sound_decode_set_output_freq(SoundDecodeHandle *this, int freq)
{
    PRECOND(this != NULL, return);
    if (UNLIKELY(freq <= 0)) {
        DLOG("Invalid frequency: %d", freq);
        return;
    }
    SoundDecodeInternal *internal = this->internal;

    internal->output_freq = freq;
    /* If the requested frequency differs from the decoding rate, we'll
     * need to resample in get_pcm(). */
    if (internal->output_freq != internal->decode_freq) {
        internal->need_resample = 1;
    }
}

/*-----------------------------------------------------------------------*/

int sound_decode_get_pcm(SoundDecodeHandle *this, int16_t *pcm_buffer,
                         int pcm_len)
{
    PRECOND(this != NULL, return 0);
    if (UNLIKELY(pcm_buffer == NULL) || UNLIKELY(pcm_len == 0)) {
        DLOG("Invalid parameters: (this=%p) %p %d", this, pcm_buffer, pcm_len);
        return 0;
    }
    SoundDecodeInternal *internal = this->internal;

    const int stereo = this->stereo;
    const unsigned int sample_size = (stereo ? 4 : 2);

    /* Set up the resampling buffer if required and not yet done.  We don't
     * call init_resample() any sooner because it implicitly reads the
     * first buffer's worth of data, which may be incorrect if the sound
     * has a short loop. */
    if (internal->need_resample && !internal->resample_active) {
        init_resample(this);
    }

    if (!internal->resample_active) {
        int loop_offset;
        const int got = this->get_pcm(this, pcm_buffer, pcm_len, &loop_offset);
        if (got == 0) {
            return 0;
        }
        internal->samples_gotten += got;
        internal->samples_gotten -= loop_offset;
        mem_clear((uint8_t *)pcm_buffer + got * sample_size,
                  (pcm_len - got) * sample_size);
        return 1;
    }

    if (internal->resample_eof) {
        return 0;
    }

    const int decode_freq = internal->decode_freq;
    const int output_freq = internal->output_freq;

    /* This EOF flag is used when interpolating; it is set one cycle later
     * than internal->resample_eof to allow interpolating between the last
     * sample in the stream and zero. */
    int resample_delayed_eof = 0;
    int copied;
    for (copied = 0; copied < pcm_len && !resample_delayed_eof; copied++) {

        if (stereo) {
            if (internal->do_interpolate) {
                const int16_t this_l =
                    (internal->resample_eof ? 0 :
                     internal->resample_buf[internal->resample_pos*2+0]);
                const int16_t this_r =
                    (internal->resample_eof ? 0 :
                     internal->resample_buf[internal->resample_pos*2+1]);
                pcm_buffer[copied*2+0] = internal->last_l
                    + ((this_l - internal->last_l) * internal->pos_frac
                       / output_freq);
                pcm_buffer[copied*2+1] = internal->last_r
                    + ((this_r - internal->last_r) * internal->pos_frac
                       / output_freq);
            } else {
                if (internal->resample_eof) {
                    break;
                }
                pcm_buffer[copied*2+0] =
                    internal->resample_buf[internal->resample_pos*2+0];
                pcm_buffer[copied*2+1] =
                    internal->resample_buf[internal->resample_pos*2+1];
            }
        } else {
            if (internal->do_interpolate) {
                const int16_t this_l =
                    (internal->resample_eof ? 0 :
                     internal->resample_buf[internal->resample_pos]);
                pcm_buffer[copied] = internal->last_l
                    + ((this_l - internal->last_l) * internal->pos_frac
                       / output_freq);
            } else {
                if (internal->resample_eof) {
                    break;
                }
                pcm_buffer[copied] =
                    internal->resample_buf[internal->resample_pos];
            }
        }

        internal->pos_frac += decode_freq;
        while (internal->pos_frac >= output_freq) {
            if (internal->do_interpolate) {
                if (internal->resample_eof) {
                    resample_delayed_eof = 1;
                    break;
                }
                if (stereo) {
                    internal->last_l =
                        internal->resample_buf[internal->resample_pos*2+0];
                    internal->last_r =
                        internal->resample_buf[internal->resample_pos*2+1];
                } else {
                    internal->last_l =
                        internal->resample_buf[internal->resample_pos];
                }
            }
            internal->pos_frac -= output_freq;
            internal->resample_pos++;
            internal->samples_gotten++;
            if (internal->loop
             && internal->resample_loopofs > 0
             && internal->samples_gotten >= this->loop_start + this->loop_length) {
                internal->resample_loopofs -= this->loop_length;
                internal->samples_gotten -= this->loop_length;
            }
            if (internal->resample_pos >= internal->resample_len) {
                internal->samples_gotten -= internal->resample_loopofs;
                fill_resample_buffer(this);
                internal->resample_pos = 0;
            }
        }

    }  // for (copied = 0; copied < pcm_len && !resample_delayed_eof; copied++)

    mem_clear((uint8_t *)pcm_buffer + copied * sample_size,
              (pcm_len - copied) * sample_size);
    return 1;
}

/*-----------------------------------------------------------------------*/

float sound_decode_get_position(SoundDecodeHandle *this)
{
    return (float)this->internal->samples_gotten / (float)this->native_freq;
}

/*-----------------------------------------------------------------------*/

void sound_decode_close(SoundDecodeHandle *this)
{
    PRECOND(this != NULL, return);
    this->close(this);
    if (this->internal->read_async_req) {
        sys_file_abort_async(this->internal->read_async_req);
        sys_file_wait_async(this->internal->read_async_req);
    }
    sys_file_close(this->internal->fh);
    mem_free(this->internal->resample_buf);
    mem_free(this->internal->read_buffer);
    mem_free(this->internal);
    mem_free(this);
}

/*************************************************************************/
/***************** Utility functions for decoder modules *****************/
/*************************************************************************/

int decode_get_data(SoundDecodeHandle *this, int pos, int len,
                    const uint8_t **ptr_ret)
{
    PRECOND(this != NULL, return 0);
    PRECOND(ptr_ret != NULL, return 0);
    SoundDecodeInternal *internal = this->internal;

    if (pos >= internal->datalen) {
        *ptr_ret = NULL;
        return 0;
    }

    if (len > internal->datalen - pos) {
        len = internal->datalen - pos;
    }

    switch (internal->data_type) {

      case SOUND_DECODE_CUSTOM:  // Impossible (since datalen is 0).
        DLOG("BUG: decode_get_data() for custom decoder");
        *ptr_ret = NULL;
        len = 0;
        break;

      case SOUND_DECODE_BUFFER:
        *ptr_ret = internal->data + pos;
        break;

      case SOUND_DECODE_FILE:

        /* Don't try to read more than the buffer size at once. */
        if (len > READ_BUFFER_SIZE) {
            len = READ_BUFFER_SIZE;
        }

        /* If the requested region overlaps with the region currently being
         * read, wait for the read to finish and update the read buffer
         * length.  Also update if an in-progress read has completed. */
        if (internal->read_async_req
            && ((pos >= internal->read_buffer_pos
                 && pos + len <= internal->read_buffer_pos + READ_BUFFER_SIZE
                 && pos + len - internal->read_buffer_pos > internal->read_async_ofs)
                || sys_file_poll_async(internal->read_async_req)))
        {
            int nread = sys_file_wait_async(internal->read_async_req);
            internal->read_async_req = 0;
            if (nread < 0) {
                DLOG("sys_file_wait_async(%d[%p,%lld,%d]): %s",
                     internal->read_async_req, internal->fh,
                     (long long)(internal->read_buffer_pos
                                 + internal->read_async_ofs),
                     READ_BUFFER_SIZE - internal->read_async_ofs,
                     sys_last_errstr());
                nread = 0;
            }
            internal->read_buffer_len = internal->read_async_ofs + nread;
        }

        /* If the requested region is not completely contained in the read
         * buffer, discard the existing buffer and immediately read in the
         * new data. */
        if (pos < internal->read_buffer_pos
         || pos + len > internal->read_buffer_pos + internal->read_buffer_len) {
            if (internal->read_async_req) {
                sys_file_abort_async(internal->read_async_req);
                (void) sys_file_wait_async(internal->read_async_req);
                internal->read_async_req = 0;
            }
            internal->read_buffer_pos = pos;
            sys_file_seek(internal->fh, internal->dataofs + pos, FILE_SEEK_SET);
            int nread = sys_file_read(internal->fh, internal->read_buffer, len);
            if (nread < 0) {
                DLOG("sys_file_read(%p,%p,%d) from %lld: %s", internal->fh,
                     internal->read_buffer, len, (long long)pos,
                     sys_last_errstr());
                internal->read_buffer_len = 0;
                return 0;
            }
            internal->read_buffer_len = nread;
            len = nread;
        }

        /* If the requested region starts in the second half of the read
         * buffer, move it to the beginning of the read buffer.  (But don't
         * do anything if an asynchronous read is in progress, since we
         * can't move that data until the read finishes.) */
        if (!internal->read_async_req
         && pos >= internal->read_buffer_pos + READ_BUFFER_SIZE/2) {
            const int ofs = pos - internal->read_buffer_pos;
            memmove(internal->read_buffer, internal->read_buffer + ofs,
                    internal->read_buffer_len - ofs);
            internal->read_buffer_pos += ofs;
            internal->read_buffer_len -= ofs;
        }

        /* If no asynchronous read is in progress and the read buffer is
         * not full, start a read operation to fill the buffer. */
        if (!internal->read_async_req
         && internal->read_buffer_len < READ_BUFFER_SIZE) {
            const int buffer_end =
                internal->read_buffer_pos + internal->read_buffer_len;
            const int toread =
                ubound(READ_BUFFER_SIZE - internal->read_buffer_len,
                       internal->datalen - buffer_end);
            float deadline =
                (float)internal->read_buffer_len / (float)this->bitrate;
            deadline -= 0.01f;  // Get the data loaded _before_ it's needed.
            internal->read_async_req = sys_file_read_async(
                internal->fh,
                internal->read_buffer + internal->read_buffer_len,
                toread,
                internal->dataofs + internal->read_buffer_pos
                    + internal->read_buffer_len,
                lbound(deadline, 0.0));
            if (!internal->read_async_req) {
                DLOG("sys_file_read_async(%p,%p,%d,%lld+%lld,%g): %s",
                     internal->fh,
                     internal->read_buffer + internal->read_buffer_len,
                     toread,
                     (long long)internal->dataofs,
                     (long long)(internal->read_buffer_pos
                                 + internal->read_buffer_len),
                     deadline,
                     sys_last_errstr());
            } else {
                internal->read_async_ofs = internal->read_buffer_len;
            }
        }

        /* The requested data is now in the read buffer, so return a
         * pointer to it. */
        *ptr_ret = internal->read_buffer + (pos - internal->read_buffer_pos);

    }  // switch (internal->data_type)

    return len;
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static int open_common(SoundDecodeHandle *this, SoundFormat format)
{
    int found = 0;
    for (int i = 0; i < lenof(decode_handlers); i++) {
        if (format == decode_handlers[i].format
         && decode_handlers[i].open != NULL) {
            if (!(*decode_handlers[i].open)(this)) {
                return 0;
            }
            found = 1;
            break;
        }
    }
    if (!found) {
        DLOG("Unsupported format 0x%X", format);
        return 0;
    }

    if (this->native_freq == 0) {
        DLOG("Invalid frequency 0");
        this->close(this);
        return 0;
    }

    /* Default to no resampling. */
    this->internal->decode_freq = this->native_freq;
    this->internal->output_freq = this->native_freq;

    return 1;
}

/*-----------------------------------------------------------------------*/

static void init_resample(SoundDecodeHandle *this)
{
    SoundDecodeInternal *internal = this->internal;
    PRECOND(!internal->resample_active, return);

    internal->resample_active = 1;
    internal->resample_pos = 0;
    internal->pos_frac = 0;
    const int sample_size = (this->stereo ? 4 : 2);
    const int bufsize = RESAMPLE_BUFLEN * sample_size;
    internal->resample_buf = mem_alloc(bufsize, 0, MEM_ALLOC_TEMP);
    if (!internal->resample_buf) {
        DLOG("Out of memory for resample buffer, forcing EOF");
        internal->resample_eof = 1;
    } else {
        /* Read in the first buffer's worth of data now. */
        fill_resample_buffer(this);
        if (internal->do_interpolate && !internal->resample_eof) {
            if (this->stereo) {
                internal->last_l =
                    internal->resample_buf[internal->resample_pos*2+0];
                internal->last_r =
                    internal->resample_buf[internal->resample_pos*2+1];
            } else {
                internal->last_l =
                    internal->resample_buf[internal->resample_pos];
            }
            internal->resample_pos++;
        }
    }
}

/*-----------------------------------------------------------------------*/

static void fill_resample_buffer(SoundDecodeHandle *this)
{
    SoundDecodeInternal *internal = this->internal;

    internal->resample_len = this->get_pcm(
        this, internal->resample_buf, RESAMPLE_BUFLEN,
        &internal->resample_loopofs);
    internal->resample_eof = (internal->resample_len == 0);
}

/*************************************************************************/
/*************************************************************************/
