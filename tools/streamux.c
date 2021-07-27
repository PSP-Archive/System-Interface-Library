/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * tools/streamux.c: Program to multiplex an H.264 video stream and PCM
 * audio stream into a movie stream in the format used by the PSP movie
 * playback code, or demultiplex back into the video and audio streams.
 */

/*
 * To create a PSP-format stream file, run this program as:
 *     streamux video.264 audio.pcm framerate >movie.str
 * The video stream must be raw H.264 video (level 2.1), and the audio
 * stream must be 44.1kHz 16-bit stereo PCM with no RIFF WAVE or other
 * header.  The frame rate can be either an integer or a rational number
 * expressed as "numerator/denominator" (such as 30000/1001).
 *
 * To extract the raw video or audio stream from a PSP-format stream file,
 * run:
 *     streamux -dv movie.str >video.264
 * or:
 *     streamux -da movie.str >audio.pcm
 * to extract the video or audio stream, respectively.
 *
 * Note that running this program on Windows systems may create broken
 * output files because of CR/LF conversion on standard output.
 */

#include "tool-common.h"
#include "util.h"

#include <math.h>

static int mux(const char *video_file, const char *audio_file,
               const char *framerate_str);
static int demux(const char *movie_file, int audio);
static uint8_t *find_next_au(uint8_t *start, uint8_t *end);
static uint8_t *fix_au(uint8_t *au, int filepos, uint32_t *len_ptr,
                       int *width_ptr, int *height_ptr);
static int handle_sps(uint8_t *sps, int len, uint8_t *out,
                      int *width_ptr, int *height_ptr);
static int32_t getbits(const uint8_t **pptr, int *bitnum, int nbits);
static int32_t getvbits(const uint8_t **pptr, int *bitnum);
static int32_t putbits(uint8_t **pptr, int *bitnum, int32_t val, int nbits);
static int32_t putvbits(uint8_t **pptr, int *bitnum, int32_t val);

/*************************************************************************/
/*************************************************************************/

/**
 * main:  Program entry point.
 *
 * [Parameters]
 *     argc: Command line argument count.
 *     argv: Command line argument array.
 * [Return value]
 *     Zero on successful completion, nonzero if an error occurred.
 */
int main(int argc, char **argv)
{
    if (argc == 3 && strcmp(argv[1], "-dv") == 0) {
        return demux(argv[2], 0);
    } else if (argc == 3 && strcmp(argv[1], "-da") == 0) {
        return demux(argv[2], 1);
    } else if (argc == 4) {
        return mux(argv[1], argv[2], argv[3]);
    } else {
        fprintf(stderr,
                "Usage: %s video.264 audio.pcm framerate >movie.str\n"
                "   or: %s -dv movie.str >video.264\n"
                "   or: %s -da movie.str >audio.pcm\n",
                argv[0], argv[0], argv[0]);
        return 1;
    }
}

/*-----------------------------------------------------------------------*/

/**
 * mux:  Multiplex video and audio streams into a PSP movie stream and
 * write the movie stream to standard output.
 *
 * [Parameters]
 *     video_file: Pathname of video stream to multiplex.
 *     audio_file: Pathname of audio stream to multiplex.
 *     framerate_str: Frame rate to write to movie header, as a string.
 * [Return value]
 *     Zero on successful completion, nonzero if an error occurred.
 */
