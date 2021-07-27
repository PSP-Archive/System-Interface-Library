/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * tools/makefont.c: Program to create a font file for use with the bitmap
 * font functionality from a texture and character list.
 */

/*
 * To use, run this program as:
 *
 *    makefont -texture=texture.tex -charlist=list.txt outfile.font
 *
 * The font texture (texture.tex) in the example command line above) can be
 * in any format recognized by SIL, including *.tex (the SIL texture file
 * format), *.dds, or *.png (if libpng is enabled when building).
 *
 * The character list (list.txt in the example command line above) is a
 * text file which describes the parameters of the font and the characters
 * (glyphs) contained within.  # anywhere on a line, except within a
 * quoted character name for the "char" keyword, begins a comment.
 * Otherwise, each nonblank line begins with a keyword, followed by
 * parameters for the keyword.  The font-global keywords are:
 *    - height <pixels>  (height of one line)
 *    - baseline <pixels>  (offset from the top of a line to the font baseline)
 * The other supported keyword is "char", which describes a single character:
 *    char <char> <x> <y> <width> <height> <ascent> <prekern> <postkern>
 * where:
 *    - <char> is either a character enclosed in single or double quotes,
 *      or a Unicode codepoint specified as U+xxxx (xxxx in hexadecimal);
 *    - <x> and <y> are the pixel coordinates of the top-left pixel in the
 *      character glyph, treating the upper-left corner of the texture as
 *      coordinates (0,0);
 *    - <width> and <height> are the pixel size of the character glyph;
 *    - <ascent> is the height above the baseline (possibly negative) at
 *      which the top pixel line of the character glyph is drawn (or
 *      equivalently, the number of pixel lines drawn above the baseline);
 *    - <prekern> is the number of pixels (possibly negative) to advance
 *      the X coordinate before drawing the character glyph; and
 *    - <postkern> is the number of pixels (possibly negative) to advance
 *      the X coordinate after drawing the character glyph and advancing by
 *      <width>.
 * The values of <prekern> and <postkern> may include fractional parts;
 * other numerical character parameters must be integers.
 *
 * Example character list:
 *    # Line height (vertical distance from the top of one line to the top
 *    # of the next) is 16 pixels; thus a font size of 16 will draw text at
 *    # a 1:1 scale relative to the texture.
 *    height 16
 *    # Baseline is 2 pixels from the bottom edge of the line.
 *    baseline 14
 *    # 11-pixel-high character on the baseline.
 *    char '!' 0 0 3 11 11 1 1
 *    # 16-pixel-high character dipping below the baseline, with the next
 *    # character shifted half a pixel backward into this one.
 *    char U+0028 3 0 6 16 14 1.5 -0.5
 */

#include "tool-common.h"
#include "util.h"
#include "../src/utility/font-file.h"

#include <errno.h>
#include <math.h>


/*************************************************************************/
/*************************************************************************/

/* Alignment for texture data, in bytes. */
#define TEXTURE_ALIGN  64

/*-----------------------------------------------------------------------*/

/* Local function declarations. */

static int read_charlist(
    const char *charlist, FontFileHeader *header_ret,
    FontFileCharInfo **charinfo_ret, int *charinfo_count_ret);

