/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * tools/extract-pkg.c: Program to list or extract data files from a
 * package file created with the build-pkg tool.
 */

#include "tool-common.h"
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

#include "../src/resource/package-pkg.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Local function declarations. */
static int read_package(const char *path, FILE **fp_ret, int *nfiles_ret,
                        PKGIndexEntry **index_ret, char **namebuf_ret);
static int match_wildcard(const char *pattern, const char *path);
static void sanitize_path(char *path);
static int mkdir_parents(char *path);
static int extract(FILE *pkg, const PKGIndexEntry *entry, char *outpath);

/*************************************************************************/
/***************************** Main routine ******************************/
/*************************************************************************/

/**
 * main:  Program entry point.
 *
 * [Parameters]
 *     argc: Command line argument count.
 *     argv: Command line argument array.
 * [Return value]
 *     0 on success, 1 on processing error, 2 on command line error.
 */
int main(int argc, char **argv)
{
    int list = 0;
    const char *outdir = NULL;
    int verbose = 0;

    while (argc > 1 && argv[1][0] == '-') {
        if (strcmp(argv[1], "-list") == 0) {
            list = 1;
        } else if (strncmp(argv[1], "-outdir=", 8) == 0) {
            outdir = argv[1]+8;
        } else if (strcmp(argv[1], "-verbose") == 0) {
            verbose = 1;
        } else {
            if (strcmp(argv[1], "-h") != 0 && strcmp(argv[1], "--help") != 0) {
                fprintf(stderr, "Unknown option %s\n", argv[1]);
            }
            goto usage;
        }
        argc--;
        if (argc > 1) {
            memmove(&argv[1], &argv[2], sizeof(*argv) * (argc-1));
        }
    }

    if (argc < 2) {
      usage:
        fprintf(stderr,
                "Usage: %s [options] <input-file> [files-to-extract]\n"
                "\n"
                "files-to-extract may contain the following wildcards:\n"
                "   ? to match a single character in a filename.\n"
                "   * to match any number of characters in a filename.\n"
                "   ** to match any number of characters across directory names.\n"
                "If no files-to-extract are given, all files are extracted.\n"
                "\n"
                "Options:\n"
                "-list: List file entries instead of extracting files.\n"
                "-outdir=PATH: Extract files under directory PATH.\n"
                "-verbose: List files as they are extracted, or show file details\n"
                "          with -list.\n",
                argv[0]);
        return 2;
    }

    /* Open the package file and read in its index. */
    FILE *pkg;
    int nfiles;
    PKGIndexEntry *index;
    char *namebuf;
    if (!read_package(argv[1], &pkg, &nfiles, &index, &namebuf)) {
        return 1;
    }

    /* Extract or list all requested files. */
    int exitcode = 0;
    int num_extracted = 0;
    if (list && verbose) {
        printf("Hash      Data size  File size  Filename\n"
               "--------  ---------  ---------  --------\n");
    }
    for (int i = 0; i < nfiles; i++) {
        const char *path = &namebuf[PKG_NAMEOFS(index[i].nameofs_flags)];
        int matched;
        if (argc < 3) {
            matched = 1;
        } else {
            matched = 0;
            for (int j = 2; j < argc; j++) {
                if (match_wildcard(argv[j], path)) {
                    matched = 1;
                    break;
                }
            }
        }
        if (matched) {
            num_extracted++;
            if (list) {
                if (verbose) {
                    printf("%08X  %9u  %9u  %s\n", index[i].hash,
                           index[i].datalen, index[i].filesize, path);
                } else {
                    printf("%s\n", path);
                }
            } else {
                if (verbose) {
                    printf("%s\n", path);
                }
                char *outpath;
                if (outdir) {
                    outpath = malloc(strlen(outdir) + strlen(path) + 2);
                    if (outpath) {
                        sprintf(outpath, "%s/%s", outdir, path);  // Safe.
                    }
                } else {
                    outpath = strdup(path);
                }
                if (!outpath) {
                    fprintf(stderr, "%s: Out of memory\n", path);
                    exitcode = 1;
                    continue;
                }
                sanitize_path(outpath);
                const int ok = extract(pkg, &index[i], outpath);
                free(outpath);
                if (!ok) {
                    exitcode = 1;
                }
            }
        }
    }

    if (argc >= 3 && num_extracted == 0) {
        fprintf(stderr, "Warning: no files matched specified patterns\n");
        exitcode = 1;
    }

    /* All done! */
    fclose(pkg);
    free(index);
    free(namebuf);
    return exitcode;
}

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