static int mux(const char *video_file, const char *audio_file,
               const char *framerate_str)
{
    /* Read in the input streams and parse the frame rate value. */
    uint32_t avc_len, pcm_len;
    uint8_t *avc_buffer = read_file(video_file, &avc_len);
    uint8_t *pcm_buffer = read_file(audio_file, &pcm_len);
    if (!avc_buffer || !pcm_buffer) {
        return 1;
    }
    uint8_t *avc_end = avc_buffer + avc_len;
    char *s = NULL;
    unsigned long fps_num = strtoul(framerate_str, &s, 10);
    unsigned long fps_den = 1;
    if (s && *s == '/') {
        fps_den = strtoul(s+1, &s, 10);
    }
    if (s && *s) {
        fprintf(stderr, "Invalid frame rate %s (must be integer or N/D)\n",
                framerate_str);
        return 1;
    }
    const double fps = (double)fps_num / (double)fps_den;

    /* Count the number of frames in the video stream. */
    int frames = 0;
    uint8_t *this_au = find_next_au(avc_buffer, avc_end);
    while (this_au) {
        frames++;
        this_au = find_next_au(this_au+1, avc_end);
    }

    /* Create and write out the stream file header. */
    uint8_t header[32];
    memset(header, 0, sizeof(header));
    header[ 0] = 'S';
    header[ 1] = 'T';
    header[ 2] = 'R';
    header[ 4] = sizeof(header);
    header[ 8] = frames>> 0 & 0xFF;
    header[ 9] = frames>> 8 & 0xFF;
    header[10] = frames>>16 & 0xFF;
    header[11] = frames>>24 & 0xFF;
    header[16] = fps_num>> 0 & 0xFF;
    header[17] = fps_num>> 8 & 0xFF;
    header[18] = fps_num>>16 & 0xFF;
    header[19] = fps_num>>24 & 0xFF;
    header[20] = fps_den>> 0 & 0xFF;
    header[21] = fps_den>> 8 & 0xFF;
    header[22] = fps_den>>16 & 0xFF;
    header[23] = fps_den>>24 & 0xFF;
    if (fwrite(header, 1, sizeof(header), stdout) != sizeof(header)) {
        perror("fwrite(stdout, header)");
        return 1;
    }
    uint8_t *framelist = malloc(8 * frames);
    if (!framelist) {
        fprintf(stderr, "No memory for frame list\n");
        return 1;
    }
    memset(framelist, 0, 8 * frames);
    if ((int)fwrite(framelist, 8, frames, stdout) != frames) {
        perror("fwrite(stdout, framelist)");
        return 1;
    }

    /* Write out video and audio, one video frame at a time. */
    uint32_t max_au = 0, max_pcm = 0;
    int width = 0, height = 0;
    frames = 0;
    uint32_t pcm_pos = 0;
    this_au = find_next_au(avc_buffer, avc_end);
    while (this_au) {
        uint8_t *next_au = find_next_au(this_au+1, avc_end);
        uint32_t au_len;
        if (next_au) {
            au_len = next_au - this_au;
        } else {
            au_len = avc_end - this_au;
        }
        this_au = fix_au(this_au, this_au - avc_buffer, &au_len,
                         &width, &height);
        uint32_t au_pad = ((au_len+3) & -4) - au_len;
        uint32_t next_pcm = (uint32_t)ceil((frames+1)/fps * 44100);
        uint32_t pcm_bytes = (next_pcm - pcm_pos) * 4;
        uint32_t pcm_pad = 0;
        uint8_t *pcm_tmpbuf = calloc(pcm_bytes, 1);
        if (!pcm_tmpbuf) {
            fprintf(stderr, "No memory for pcm_tmpbuf (%d)\n", pcm_bytes);
            return 1;
        }
        if (pcm_pos < pcm_len/4) {
            memcpy(pcm_tmpbuf, pcm_buffer + pcm_pos*4,
                   ubound(pcm_bytes, pcm_len - pcm_pos*4));
        }
        uint8_t frameheader[16];
        const uint32_t frameofs  = ftell(stdout);
        const uint32_t framesize = sizeof(frameheader)
                                   + au_len + au_pad + pcm_bytes + pcm_pad;
        framelist[8*frames+0] = frameofs >> 0 & 0xFF;
        framelist[8*frames+1] = frameofs >> 8 & 0xFF;
        framelist[8*frames+2] = frameofs >>16 & 0xFF;
        framelist[8*frames+3] = frameofs >>24 & 0xFF;
        framelist[8*frames+4] = framesize>> 0 & 0xFF;
        framelist[8*frames+5] = framesize>> 8 & 0xFF;
        framelist[8*frames+6] = framesize>>16 & 0xFF;
        framelist[8*frames+7] = framesize>>24 & 0xFF;
        frameheader[ 0] = au_len   >> 0 & 0xFF;
        frameheader[ 1] = au_len   >> 8 & 0xFF;
        frameheader[ 2] = au_len   >>16 & 0xFF;
        frameheader[ 3] = au_len   >>24 & 0xFF;
        frameheader[ 4] = au_pad   >> 0 & 0xFF;
        frameheader[ 5] = au_pad   >> 8 & 0xFF;
        frameheader[ 6] = au_pad   >>16 & 0xFF;
        frameheader[ 7] = au_pad   >>24 & 0xFF;
        frameheader[ 8] = pcm_bytes>> 0 & 0xFF;
        frameheader[ 9] = pcm_bytes>> 8 & 0xFF;
        frameheader[10] = pcm_bytes>>16 & 0xFF;
        frameheader[11] = pcm_bytes>>24 & 0xFF;
        frameheader[12] = pcm_pad  >> 0 & 0xFF;
        frameheader[13] = pcm_pad  >> 8 & 0xFF;
        frameheader[14] = pcm_pad  >>16 & 0xFF;
        frameheader[15] = pcm_pad  >>24 & 0xFF;
        if (fwrite(frameheader, 1, sizeof(frameheader), stdout)
                                                     != sizeof(frameheader)
         || fwrite(this_au, 1, au_len, stdout) != au_len
         || (au_pad > 0 && fwrite("\0\0\0", 1, au_pad, stdout) != au_pad)
         || fwrite(pcm_tmpbuf, 1, pcm_bytes, stdout) != pcm_bytes
         || (pcm_pad > 0 && fwrite("\0\0\0", 1, pcm_pad, stdout) != pcm_pad)
        ) {
            perror("fwrite(stdout)");
            return 1;
        }
        free(pcm_tmpbuf);
        if (au_len > max_au) {
            max_au = au_len;
        }
        if (pcm_bytes > max_pcm) {
            max_pcm = pcm_bytes;
        }
        frames++;
        pcm_pos = next_pcm;
        this_au = next_au;
    }

    /* Record the image size and the maximum video and audio frame sizes in
     * the file header. */
    char buf[8];
    if (fseek(stdout, 12, SEEK_SET) != 0) {
        perror("fseek(stdout, 12, SEEK_SET)");
        return 1;
    }
    buf[0] = width >>0 & 0xFF;
    buf[1] = width >>8 & 0xFF;
    buf[2] = height>>0 & 0xFF;
    buf[3] = height>>8 & 0xFF;
    if (fwrite(buf, 1, 4, stdout) != 4) {
        perror("fwrite(stdout, header_fixup_1)");
        return 1;
    }
    if (fseek(stdout, 24, SEEK_SET) != 0) {
        perror("fseek(stdout, 24, SEEK_SET)");
        return 1;
    }
    buf[0] = max_au >> 0 & 0xFF;
    buf[1] = max_au >> 8 & 0xFF;
    buf[2] = max_au >>16 & 0xFF;
    buf[3] = max_au >>24 & 0xFF;
    buf[4] = max_pcm>> 0 & 0xFF;
    buf[5] = max_pcm>> 8 & 0xFF;
    buf[6] = max_pcm>>16 & 0xFF;
    buf[7] = max_pcm>>24 & 0xFF;
    if (fwrite(buf, 1, 8, stdout) != 8) {
        perror("fwrite(stdout, header_fixup_2)");
        return 1;
    }

    /* Write the frame index to the header. */
    if ((int)fwrite(framelist, 8, frames, stdout) != frames) {
        perror("fwrite(stdout, framelist_fixup)");
        return 1;
    }

    /* Success! */
    return 0;
}

