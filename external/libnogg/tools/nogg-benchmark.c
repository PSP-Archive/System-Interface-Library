/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2019 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

/*
 * This file implements a simple benchmark program for comparing the
 * output and performance of libnogg against the reference libogg/libvorbis
 * library (and optionally the integer-only libvorbis implementation,
 * Tremor).  See the usage() function (or run the program with the --help
 * option) for details of the command-line interface.
 */

#ifdef __linux__
# define _POSIX_C_SOURCE 200809L
#endif

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__)
# include <mach/mach.h>
# include <mach/mach_time.h>
#elif defined(_WIN32)
# include <windows.h>
#elif defined(__linux__)
# include <time.h>
# include <sys/time.h>
#else
# include <time.h>
#endif


#include <nogg.h>

#include <ogg/ogg.h>
#define OV_EXCLUDE_STATIC_CALLBACKS
#include <vorbis/vorbisfile.h>

#ifdef HAVE_TREMOR
# undef _vorbis_codec_h_
# undef _OV_FILE_H_
# include "tremor-wrapper.h"
# include "ivorbisfile.h"
# define TREMOR_UNDEF  // Undo the renames so we can access libvorbis
# include "tremor-wrapper.h"
# undef TREMOR_UNDEF
#endif


#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
# define ALIGN(alignment)  _Alignas(alignment)
#elif defined(__GNUC__) || defined(__clang__)
# define ALIGN(alignment)  __attribute__((aligned(alignment)))
#else
# define ALIGN(alignment)  /*nothing*/
#endif

#define lenof(array)  ((int)(sizeof((array)) / sizeof(*(array))))

/*************************************************************************/
/********************** libvorbis/Tremor callbacks ***********************/
/*************************************************************************/

static unsigned int nogg_options;  // Set by main().

struct ogg_buffer {
    const char *data;
    size_t size;
    size_t pos;
};

static size_t ogg_read(void *ptr, size_t size, size_t nmemb, void *datasource)
{
    struct ogg_buffer *buffer = (struct ogg_buffer *)datasource;

    size_t bytes = size * nmemb;
    if (bytes > buffer->size - buffer->pos) {
        bytes = buffer->size - buffer->pos;
    }
    memcpy(ptr, buffer->data + buffer->pos, bytes);
    buffer->pos += bytes;
    errno = 0;
    return bytes / size;
}

static int ogg_seek(void *datasource, ogg_int64_t offset, int whence)
{
    struct ogg_buffer *buffer = (struct ogg_buffer *)datasource;

    switch (whence) {
        case 0: buffer->pos = (size_t)offset;                break;
        case 1: buffer->pos = (size_t)offset + buffer->pos;  break;
        case 2: buffer->pos = (size_t)offset + buffer->size; break;
    }
    if (buffer->pos > buffer->size) {
        buffer->pos = buffer->size;
    }
    return 0;
}

static long ogg_tell(void *datasource)
{
    struct ogg_buffer *buffer = (struct ogg_buffer *)datasource;
    return (long)buffer->pos;
}

static const ov_callbacks ogg_callbacks = {
    .read_func = ogg_read,
    .seek_func = ogg_seek,
    .tell_func = ogg_tell,
};

#ifdef HAVE_TREMOR
static const tremor_ov_callbacks tremor_ogg_callbacks = {
    .read_func = ogg_read,
    .seek_func = ogg_seek,
    .tell_func = ogg_tell,
};
#endif

/*************************************************************************/
/********************** Decoder library interfaces ***********************/
/*************************************************************************/

/* Constants identifying decoder libraries for these functions. */
typedef enum DecoderLibrary {
    LIBNOGG,
    LIBVORBIS,
    TREMOR,
} DecoderLibrary;

/* Structure encapsulating a library-specific decoder handle. */
typedef struct DecoderHandle {
    DecoderLibrary library;
    void *handle;
    bool error;
} DecoderHandle;

/*-----------------------------------------------------------------------*/