static int write_font(
    const char *outfile, const FontFileHeader *header,
    const void *texture_data, int32_t texture_size,
    const FontFileCharInfo *charinfo, int charinfo_count);

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
    const char *texfile = NULL, *charlist = NULL, *outfile = NULL;
    int argi;
    for (argi = 1; argi < argc && argv[argi][0] == '-'; argi++) {
        if (strncmp(argv[argi], "-texture=", 9) == 0) {
            if (argv[argi][9] == 0) {
                fprintf(stderr, "Missing argument for option -texture\n");
                goto usage;
            }
            texfile = &argv[argi][9];
        } else if (strncmp(argv[argi], "-charlist=", 10) == 0) {
            if (argv[argi][10] == 0) {
                fprintf(stderr, "Missing argument for option -charlist\n");
                goto usage;
            }
            charlist = &argv[argi][10];
        } else {
            goto usage;
        }
    }
    if (!texfile || !charlist || argi != argc-1) {
      usage:
        fprintf(stderr, "Usage: %s -texture=texture.tex"
                " -charlist=charlist.txt outfile.font\n", argv[0]);
        return 1;
    }
    outfile = argv[argi];

    uint32_t texture_size;
    void *texture_data = read_file(texfile, &texture_size);
    if (!texture_data) {
        fprintf(stderr, "Failed to read %s: %s\n", texfile, strerror(errno));
        return 1;
    }
    if (texture_size > 0x7FFFFFFFU) {
        fprintf(stderr, "%s: File too large\n", texfile);
        return 1;
    }

    FontFileHeader header;
    FontFileCharInfo *charinfo;
    int charinfo_count;
    if (!read_charlist(charlist, &header, &charinfo, &charinfo_count)) {
        return 1;
    }

    if (!write_font(outfile, &header, texture_data, texture_size, charinfo,
                    charinfo_count)) {
        return 1;
    }

    free(texture_data);
    free(charinfo);
    return 0;
}

/*-----------------------------------------------------------------------*/

/**
 * read_charlist:  Read a font description file and return the font data.
 *
 * [Parameters]
 *     charlist: Path to font description file.
 *     header_ret: Pointer to FontFileHeader structure to be filled in.
 *     charinfo_ret: Pointer to variable to receive a pointer to a buffer
 *         (allocated with malloc()) containing the read-in FontFileCharInfo
 *         entries.
 *     charinfo_count_ret: Pointer to variable to receive the number of
 *         FontFileCharInfo entries stored in *charinfo_ret.
 * [Return value]
 *     True on success, false on error.
 */