/*-----------------------------------------------------------------------*/

/**
 * demux:  Extract the video or audio stream from a PSP-format movie stream
 * and write it to standard output.
 *
 * [Parameters]
 *     movie_file: Pathname of movie file to demultiplex.
 *     audio: True to extract the audio stream, false to extract the video
 *         stream.
 * [Return value]
 *     Zero on successful completion, nonzero if an error occurred.
 */
static int demux(const char *movie_file, int audio)
{
    /* Read in the movie stream. */
    uint32_t movie_len;
    uint8_t *movie_buffer = read_file(movie_file, &movie_len);
    if (!movie_buffer) {
        return 1;
    }
    if (movie_len < 32) {
        fprintf(stderr, "%s: File too short\n", movie_file);
        return 1;
    }

    /* Parse the movie header. */
    if (memcmp(movie_buffer, "STR\0", 4) != 0) {
        fprintf(stderr, "%s: Invalid header signature\n", movie_file);
        return 1;
    }
    const uint32_t header_size = movie_buffer[4] <<  0
                               | movie_buffer[5] <<  8
                               | movie_buffer[6] << 16
                               | movie_buffer[7] << 24;
    const uint32_t num_frames = movie_buffer[ 8] <<  0
                              | movie_buffer[ 9] <<  8
                              | movie_buffer[10] << 16
                              | movie_buffer[11] << 24;
    if (header_size != 32) {
        fprintf(stderr, "%s: Invalid header size %u\n", movie_file,
                header_size);
        return 1;
    }
    if (num_frames == 0) {
        return 0;  // Nothing to output.
    }
    if (movie_len < 32 + num_frames*8) {
        fprintf(stderr, "%s: File too short for %u frames\n", movie_file,
                num_frames);
        return 1;
    }

    /* Parse the frame offsets. */
    uint32_t *frame_offsets = malloc(sizeof(*frame_offsets) * num_frames);
    if (!frame_offsets) {
        fprintf(stderr, "%s: No memory for frame offset list (%u frames)\n",
                movie_file, num_frames);
        return 1;
    }
    for (uint32_t i = 0; i < num_frames; i++) {
        frame_offsets[i] = movie_buffer[32+i*8+0] <<  0
                         | movie_buffer[32+i*8+1] <<  8
                         | movie_buffer[32+i*8+2] << 16
                         | movie_buffer[32+i*8+3] << 24;
    }

    /* Write out the audio or video data from each frame. */
    for (uint32_t i = 0; i < num_frames; i++) {
        if (movie_len < frame_offsets[i]+16) {
            fprintf(stderr, "%s: File truncated at frame %u\n", movie_file, i);
            return 1;
        }
        const uint8_t *ptr = &movie_buffer[frame_offsets[i]];
        uint32_t v_size = ptr[0]<<0 | ptr[1]<<8 | ptr[2]<<16 | ptr[3]<<24;
        uint32_t v_padding = ptr[4]<<0 | ptr[5]<<8 | ptr[6]<<16 | ptr[7]<<24;
        uint32_t a_size = ptr[8]<<0 | ptr[9]<<8 | ptr[10]<<16 | ptr[11]<<24;
        if (movie_len < frame_offsets[i]+16 + v_size + v_padding + a_size) {
            fprintf(stderr, "%s: File truncated in frame %u\n", movie_file, i);
            return 1;
        }
        const void *outptr;
        size_t outlen;
        if (audio) {
            outptr = &movie_buffer[frame_offsets[i]+16 + v_size + v_padding];
            outlen = a_size;
        } else {
            outptr = &movie_buffer[frame_offsets[i]+16];
            outlen = v_size;
        }
        if (fwrite(outptr, 1, outlen, stdout) != outlen) {
            perror("fwrite(stdout)");
            return 1;
        }
    }

    /* Success! */
    return 0;
}

