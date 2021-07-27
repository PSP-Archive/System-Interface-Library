/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * tools/build-pkg.c: Program to build package files for fast data file
 * access from the game.
 */

/*
 * This program uses a control file to generate PKG-format package files.
 * The control file is basically a list of data files to include in the
 * package, one per line; the filename (but not a directory name) can
 * include the wildcard "%" to match any number of characters.  Prepending
 * "deflate:" to the pathname causes the file(s) to be compressed.
 *
 * If a pathname includes any whitespace characters or begins with a
 * double-quote character, enclose the entire pathname in double quotes and
 * use a backslash to escape any double-quote or backslash characters in
 * the pathname.  If the pathname is not quoted, double-quote and backslash
 * characters are treated normally.
 *
 * It is also possible to give a file a different name in the package than
 * its current name on the host filesystem; for example, the line
 *     logo.png = testing/newlogo.png
 * would read "testing/newlogo.png" from the host filesystem, but store it
 * as "logo.png" for access from the game.  If the host filesystem pathname
 * has a wildcard, the renamed path should also include a wildcard; for
 * example,
 *     data/%.dat = data/RELEASE-%.dat
 * would include all files matching "data/RELEASE-*.dat" and strip
 * "RELEASE-" from each filename.
 *
 * Blank lines and lines starting with "#" (comments) are ignored.
 *
 * Invoke the program as:
 *     build-pkg <control-file> <output-file>
 */

#include "tool-common.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

#include "../src/resource/package-pkg.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

#define LINEMAX  1000  // Maximum length of a line in the control file.

/*-----------------------------------------------------------------------*/

/* Data structure holding information about a single file.  This is stored
 * separately from the package index (1) because files can be given
 * different pathnames in the package and (2) we need a list of files in
 * write order, rather than the hash order used in the package index. */

typedef struct FileInfo_ {
    const char *pathname;  // Pathname used in the package.
    const char *realfile;  // Actual file pathname to read from.
    uint32_t flags;        // Index flags (PKGF_*).
    int index_entry;       // Entry number in the package index.
} FileInfo;

/*-----------------------------------------------------------------------*/

/* Parameters used for data output. */
static uint32_t alignment = 4;
static double compress_min_ratio = 0.0;
static uint32_t compress_min_size = 0;

/*-----------------------------------------------------------------------*/

/* Local function declarations. */
static FileInfo *read_control_file(const char *filename, uint32_t *nfiles_ret);
static char *strtopath(char *s, char **end);
static int append_one_file(FileInfo **filelist_ptr, uint32_t *nfiles_ptr,
                           const char *pathname, const char *realfile,
                           uint32_t flags);
static int append_matching_files(FileInfo **filelist_ptr, uint32_t *nfiles_ptr,
                                 const char *replace, const char *pattern,
                                 uint32_t flags);
static PKGIndexEntry *filelist_to_index(FileInfo *filelist, int nfiles,
                                         char **namebuf_ret,
                                         uint32_t *namesize_ret);
static int write_package(const char *filename, FileInfo *filelist,
                         PKGIndexEntry *index, uint32_t nfiles,
                         const char *namebuf, uint32_t namesize);

static void pkg_sort(PKGIndexEntry * const index, const uint32_t nfiles,
                      const char *namebuf,
                      const uint32_t left, const uint32_t right);

/*************************************************************************/
/***************************** Main routine ******************************/
/*************************************************************************/

/**
 * main:  Program entry point.  Command line usage is as follows:
 *    build-pkg [options] control-file.txt output-file.pkg
 * where options can be any of:
 *    -alignment=N: Alignment, in bytes, for data files.  Padding will be
 *        inserted as necessary to ensure each file's data starts at an
 *        offset which is a multiple of this value.  The default is 4.
 *    -compress-min-size=N: Minimum size, in bytes, at which to enable
 *        compression of data files.  Files smaller than this size will
 *        never be compressed, even if compression is specified in the
 *        control file.  The default is 0 (all files designated for
 *        compression will be compressed).
 *    -compress-min-ratio=N: Minimum compression ratio to accept, where
 *        a ratio of 0.0 means no change in size and values approaching
 *        1.0 indicate smaller compressed output.  After compressing a
 *        file, if the resulting compression ratio is less than this
 *        value, the compressed data will be discarded and the file will
 *        be stored uncompressed.  The default is 0.0 (compressed data
 *        will always be accepted unless it is larger than the input data).
 *
 * [Parameters]
 *     argc: Command line argument count.
 *     argv: Command line argument array.
 * [Return value]
 *     0 on success, 1 on processing error, 2 on command line error.
 */