static int read_charlist(
    const char *charlist, FontFileHeader *header_ret,
    FontFileCharInfo **charinfo_ret, int *charinfo_count_ret)
{
    PRECOND(charlist != NULL);
    PRECOND(header_ret != NULL);
    PRECOND(charinfo_ret != NULL);
    PRECOND(charinfo_count_ret != NULL);

    static const char WHITESPACE[] = " \t\v\r\n";

    FILE *f = fopen(charlist, "r");
    if (!f) {
        fprintf(stderr, "Failed to open %s: %s\n", charlist, strerror(errno));
        return 0;
    }

    FontFileCharInfo *charinfo = NULL;
    int charinfo_size = 0, charinfo_count = 0;

    int errors = 0;
    int linenum = 0;
    char line[1000];
    while (fgets(line, sizeof(line), f)) {
        ASSERT(strlen(line) > 0);
        if(line[strlen(line)-1] != '\n') {
            fprintf(stderr, "%s:%d: Line too long, aborting\n", charlist,
                    linenum);
            fclose(f);
            return 0;
        }

        char *s;

        /* Strip comments.  This is a little tricky because we need to
         * allow "char '#'". */
        s = line + strspn(line, WHITESPACE);
        if (strncmp(s,"char",4) == 0 && s[4] != 0 && strchr(WHITESPACE,s[4])) {
            s += 4;
            s += strspn(s, WHITESPACE);
            if ((*s == '\'' || *s == '"') && s[1] == '#') {
                s += 2;
            }
        }
        s = strchr(s, '#');
        if (s) {
            *s = 0;
        }

        /* Parse the line. */
        s = strtok(line, WHITESPACE);
        if (!s) {
            continue;
        }
        if (strcmp(s, "height") == 0) {
            s = strtok(NULL, WHITESPACE);
            if (!s) {
                fprintf(stderr, "%s:%d: Missing argument for keyword"
                        " \"height\"\n", charlist, linenum);
                errors++;
                continue;
            }
            long height = strtol(s, &s, 10);
            if (*s != 0 || height < 1 || height > 255) {
                fprintf(stderr, "%s:%d: Invalid argument for keyword"
                        " \"height\" (must be an integer 1-255)\n",
                        charlist, linenum);
                errors++;
                continue;
            }
            header_ret->height = (uint8_t)height;

        } else if (strcmp(s, "baseline") == 0) {
            s = strtok(NULL, WHITESPACE);
            if (!s) {
                fprintf(stderr, "%s:%d: Missing argument for keyword"
                        " \"baseline\"\n", charlist, linenum);
                errors++;
                continue;
            }
            long baseline = strtol(s, &s, 10);
            if (*s != 0 || baseline < 0 || baseline > 255) {
                fprintf(stderr, "%s:%d: Invalid argument for keyword"
                        " \"baseline\" (must be an integer 0-255)\n",
                        charlist, linenum);
                errors++;
                continue;
            }
            header_ret->baseline = (uint8_t)baseline;

        } else if (strcmp(s, "char") == 0) {
            int32_t ch;
            s = strtok(NULL, "");
            s += strspn(s, WHITESPACE);
            if ((s[0] == '\'' || s[0] == '"') && s[1] != 0) {
                const char quote = *s++;
                ch = utf8_read((const char **)&s);
                if (*s++ != quote) {
                    ch = -1;
                }
            } else if (*s == 'U' && s[1] == '+' && s[2] != 0) {
                unsigned long codepoint = strtoul(s+2, &s, 16);
                if (codepoint > 0x7FFFFFFFUL || !strchr(WHITESPACE, *s)) {
                    ch = -1;
                } else {
                    ch = (int32_t)codepoint;
                }
            } else {
                ch = -1;
            }
            if (ch == -1) {
                fprintf(stderr, "%s:%d: Invalid character specification,"
                        " must be 'c' or U+xxxx\n", charlist, linenum);
                errors++;
                continue;
            }
            int x, y, w, h, ascent, bytes_read;
            float prekern, postkern;
            if (sscanf(s, "%d %d %d %d %d %f %f%n", &x, &y, &w, &h, &ascent,
                       &prekern, &postkern, &bytes_read) < 7) {
                fprintf(stderr, "%s:%d: Missing argument(s) in character"
                        " specification\n", charlist, linenum);
                errors++;
                continue;
            }
            s += bytes_read;
            if (s[strspn(s, WHITESPACE)]) {
                fprintf(stderr, "%s:%d: Extraneous argument(s) in character"
                        " specification\n", charlist, linenum);
                errors++;
                continue;
            }
            if (x < 0 || x > 65535 || y < 0 || y > 65535) {
                fprintf(stderr, "%s:%d: Texture coordinates %d,%d out of"
                        " range; must be in [0...65535]\n", charlist, linenum,
                        x, y);
                errors++;
                continue;
            }
            if (w < 0 || w > 255 || h < 0 || h > 255) {
                fprintf(stderr, "%s:%d: Glyph size %dx%d out of range; must"
                        " be in [0...255]\n", charlist, linenum, w, h);
                errors++;
                continue;
            }
            if (ascent < -128 || ascent > 127) {
                fprintf(stderr, "%s:%d: Ascent %d out of range; must be"
                        " in [-128...+127]\n", charlist, linenum, ascent);
                errors++;
                continue;
            }
            if (prekern < -128.0f || prekern > 32767/256.0f) {
                fprintf(stderr, "%s:%d: Prekern %g out of range; must be in"
                        " [-128...+128)\n", charlist, linenum, prekern);
                errors++;
                continue;
            }
            if (prekern < -128.0f || prekern > 32767/256.0f) {
                fprintf(stderr, "%s:%d: Postkern %.5g out of range; must be in"
                        " [-128...+128)\n", charlist, linenum, postkern);
                errors++;
                continue;
            }

            if (charinfo_count >= charinfo_size) {
                charinfo_size += 100;
                charinfo = realloc(charinfo, sizeof(*charinfo) * charinfo_size);
                if (!charinfo) {
                    fprintf(stderr, "%s:%d: Out of memory\n", charlist,
                            linenum);
                    return 0;
                }
            }
            charinfo[charinfo_count].ch       = ch;
            charinfo[charinfo_count].x        = x;
            charinfo[charinfo_count].y        = y;
            charinfo[charinfo_count].w        = w;
            charinfo[charinfo_count].h        = h;
            charinfo[charinfo_count].ascent   = ascent;
            charinfo[charinfo_count].prekern  = (int16_t)roundf(prekern*256);
            charinfo[charinfo_count].postkern = (int16_t)roundf(postkern*256);
            charinfo_count++;

        } else {
            fprintf(stderr, "%s:%d: Invalid keyword \"%s\"\n", charlist,
                    linenum, s);
            errors++;
        }
    }

    if (header_ret->baseline > header_ret->height) {
        fprintf(stderr, "%s: Baseline cannot be greater than line height\n",
                charlist);
        errors++;
    }

    if (errors > 0) {
        free(charinfo);
        return 0;
    } else {
        *charinfo_ret = charinfo;
        *charinfo_count_ret = charinfo_count;
        return 1;
    }
}