/*-----------------------------------------------------------------------*/

/**
 * find_next_au:  Find the next H.264 AU (access unit) starting from the
 * given memory address.
 *
 * [Parameters]
 *     start: Memory address to search from.
 *     end: Last byte + 1 of the memory range to search.
 * [Return value]
 *     Pointer to the first AU found, or NULL if none was found.
 */
static uint8_t *find_next_au(uint8_t *start, uint8_t *end)
{
    while (start <= end-4) {
        if (memcmp(start, "\x00\x00\x00\x01\x09", 5) == 0) {
            return start;
        }
        start++;
    }
    return NULL;
}

/*-----------------------------------------------------------------------*/

/**
 * fix_au:  Fix an H.264 access unit so the PSP can decode it properly.
 *
 * [Parameters]
 *     au: Pointer to the AU.
 *     filepos: Position of the AU in the input file (for error messages).
 *     len_ptr: Pointer to AU length, in bytes; will be modified on return.
 *     width_ptr: Pointer to image width or zero if not yet known; may be
 *         modified on return.
 *     height_ptr: Pointer to image height or zero if not yet known; may be
 *         modified on return.
 * [Return value]
 *     Pointer to modified AU data.
 */
static uint8_t *fix_au(uint8_t *au, int filepos, uint32_t *len_ptr,
                       int *width_ptr, int *height_ptr)
{
    if (!au || !len_ptr) {
        fprintf(stderr, "fix_au(): bad parameters: %p %d %p\n", au, filepos,
                len_ptr);
        exit(1);
    }
    if (*len_ptr < 4) {
        fprintf(stderr, "fix_au(): AU at 0x%X too small: %d\n", filepos,
                *len_ptr);
        exit(1);
    }
    if (memcmp(au, "\0\0\0\1", 4) != 0) {
        fprintf(stderr, "fix_au(): AU at 0x%X does not begin with a start code"
                " (%02X%02X%02X)\n", filepos, au[0], au[1], au[2]);
        exit(1);
    }

    const int init_filepos = filepos;
    static int frames_since_I = -1;  // For pic_timing generation.
    static uint8_t aubuf[0x20000];
    int len = *len_ptr;
    int newlen = 0;
    int saw_pic_timing = 0;
    int pic_timing_bits = 0;

    while (len > 0) {
        /* Find the next NAL start code. */
        uint8_t *next = NULL;
        int i = 0;
        while (i < len && au[i] == 0) {
            i++;
        }
        for (; i <= len-4; i++) {
            if (au[i] == 0 && au[i+1] == 0 && au[i+2] < 2) {
                next = au+i;
                break;
            }
        }
        if (!next) {
            next = au + len;  // The current NAL extends to the end of the AU.
        }
        const int nal_len = next - au;
        int nal_start = 0;
        while (nal_start < nal_len && au[nal_start] == 0) {
            nal_start++;
        }
        nal_start++;
        uint8_t *nal = au + nal_start;
        /* Figure out whether we should copy this NAL. */
        int copy_nal = 1;
        if ((nal[0] & 0x1F) == 6 && nal[1] == 5) {
            /* The PSP can't handle user information (like the x264
             * parameter block). */
            copy_nal = 0;
        }
        /* Parse the SPS data and add a HRD if needed (for pic_timing). */
        if (pic_timing_bits == 0 && (nal[0] & 0x1F) == 7) {
            memcpy(aubuf+newlen, au, nal_start+1);
            int newsps_len = handle_sps(nal+1, nal_len-(nal_start+1),
                                        aubuf+newlen + nal_start+1,
                                        width_ptr, height_ptr);
            if (newsps_len) {
                newlen += nal_start+1 + newsps_len;
                copy_nal = 0;
            } else {
                fprintf(stderr, "Failed to process SPS at 0x%X\n",
                        filepos + (int)((nal+1) - au));
                exit(1);
            }
        }
        /* Insert a pic_timing SEI if needed. */
        if ((nal[0] & 0x1F) == 1 || (nal[0] & 0x1F) == 5) {
            frames_since_I++;
            if (!saw_pic_timing) {
                if (newlen+16 > (int)sizeof(aubuf)) {
                    fprintf(stderr, "AU at 0x%X too large!! (insert"
                            " pic_timing)\n", init_filepos);
                    exit(1);
                }
                aubuf[newlen++] = 0x00;
                aubuf[newlen++] = 0x00;
                aubuf[newlen++] = 0x00;
                aubuf[newlen++] = 0x01;
                aubuf[newlen++] = 0x06;
                aubuf[newlen++] = 0x01;
                aubuf[newlen++] = 0x08;
                if (frames_since_I > 63) {
                    fprintf(stderr, "Too many frames since I!! (at 0x%X)\n",
                            filepos);
                    exit(1);
                }
                aubuf[newlen++] = frames_since_I << 2;
                aubuf[newlen++] = 0x08;
                aubuf[newlen++] = 0x24;
                aubuf[newlen++] = 0x68;
                aubuf[newlen++] = 0x00;
                aubuf[newlen++] = 0x00;
                aubuf[newlen++] = 0x03;
                aubuf[newlen++] = 0x00;
                aubuf[newlen++] = 0x01;
                aubuf[newlen++] = 0x80;
            }
            if ((nal[0] & 0x1F) == 5) {
                frames_since_I = 0;
            }
        } else if ((nal[0] & 0x1F) == 6 && nal[1] == 1) {
            saw_pic_timing = 1;
        }
        /* Copy this NAL if it's needed. */
        if (copy_nal) {
            if (next - au > (int)sizeof(aubuf) - newlen) {
                fprintf(stderr, "AU at 0x%X too large!!\n", init_filepos);
                exit(1);
            }
            memcpy(aubuf + newlen, au, nal_len);
            newlen += nal_len;
        }
        /* Update the remaining length and proceed to the next NAL. */
        len -= nal_len;
        filepos += next - au;
        au = next;
    }

    /* Return the fixed AU. */
    *len_ptr = newlen;
    return aubuf;
}