int main(int argc, char **argv)
{
    while (argc > 1 && argv[1][0] == '-') {
        if (strncmp(argv[1], "-alignment=", 11) == 0) {
            char *s;
            alignment = strtoul(argv[1]+11, &s, 0);
            if (*s || alignment == 0) {
                fprintf(stderr, "Invalid alignment value: %s\n", argv[1]+11);
                return 2;
            }
        } else if (strncmp(argv[1], "-compress-min-size=", 19) == 0) {
            char *s;
            compress_min_size = strtoul(argv[1]+19, &s, 0);
            if (*s) {
                fprintf(stderr, "Invalid size: %s\n", argv[1]+19);
                return 2;
            }
        } else if (strncmp(argv[1], "-compress-min-ratio=", 20) == 0) {
            char *s;
            compress_min_ratio = strtod(argv[1]+20, &s);
            if (*s || compress_min_ratio > 1.0) {
                fprintf(stderr, "Invalid compression ratio: %s\n", argv[1]+20);
                return 2;
            }
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

    if (argc != 3) {
      usage:
        fprintf(stderr,
                "Usage: %s [options] <control-file> <output-file>\n"
                "Options:\n"
                "-alignment=N: Align files offsets to a multiple of N bytes.\n"
                "-compress-min-size=N: Don't compress files smaller than N bytes.\n"
                "-compress-min-ratio=N: Skip compression if gain is < N (0.0-1.0).\n",
                argv[0]);
        return 2;
    }

    /* (1) Read in the control file. */
    uint32_t nfiles;
    FileInfo *filelist = read_control_file(argv[1], &nfiles);
    if (!filelist) {
        return 1;
    }

    /* (2) Create the package index. */
    char *namebuf;
    uint32_t namesize;
    PKGIndexEntry *index = filelist_to_index(filelist, nfiles,
                                             &namebuf, &namesize);
    if (!index) {
        return 1;
    }

    /* (3) Write out the package file. */
    if (!write_package(argv[2], filelist, index, nfiles, namebuf, namesize)) {
        return 1;
    }

    /* All done! */
    return 0;
}

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

/**
 * read_control_file:  Read in the control file and generate the input
 * file list.
 *
 * [Parameters]
 *     filename: Control file pathname.
 *     nfiles_ret: Pointer to variable to receive the number of files.
 * [Return value]
 *     Newly-allocated array of FileInfo structures, or NULL on error.
 */
static FileInfo *read_control_file(const char *filename, uint32_t *nfiles_ret)
{
    FileInfo *filelist = NULL;
    uint32_t nfiles = 0;

    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "fopen(%s): %s\n", filename, strerror(errno));
        goto error_return;
    }

    int line = 0;
    char buf[LINEMAX+2];  // Leave room for \n\0 at the end of the buffer.
    while (fgets(buf, sizeof(buf), f)) {
        line++;
        char *s = buf + strlen(buf);
        if (s > buf && s[-1] == '\n') {
            *--s = 0;
        }
        if (s > buf && s[-1] == '\r') {
            *--s = 0;
        }
        s = buf + strspn(buf, " \t");
        if (!*s || *s == '#') {
            continue;
        }
        if (*s == '=') {
            fprintf(stderr, "%s:%d: Pathname missing\n", filename, line);
            goto error_close_file;
        }

        char *pathname = buf;
        uint32_t flags = 0;
        if (strnicmp(pathname, "deflate:", 8) == 0) {
            flags = PKGF_DEFLATED;
            pathname += 8;
        }

        char *realfile;
        pathname = strtopath(pathname, &s);
        if (!pathname) {
            fprintf(stderr, "%s:%d: Pathname missing or invalid\n",
                    filename, line);
            goto error_close_file;
        }
        s += strspn(s, " \t");
        if (!*s) {
            realfile = pathname;
            pathname = NULL;
        } else {
            if (*s != '=') {
                fprintf(stderr, "%s:%d: Invalid format (unquoted spaces"
                        " not allowed in pathnames)\n", filename, line);
                goto error_close_file;
            }
            realfile = strtopath(s+1, &s);
            if (!realfile) {
                fprintf(stderr, "%s:%d: Real filename missing or invalid\n",
                        filename, line);
                goto error_close_file;
            }
            s += strspn(s, " \t");
            if (*s) {
                fprintf(stderr, "%s:%d: Junk at end of line\n",
                        filename, line);
                goto error_close_file;
            }
        }

        if (!strchr(realfile, '%')) {
            if (!append_one_file(&filelist, &nfiles, pathname, realfile,
                                 flags)) {
                fprintf(stderr, "%s:%d: Error adding file\n", filename, line);
                goto error_close_file;
            }
        } else {
            if (!append_matching_files(&filelist, &nfiles, pathname,
                                       realfile, flags)) {
                fprintf(stderr, "%s:%d: Error adding files\n", filename, line);
                goto error_close_file;
            }
        }
    }

    fclose(f);
    *nfiles_ret = nfiles;
    return filelist;

  error_close_file:
    fclose(f);
  error_return:
    free(filelist);
    return NULL;
}