/*-----------------------------------------------------------------------*/

/**
 * write_font:  Write out a font file.
 *
 * [Parameters]
 *     outfile: Path of font file to write.
 *     header: Pointer to FontFileHeader structure for the font (offset and
 *         size fields do not need to be filled in).
 *     texture_data: Pointer to font texture data.
 *     texture_size: Size of font texture data, in bytes.
 *     charinfo: Pointer to array of FontFileCharInfo entries for the font.
 *     charinfo_count: Number of FontFileCharInfo entries in charinfo[].
 * [Return value]
 *     True on success, false on error.
 */
static int write_font(
    const char *outfile, const FontFileHeader *header,
    const void *texture_data, int32_t texture_size,
    const FontFileCharInfo *charinfo, int charinfo_count)
{
    PRECOND(outfile != NULL);
    PRECOND(header != NULL);
    PRECOND(texture_data != NULL);
    PRECOND(texture_size >= 0);
    PRECOND(charinfo != NULL);
    PRECOND(charinfo_count < 65536);

    const uint32_t datasize =
        sizeof(*header) + (sizeof(FontFileCharInfo) * charinfo_count);
    ASSERT(datasize <= 0x7FFFFFFFU);
    if (datasize + (uint32_t)texture_size > 0x7FFFFFFFU) {
        fprintf(stderr, "Font file too large (%u > 2147483647)\n",
                datasize + (uint32_t)texture_size);
        return 0;
    }
    const int32_t tex_align = (align_up(datasize, TEXTURE_ALIGN) - datasize);
    const int32_t bufsize = datasize + tex_align + texture_size;
    char *buffer = calloc(1, bufsize);
    if (!buffer) {
        fprintf(stderr, "Failed to allocate %u bytes for output data\n",
                bufsize);
        return 0;
    }

    FontFileHeader *out_header = (FontFileHeader *)buffer;
    memcpy(out_header->magic, FONT_FILE_MAGIC, sizeof(out_header->magic));
    out_header->version = FONT_FILE_VERSION;
    out_header->height = header->height;
    out_header->baseline = header->baseline;
    out_header->charinfo_offset = s32_to_be(sizeof(FontFileHeader));
    out_header->charinfo_count = s16_to_be(charinfo_count);
    out_header->charinfo_size = s16_to_be(sizeof(FontFileCharInfo));
    out_header->texture_offset = s32_to_be((int32_t)datasize + tex_align);
    out_header->texture_size = s32_to_be(texture_size);

    FontFileCharInfo *out_charinfo =
        (FontFileCharInfo *)(buffer + be_to_u32(out_header->charinfo_offset));
    for (int i = 0; i < charinfo_count; i++) {
        out_charinfo[i].ch       = s32_to_be(charinfo[i].ch);
        out_charinfo[i].x        = u16_to_be(charinfo[i].x);
        out_charinfo[i].y        = u16_to_be(charinfo[i].y);
        out_charinfo[i].w        = charinfo[i].w;
        out_charinfo[i].h        = charinfo[i].h;
        out_charinfo[i].ascent   = charinfo[i].ascent;
        out_charinfo[i].prekern  = s16_to_be(charinfo[i].prekern);
        out_charinfo[i].postkern = s16_to_be(charinfo[i].postkern);
    }

    memcpy(buffer + be_to_u32(out_header->texture_offset), texture_data,
           texture_size);

    if (!write_file(outfile, buffer, bufsize)) {
        fprintf(stderr, "Failed to write %s: %s\n", outfile, strerror(errno));
        free(buffer);
        return 0;
    }

    free(buffer);
    return 1;
}

/*************************************************************************/
/*************************************************************************/