/**
 * decoder_open:  Create a decoder handle for decoding the given stream
 * using the specified library.
 *
 * [Parameters]
 *     library: Library to use.
 *     data: Stream data.
 *     size: Stream size, in bytes.
 * [Return value]
 *     Decoder handle, or NULL on error.
 */
static DecoderHandle *decoder_open(DecoderLibrary library,
                                   const void *data, size_t size)
{
    DecoderHandle *decoder = malloc(sizeof(*decoder));
    if (!decoder) {
        fprintf(stderr, "Out of memory (DecoderHandle)\n");
        return NULL;
    }
    decoder->library = library;
    decoder->handle = NULL;
    decoder->error = false;

    switch (library) {

      case LIBNOGG: {
        vorbis_error_t error;
        decoder->handle = vorbis_open_buffer(data, size, nogg_options, &error);
        if (!decoder->handle) {
            fprintf(stderr, "Failed to create libnogg decoder handle: %d\n",
                    error);
        }
        break;
      }  // case LIBNOGG

      case LIBVORBIS: {
        OggVorbis_File *vf = malloc(sizeof(*vf) + sizeof(struct ogg_buffer));
        if (!vf) {
            fprintf(stderr, "Out of memory (OggVorbis_File)\n");
            break;
        }
        /* We cast through void * to suppress a cast-align warning. */
        struct ogg_buffer *buffer =
            (struct ogg_buffer *)(void *)((char *)vf + sizeof(*vf));
        buffer->data = data;
        buffer->size = size;
        buffer->pos = 0;
        if (ov_open_callbacks(buffer, vf, NULL, 0, ogg_callbacks) != 0) {
            fprintf(stderr, "ov_open() failed\n");
            free(vf);
            break;
        }
        decoder->handle = vf;
        break;
      }  // case LIBVORBIS

      case TREMOR: {
#if !defined(HAVE_TREMOR)
        fprintf(stderr, "Tremor support not available in this build\n");
#else
        tremor_OggVorbis_File *vf =
            malloc(sizeof(*vf) + sizeof(struct ogg_buffer));
        if (!vf) {
            fprintf(stderr, "Out of memory (tremor_OggVorbis_File)\n");
            break;
        }
        struct ogg_buffer *buffer =
            (struct ogg_buffer *)(void *)((char *)vf + sizeof(*vf));
        buffer->data = data;
        buffer->size = size;
        buffer->pos = 0;
        if (tremor_ov_open_callbacks(buffer, vf, NULL, 0,
                                     tremor_ogg_callbacks) != 0) {
            fprintf(stderr, "tremor_ov_open() failed\n");
            free(vf);
            break;
        }
        decoder->handle = vf;
#endif  // HAVE_TREMOR
        break;
      }  // case TREMOR

      default:
        fprintf(stderr, "Invalid library ID: %d\n", library);
        break;

    }  // switch (library)

    if (!decoder->handle) {
        free(decoder);
        return NULL;
    }

    return decoder;
}

/*-----------------------------------------------------------------------*/

/**
 * decoder_rewind:  Reset the read position for the given decoder handle
 * to the beginning of the stream.
 *
 * [Parameters]
 *     decoder: Decoder handle.
 * [Return value]
 *     True on success, false on error.
 */
static bool decoder_rewind(DecoderHandle *decoder)
{
    switch (decoder->library) {
      case LIBNOGG:
        return vorbis_seek(decoder->handle, 0);

      case LIBVORBIS:
        return ov_pcm_seek(decoder->handle, 0) == 0;

      case TREMOR:
#ifdef HAVE_TREMOR
        return tremor_ov_pcm_seek(decoder->handle, 0) == 0;
#else
        break;
#endif
    }

    return false;
}

/*-----------------------------------------------------------------------*/

/**
 * decoder_seek_to_end:  Set the read position for the given decoder handle
 * to the end of the stream.
 *
 * [Parameters]
 *     decoder: Decoder handle.
 * [Return value]
 *     True on success, false on error.
 */
