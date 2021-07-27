/*
 * libwebmdec: a decoder library for WebM audio/video streams
 * Copyright (c) 2014-2019 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

/*
 * This file is intended to illustrate typical usage of the libwebmdec
 * library.  It implements a simple frontend to the libwebmdec library
 * which reads a stream from either a file or standard input and writes
 * the video and/or audio streams as either raw or decoded data.
 *
 * See the usage() function (or run the program with the --help option)
 * for details of the command-line interface.
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <webmdec.h>

/*************************************************************************/
/*************************** Stream callbacks ****************************/
/*************************************************************************/

/*
 * This callback set is used when streaming data from standard input, or
 * when the library is built without stdio support.  Since streaming input
 * does not allow seeking, we don't need to define any of the seek-related
 * functions.
 */

static long streaming_read(void *opaque, void *buffer, long length)
{
    return (long)fread(buffer, 1, (size_t)length, (FILE *)opaque);
}

/* We technically don't need this wrapper, but we use it anyway to work
 * around excessively pedantic compiler warnings. */
static void streaming_close(void *opaque)
{
    fclose(opaque);
}

static const webmdec_callbacks_t streaming_callbacks = {
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
            "Read INPUT-FILE or standard input as a WebM stream, display\n"
            "stream information on standard output, and optionally write out\n"
            "decoded or raw audio and video data to separate files.\n"
            "\n"
            "Options:\n"
            "   -a FILE      Write audio data to FILE.\n"
            "   -h, --help   Display this text and exit.\n"
            "   -r           Write raw audio/video data instead of decoding.\n"
            "   -p PREFIX    Write video images to files starting with PREFIX.\n"
            "   -v FILE      Write video data to FILE.\n"
            "   --version    Display the program's version and exit.\n"
            "\n"
            "If INPUT-FILE is \"-\" or omitted, the WebM stream is read from\n"
            "standard input.\n"
            "\n"
            "Decoded audio data is written as 16-bit signed little-endian PCM\n"
            "data with channels interleaved.  Decoded video data is written as\n"
            "32-bit-per-pixel data in BGRx (blue, green, red, padding) byte order.\n"
            "\n"
            "If the -p option is given, each video frame is written as a PPM-format\n"
            "image to a filename formed by concatenating PREFIX with the 10-digit,\n"
            "zero-padded frame number and a \".ppm\" suffix.  The -p option is\n"
            "ignored if the -r option is given.\n"
            "\n"
            "Examples:\n"
            "   %s -a audio.pcm -p video/ input.webm\n"
            "      Decode the file \"input.webm\", writing the decoded audio\n"
            "      stream to \"audio.pcm\" and video images to files\n"
            "      \"video/0000000000.ppm\", \"video/0000000001.ppm\", and so on.\n"
            "   cat input.webm | %s -r -v video.raw\n"
            "      Read a WebM stream from standard input and extract the raw\n"
            "      video data to \"video.raw\", ignoring any audio data.\n",
            argv0, argv0, argv0);
}

/*************************************************************************/
/***************************** Main routine ******************************/
/*************************************************************************/

