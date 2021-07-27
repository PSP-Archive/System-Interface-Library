/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014-2019 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

/*
 * This file is intended to illustrate typical usage of the libnogg
 * library.  It implements a simple frontend to the libnogg library
 * which reads a stream from either a file or standard input and writes
 * the decoded PCM stream to a file or standard output.
 *
 * See the usage() function (or run the program with the --help option)
 * for details of the command-line interface.
 */

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <nogg.h>

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
# define ALIGN(alignment)  _Alignas(alignment)
#elif defined(__GNUC__) || defined(__clang__)
# define ALIGN(alignment)  __attribute__((aligned(alignment)))
#else
# define ALIGN(alignment)  /*nothing*/
#endif

/*************************************************************************/
/*************************** Stream callbacks ****************************/
/*************************************************************************/

/*
 * This callback set is used when streaming data from standard input, or
 * when the library is built without stdio support.  Since streaming input
 * does not allow seeking, we don't need to define any of the seek-related
 * functions.
 */

static int32_t streaming_read(void *opaque, void *buffer, int32_t length)
{
    return (int32_t)fread(buffer, 1, (size_t)length, (FILE *)opaque);
}

/* This fclose() wrapper is unnecessary on current platforms, but it
 * suppresses a warning from GCC 8 about casting between functions of
 * different return types. */
static void streaming_close(void *opaque)
{
    (void) fclose(opaque);
}

static const vorbis_callbacks_t streaming_callbacks = {
    .read = streaming_read,
    .close = streaming_close,
    /* All other function pointers are left at NULL. */
};

/*************************************************************************/
/*************************** Helper functions ****************************/
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
            "Usage: %s [OPTION]... [INPUT-FILE]\n"
            "Read INPUT-FILE or standard input as a Vorbis stream, display stream\n"
            "information on standard output, and optionally write the decoded PCM\n"
            "audio to a separate file.\n"
            "\n"
            "Options:\n"
            "   -f           Write PCM data in floating-point format.\n"
            "   -h, --help   Display this text and exit.\n"
            "   -l           Lax conformance: allow junk data between Ogg pages.\n"
            "   -o FILE      Write decoded PCM data to FILE (\"-\" for standard output).\n"
            "   -q           Do not display stream information (useful with -o -).\n"
            "   -w           Prepend a RIFF WAVE header to PCM output.\n"
            "   --version    Display the program's version and exit.\n"
            "\n"
            "If INPUT-FILE is \"-\" or omitted, the Vorbis stream is read from\n"
            "standard input.\n"
            "\n"
            "If the FILE argument to -o is \"-\", the decoded PCM data is written to\n"
            "standard output.  In this case, the -q option must also be specified.\n"
            "(On Windows systems, writing to standard output may result in corrupt\n"
            "data due to CRLF translation.)\n"
            "\n"
            "Decoded audio data is written as 16-bit signed little-endian PCM data\n"
            "with channels interleaved.  If the -w option is given, a RIFF WAVE\n"
            "header is prepended to the data (this requires the output file to be\n"
            "seekable).\n"
            "\n"
            "Examples:\n"
            "   %s file.ogg\n"
            "      Display information about the Vorbis stream \"file.ogg\".\n"
            "   cat file.ogg | %s -w -o file.wav\n"
            "      Read a Vorbis stream from standard input and decode it to\n"
            "      \"file.wav\", prepending a RIFF WAVE header to the data.\n",
            argv0, argv0, argv0);
}

/*************************************************************************/
/***************************** Main routine ******************************/
/*************************************************************************/