static bool decoder_seek_to_end(DecoderHandle *decoder)
{
    int64_t length;

    switch (decoder->library) {
      case LIBNOGG:
        length = vorbis_length(decoder->handle);
        if (length < 0) {
            return false;
        }
        return vorbis_seek(decoder->handle, length);

      case LIBVORBIS:
        length = ov_pcm_total(decoder->handle, -1);
        if (length < 0) {
            return false;
        }
        return ov_pcm_seek(decoder->handle, length) == 0;

      case TREMOR:
#ifdef HAVE_TREMOR
        length = ov_pcm_total(decoder->handle, -1);
        if (length < 0) {
            return false;
        }
        return tremor_ov_pcm_seek(decoder->handle, length) == 0;
#else
        break;
#endif
    }

    return false;
}

/*-----------------------------------------------------------------------*/

/**
 * decoder_read:  Read PCM samples from a decoder handle.  The number of
 * samples read will always be a multiple of the number of channels in the
 * stream, and thus a return value of less than the requested number of
 * samples does not necessarily indicate end-of-stream.
 *
 * [Parameters]
 *     decoder: Decoder handle to read from.
 *     buf: Buffer into which to read samples.
 *     count: Maximum number of samples to read.
 * [Return value]
 *     Number of samples returned, or 0 on end-of-stream or error.
 */
static long decoder_read(DecoderHandle *decoder, int16_t *buf, long count)
{
    long result = 0;

    switch (decoder->library) {

      case LIBNOGG: {
        const int channels = vorbis_channels(decoder->handle);
        while (count >= channels) {
            long this_result;
            vorbis_error_t error;
            do {
                this_result = (vorbis_read_int16(decoder->handle, buf,
                                                 count/channels, &error)
                               * channels);
            } while (this_result == 0
                     && error == VORBIS_ERROR_DECODE_RECOVERED);
            decoder->error = (error != 0 && error != VORBIS_ERROR_STREAM_END
                              && error != VORBIS_ERROR_DECODE_RECOVERED);
            if (this_result == 0 || decoder->error) {
                break;
            }
            result += this_result;
            buf += this_result;
            count -= this_result;
        }
        break;
      }

      case LIBVORBIS: {
        vorbis_info *info = ov_info(decoder->handle, -1);
        const int channels = info->channels;
        while (count >= channels) {
            long this_result;
            do {
                int bitstream_unused;
                this_result = ov_read(
                    decoder->handle, (char *)buf,
                    (int)(count/channels) * (2*channels), /*bigendianp*/ 0,
                    /*word*/ 2, /*sgned*/ 1, &bitstream_unused);
            } while (this_result == OV_HOLE);
            decoder->error = (this_result < 0);
            if (this_result <= 0) {
                break;
            }
            this_result /= 2;
            result += this_result;
            buf += this_result;
            count -= this_result;
        }
        break;
      }

      case TREMOR: {
#ifdef HAVE_TREMOR
        tremor_vorbis_info *info = tremor_ov_info(decoder->handle, -1);
        const int channels = info->channels;
        while (count >= channels) {
            long this_result;
            do {
                int bitstream_unused;
                this_result = tremor_ov_read(
                    decoder->handle, (char *)buf,
                    (int)(count/channels) * (2*channels), &bitstream_unused);
            } while (this_result == OV_HOLE);
            decoder->error = (this_result < 0);
            if (this_result <= 0) {
                break;
            }
            this_result /= 2;
            result += this_result;
            buf += this_result;
            count -= this_result;
        }
#endif
        break;
      }

    }

    return result;
}

/*-----------------------------------------------------------------------*/

/**
 * decoder_status:  Return the current decode status of the decoder.
 *
 * [Parameters]
 *     decoder: Decoder handle.
 * [Return value]
 *     False if a fatal decoding error was detected, true otherwise.
 */
static bool decoder_status(DecoderHandle *decoder)
{
    return !decoder->error;
}

/*-----------------------------------------------------------------------*/