/*-----------------------------------------------------------------------*/

/**
 * handle_sps:  Copy an SPS packet, modifying it as necessary.
 *
 * No length checks are performed on the output data, but the caller
 * allocates a large enough buffer that overrun should not occur.
 *
 * [Parameters]
 *     sps: Pointer to input SPS data (destroyed).
 *     len: Length of input SPS data, in bytes.
 *     out: Pointer to buffer in which to store output SPS data.
 *     width_ptr: Pointer to image width or zero if not yet known; may be
 *         modified on return.
 *     height_ptr: Pointer to image height or zero if not yet known; may be
 *         modified on return.
 * [Return value]
 *     Length of output SPS data, in bytes.
 */
static int handle_sps(uint8_t *sps, int len, uint8_t *out,
                      int *width_ptr, int *height_ptr)
{
    if (sps == NULL || len <= 0 || out == NULL) {
        fprintf(stderr, "handle_sps(): Invalid parameters: %p %d %p\n",
                sps, len, out);
        return 0;
    }

    const uint8_t *in_ptr = sps;
    uint8_t *out_ptr = out;
    int width, height;
    int in_bitnum = 0, out_bitnum = 0;
    int32_t val;

    /* Shortcuts for reading or writing values (using the variable "val"). */
    #define GETBITS(nbits)  (val = getbits(&in_ptr, &in_bitnum, (nbits)))
    #define GETVBITS()      (val = getvbits(&in_ptr, &in_bitnum))
    #define PUTBITS(val,nbits)  putbits(&out_ptr, &out_bitnum, (val), (nbits))
    #define PUTVBITS(val)       putvbits(&out_ptr, &out_bitnum, (val))
    /* Shortcuts for copying bits straight to the output buffer. */
    #define COPYBITS(nbits) PUTBITS(GETBITS((nbits)), (nbits))
    #define COPYVBITS()     PUTVBITS(GETVBITS())

    /* First scan through the data and remove emulation prevention bytes. */
    for (int i = 0; i <= len-3; i++) {
        if (sps[i] == 0 && sps[i+1] == 0 && sps[i+2] == 3) {
            memmove(&sps[i+2], &sps[i+3], len - (i+3));
            len--;
            i++;  // Advance 2 bytes instead of 1.
        }
    }

    /* Copy or insert SPS data to the output buffer. */

    /* profile_idc */
    if (COPYBITS(8) != 77) {
        fprintf(stderr, "SPS: bad profile_idc %d (should be 77)\n", val);
        return 0;
    }
    /* constraint_set{0,1,2}_flag, reserved_zero_5bits */
    if (COPYBITS(8) != 0x40) {
        fprintf(stderr, "SPS: bad constraints 0x%02X (should be 0x40)\n", val);
        return 0;
    }
    /* level_idc */
    if (COPYBITS(8) != 21) {
        fprintf(stderr, "SPS: bad level_idc %d (should be 21)\n", val);
        return 0;
    }
    /* seq_parameter_set_id */
    COPYVBITS();
    /* log2_max_frame_num_minus4 */
    COPYVBITS();
    /* pic_order_cnt_type */
    if (COPYVBITS() == 0) {
        /* log2_max_pic_order_cnt_lsb_minus4 */
        COPYVBITS();
    } else if (val == 1) {
        /* delta_pic_order_always_zero_flag */
        COPYBITS(1);
        /* offset_for_non_ref_pic */
        COPYVBITS();
        /* offset_for_top_to_bottom_field */
        COPYVBITS();
        /* num_ref_frames_in_pic_order_cnt_cycle */
        int n = COPYVBITS();
        int i;
        for (i = 0; i < n; i++) {
            /* offset_for_ref_frame[i] */
            COPYVBITS();
        }
    } else if (val != 2) {
        fprintf(stderr, "SPS: bad pic_order_cnt_type %d\n", val);
        return 0;
    }
    /* num_ref_frames */
    COPYVBITS();
    /* gaps_in_frame_num_value_allowed_flag */
    COPYBITS(1);
    /* pic_width_in_mbs_minus1 */
    width = (COPYVBITS() + 1) * 16;
    /* pic_height_in_map_units_minus1 */
    height = (COPYVBITS() + 1) * 16;
    /* frame_mbs_only_flag */
    if (!COPYBITS(1)) {
        /* mb_adaptive_frame_field_flag */
        COPYBITS(1);
    }
    /* direct_8x8_inference_flag */
    COPYBITS(1);
    /* frame_cropping_flag */
    if (COPYBITS(1)) {
        /* frame_crop_left_offset */
        width -= COPYVBITS() * 2;
        /* frame_crop_right_offset */
        width -= COPYVBITS() * 2;
        /* frame_crop_top_offset */
        height -= COPYVBITS() * 2;
        /* frame_crop_bottom_offset */
        height -= COPYVBITS() * 2;
    }
    /* vui_parameters_present_flag */
    if (!COPYBITS(1)) {
        fprintf(stderr, "SPS: vui parameters missing\n");
        return 0;
    }
    /* aspect_ratio_info_present_flag */
    if (COPYBITS(1)) {
        /* aspect_ratio_idc */
        if (COPYBITS(8) != 1) {
            /* The value doesn't really matter, but check it anyway. */
            fprintf(stderr, "SPS: bad aspect_ratio_idc %d (should be 1)\n",
                    val);
            return 0;
        }
    }
    /* overscan_info_present_flag */
    if (COPYBITS(1)) {
        /* overscan_appropriate_flag */
        COPYBITS(1);
    }
    /* video_signal_type_present_flag */
    if (COPYBITS(1)) {
        /* video_format */
        COPYBITS(3);
        /* video_full_range_flag */
        COPYBITS(1);
        /* colour_description_present_flag */
        if (COPYBITS(1)) {
            /* colour_primaries */
            COPYBITS(8);
            /* transfer_characteristics */
            COPYBITS(8);
            /* matrix_coefficients */
            COPYBITS(8);
        }
    }
    /* chroma_loc_info_present_flag */
    if (COPYBITS(1)) {
        /* chroma_sample_loc_type_top_field */
        COPYVBITS();
        /* chroma_sample_loc_type_bottom_field */
        COPYVBITS();
    }
    /* timing_info_present_flag */
    if (COPYBITS(1)) {
        /* num_units_in_tick */
        COPYBITS(32);
        /* time_scale */
        COPYBITS(32);
        /* fixed_frame_rate_flag */
        COPYBITS(1);
    }
    int saw_hrd = 0;
    /* nal_hrd_parameters_present_flag */
    if (GETBITS(1)) {
        saw_hrd = 1;
        PUTBITS(1, 1);
        /* cpb_cnt_minus1 */
        int cpb_cnt_minus1 = COPYVBITS();
        /* bit_rate_scale */
        COPYBITS(4);
        /* cpb_size_scale */
        COPYBITS(4);
        int i;
        for (i = 0; i <= cpb_cnt_minus1; i++) {
            /* bit_rate_value_minus1[i] */
            COPYVBITS();
            /* cpb_size_value_minus1[i] */
            COPYVBITS();
            /* cbr_flag[i] */
            COPYBITS(1);
        }
        /* initial_cpb_removal_delay_length_minus1 */
        COPYBITS(5);
        /* cpb_removal_delay_length_minus1 */
        COPYBITS(5);
        /* dpb_output_delay_length_minus1 */
        COPYBITS(5);
        /* time_offset_length */
        COPYBITS(5);
    } else {  // nal_hrd_parameters_present_flag == 0
        PUTBITS(1, 1);
        /* cpb_cnt_minus1 */
        PUTVBITS(0);
        /* bit_rate_scale */
        PUTBITS(1, 4);
        /* cpb_size_scale */
        PUTBITS(3, 4);
        /* bit_rate_value_minus1[0] */
        PUTVBITS(15624);
        /* cpb_size_value_minus1[0] */
        PUTVBITS(15624);
        /* cbr_flag[0] */
        PUTBITS(0, 1);
        /* initial_cpb_removal_delay_length_minus1 */
        PUTBITS(17, 5);
        /* cpb_removal_delay_length_minus1 */
        PUTBITS(6, 5);
        /* dpb_output_delay_length_minus1 */
        PUTBITS(6, 5);
        /* time_offset_length */
        PUTBITS(24, 5);
    }  // nal_hrd_parameters
    /* vcl_hrd_parameters_present_flag */
    if (GETBITS(1)) {
        saw_hrd = 1;
        PUTBITS(1, 1);
        /* cpb_cnt_minus1 */
        int cpb_cnt_minus1 = COPYVBITS();
        /* bit_rate_scale */
        COPYBITS(4);
        /* cpb_size_scale */
        COPYBITS(4);
        int i;
        for (i = 0; i <= cpb_cnt_minus1; i++) {
            /* bit_rate_value_minus1[i] */
            COPYVBITS();
            /* cpb_size_value_minus1[i] */
            COPYVBITS();
            /* cbr_flag[i] */
            COPYBITS(1);
        }
        /* initial_cpb_removal_delay_length_minus1 */
        COPYBITS(5);
        /* cpb_removal_delay_length_minus1 */
        COPYBITS(5);
        /* dpb_output_delay_length_minus1 */
        COPYBITS(5);
        /* time_offset_length */
        COPYBITS(5);
    } else {  // vcl_hrd_parameters_present_flag == 0
        PUTBITS(1, 1);
        /* cpb_cnt_minus1 */
        PUTVBITS(0);
        /* bit_rate_scale */
        PUTBITS(1, 4);
        /* cpb_size_scale */
        PUTBITS(3, 4);
        /* bit_rate_value_minus1[0] */
        PUTVBITS(15624);
        /* cpb_size_value_minus1[0] */
        PUTVBITS(15624);
        /* cbr_flag[0] */
        PUTBITS(0, 1);
        /* initial_cpb_removal_delay_length_minus1 */
        PUTBITS(17, 5);
        /* cpb_removal_delay_length_minus1 */
        PUTBITS(6, 5);
        /* dpb_output_delay_length_minus1 */
        PUTBITS(6, 5);
        /* time_offset_length */
        PUTBITS(24, 5);
    }  // vcl_hrd_parameters
    /* low_delay_hrd_flag */
    if (saw_hrd) {
        COPYBITS(1);
    } else {
        PUTBITS(0, 1);
    }
    /* pic_struct_present_flag */
    COPYBITS(1);
    /* bitstream_restriction_flag */
    if (COPYBITS(1)) {
        /* motion_vectors_over_pic_boundaries_flag */
        COPYBITS(1);
        /* max_bytes_per_pic_denom */
        COPYVBITS();
        /* max_bytes_per_mb_denom */
        COPYVBITS();
        /* log2_max_mv_length_horizontal */
        COPYVBITS();
        /* log2_max_mv_length_vertical */
        COPYVBITS();
        /* num_reorder_frames */
        COPYVBITS();
        /* max_dec_frame_buffering */
        COPYVBITS();
    }
    /* rbsp_trailing_bits */
    if (!COPYBITS(1)) {
        fprintf(stderr, "SPS: stop bit not found at byte 0x%X bit %d\n",
                (int)(in_ptr - sps) - (in_bitnum==0 ? 1 : 0),
                (in_bitnum-1) & 7);
        return 0;
    }
    while (in_bitnum != 0) {
        if (GETBITS(1)) {
            fprintf(stderr, "SPS: trailing bit not zero at byte 0x%X bit %d\n",
                    (int)(in_ptr - sps) - (in_bitnum==0 ? 1 : 0),
                    (in_bitnum-1) & 7);
            return 0;
        }
    }
    while (out_bitnum != 0) {
        PUTBITS(0, 1);
    }

    if (in_ptr - sps != len) {
        fprintf(stderr, "SPS parse error: only read %d of %d bytes\n",
                (int)(in_ptr - sps), len);
        return 0;
    }

    /* Scan through the output buffer and insert emulation prevention bytes
     * where necessary. */
    int out_len = out_ptr - out;
    for (int i = 0; i <= out_len-3; i++) {
        if (out[i] == 0 && out[i+1] == 0 && out[i+2] < 4) {
            memmove(&out[i+3], &out[i+2], out_len - (i+2));
            out[i+2] = 3;
            out_len++;
            i += 2;  // Advance past the inserted emulation byte.
        }
    }

    /* Update or check the width/height return values. */
    if (width <= 0 || height <= 0) {
        fprintf(stderr, "SPS: invalid cropped width/height (%dx%d)",
                width, height);
        return 0;
    }
    if (!*width_ptr) {
        *width_ptr = width;
        *height_ptr = height;
    } else if (width != *width_ptr || height != *height_ptr) {
        fprintf(stderr, "SPS: image size change (%dx%d -> %dx%d) not"
                " allowed\n", *width_ptr, *height_ptr, width, height);
        return 0;
    }

    return out_len;

    #undef GETBITS
    #undef GETVBITS
    #undef PUTBITS
    #undef PUTVBITS
    #undef COPYBITS
    #undef COPYVBITS
}