/**
 * read_package:  Read and return the given package file's index along with
 * an open file handle for the package file.
 *
 * [Parameters]
 *     path: Package file pathname.
 *     fp_ret: Pointer to variable to receive an open file handle for
 *         reading from the package file.
 *     nfiles_ret: Pointer to variable to receive the number of data files
 *         stored in the package file.
 *     index_ret: Pointer to variable to receive a malloc()-allocated array
 *         of index entries for the package.
 *     namebuf_ret: Pointer to variable to receive the malloc()-allocated
 *         filename buffer for data file names.
 * [Return value]
 *     True on success, false on error.
 */
static int read_package(const char *path, FILE **fp_ret, int *nfiles_ret,
                        PKGIndexEntry **index_ret, char **namebuf_ret)
{
    /* Open the package file. */
    *fp_ret = fopen(path, "rb");
    if (!*fp_ret) {
        fprintf(stderr, "fopen(%s): %s\n", path, strerror(errno));
        goto error_return;
    }

    /* Read in and verify the header data. */
    PKGHeader header;
    if (fread(&header, 1, 16, *fp_ret) != 16) {
        fprintf(stderr, "EOF reading %s\n", path);
        goto error_close_file;
    }
    if (memcmp(header.magic, PKG_MAGIC, 4) != 0) {
        fprintf(stderr, "Bad magic number reading %s (got %02X%02X%02X%02X,"
                " expected %02X%02X%02X%02X)\n", path, header.magic[0],
                header.magic[1], header.magic[2], header.magic[3],
                PKG_MAGIC[0], PKG_MAGIC[1], PKG_MAGIC[2], PKG_MAGIC[3]);
        goto error_close_file;
    }
    PKG_HEADER_SWAP_BYTES(header);
    if (header.header_size != sizeof(PKGHeader)) {
        fprintf(stderr, "Bad header size %d in %s\n", header.header_size,
                path);
        goto error_close_file;
    }
    if (header.entry_size != sizeof(PKGIndexEntry)) {
        fprintf(stderr, "Bad index entry size %d in %s\n", header.entry_size,
                path);
        goto error_close_file;
    }

    /* Allocate memory for the index and pathname buffers. */
    *nfiles_ret = header.entry_count;
    const int index_size = sizeof(**index_ret) * *nfiles_ret;
    *index_ret = malloc(index_size);
    if (!*index_ret) {
        fprintf(stderr, "No memory for %s directory (%d*%d)\n", path,
             *nfiles_ret, (int)sizeof(**index_ret));
        goto error_close_file;
    }
    *namebuf_ret = malloc(header.name_size);
    if (!*namebuf_ret) {
        fprintf(stderr, "No memory for %s pathnames (%u bytes)\n", path,
             header.name_size);
        goto error_free_index;
    }

    /* Read in the file index. */
    if ((int)fread(*index_ret, 1, index_size, *fp_ret) != index_size) {
        fprintf(stderr, "EOF reading %s directory\n", path);
        goto error_free_namebuf;
    }
    if ((int)fread(*namebuf_ret, 1, header.name_size, *fp_ret)
        != (int)header.name_size)
    {
        fprintf(stderr, "EOF reading %s pathname table\n", path);
        goto error_free_namebuf;
    }
    PKG_INDEX_SWAP_BYTES(*index_ret, *nfiles_ret);

    /* Success! */
    return 1;

  error_free_namebuf:
    free(*namebuf_ret);
  error_free_index:
    free(*index_ret);
  error_close_file:
    fclose(*fp_ret);
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

/**
 * match_wildcard:  Return whether the given path matches the given
 * wildcard pattern.
 *
 * [Parameters]
 *     pattern: Wildcard pattern to match against.
 *     path: Pathname to test.
 * [Return value]
 *     True if the path matches the pattern, false if not.
 */
static int match_wildcard(const char *pattern, const char *path)
{
    while (*pattern) {
        const char ch = *pattern++;
        if (ch == '*') {
            int double_star = 0;
            if (*pattern == '*') {
                pattern++;
                double_star = 1;
            }
            while (!match_wildcard(pattern, path)) {
                if (!*path || (!double_star && *path == '/')) {
                    return 0;
                }
                path++;
            }
        } else if (ch == '?') {
            if (!*path || *path == '/') {
                return 0;
            }
            path++;
        } else {
            if (*path != ch) {
                return 0;
            }
            path++;
        }
    }
    return *path == 0;
}

/*-----------------------------------------------------------------------*/

/**
 * sanitize_path:  Sanitize the given pathname by removing all ../
 * components.  A warning is printed to stderr if any components are removed.
 *
 * [Parameters]
 *     path: Pathname to sanitize (modified in place).
 */
static void sanitize_path(char *path)
{
    int warned = 0;
    for (char *s = strstr(path, "../"); s; s = strstr(s, "../")) {
        if (s > path && s[-1] != '/') {
            /* Just a ".." at the end of something else, which is harmless. */
            s += 3;
            continue;
        }
        if (!warned) {
            fprintf(stderr, "%s: warning: removing ../ components\n", path);
            warned = 1;
        }
        memmove(s, s+3, strlen(s+3)+1);
    }
}

/*-----------------------------------------------------------------------*/

/**
 * mkdir_parents:  Create any nonexistent parent directories of the given path.
 *
 * [Parameters]
 *     path: Pathname for which to create parent directories (will be
 *         modified by this function but restored to its original state on
 *         return).
 * [Return value]
 *     True on success, false on error.
 */
int mkdir_parents(char *path)
{
    char *slash = strrchr(path, '/');
    if (!slash || slash == path) {
        return 1;  // No parents to create.
    }
    *slash = 0;
    int ok = 0;
    if (mkdir_parents(path)) {
        if (mkdir(path, 0777) == 0 || errno == EEXIST) {
            ok = 1;
        } else {
            fprintf(stderr, "mkdir(%s): %s\n", path, strerror(errno));
        }
    }
    *slash = '/';
    return ok;
}

/*-----------------------------------------------------------------------*/

/**
 * extract:  Extract a data file from a package file.
 *
 * [Parameters]
 *     pkg: Open file handle to the package file.
 *     entry: Index entry for the data file to extract.
 *     outpath: Pathname to which to write the data file.
 * [Return value]
 *     True on success, false on error.
 */
static int extract(FILE *pkg, const PKGIndexEntry *entry, char *outpath)
{
    char readbuf[65536];

    if (fseek(pkg, entry->offset, SEEK_SET) != 0) {
        perror("fseek()");
        return 0;
    }
    if (!mkdir_parents(outpath)) {
        fprintf(stderr, "%s: Failed to create parent directories\n", outpath);
        return 0;
    }
    FILE *out = fopen(outpath, "wb");
    if (!out) {
        perror(outpath);
        return 0;
    }

    z_stream inflater;
    if (entry->nameofs_flags & PKGF_DEFLATED) {
        inflater.next_in = Z_NULL;
        inflater.avail_in = 0;
        inflater.zalloc = Z_NULL;
        inflater.zfree = Z_NULL;
        if (inflateInit(&inflater) != Z_OK) {
            fprintf(stderr, "%s: Failed to initialize inflater\n", outpath);
            goto fail;
        }
    }

    for (uint32_t pos = 0; pos < entry->datalen; pos += sizeof(readbuf)) {
        const int to_read = ubound(entry->datalen - pos, sizeof(readbuf));
        const int nread = fread(readbuf, 1, to_read, pkg);
        if (nread != to_read) {
            fprintf(stderr, "%s: Short read on package file\n", outpath);
            goto fail;
        }
        if (entry->nameofs_flags & PKGF_DEFLATED) {
            char outbuf[65536];
            inflater.next_in = (void *)readbuf;
            inflater.avail_in = nread;
            int result;
            while (inflater.next_out = (void *)outbuf,
                   inflater.avail_out = sizeof(outbuf),
                   result = inflate(&inflater, Z_NO_FLUSH),
                   result == Z_OK
                   || result == Z_STREAM_END
                   || (result == Z_BUF_ERROR && inflater.avail_in > 0)) {
                const int to_write = (char *)inflater.next_out - outbuf;
                if (to_write > 0) {
                    const int nwritten = fwrite(outbuf, 1, to_write, out);
                    if (nwritten != to_write) {
                        perror(outpath);
                        goto fail;
                    }
                }
                if (result == Z_STREAM_END) {
                    break;
                }
            }
            if (result != Z_STREAM_END && result != Z_BUF_ERROR) {
                fprintf(stderr, "%s: Decompression error %d\n", outpath, result);
                goto fail;
            }
        } else {
            const int nwritten = fwrite(readbuf, 1, nread, out);
            if (nwritten != nread) {
                perror(outpath);
                goto fail;
            }
        }
    }

    fclose(out);
    if (entry->nameofs_flags & PKGF_DEFLATED) {
        inflateEnd(&inflater);
    }
    return 1;

  fail:
    fclose(out);
    remove(outpath);
    if (entry->nameofs_flags & PKGF_DEFLATED) {
        inflateEnd(&inflater);
    }
    return 0;
}

/*************************************************************************/
/*************************************************************************/