/**
 * decoder_close:  Close a decoder handle.
 *
 * [Parameters]
 *     decoder: Decoder handle to close.  May be NULL.
 */
static void decoder_close(DecoderHandle *decoder)
{
    if (!decoder) {
        return;
    }

    switch (decoder->library) {

      case LIBNOGG:
        vorbis_close(decoder->handle);
        break;

      case LIBVORBIS:
        ov_clear(decoder->handle);
        free(decoder->handle);
        break;

      case TREMOR:
#ifdef HAVE_TREMOR
        tremor_ov_clear(decoder->handle);
        free(decoder->handle);
#endif
        break;

    }
}

/*************************************************************************/
/************************ Other helper functions *************************/
/*************************************************************************/

/**
 * usage:  Print a usage message to standard error.
 *
 * [Parameters]
 *     argv0: The value of argv[0] (the program's name).
 */
static void usage(const char *argv0)
{
    fprintf(stderr,
            "Usage: %s [OPTION]... INPUT-FILE\n"
            "Decode INPUT-FILE using different Ogg Vorbis decoder libraries and\n"
            "report the decoding speed of each library as well as whether any\n"
            "significant differences (differences of more than +/-4 in decoded\n"
            "16-bit PCM samples, or differences in the number of samples output)\n"
            "detected during decoding.\n"
            "\n"
            "The input file will be read into and decoded from memory, to minimize\n"
            "the impact of I/O operations on the result.\n"
            "\n"
            "This build supports: libnogg, libvorbis"
#ifdef HAVE_TREMOR
                                                   ", Tremor"
#endif
                                                           "\n"
            "\n"
            "Options:\n"
            "   -h, --help   Display this text and exit.\n"
            "   -i           Time initialization instead of decoding.\n"
            "   -l           Lax conformance: allow junk data between Ogg pages.\n"
            "   -n COUNT     Decode the stream COUNT times per library (default 10).\n"
            "   -s           Time seek-to-end instead of decoding.\n"
            "   -t           Time libnogg only (not other libraries).\n"
            "   --version    Display the program's version and exit.\n",
            argv0);
}

/*-----------------------------------------------------------------------*/

/**
 * time_now:  Return a timestamp representing the current time.  The value
 * of the timestamp is only meaningful when compared with other timestamps
 * returned by this function.  Note that timestamps may wrap around.
 *
 * [Return value]
 *     Current timestamp, in units specified by time_unit().
 */
static uint64_t time_now(void)
{
#if defined(__APPLE__)
    return mach_absolute_time();
#elif defined(_WIN32)
    LARGE_INTEGER now_buf;
    QueryPerformanceCounter(&now_buf);
    return now_buf.QuadPart;
#elif defined(__linux__)
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) == 0
     || clock_gettime(CLOCK_MONOTONIC, &ts) == 0
     || clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        return (uint64_t)ts.tv_sec * UINT64_C(1000000000)
             + (uint64_t)ts.tv_nsec;
    } else {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return (uint64_t)tv.tv_sec * UINT64_C(1000000000)
             + (uint64_t)tv.tv_usec * 1000;
    }
#else
    return time(NULL);
#endif
}

/*-----------------------------------------------------------------------*/

/**
 * time_unit:  Return the unit of time represented by a difference of 1 in
 * timestamps returned by time_now().  Multiplying a difference of two
 * timestamps by this value will give a time interval in seconds.
 *
 * [Return value]
 *     Unit of time_now() timestamps, in seconds.
 */
static double time_unit(void)
{
#if defined(__APPLE__)
    mach_timebase_info_data_t timebase_info;
    mach_timebase_info(&timebase_info);
    return timebase_info.numer / (1.0e9 * timebase_info.denom);
#elif defined(_WIN32)
    LARGE_INTEGER frequency_buf;
    QueryPerformanceFrequency(&frequency_buf);
    return 1.0 / frequency_buf.QuadPart;
#elif defined(__linux__)
    return 1.0e-9;
#else
    return 1.0;
#endif
}