int main(int argc, char **argv)
{
    /* Pathname of the input file, or NULL if reading from standard input. */
    const char *input_path = NULL;
    /* Pathname of the PCM output file, "-" for stdout, or NULL for no
     * output. */
    const char *output_path = NULL;
    /* Use floating-point output? */
    bool output_float = false;
    /* Allow junk between Ogg pages? */
    bool lax_conformance = false;
    /* Suppress information display? */
    bool quiet_mode = false;
    /* Write a RIFF WAVE header? */
    bool wave_header = false;

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
                printf("nogg-decode %s (using libnogg %s)\n",
                       VERSION, nogg_version());
                return 0;

            } else if (argv[argi][1] == '-') {
                if (!argv[argi][2]) {
                    in_options = false;
                } else {
                    /* We don't support double-dash arguments other than
                     * --help and --version, but we parse them anyway so
                     * we can display a sensible error message. */
                    const int arglen = (int)strcspn(argv[argi], "=");
                    fprintf(stderr, "%s: unrecognized option \"%.*s\"\n",
                            argv[0], arglen, argv[argi]);
                    goto try_help;
                }

            } else if (argv[argi][1] == 'f') {
                output_float = true;
                if (argv[argi][2]) {
                    /* Handle things like "-fwo output.wav". */
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

            } else if (argv[argi][1] == 'o') {
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
                output_path = value;

            } else if (argv[argi][1] == 'q') {
                quiet_mode = true;
                if (argv[argi][2]) {
                    memmove(&argv[argi][1], &argv[argi][2],
                            strlen(&argv[argi][2])+1);
                    argi--;
                }

            } else if (argv[argi][1] == 'w') {
                wave_header = true;
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

    if (output_path && strcmp(output_path, "-") == 0 && !quiet_mode) {
        fprintf(stderr,
                "%s: -q must be used when decoding to standard output\n",
                argv[0]);
        goto try_help;
    }

    if (input_path && strcmp(input_path, "-") == 0) {
        input_path = NULL;  /* An input path of "-" means standard input. */
    }

    const unsigned int options =
        (lax_conformance ? VORBIS_OPTION_SCAN_FOR_NEXT_PAGE : 0);

    /*
     * Open a libnogg handle for the stream.
     */
    vorbis_t *handle = NULL;
    vorbis_error_t error;
    if (input_path) {
        /* The input comes from a file, so attempt to use the built-in stdio
         * interface to read from the file. */
        handle = vorbis_open_file(input_path, options, &error);
        if (!handle) {
            if (error == VORBIS_ERROR_DISABLED_FUNCTION) {
                /* This build of libnogg does not support stdio.  In
                 * this case, we can fall back to our own streaming read
                 * callbacks, so we don't treat this as a fatal error. */
                fprintf(stderr, "Note: libnogg stdio support disabled,"
                        " using streaming reads\n");
                if (!freopen(input_path, "rb", stdin)) {
                    perror(input_path);
                    return 1;
                }
            } else {
                if (error == VORBIS_ERROR_FILE_OPEN_FAILED) {
                    /* In this case, the error code is left in errno, so we can
                     * just use the perror() function to print the error. */
                    perror(input_path);
                } else if (error == VORBIS_ERROR_INSUFFICIENT_RESOURCES) {
                    fprintf(stderr, "Out of memory\n");
                } else if (error == VORBIS_ERROR_STREAM_INVALID) {
                    fprintf(stderr, "Invalid stream format\n");
                } else if (error == VORBIS_ERROR_STREAM_END) {
                    fprintf(stderr, "Unexpected EOF\n");
                } else if (error == VORBIS_ERROR_DECODE_SETUP_FAILED) {
                    fprintf(stderr, "Failed to initialize decoder\n");
                } else {
                    fprintf(stderr, "Unexpected libnogg error %d\n", error);
                }
                return 1;
            }
        }
    }

    if (!handle) {
        /* Either the stream is being fed through standard input, or the
         * library doesn't support stdio (in which case standard input has
         * been redirected to the desired input file).  Use our streaming
         * read callback set to process the data. */
        handle = vorbis_open_callbacks(streaming_callbacks, stdin, options,
                                       &error);
        if (!handle) {
            if (error == VORBIS_ERROR_INSUFFICIENT_RESOURCES) {
                fprintf(stderr, "Out of memory\n");
            } else if (error == VORBIS_ERROR_STREAM_INVALID) {
                fprintf(stderr, "Invalid stream format\n");
            } else if (error == VORBIS_ERROR_STREAM_END) {
                fprintf(stderr, "Unexpected EOF\n");
            } else if (error == VORBIS_ERROR_DECODE_SETUP_FAILED) {
                fprintf(stderr, "Failed to initialize decoder\n");
            } else {
                fprintf(stderr, "Unexpected libnogg error %d\n", error);
            }
            return 1;
        }
    }

    /*
     * Retrieve basic stream information, and display it if not in quiet mode.
     */
    const int channels = vorbis_channels(handle);
    const uint32_t rate = vorbis_rate(handle);
    if (!quiet_mode) {
        printf("Audio data format: %d channels, %lu Hz\n",
               channels, (unsigned long)rate);
        const int64_t length = vorbis_length(handle);
        if (length >= 0) {
            printf("Stream length: %"PRId64" samples (%.2f seconds)\n",
                   length, (double)length / (double)rate);
        } else {
            printf("Stream length: unknown\n");
        }
    }

    /*
     * Decode the audio, if requested.
     */
    if (output_path) {

        const int sample_size = (output_float ? 4 : 2);

        /*
         * Open the output file.
         */
        FILE *output_fp;
        if (strcmp(output_path, "-") == 0) {
            output_fp = stdout;
        } else {
            output_fp = fopen(output_path, "wb");
            if (!output_fp) {
                perror(output_path);
                vorbis_close(handle);
                return 1;
            }
        }

        /*
         * Write a RIFF WAVE header if one was requested.  We'll go back
         * and fill in the various length fields when we're done.
         */
        if (wave_header) {
            static const unsigned char header_template[44] =
                "RIFF....WAVEfmt \x10\0\0\0.\0.............\0data....";
            unsigned char header[44];
            memcpy(header, header_template, sizeof(header_template));
            header[20] = output_float ? 3 : 1;
            header[22] = channels>>0 & 255;
            header[23] = channels>>8 & 255;
            header[24] = rate>> 0 & 255;
            header[25] = rate>> 8 & 255;
            header[26] = rate>>16 & 255;
            header[27] = rate>>24 & 255;
            header[28] = (rate*channels*sample_size)>> 0 & 255;
            header[29] = (rate*channels*sample_size)>> 8 & 255;
            header[30] = (rate*channels*sample_size)>>16 & 255;
            header[31] = (rate*channels*sample_size)>>24 & 255;
            header[32] = (channels*sample_size)>>0 & 255;
            header[33] = (channels*sample_size)>>8 & 255;
            header[34] = sample_size*8;
            if (fwrite(header, sizeof(header), 1, output_fp) != 1) {
                perror(output_path);
                fclose(output_fp);
                vorbis_close(handle);
                return 0;
            }
        }

        /*
         * Decode the audio.  We track the number of samples read only for
         * updating the WAVE header when done, so we don't worry about
         * 32-bit overflow.
         */
        int32_t samples_read = 0;
        const int32_t samples_read_limit =
            (0xFFFFFFFFUL - 36) / (channels*sample_size);
        bool samples_read_overflow = false;
        /* We align the buffer to help performance, and we make it static
         * because some compilers have trouble aligning things on the stack. */
        static ALIGN(64) union {
            int16_t i[4096];
            float f[4096];
        } buffer;
        const int buffer_len = (sizeof(buffer.i)/2) / channels;
        int count;
        do {
          retry:
            if (output_float) {
                count = vorbis_read_float(handle, buffer.f, buffer_len, &error);
            } else {
                count = vorbis_read_int16(handle, buffer.i, buffer_len, &error);
            }
            if (count > 0) {
                const int samples_written =
                    fwrite(&buffer, channels*sample_size, count, output_fp);
                if (samples_written != count) {
                    perror(output_path);
                    fclose(output_fp);
                    vorbis_close(handle);
                    return 0;
                }
                if (wave_header && !samples_read_overflow) {
                    if (count > samples_read_limit - samples_read) {
                        fprintf(stderr, "Warning: output file exceeds 4GB,"
                                " WAVE header will be invalid\n");
                        samples_read_overflow = true;
                    }
                }
                samples_read += count;
            }
            if (error == VORBIS_ERROR_DECODE_RECOVERED) {
                fprintf(stderr, "Warning: possible corruption at sample %ld\n",
                        (long)samples_read);
                goto retry;
            } else if (error && error != VORBIS_ERROR_STREAM_END) {
                if (error == VORBIS_ERROR_DECODE_FAILED) {
                    fprintf(stderr, "Decode failed at sample %ld\n",
                            (long)samples_read);
                } else if (error == VORBIS_ERROR_INSUFFICIENT_RESOURCES) {
                    fprintf(stderr, "Out of memory at sample %ld\n",
                            (long)samples_read);
                } else {
                    fprintf(stderr, "Unexpected libnogg error %d at sample"
                            " %ld\n", error, (long)samples_read);
                }
                fclose(output_fp);
                vorbis_close(handle);
                return 0;
            }
        } while (count > 0);

        /*
         * Update the WAVE header, if necessary.
         */
        if (wave_header) {
            const int32_t data_size = samples_read * (channels*sample_size);
            if (fseek(output_fp, 4, SEEK_SET) != 0
             || fputc((data_size+36)>> 0 & 255, output_fp) == EOF
             || fputc((data_size+36)>> 8 & 255, output_fp) == EOF
             || fputc((data_size+36)>>16 & 255, output_fp) == EOF
             || fputc((data_size+36)>>24 & 255, output_fp) == EOF
             || fseek(output_fp, 40, SEEK_SET) != 0
             || fputc(data_size>> 0 & 255, output_fp) == EOF
             || fputc(data_size>> 8 & 255, output_fp) == EOF
             || fputc(data_size>>16 & 255, output_fp) == EOF
             || fputc(data_size>>24 & 255, output_fp) == EOF)
            {
                perror(output_path);
                fclose(output_fp);
                vorbis_close(handle);
                return 0;
            }
        }

    }  // if (output_path)

    /*
     * Close the stream, freeing all associated resources.  Since the
     * program immediately exits after this call, it's not strictly
     * necessary to explicitly close the stream in this case, but the
     * close operation is important to avoid resource leaks when stream
     * decoding is only one part of a longer-lived program.
     */
    vorbis_close(handle);

    /*
     * Terminate the program with a successful error code.
     */
    return 0;
}

/*************************************************************************/
/*************************************************************************/