/*-----------------------------------------------------------------------*/

/**
 * strtopath:  Read a pathname starting from the first non-whitespace
 * character in "s".  If the first non-whitespace character is a double
 * quote character, the pathname is treated as a quoted string and is
 * terminated by a second (unescaped) double quote character; otherwise,
 * the pathname is terminated by a whitespace character.
 *
 * Note that the portion of the input string following the filename may be
 * moved from its original position by this function.
 *
 * [Parameters]
 *     s: String from which to read pathname.
 *     end: Pointer to variable to receive a pointer to the first character
 *         after the end of the pathname.
 * [Return value]
 *     Parsed pathname (copied with strdup()), or NULL if no pathname was
 *     found.
 */
static char *strtopath(char *s, char **end)
{
    while (*s && isspace(*s)) {
        s++;
    }
    if (!*s) {
        return NULL;
    }

    char *filename = s;
    if (*s++ == '"') {
        filename = s;
        while (*s && *s != '"') {
            if (*s == '\\') {
                if (!s[1]) {
                    fprintf(stderr, "Stray backslash at end of line: %s\n",
                            filename);
                    return NULL;
                }
                memmove(s, s+1, strlen(s+1)+1);
            }
            s++;
        }
        if (!*s) {
            fprintf(stderr, "Unterminated quoted pathname: %s\n", filename);
            return NULL;
        }
        *s++ = 0;
        filename = strdup(filename);
    } else {
        while (*s && !isspace(*s)) {
            s++;
        }
        char c = *s;
        *s = 0;
        filename = strdup(filename);
        *s = c;
    }
    *end = s;
    return filename;
}

/*-----------------------------------------------------------------------*/

/**
 * append_one_file:  Append a single file to the file list.
 *
 * [Parameters]
 *     filelist_ptr: Pointer to variable holding the FileInfo structure array.
 *     nfiles_ptr: Pointer to variable holding the number of files.
 *     pathname: Pathname to record in the package, or NULL to use the value
 *         of realfile.
 *     realfile: Pathname of the file to read from.
 *     flags: Index flags (PKGF_*).
 * [Return value]
 *     True on success, false on error (out of memory).
 */