/*************************************************************************/
/***************************** Main routine ******************************/
/*************************************************************************/

int main(int argc, char **argv)
{
    /* List of available libraries.  We also make room for a decoder handle
     * for each library for convenience when comparing outputs. */
    struct {
        const char * const name;
        const DecoderLibrary library;
        DecoderHandle *decoder;
        /* Have we warned about a mismatch yet?  (Not used for libvorbis,
         * which is the base for comparison, and libnogg, for which we
         * error out of the program.) */
        bool warned;
    } libraries[] = {
        {.name = "libvorbis", .library = LIBVORBIS, .decoder = NULL,
         .warned = false},
#ifdef HAVE_TREMOR
        {.name = "Tremor", .library = TREMOR, .decoder = NULL,
         .warned = false},
#endif
        {.name = "libnogg", .library = LIBNOGG, .decoder = NULL,
         .warned = false},
    };

    /* Number of decode iterations for timing. */
    int decode_iterations = 10;
    /* Time initialization instead of decoding? */
    bool time_init = false;
    /* Time seek-to-end instead of decoding? */
    bool time_seek = false;
    /* Allow junk between Ogg pages? */
    bool lax_conformance = false;
    /* Time libnogg only? */
    bool time_libnogg = false;
    /* Pathname of the input file. */
    const char *input_path = NULL;

    /*
     * Parse command-line arguments.
     */
    bool in_options = true;  // Flag for detecting the "--" option terminator.
    for (int argi = 1; argi < argc; argi++) {
        if (in_options && argv[argi][0] == '-') {
            if (strcmp(argv[argi], "-h") == 0
             || strcmp(argv[argi], "--help") == 0) {
                usage(argv[0]);
                return 0;

            } else if (strcmp(argv[argi], "--version") == 0) {
                printf("nogg-benchmark %s (using libnogg %s)\n",
                       VERSION, nogg_version());
                return 0;

            } else if (argv[argi][1] == '-') {
                if (!argv[argi][2]) {
                    in_options = false;
                } else {
                    /* We don't support double-dash arguments, but we
                     * parse them anyway so we can display a sensible error
                     * message. */
                    const int arglen = (int)strcspn(argv[argi], "=");
                    fprintf(stderr, "%s: unrecognized option \"%.*s\"\n",
                            argv[0], arglen, argv[argi]);
                    goto try_help;
                }

            } else if (argv[argi][1] == 'i') {
                time_init = true;
                time_seek = false;
                if (argv[argi][2]) {
                    /* Handle things like "-in100". */
                    memmove(&argv[argi][1], &argv[argi][2],
                            strlen(&argv[argi][2])+1);
                    argi--;
                }

            } else if (argv[argi][1] == 'l') {
                lax_conformance = true;
                if (argv[argi][2]) {
                    memmove(&argv[argi][1], &argv[argi][2],
                            strlen(&argv[argi][2])+1);
                    argi--;
                }

            } else if (argv[argi][1] == 'n') {
                const char option = argv[argi][1];
                const char *value;
                if (argv[argi][2]) {
                    value = &argv[argi][2];
                } else {
                    argi++;
                    if (argi >= argc) {
                        fprintf(stderr, "%s: option -%c requires a value\n",
                                argv[0], option);
                        goto try_help;
                    }
                    value = argv[argi];
                }
                decode_iterations = (int)strtol(value, (char **)&value, 10);
                if (decode_iterations < 0 || *value != '\0') {
                    fprintf(stderr, "%s: option -%c requires a nonnegative"
                            " integer value\n", argv[0], option);
                    goto try_help;
                }

            } else if (argv[argi][1] == 's') {
                time_seek = true;
                time_init = false;
                if (argv[argi][2]) {
                    memmove(&argv[argi][1], &argv[argi][2],
                            strlen(&argv[argi][2])+1);
                    argi--;
                }

            } else if (argv[argi][1] == 't') {
                time_libnogg = true;
                if (argv[argi][2]) {
                    memmove(&argv[argi][1], &argv[argi][2],
                            strlen(&argv[argi][2])+1);
                    argi--;
                }

            } else {
                fprintf(stderr, "%s: unrecognized option \"-%c\"\n",
                        argv[0], argv[argi][1]);
                goto try_help;

            }

        } else {  /* Non-option argument. */
            if (input_path) {
                fprintf(stderr, "%s: too many input files\n", argv[0]);
              try_help:
                fprintf(stderr, "Try \"%s --help\" for more information.\n",
                        argv[0]);
                return 2;
            }
            input_path = argv[argi];
        }
    }

    if (!input_path) {
        fprintf(stderr, "%s: input file missing\n", argv[0]);
        goto try_help;
    }

    nogg_options = ((lax_conformance ? VORBIS_OPTION_SCAN_FOR_NEXT_PAGE : 0)
                    | VORBIS_OPTION_READ_INT16_ONLY);

    /*
     * Read the stream into memory.
     */
    FILE *f;
    if (strcmp(input_path, "-") == 0) {
        f = stdin;
    } else {
        f = fopen(input_path, "rb");
        if (!f) {
            perror(input_path);
            return 1;
        }
    }
    char *file_data = NULL;
    size_t file_size = 0;
    for (;;) {
        const uint32_t expand_unit = 65536;
        char *new_file_data = realloc(file_data, file_size + expand_unit);
        if (!new_file_data) {
            fprintf(stderr, "Out of memory\n");
            free(file_data);
            fclose(f);
            return 1;
        }
        file_data = new_file_data;
        const size_t bytes_read =
            fread(file_data + file_size, 1, expand_unit, f);
        if (bytes_read == 0) {
            break;
        }
        file_size += bytes_read;
    }
    fclose(f);

    bool success = true;

    /*
     * Decode the stream once with each library and verify that we get the
     * same output (ignoring rounding and similar trivial differences).
     * Also save the stream length so we can create a properly-sized buffer
     * later.
     */
    long stream_len = 0;
    printf("Comparing library outputs... ");
    fflush(stdout);
    for (int i = 0; i < lenof(libraries); i++) {
        libraries[i].decoder = decoder_open(libraries[i].library,
                                            file_data, file_size);
        if (!libraries[i].decoder) {
            printf("ERROR: Failed to create decoder handle!\n");
            success = false;
            break;
        }
    }
    while (success) {
        static ALIGN(64) int16_t buf1[4096], buf2[4096];
        const long chunk_size =
            decoder_read(libraries[0].decoder, buf1, lenof(buf1));
        if (!decoder_status(libraries[0].decoder)) {
            printf("ERROR: %s reported a decoding error.  The stream may be"
                   " corrupt.\n", libraries[0].name);
            success = false;
            break;
        }
        for (int i = 1; i < lenof(libraries); i++) {
            const long this_chunk =
                decoder_read(libraries[i].decoder, buf2, lenof(buf2));
            if (!decoder_status(libraries[i].decoder)) {
                printf("ERROR: %s reported a decoding error.  The stream may"
                       " be corrupt.\n", libraries[i].name);
                success = false;
                break;
            } else if (this_chunk != chunk_size) {
                printf("ERROR: Stream length mismatch! (%s = %ld%s,"
                       " %s = %ld%s)\n",
                       libraries[0].name, stream_len + chunk_size,
                       chunk_size==lenof(buf1) ? "+" : "",
                       libraries[i].name, stream_len + this_chunk,
                       this_chunk==lenof(buf2) ? "+" : "");
                success = false;
                break;
            } else {
                for (int j = 0; j < chunk_size; j++) {
                    if (buf2[j] < (int32_t)buf1[j] - 4
                     || buf2[j] > (int32_t)buf1[j] + 4) {
                        if (libraries[i].library == LIBNOGG) {
                            printf("ERROR: Sample data mismatch! (sample %ld:"
                                   " %s = %d, %s = %d)\n", stream_len + j,
                                   libraries[0].name, buf1[j],
                                   libraries[i].name, buf2[j]);
                            success = false;
                            break;
                        } else if (!libraries[i].warned) {
                            printf("Note: Sample data mismatch for %s (sample"
                                   " %ld: %s = %d, %s = %d)\n",
                                   libraries[i].name, stream_len + j,
                                   libraries[0].name, buf1[j],
                                   libraries[i].name, buf2[j]);
                            libraries[i].warned = true;
                        }
                    }
                }
            }
        }
        if (chunk_size == 0) {
            break;
        }
        stream_len += chunk_size;
    }
    if (success) {
        printf("no errors found.\n");
    }

    /*
     * Perform timing comparisons (if requested and if no comparison errors
     * were detected).  For each decoder, we do a dummy run first to prime
     * the instruction cache followed by the requested number of iterations.
     */
    if (success && decode_iterations > 0) {
        if (time_init) {
            /* Even when timing initialization, we read a few samples from
             * the stream to trigger any lazy initialization the library
             * may perform. */
            vorbis_t *vorbis =
                vorbis_open_buffer(file_data, file_size, nogg_options, NULL);
            const int channels = vorbis_channels(vorbis);
            vorbis_close(vorbis);
            if (stream_len > 10*channels) {
                stream_len = 10*channels;
            }
        }
        int16_t *decode_buf_base = malloc(stream_len*2 + 63);
        if (!decode_buf_base && stream_len > 0) {
            fprintf(stderr, "Out of memory (decode buffer: %ld samples)\n",
                    stream_len);
        }
        int16_t *decode_buf =
            (void *)(((uintptr_t)decode_buf_base + 63) & ~(uintptr_t)63);

        for (int i = 0; success && i < lenof(libraries); i++) {
            if (time_libnogg && libraries[i].library != LIBNOGG) {
                continue;
            }
            printf("Timing %s (%s)... %*s", libraries[i].name,
                   time_init ? "initialization" : time_seek ? "seek" : "decode",
                   (int)(10 - strlen(libraries[i].name)), "");
            fflush(stdout);

            /* start is initialized to avoid a spurious warning. */
            uint64_t start = 0, end;
            DecoderHandle *decoder = decoder_open(libraries[i].library,
                                                  file_data, file_size);
            for (int iter = -1; iter < decode_iterations; iter++) {
                if (iter == 0) {
                    start = time_now();
                }
                if (time_init) {
                    decoder_close(decoder);
                    decoder = decoder_open(libraries[i].library,
                                           file_data, file_size);
                } else if (time_seek) {
                    decoder_close(decoder);
                    decoder = decoder_open(libraries[i].library,
                                           file_data, file_size);
                    if (!decoder_seek_to_end(decoder)) {
                        printf("ERROR: Seek-to-end failed on iteration %d!\n",
                               iter+1);
                        success = false;
                        break;
                    }
                } else {
                    if (!decoder_rewind(decoder)) {
                        printf("ERROR: Rewind failed on iteration %d!\n",
                               iter+1);
                        success = false;
                        break;
                    }
                }
                if (!time_seek) {
                    long decoded =
                        decoder_read(decoder, decode_buf, stream_len);
                    if (decoded != stream_len) {
                        printf("ERROR: Truncated decode on iteration %d!"
                               " (expected %ld, got %ld)\n", iter+1,
                               stream_len, decoded);
                        success = false;
                        break;
                    }
                }
            }
            end = time_now();

            decoder_close(decoder);
            if (success) {
                printf("%.3f seconds (%d iteration%s).\n",
                       (double)(end - start) * time_unit(),
                       decode_iterations, decode_iterations==1 ? "" : "s");
            }
        }

        free(decode_buf_base);
    }

    /*
     * Free allocated data and exit.
     */
    for (int i = 0; i < lenof(libraries); i++) {
        decoder_close(libraries[i].decoder);
    }
    free(file_data);
    return success ? 0 : 1;
}

/*************************************************************************/
/*************************************************************************/