int main(int argc, char **argv)
{
    /* Pathname of the input file, or NULL if reading from standard input. */
    const char *input_path = NULL;
    /* Pathname of the audio output file, or NULL to ignore audio. */
    const char *audio_path = NULL;
    /* Pathname of the video output file, or NULL to ignore video. */
    const char *video_path = NULL;
    /* Path prefix for PPM image output files, or NULL to not generate them. */
    const char *image_prefix = NULL;
    /* Flag for raw output. */
    int raw_output = 0;

    /*
     * Parse command-line arguments.
     */
    int in_options = 1;  /* Flag for detecting the "--" option terminator. */
    for (int argi = 1; argi < argc; argi++) {
        if (in_options && argv[argi][0] == '-') {
            if (strcmp(argv[argi], "-h") == 0
             || strcmp(argv[argi], "--help") == 0) {
                usage(argv[0]);
                return 0;

            } else if (strcmp(argv[argi], "--version") == 0) {
                printf("webmdec %s (using libwebmdec %s)\n",
                       VERSION, webmdec_version());
                return 0;

            } else if (argv[argi][1] == '-') {
                if (!argv[argi][2]) {
                    in_options = 0;
                } else {
                    /* We don't support double-dash arguments, but we
                     * parse them anyway so we can display a sensible error
                     * message. */
                    int arglen = strcspn(argv[argi], "=");
                    fprintf(stderr, "%s: unrecognized option \"%.*s\"\n",
                            argv[0], arglen, argv[argi]);
                    goto try_help;
                }

            } else if (strchr("apv", argv[argi][1])) {
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
                switch (option) {
                    case 'a': audio_path   = value; break;
                    case 'p': image_prefix = value; break;
                    case 'v': video_path   = value; break;
                }

            } else if (argv[argi][1] == 'r') {
                raw_output = 1;
                if (argv[argi][2]) {
                    /* Handle things like "-rv video.raw". */
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

    if (input_path && strcmp(input_path, "-") == 0) {
        input_path = NULL;  /* An input path of "-" means standard input. */
    }

    /*
     * Open a libwebmdec handle for the stream.
     */
    webmdec_t *handle = NULL;
    webmdec_error_t error;
    if (input_path) {
        /* The input comes from a file, so attempt to use the built-in stdio
         * interface to read from the file. */
        handle = webmdec_open_from_file(input_path, WEBMDEC_OPEN_ANY, &error);
        if (!handle) {
            if (error == WEBMDEC_ERROR_DISABLED_FUNCTION) {
                /* This build of libwebmdec does not support stdio.  In
                 * this case, we can fall back to our own streaming read
                 * callbacks, so we don't treat this as a fatal error. */
                fprintf(stderr, "Note: libwebmdec stdio support disabled,"
                        " using streaming reads\n");
                if (!freopen(input_path, "rb", stdin)) {
                    perror(input_path);
                    return 1;
                }
            } else if (error == WEBMDEC_ERROR_FILE_OPEN_FAILED) {
                /* In this case, the error code is left in errno, so we can
                 * just use the perror() function to print the error. */
                perror(input_path);
                return 1;
            } else if (error == WEBMDEC_ERROR_INSUFFICIENT_RESOURCES) {
                fprintf(stderr, "Out of memory\n");
                return 1;
            } else {
                fprintf(stderr, "Unexpected libwebmdec error %d\n", error);
                return 1;
            }
        }
    }

    if (!handle) {
        /* Either the stream is being fed through standard input, or the
         * library doesn't support stdio (in which case standard input has
         * been redirected to the desired input file).  Use our streaming
         * read callback set to process the data. */
        handle = webmdec_open_from_callbacks(streaming_callbacks, stdin,
                                             WEBMDEC_OPEN_ANY, &error);
        if (!handle) {
            if (error == WEBMDEC_ERROR_INSUFFICIENT_RESOURCES) {
                fprintf(stderr, "Out of memory\n");
            } else {
                fprintf(stderr, "Unexpected libwebmdec error %d\n", error);
            }
            return 1;
        }
    }

    /*
     * Check for inappropriate options for the stream format.  In our case,
     * we make sure that if the user has requested audio data, the stream
     * actually contains audio.
     */
    if (audio_path && webmdec_audio_channels(handle) == 0) {
        fprintf(stderr,
                "Audio data requested but stream does not contain audio\n");
        return 1;
    }

    /*
     * Print basic stream information.
     */
    printf("Video frame size: %d x %d pixels\n",
           webmdec_video_width(handle), webmdec_video_height(handle));
    if (webmdec_audio_channels(handle)) {
        printf("Audio data format: %d channels, %d Hz\n",
               webmdec_audio_channels(handle), webmdec_audio_rate(handle));
    } else {
        printf("No audio stream\n");
    }

    /*
     * Open output files.
     */
    FILE *video_fp, *audio_fp;
    if (video_path) {
        video_fp = fopen(video_path, "wb");
        if (!video_fp) {
            perror(video_path);
            webmdec_close(handle);
            return 1;
        }
    } else {
        video_fp = NULL;
    }
    if (audio_path) {
        audio_fp = fopen(audio_path, "wb");
        if (!audio_fp) {
            perror(audio_path);
            fclose(video_fp);
            webmdec_close(handle);
            return 1;
        }
    } else {
        audio_fp = NULL;
    }

    /*
     * Read and process frames from the stream.
     */
    const int want_video =
        (video_fp != NULL || (!raw_output && image_prefix != NULL));
    const int want_audio = (audio_fp != NULL);
    int success = 1;

    if (raw_output) {

        const void *video_data, *audio_data;
        int video_length, audio_length;
        double video_time, audio_time;

        while (webmdec_read_frame(handle,
                                  want_video ? &video_data : NULL,
                                  want_video ? &video_length : NULL,
                                  &video_time,
                                  want_audio ? &audio_data : NULL,
                                  want_audio ? &audio_length : NULL,
                                  &audio_time)) {
            printf("Read a frame: V=%.3f A=%.3f\n", video_time, audio_time);
            if (video_fp && video_data) {
                if (fwrite(video_data, video_length, 1, video_fp) != 1) {
                    fprintf(stderr, "Write error on %s: %s\n",
                            video_path, strerror(errno));
                    success = 0;
                    break;
                }
            }
            if (audio_fp && audio_data) {
                if (fwrite(audio_data, audio_length, 1, audio_fp) != 1) {
                    fprintf(stderr, "Write error on %s: %s\n",
                            audio_path, strerror(errno));
                    success = 0;
                    break;
                }
            }
        }

    } else {  /* !raw_output */

        const int video_width = webmdec_video_width(handle);
        const int video_height = webmdec_video_height(handle);
        const int audio_channels = webmdec_audio_channels(handle);
        int frame = 0;
        const void *video_data;
        const float *audio_pcm;
        int audio_samples;
        double video_time, audio_time;

        while (webmdec_decode_frame(handle,
                                    want_video ? &video_data : NULL,
                                    &video_time,
                                    want_audio ? &audio_pcm : NULL,
                                    want_audio ? &audio_samples : NULL,
                                    &audio_time)) {
            printf("Decoded a frame: V=%.3f A=%.3f\n", video_time, audio_time);
            if (video_fp && video_data) {
                if (fwrite(video_data, video_width*video_height*3/2, 1,
                           video_fp) != 1) {
                    fprintf(stderr, "Write error on %s: %s\n",
                            video_path, strerror(errno));
                    success = 0;
                    break;
                }
            }
            if (image_prefix && video_data) {
                char image_path[1000];
                if (snprintf(image_path, sizeof(image_path), "%s%010d.ppm",
                             image_prefix, frame) >= (int)sizeof(image_path)) {
                    fprintf(stderr, "Buffer overflow on image pathname\n");
                    success = 0;
                    break;
                }
                FILE *fp = fopen(image_path, "wb");
                if (!fp) {
                    perror(image_path);
                    success = 0;
                    break;
                }
                if (fprintf(fp, "P6\n%d %d 255\n",
                            video_width, video_height) < 0) {
                    perror(image_path);
                    fclose(fp);
                    success = 0;
                    break;
                }
                const unsigned char *y_row = video_data;
                const unsigned char *u_row = y_row + video_width*video_height;
                const unsigned char *v_row =
                    u_row + (video_width/2)*(video_height/2);
                for (int y = 0; y < video_height;
                     y_row += video_width, u_row += (y%2)*(video_width/2),
                     v_row += (y%2)*(video_width/2), y++)
                {
                    for (int x = 0; x < video_width; x++) {
                        int Y, U, V, R, G, B;
                        Y = (y_row[x] - 0x10) * 9539;
                        U = u_row[x/2] - 0x80;
                        V = v_row[x/2] - 0x80;
                        R = (Y           + 13075*V + (1<<12)) >> 13;
                        G = (Y -  3209*U -  6660*V + (1<<12)) >> 13;
                        B = (Y + 16525*U           + (1<<12)) >> 13;
                        if (fputc(R,fp)<0 || fputc(G,fp)<0 || fputc(B,fp)<0) {
                            perror(image_path);
                            fclose(fp);
                            success = 0;
                            break;
                        }
                    }
                }
                fclose(fp);
            }
            if (audio_fp && audio_pcm) {
                for (int i = 0; i < audio_samples * audio_channels; i++) {
                    float sample_in = audio_pcm[i];
                    if (sample_in > 1.0f) {
                        sample_in = 1.0f;
                    } else if (sample_in < -1.0f) {
                        sample_in = -1.0f;
                    }
                    int sample_out = (int)(sample_in*32767.0f + 0.5f);
                    if (sample_out < 0) {
                        sample_out += 65536;
                    }
                    if (fputc(sample_out>>0 & 0xFF, audio_fp) < 0
                     || fputc(sample_out>>8 & 0xFF, audio_fp) < 0) {
                        fprintf(stderr, "Write error on %s: %s\n",
                                audio_path, strerror(errno));
                        success = 0;
                        break;
                    }
                }
            }
            frame++;
        }

    }

    if (success && webmdec_last_error(handle) != WEBMDEC_ERROR_STREAM_END) {
        switch (webmdec_last_error(handle)) {
          case WEBMDEC_ERROR_DISABLED_FUNCTION:
            fprintf(stderr, "Decoding not available in this build\n");
            break;
          case WEBMDEC_ERROR_INSUFFICIENT_RESOURCES:
            fprintf(stderr, "Out of memory\n");
            break;
          case WEBMDEC_ERROR_STREAM_READ_FAILURE:
            fprintf(stderr, "Error reading from stream\n");
            break;
          case WEBMDEC_ERROR_DECODE_SETUP_FAILURE:
            fprintf(stderr, "Error initializing decoder\n");
            break;
          case WEBMDEC_ERROR_DECODE_FAILURE:
            fprintf(stderr, "Error decoding stream\n");
            break;
          default:
            fprintf(stderr, "Unexpected error %d\n",
                    webmdec_last_error(handle));
            break;
        }
        success = 0;
    }

    /*
     * Close output files.
     */
    if (video_fp) {
        fclose(video_fp);
    }
    if (audio_fp) {
        fclose(audio_fp);
    }

    /*
     * Close the stream, freeing all associated resources.  Since the
     * program immediately exits after this call, it's not strictly
     * necessary to explicitly close the stream in this case, but the
     * close operation is important to avoid resource leaks when stream
     * decoding is only one part of a longer-lived program.
     */
    webmdec_close(handle);

    /*
     * Terminate the program, indicating success or failure of the operation.
     */
    return success ? 0 : 1;
}

/*************************************************************************/
/*************************************************************************/