/*-----------------------------------------------------------------------*/

/**
 * getbits, getvbits:  Read bits from an SPS packet bitstream.  getbits()
 * reads a fixed-length value, while getvbits() reads a variable-length
 * value.
 *
 * [Parameters]
 *     pptr: Pointer to bitstream pointer.
 *     bitnum: Pointer to current bit index (must be initialized to zero).
 *     nbits: Number of bits to read.
 * [Return value]
 *     Value read from bitstream.
 */
static int32_t getbits(const uint8_t **pptr, int *bitnum, int nbits)
{
    int32_t val = 0;
    while (nbits > 0) {
        val <<= 1;
        val |= (*(*pptr) >> (7 - (*bitnum))) & 1;
        (*bitnum)++;
        if ((*bitnum) >= 8) {
            (*pptr)++;
            (*bitnum) = 0;
        }
        nbits--;
    }
    return val;
}

static int32_t getvbits(const uint8_t **pptr, int *bitnum)
{
    int nbits = 0;
    while (!getbits(pptr, bitnum, 1)) {
        nbits++;
    }
    return ((1<<nbits)-1) + getbits(pptr, bitnum, nbits);
}

/*-----------------------------------------------------------------------*/

/**
 * putbits, putvbits:  Write bits to an SPS packet bitstream.  putbits()
 * writes a fixed-length value, while putvbits() writes a variable-length
 * value.
 *
 * [Parameters]
 *     pptr: Pointer to bitstream pointer.
 *     bitnum: Pointer to current bit index (must be initialized to zero).
 *     val: Value to write.
 *     nbits: Number of bits to write.
 * [Return value]
 *     val
 */
static int32_t putbits(uint8_t **pptr, int *bitnum, int32_t val, int nbits)
{
    while (nbits > 0) {
        nbits--;
        if ((*bitnum) == 0) {
            *(*pptr) = 0;
        }
        *(*pptr) |= ((val >> nbits) & 1) << (7 - (*bitnum));
        (*bitnum)++;
        if ((*bitnum) >= 8) {
            (*pptr)++;
            (*bitnum) = 0;
        }
    }
    return val;
}

static int32_t putvbits(uint8_t **pptr, int *bitnum, int32_t val)
{
    if (val < 0) {
        fprintf(stderr, "putvbits(): val<0 (%d) not supported\n", val);
        exit(1);
    }
    int nbits = 0;
    int32_t maxval = 0;
    while (maxval < val) {
        putbits(pptr, bitnum, 0, 1);
        nbits++;
        maxval = ((1<<nbits)-1) << 1;
    }
    putbits(pptr, bitnum, 1, 1);
    putbits(pptr, bitnum, val - ((1<<nbits)-1), nbits);
    return val;
}

/*************************************************************************/
/************************* tools/streamux.c END **************************/
/*************************************************************************/