static int append_one_file(FileInfo **filelist_ptr, uint32_t *nfiles_ptr,
                           const char *pathname, const char *realfile,
                           uint32_t flags)
{
    PRECOND(filelist_ptr != NULL, return 0);
    PRECOND(nfiles_ptr != NULL, return 0);
    PRECOND(realfile != NULL, return 0);

    FileInfo *filelist = *filelist_ptr;
    int nfiles = *nfiles_ptr;

    filelist = realloc(filelist, (nfiles+1) * sizeof(*filelist));
    if (!filelist) {
        fprintf(stderr, "Out of memory\n");
        return 0;
    }
    *filelist_ptr = filelist;
    filelist[nfiles].pathname    = strdup(pathname ? pathname : realfile);
    filelist[nfiles].realfile    = strdup(realfile);
    filelist[nfiles].flags       = flags;
    filelist[nfiles].index_entry = -1;  // Not yet set.
    if (!filelist[nfiles].pathname || !filelist[nfiles].realfile) {
        fprintf(stderr, "Out of memory\n");
        return 0;
    }
    *nfiles_ptr = nfiles+1;
    return 1;
}

/*-----------------------------------------------------------------------*/

static int strcmp_ptr(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

/**
 * append_matching_files:  Append all files matching a pattern to the file
 * list.
 *
 * [Parameters]
 *     filelist_ptr: Pointer to variable holding the FileInfo structure array.
 *     nfiles_ptr: Pointer to variable holding the number of files.
 *     replace: Replacement pattern with which to generate package pathnames,
 *         or NULL for no change in pathname.
 *     pattern: Pattern for matching files.
 *     flags: Index flags (PKGF_*).
 * [Return value]
 *     True on success, false on error.
 */
static int append_matching_files(FileInfo **filelist_ptr, uint32_t *nfiles_ptr,
                                 const char *replace, const char *pattern,
                                 uint32_t flags)
{
    PRECOND(filelist_ptr != NULL, return 0);
    PRECOND(nfiles_ptr != NULL, return 0);
    PRECOND(pattern != NULL, return 0);
    PRECOND(strchr(pattern,'%') != NULL, return 0);
    if (!replace) {
        replace = pattern;
    }

    char dirpath[1000], patbefore[1000], patafter[1000];
    char substbefore[1000], substafter[1000];
    const char *s = strrchr(pattern, '/');
    if (s) {
        unsigned int n = snprintf(dirpath, sizeof(dirpath), "%.*s",
                                  (int)(s-pattern), pattern);
        if (n >= sizeof(dirpath)) {
            fprintf(stderr, "Buffer overflow on directory name\n");
            return 0;
        } else if (strchr(dirpath, '%')) {
            fprintf(stderr, "'%%' not allowed in directory name\n");
            return 0;
        }
        pattern = s+1;
    } else {
        strcpy(dirpath, ".");
    }
    s = strchr(pattern, '%');
    const unsigned int beforelen =
        snprintf(patbefore, sizeof(patbefore), "%.*s",
                 (int)(s-pattern), pattern);
    if (beforelen >= sizeof(patbefore)) {
        fprintf(stderr, "Buffer overflow on pattern prefix\n");
        return 0;
    }
    const unsigned int afterlen =
        snprintf(patafter, sizeof(patafter), "%s", s+1);
    if (afterlen >= sizeof(patafter)) {
        fprintf(stderr, "Buffer overflow on pattern suffix\n");
        return 0;
    }
    s = strchr(replace, '%');
    if (!s) {
        fprintf(stderr, "No '%%' found in replacement string\n");
        return 0;
    }
    unsigned int n = snprintf(substbefore, sizeof(substbefore), "%.*s",
                              (int)(s-replace), replace);
    if (n >= sizeof(patbefore)) {
        fprintf(stderr, "Buffer overflow on replacement prefix\n");
        return 0;
    }
    n = snprintf(substafter, sizeof(substafter), "%s", s+1);
    if (n >= sizeof(substafter)) {
        fprintf(stderr, "Buffer overflow on replacement suffix\n");
        return 0;
    }

    char **dirfiles = NULL;
    int dirfiles_size = 0;
    DIR *dir = opendir(dirpath);
    if (!dir) {
        if (errno == ENOENT) {
            fprintf(stderr, "Warning: %s: %s\n", dirpath, strerror(errno));
            return 1;  // Nonexistence isn't an error, it just means no match.
        } else {
            perror(dirpath);
        }
    }
    struct dirent *de;
    while ((de = readdir(dir)) != NULL) {
        char *fullpath = malloc(strlen(dirpath) + 1 + strlen(de->d_name) + 1);
        if (!fullpath) {
            fprintf(stderr, "Out of memory\n");
            closedir(dir);
            return 0;
        }
        sprintf(fullpath, "%s/%s", dirpath, de->d_name);
        struct stat st;
        if (stat(fullpath, &st) != 0) {
            fprintf(stderr, "stat(%s): %s\n", fullpath, strerror(errno));
            closedir(dir);
            return 0;
        }
        if (!S_ISREG(st.st_mode)) {
            continue;
        }
        dirfiles = realloc(dirfiles, sizeof(char *) * (dirfiles_size+1));
        if (!dirfiles) {
            fprintf(stderr, "Out of memory\n");
            closedir(dir);
            return 0;
        }
        dirfiles[dirfiles_size] = strdup(de->d_name);
        if (!dirfiles[dirfiles_size]) {
            fprintf(stderr, "Out of memory\n");
            closedir(dir);
            return 0;
        }
        dirfiles_size++;
    }
    closedir(dir);

    qsort(dirfiles, dirfiles_size, sizeof(char *), strcmp_ptr);

    for (int i = 0; i < dirfiles_size; i++) {
        const char *file = dirfiles[i];
        if (beforelen > 0 && strncmp(file, patbefore, beforelen) != 0) {
            continue;
        }
        if (afterlen > 0) {
            if (afterlen > strlen(file)
             || strncmp(file+strlen(file)-afterlen, patafter, afterlen) != 0) {
                continue;
            }
        }
        char middle[1000];
        n = snprintf(middle, sizeof(middle), "%.*s",
                     (int)strlen(file)-beforelen-afterlen, file+beforelen);
        if (n >= sizeof(middle)) {
            fprintf(stderr, "Buffer overflow on pattern check for %s\n", file);
            return 0;
        }
        char pathname[1000], realfile[1000];
        n = snprintf(realfile, sizeof(realfile), "%s/%s", dirpath, file);
        if (n >= sizeof(realfile)) {
            fprintf(stderr, "Buffer overflow on path generation for %s\n",
                    file);
            return 0;
        }
        n = snprintf(pathname, sizeof(pathname), "%s%s%s",
                     substbefore, middle, substafter);
        if (n >= sizeof(pathname)) {
            fprintf(stderr, "Buffer overflow on pattern substitution for %s\n",
                    file);
            return 0;
        }
        if (!append_one_file(filelist_ptr, nfiles_ptr, pathname, realfile,
                             flags)) {
            fprintf(stderr, "append_one_file() failed for %s = %s\n",
                    pathname, realfile);
            return 0;
        }
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

/**
 * filelist_to_index:  Generate the package index from the file list.
 *
 * [Parameters]
 *     filelist: Array of FileInfo structures.
 *     nfiles: Number of files.
 *     namebuf_ret: Pointer to variables to receive the pathname data
 *         buffer (newly allocated).
 *     namesize_ret: Pointer to variables to receive the pathname data
 *         buffer size, in bytes.
 * [Return value]
 *     Newly-allocated array of PKGIndexEntry structures, or NULL on error.
 */
static PKGIndexEntry *filelist_to_index(FileInfo *filelist, int nfiles,
                                        char **namebuf_ret,
                                        uint32_t *namesize_ret)
{
    PKGIndexEntry *index = NULL;
    char *namebuf = NULL;
    uint32_t namesize = 0;

    index = malloc(sizeof(*index) * nfiles);
    if (!index) {
        fprintf(stderr, "Out of memory for index\n");
        goto error_return;
    }

    for (int i = 0; i < nfiles; i++) {
        FILE *f = fopen(filelist[i].realfile, "rb");
        if (!f) {
            fprintf(stderr, "Failed to open %s", filelist[i].realfile);
            if (strcmp(filelist[i].realfile, filelist[i].pathname) != 0) {
                fprintf(stderr, " (for %s)", filelist[i].pathname);
            }
            fprintf(stderr, ": %s\n", strerror(errno));
            goto error_return;
        }
        if (fseek(f, 0, SEEK_END) != 0) {
            fprintf(stderr, "fseek(%s): %s\n", filelist[i].realfile,
                    strerror(errno));
            fclose(f);
            goto error_return;
        }
        const uint32_t filesize = ftell(f);
        fclose(f);

        if (filesize < compress_min_size) {
            filelist[i].flags &= ~PKGF_DEFLATED;
        }

        index[i].hash          = pkg_hash(filelist[i].pathname);
        index[i].nameofs_flags = namesize | filelist[i].flags;
        /*       offset will be set later. */
        index[i].datalen       = filesize;
        index[i].filesize      = filesize;

        const uint32_t thisnamelen = strlen(filelist[i].pathname) + 1;
        namebuf = realloc(namebuf, namesize + thisnamelen);
        if (!namebuf) {
            fprintf(stderr, "Out of memory for namebuf (%s)\n",
                    filelist[i].pathname);
            goto error_return;
        }
        memcpy(namebuf + namesize, filelist[i].pathname, thisnamelen);
        namesize += thisnamelen;
    }

    pkg_sort(index, nfiles, namebuf, 0, nfiles-1);
    for (int i = 0; i < nfiles; i++) {
        filelist[i].index_entry = -1;
        const uint32_t hash = pkg_hash(filelist[i].pathname);
        for (int j = 0; j < nfiles; j++) {
#define NAME(a)  (namebuf + PKG_NAMEOFS(index[(a)].nameofs_flags))
            if (index[j].hash == hash
             && stricmp(filelist[i].pathname, NAME(j)) == 0) {
                filelist[i].index_entry = j;
                break;
            }
#undef NAME
        }
        if (filelist[i].index_entry < 0) {
            fprintf(stderr, "File %s lost from index!\n",
                    filelist[i].pathname);
            goto error_return;
        }
    }

    *namebuf_ret = namebuf;
    *namesize_ret = namesize;
    return index;

  error_return:
    free(namebuf);
    free(index);
    return NULL;
}

/*-----------------------------------------------------------------------*/

/**
 * write_package:  Write out the package file.
 *
 * [Parameters]
 *     filename: Pathname of package file to create.
 *     filelist: Array of FileInfo structures.
 *     index: Array of PKGIndexEntry structures (contents will be destroyed).
 *     nfiles: Number of files.
 *     namebuf: Pathname data buffer.
 *     namesize: Pathname data buffer size, in bytes.
 * [Return value]
 *     True on success, false on error.
 */
static int write_package(const char *filename, FileInfo *filelist,
                         PKGIndexEntry *index, uint32_t nfiles,
                         const char *namebuf, uint32_t namesize)
{
    FILE *pkg = fopen(filename, "wb");
    if (!pkg) {
        fprintf(stderr, "Failed to create %s: %s\n", filename,
                strerror(errno));
        goto error_return;
    }
    uint32_t offset = 0;

    PKGHeader header;
    memset(&header, 0, sizeof(header));
    memcpy(header.magic, PKG_MAGIC, sizeof(header.magic));
    header.header_size = sizeof(PKGHeader);
    header.entry_size  = sizeof(PKGIndexEntry);
    header.entry_count = nfiles;
    header.name_size   = namesize;
    PKG_HEADER_SWAP_BYTES(header);
    if (fwrite(&header, sizeof(header), 1, pkg) != 1) {
        fprintf(stderr, "Write error on %s (header): %s\n", filename,
                strerror(errno));
        goto error_close_pkg;
    }
    offset += sizeof(header);

    const uint32_t index_offset = offset;
    PKG_INDEX_SWAP_BYTES(index, nfiles);
    if (fwrite(index, sizeof(*index), nfiles, pkg) != nfiles) {
        fprintf(stderr, "Write error on %s (index): %s\n", filename,
                strerror(errno));
        goto error_close_pkg;
    }
    PKG_INDEX_SWAP_BYTES(index, nfiles);  // So we can use them later.
    offset += sizeof(*index) * nfiles;

    if (fwrite(namebuf, namesize, 1, pkg) != 1) {
        fprintf(stderr, "Write error on %s (name table): %s\n", filename,
                strerror(errno));
        goto error_close_pkg;
    }
    offset += namesize;

    for (uint32_t i = 0; i < nfiles; i++) {

        while (offset % alignment != 0) {
            if (fputc(0, pkg) == EOF) {
                fprintf(stderr, "Write error on %s (padding for %s): %s\n",
                        filename, filelist[i].pathname, strerror(errno));
                goto error_close_pkg;
            }
            offset++;
        }

        index[filelist[i].index_entry].offset = offset;  // Rewritten later.

        FILE *f = fopen(filelist[i].realfile, "rb");
        if (!f) {
            fprintf(stderr, "Failed to open %s while writing package: %s\n",
                    filelist[i].realfile, strerror(errno));
            goto error_close_pkg;
        }

      retry_without_compression:;
        z_stream *deflater = NULL, deflater_buf;
        if (filelist[i].flags & PKGF_DEFLATED) {
            deflater = &deflater_buf;
            deflater->zalloc = Z_NULL;
            deflater->zfree = Z_NULL;
            deflater->opaque = Z_NULL;
            if (deflateInit(deflater, 9) != Z_OK) {
                fprintf(stderr, "deflateInit() failed for %s: %s",
                        filelist[i].pathname, deflater->msg);
                goto error_close_pkg;
            }
        }

        const uint32_t filesize = index[filelist[i].index_entry].filesize;
        uint8_t buf[65536];
        uint32_t copied = 0;
        while (copied < filesize) {
            const uint32_t tocopy = ubound(filesize - copied, sizeof(buf));
            const int32_t nread = fread(buf, 1, tocopy, f);
            if (nread < 0 || (uint32_t)nread != tocopy) {
                fprintf(stderr, "Failed to read from %s while writing"
                        " package: %s\n", filelist[i].realfile,
                        (nread < 0) ? strerror(errno) : "Unexpected EOF");
                fclose(f);
                goto error_close_pkg;
            }

            uint32_t towrite;
            if (deflater) {
                uint8_t inbuf[sizeof(buf)];
                memcpy(inbuf, buf, nread);
                deflater->next_in = inbuf;
                deflater->avail_in = nread;
                deflater->next_out = buf;
                deflater->avail_out = sizeof(buf);
                int result = deflate(deflater, Z_NO_FLUSH);
                if (result != Z_OK) {
                    fprintf(stderr, "deflate() failed for %s: %s\n",
                            filelist[i].pathname, deflater->msg);
                    fclose(f);
                    goto error_close_pkg;
                }
                towrite = deflater->next_out - buf;
            } else {
                towrite = (uint32_t)nread;
            }

            if (towrite > 0 && fwrite(buf, 1, towrite, pkg) != towrite) {
                fprintf(stderr, "Write error on %s (data for %s): %s\n",
                        filename, filelist[i].pathname, strerror(errno));
                fclose(f);
                goto error_close_pkg;
            }

            copied += nread;
            offset += towrite;
        }  // while (copied < filesize)

        if (deflater) {
            int result;
            do {
                deflater->avail_in = 0;
                deflater->next_out = buf;
                deflater->avail_out = sizeof(buf);
                result = deflate(deflater, Z_FINISH);
                if (result != Z_OK && result != Z_STREAM_END) {
                    fprintf(stderr, "deflate(Z_FINISH) failed for %s: %s\n",
                            filelist[i].pathname, deflater->msg);
                    fclose(f);
                    goto error_close_pkg;
                }
                const uint32_t towrite = deflater->next_out - buf;
                if (towrite > 0 && fwrite(buf, 1, towrite, pkg) != towrite) {
                    fprintf(stderr, "Write error on %s (data for %s): %s\n",
                            filename, filelist[i].pathname, strerror(errno));
                    fclose(f);
                    goto error_close_pkg;
                }
                offset += towrite;
            } while (result != Z_STREAM_END);

            index[filelist[i].index_entry].datalen =
                offset - index[filelist[i].index_entry].offset;
            double ratio = 1.0 - (index[filelist[i].index_entry].datalen
                                  / index[filelist[i].index_entry].filesize);
            if (ratio < compress_min_ratio) {
                offset = index[filelist[i].index_entry].offset;
                if (fseek(pkg, offset, SEEK_SET) != 0) {
                    fprintf(stderr, "Failed to seek in %s while undoing"
                            " compression of %s: %s\n", filename,
                            filelist[i].pathname, strerror(errno));
                    fclose(f);
                    goto error_close_pkg;
                }
                if (ftruncate(fileno(pkg), offset) != 0) {
                    fprintf(stderr, "Failed to truncate %s while undoing"
                            " compression of %s: %s\n", filename,
                            filelist[i].pathname, strerror(errno));
                    fclose(f);
                    goto error_close_pkg;
                }
                filelist[i].flags &= ~PKGF_DEFLATED;
                index[filelist[i].index_entry].nameofs_flags &= ~PKGF_DEFLATED;
                index[filelist[i].index_entry].datalen =
                    index[filelist[i].index_entry].filesize;
                if (fseek(f, 0, SEEK_SET) != 0) {
                    fprintf(stderr, "Failed to seek in %s while undoing"
                            " compression: %s\n", filelist[i].pathname,
                            strerror(errno));
                    fclose(f);
                    goto error_close_pkg;
                }
                goto retry_without_compression;
            }
        }  // if (deflater)

        fclose(f);

    }  // for (i = 0; i < nfiles; i++)

    /* Now that all file offsets are known, rewrite the package index. */
    if (fseek(pkg, index_offset, SEEK_SET) != 0) {
        fprintf(stderr, "Seek error on %s (index rewrite): %s\n",
                filename, strerror(errno));
        goto error_close_pkg;
    }
    PKG_INDEX_SWAP_BYTES(index, nfiles);
    if (fwrite(index, sizeof(*index), nfiles, pkg) != nfiles) {
        fprintf(stderr, "Write error on %s (index rewrite): %s\n",
                filename, strerror(errno));
        goto error_close_pkg;
    }

    fclose(pkg);
    return 1;

  error_close_pkg:
    fclose(pkg);
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

/**
 * pkg_sort:  Sort an array of PKGIndexEntry structures using the
 * quicksort algorithm.
 *
 * [Parameters]
 *     index: Array of PKGIndexEntry structures.
 *     nfiles: Number of entries in index[] (i.e., number of files).
 *     namebuf: Pathname data buffer.
 *     left: Lowest entry number to sort.
 *     right: Highest entry number to sort.
 */
static void pkg_sort(PKGIndexEntry * const index, const uint32_t nfiles,
                     const char *namebuf,
                     const uint32_t left, const uint32_t right)
{
#define NAME(a)  (namebuf + PKG_NAMEOFS(index[(a)].nameofs_flags))
#define LESS(a)  (index[(a)].hash < pivot_hash   \
               || (index[(a)].hash == pivot_hash \
                   && stricmp(NAME(a), pivot_name) < 0))
#define SWAP(a,b) do {        \
    PKGIndexEntry tmp;        \
    tmp = index[(a)];         \
    index[(a)] = index[(b)];  \
    index[(b)] = tmp;         \
} while (0)

    if (left >= right) {
        return;
    }
    const uint32_t pivot = (left + right + 1) / 2;
    const uint32_t pivot_hash = index[pivot].hash;
    const char * const pivot_name = NAME(pivot);
    SWAP(pivot, right);

    uint32_t store = left;
    while (store < right && LESS(store)) {
        store++;
    }
    for (uint32_t i = store+1; i <= right-1; i++) {
        if (LESS(i)) {
            SWAP(i, store);
            store++;
        }
    }
    SWAP(right, store);  // right == old pivot

    if (store > 0) {
        pkg_sort(index, nfiles, namebuf, left, store-1);
    }
    pkg_sort(index, nfiles, namebuf, store+1, right);
}

/*************************************************************************/
/*************************************************************************/
