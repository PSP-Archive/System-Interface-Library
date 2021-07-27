/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * tools/pngtotex.c: Program to convert PNG images to the *.tex custom
 * texture format used by this library.  Requires libpng for reading PNG
 * images.
 */

/*
 * To use, run this program as:
 *
 *    pngtotex [options] file1.png [file2.png...]
 *
 * This will convert all named PNG files to the custom format used by this
 * library, saving the converted files with an extension of .tex replacing
 * any .png extension in the input filenames.
 *
 * The following options can be given (all options must precede the first
 * filename):
 *
 *    -8: Convert textures to 8-bit indexed format.
 *
 *    -a=LO,HI: Specify the low and high alpha thresholds when encoding to
 *        non-32bpp formats; alpha values less than or equal to LO will be
 *        forced to 0, and alpha values greater than or equal to HI will
 *        be forced to 255.  This can improve compression quality by
 *        minimizing variations in alpha that will typically be invisible.
 *        The default is no thresholding, equivalent to -a=0,255.
 *
 *    -bgra: Write 32bpp textures in BGRA pixel order.  If not given, RGBA
 *        pixel order is used.  This does not affect 8bpp paletted textures.
 *
 *    -crop=X,Y+WxH: Crop the input image to X,Y+WxH before resizing and
 *        converting.
 *
 *    -dxt{1|3|5}: Convert textures to DXT1/3/5 compressed format.  If the
 *        texture is opaque, DXT1 is always used because all three formats
 *        encode color components the same way.  Requires a program
 *        "dxtcomp" in the current executable search path which can be
 *        called as:
 *           dxtcomp -{1|3|5} in.rgba out.raw width height
 *        where in.rgba and out.rgba are raw 32bpp pixel data in RGBA byte
 *        order.  On Windows, or on Linux with WINE installed, PVRTexTool
 *        from version 3.23 of the PowerVR SDK can be used for DXTn
 *        compression by using the following Perl script for "dxtcomp":
 *
use strict;
use warnings;
die "Usage: $0 {-1|-3|-5} infile outfile width height\n" if @ARGV != 5;
my $format = "DXT" . substr($ARGV[0],1,1);
my $strip_alpha = ($format eq "DXT1");
shift @ARGV;
my ($infile, $outfile, $width, $height) = @ARGV;
open IN, "<$infile" or die "$infile: $!\n";
binmode IN;
open OUT, ">$outfile.tga" or die "$outfile.tga: $!";
binmode OUT;
print OUT "\0\0\2\0\0\0\0\0\0\0\0\0" or die "$outfile.tga: $!";
print OUT pack("vvv", $width, $height, $strip_alpha ? 24 : 32)
    or die "$outfile.tga: $!";
my $data;
read(IN, $data, $width*$height*4) == $width*$height*4
    or die "$infile: Premature EOF";
my @data = unpack("N*", $data);
for (my $y = $height-1; $y >= 0; $y--) {
    my @row = @data[$y*$width..($y+1)*$width-1];
    # The byte order difference here is intentional: PVRTexTool expects
    # RGB for 24-bit data but BGRA for 32-bit data.
    if ($strip_alpha) {
        print OUT join(
            "", map {pack("CCC", $_>>24 & 255, $_>>16 & 255, $_>>8 & 255)} @row
        );
        for (my $i = 0; $i < @row; $i++) {
            print OUT pack("CCC", $row[$i]>>24 & 255, $row[$i]>>16 & 255,
                                  $row[$i]>>8 & 255);
        }
    } else {
        print OUT pack("V*", map {$_>>8 | $_<<24} @row);
    }
}
close IN;
close OUT;
my @program;
if (`uname -o 2>/dev/null` =~ /Linux/) {
    @program = ("wine", "PVRTexTool.exe");
} else {
    @program = ("PVRTexTool");
}
system @program, "-silent", "-dds", "-f$format", "-i$outfile.tga", "-o$outfile.dds";
$? == 0 or exit $?;
unlink "$outfile.tga" or print STDERR "warning: unlink($outfile.tga): $!\n";
open IN, "<$outfile.dds" or die "$outfile.dds: $!\n";
binmode IN;
open OUT, ">$outfile" or die "$outfile: $!";
binmode OUT;
seek IN, 128, 0;
while (read(IN, $data, 65536) > 0) {
    print OUT $data;
}
close IN;
close OUT;
unlink "$outfile.dds" or print STDERR "warning: unlink($outfile.dds): $!\n";
 *
 *    -hq: When converting to PVRTC format, use the high-quality (slow)
 *        compressor.
 *
 *    -make-square[-center]: When converting to DXT or PVRTC format, if
 *        the texture is not a power-of-two-sized square, expand it unless
 *        doing so would make the compressed data the same size as or
 *        larger than the original data, in which case the data is output
 *        as 32bpp RGBA instead (with no change in size).  If -center is
 *        appended to the option name, the original image is centered in
 *        the final texture; otherwise, the texture data is left at the
 *        (0,0) corner.  The expanded area is filled with transparent black
 *        if the texture is not opaque, else with opaque black.
 *
 *    -make-pot:
 *        texture is not square, expand it unless doing so would make the
 *        compressed data the same size as or larger than the original
 *        data, in which case the data is output as 32bpp RGBA instead.
 *        If -center is appended to the option name, the original image is
 *        centered in the final texture; otherwise, the texture data is
 *        left at the (0,0) corner.  The expanded area is filled with
 *        transparent black if the texture is not opaque, else with opaque
 *        black.
 *
 *    -mipmaps[=N]: Generate mipmaps for the texture.  If N is specified,
 *        only N mipmaps will be generated; otherwise, as many as
 *        appropriate for the texture size will be generated.
 *
 *    -mipmap-regions=x:y:w:h[,...]: Specify subtexture areas for mipmap
 *        generation, such as for a texture atlas.  Each of these areas
 *        will be shrunk separately, and the result will replace the
 *        corresponding region of the shrunk original texture.  Regions
 *        with odd coordinates, either because they were specified that
 *        way or due to shrinking in previous mipmaps, are discarded since
 *        they cannot be mapped directly to a set of output pixels.
 *
 *    -mipmaps-transparent-at=N: Force all mipmaps at level N and greater
 *        (i.e., of size 1/2^N and smaller) to be completely transparent.
 *        This can help avoid graphical glitches due to degenerate or
 *        very thin triangles which select inappropriately small mipmaps.
 *
 *    -opaque-bitmap: Include a bitmap of opaque pixels in the texture.
 *        Useful if opacity data is needed by the program, since texture
 *        pixel data cannot be read on OpenGL ES platforms.
 *
 *    -outdir=OUTDIR: Write output files to the directory OUTDIR.  For
 *        example, "pngtotex [...] -outdir=outdir sub/file.png" writes the
 *        converted texture to "outdir/file.tex".
 *
 *    -psp: Encode textures for the PSP.  Textures will be scaled to half
 *        size (unless -resize is given), set to a scale factor of 0.5
 *        (unless -scale is given), aligned to PSP alignment requiements,
 *        and swizzled.  Note that compression (-dxt, -pvrtc) cannot be
 *        used in PSP mode.
 *
 *    -pvrtc{2|4}: Convert textures to 2-bit (-pvrtc2) or 4-bit (-pvrtc4)
 *        PVRTC compressed format.  Requires the PVRTexToolCLI program to
 *        be located in the current executable search path, unless the
 *        -pvrtextool option is used to specify the program's path.
 *
 *    -pvrtextool=PATH: Specify the path for the PVRTexToolCLI program.
 *
 *    -resize=WxH: Resize the input image to the given size before
 *        converting.  For PSP, this overrides the default action of
 *        shrinking to half size.
 *
 *    -scale=N: Specify the scale factor for the input image (default 1)
 *        without changing the input data.  A scale factor of 0.5 indicates
 *        that the input image is half the size of the original texture;
 *        this allows shrinking of specific textures to save space without
 *        affecting program behavior.
 *
 *    -verbose: Enable verbose output.
 */

#include "tool-common.h"
#include <errno.h>
#include <math.h>
#include <unistd.h>
/* The OSX SDK improperly locks the mkdtemp() declaration behind
 * __DARWIN_C_FULL rather than POSIX-2008.1 (_POSIX_C_SOURCE=200809L),
 * so we have to declare it manually. */
#ifdef __APPLE__
extern char *mkdtemp(char *);
#endif

#include "../src/texture.h"
#include "../src/utility/tex-file.h"

#include "quantize.h"
#include "zoom.h"


#define PNG_USER_MEM_SUPPORTED
#include <png.h>

static jmp_buf png_jmpbuf;

/*************************************************************************/

/* Structure for holding texture data. */
typedef struct Texture Texture;
struct Texture {
    int16_t width, height;  // Texture size (pixels).
    int16_t stride;         // Texture line stride (pixels, always a
                            //    multiple of 16 bytes for the PSP).
    uint8_t format;         // Pixel format (TEX_FORMAT_*).
    uint8_t swizzled;       // 1 = data is swizzled (PSP only).
    uint8_t mipmaps;        // Number of mipmap levels, _not_ including
                            //    primary texture data; odd sizes are
                            //    rounded down when halving to compute
                            //    mipmap width/height.
    uint32_t *palette;      // Color palette (for indexed-color images).
    uint8_t *pixels;        // Pixel data; mipmaps are appended immediately
                            //    following the primary texture data in
                            //    decreasing size order.
    uint8_t *opaque_bitmap; // Opaque bitmap data, or NULL if not present.
};

/*-----------------------------------------------------------------------*/

/* Target pixel format. */
static int format = TEX_FORMAT_RGBA8888;

/* Alpha thresholds. (-a) */
static uint8_t alpha_lo = 0, alpha_hi = 255;

/* Use BGRA pixel order for 32bpp textures? (-bgra) */
static uint8_t bgra = 0;

/* Crop region (all zero if none). (-crop) */
static unsigned int crop_x, crop_y, crop_w, crop_h;

/* Use high-quality compression? (-hq) */
static uint8_t hq = 0;

/* Force texture size to square in DXT/PVRTC mode? (-make-square) */
static uint8_t make_square = 0;

/* Center texture when expanding to a square? (-make-square-center) */
static uint8_t make_square_center = 0;

/* How many mipmaps should we add? (-mipmaps) */
static int num_mipmaps = 0;

/* List of mipmap regions for independent shrinking. (-mipmap-regions) */
static struct {
    unsigned int x, y, w, h;
} *mipmap_regions;
static int num_mipmap_regions;

/* At what mipmap level should we make mipmaps transparent? (0 = never)
 * (-mipmaps-transparent-at) */
static int mipmaps_transparent_at = 0;

/* Generate opaque pixel bitmap? (-opaque-bitmap) */
static uint8_t do_opaque_bitmap = 0;

/* Output directory, or NULL to output to the source file's directory.
 * (-outdir) */
static const char *outdir = NULL;

/* Output for PSP? (-psp) */
static uint8_t psp = 0;

/* PVRTexToolCLI program path. (-pvrtextool) */
static const char *pvrtextool = "PVRTexToolCLI";

/* Target size for resizing (all zero if no resizing). (-resize) */
static unsigned int resize_w, resize_h;

/* Texture scale factor, as a 16.16 fixed-point value. (-scale) */
static int32_t scale_fixed = 0;  // Default depends on whether -psp is given.

/* Enable verbose output? (-verbose) */
static uint8_t verbose;

/*-----------------------------------------------------------------------*/

static Texture *read_png(const char *path);
static void png_warning_callback(png_structp png, const char *message);
static void png_error_callback(png_structp png, const char *message);

static int write_tex(Texture *texture, const char *path);

static Texture *convert_format(Texture *texture, const char *path);
static int crop_texture(Texture *texture,
                        int left, int top, int width, int height);
static int shrink_texture(Texture *texture, int new_width, int new_height);
static Texture *generate_mipmaps(Texture *texture, int mipmaps,
                                 uint32_t *extra_pixels_ret);
static int quantize_texture(Texture *texture);
static int generate_opaque_bitmap(Texture *texture);
static Texture *align_texture_psp(Texture *texture);
static int swizzle_texture(Texture *texture);
static int compress_dxt(Texture *texture, int type);
static int run_dxtcomp(const uint8_t *src, unsigned int width,
                       unsigned int height, unsigned int type, uint8_t *dest);
static int compress_pvrtc(Texture *texture, unsigned int bpp);
static int run_pvrtextool(const uint8_t *src, unsigned int width,
                          unsigned int height, unsigned int bpp, int alpha,
                          uint8_t *dest);
static void convert_rgba_to_bgra(Texture *texture);

static void spread_border(uint8_t * const pixels, const unsigned int width,
                          const unsigned int height);
static void fix_pvrtc4_alpha(const uint8_t *original, uint8_t *compressed,
                             unsigned int width, unsigned int height);
static CONST_FUNCTION int morton_index(unsigned int x, unsigned int y,
                                       unsigned int w, unsigned int h);
static inline void block_data_to_colors(
    uint32_t block_data,
    unsigned int *rA, unsigned int *gA, unsigned int *bA, unsigned int *aA,
    unsigned int *rB, unsigned int *gB, unsigned int *bB, unsigned int *aB);
static CONST_FUNCTION inline uint32_t colordiff_sq(
    unsigned int r1, unsigned int g1, unsigned int b1, unsigned int a1,
    unsigned int r2, unsigned int g2, unsigned int b2, unsigned int a2);
static void free_texture(Texture *texture);

/*************************************************************************/
/*************************************************************************/

/**
 * main:  Program entry point.  Parses command-line parameters and converts
 * each input file in turn.
 *
 * [Parameters]
 *     argc: Command line argument count.
 *     argv: Command line argument array.
 * [Return value]
 *     Zero on successful completion, nonzero if an error occurred.
 */
int main(int argc, char **argv)
{
    int argi = 1;
    while (argi < argc && argv[argi][0] == '-') {
        if (strcmp(argv[argi], "-8") == 0) {
            format = TEX_FORMAT_PALETTE8_RGBA8888;
        } else if (strcmp(argv[argi], "-alpha") == 0) {
            format = TEX_FORMAT_A8;
        } else if (strncmp(argv[argi], "-a=", 3) == 0) {
            if (argv[argi][3] == 0) {
                fprintf(stderr, "Missing argument for option -a\n");
                goto usage;
            }
            char *s;
            alpha_lo = strtoul(&argv[argi][3], &s, 10);
            if (*s != ',') {
                fprintf(stderr, "Invalid argument for option -a\n");
                goto usage;
            }
            alpha_hi = strtoul(s+1, &s, 10);
            if (*s != 0) {
                fprintf(stderr, "Invalid argument for option -a\n");
                goto usage;
            }
            if (alpha_lo >= alpha_hi) {
                fprintf(stderr, "Invalid argument (LO >= HI) for option -a\n");
                goto usage;
            }
        } else if (strcmp(argv[argi], "-bgra") == 0) {
            bgra = 1;
        } else if (strncmp(argv[argi], "-crop=", 6) == 0) {
            if (sscanf(&argv[argi][6], "%u,%u+%ux%u",
                       &crop_x, &crop_y, &crop_w, &crop_h) != 4) {
                fprintf(stderr, "Invalid argument for option -crop\n");
                goto usage;
            }
            if (crop_w == 0 || crop_h == 0) {
                fprintf(stderr, "Invalid argument for option -crop"
                        " (size is zero)\n");
                goto usage;
            }
        } else if (strcmp(argv[argi], "-dxt1") == 0) {
            format = TEX_FORMAT_S3TC_DXT1;
        } else if (strcmp(argv[argi], "-dxt3") == 0) {
            /* DXT3/5 formats will be automatically downgraded to DXT1
             * (non-alpha) if the texture is completely opaque, since
             * all three share the same color data format. */
            format = TEX_FORMAT_S3TC_DXT3;
        } else if (strcmp(argv[argi], "-dxt5") == 0) {
            format = TEX_FORMAT_S3TC_DXT5;
        } else if (strcmp(argv[argi], "-hq") == 0) {
            hq = 1;
        } else if (strcmp(argv[argi], "-make-square") == 0) {
            make_square = 1;
            make_square_center = 0;
        } else if (strcmp(argv[argi], "-make-square-center") == 0) {
            make_square = 1;
            make_square_center = 1;
        } else if (strncmp(argv[argi], "-mipmaps", 8) == 0
                   && (argv[argi][8] == 0 || argv[argi][8] == '=')) {
            if (argv[argi][8] == 0) {
                num_mipmaps = 99;
            } else {
                char *s;
                num_mipmaps = strtoul(argv[argi]+9, &s, 10);
                if (*s) {
                    fprintf(stderr, "Invalid argument for option -mipmaps\n");
                    goto usage;
                }
            }
        } else if (strncmp(argv[argi], "-mipmap-regions=", 16) == 0) {
            char *s = &argv[argi][16];
            do {
                char *start = s;
                unsigned int x, y, w, h;
                x = strtoul(s, &s, 0);
                if (*s != ':') {
                    goto mipmap_regions_error;
                }
                y = strtoul(s+1, &s, 0);
                if (*s != ':') {
                    goto mipmap_regions_error;
                }
                w = strtoul(s+1, &s, 0);
                if (*s != ':') {
                    goto mipmap_regions_error;
                }
                h = strtoul(s+1, &s, 0);
                if (*s != 0 && *s != ',') {
                  mipmap_regions_error:
                    s = strchr(start, ',');
                    if (s) {
                        *s = 0;
                    }
                    fprintf(stderr, "Invalid mipmap region: %s\n", start);
                    goto usage;
                }
                const unsigned int index = num_mipmap_regions++;
                mipmap_regions = realloc(
                    mipmap_regions,
                    num_mipmap_regions * sizeof(*mipmap_regions));
                if (!mipmap_regions) {
                    fprintf(stderr,
                            "Out of memory processing -mipmap-regions\n");
                    return 1;
                }
                mipmap_regions[index].x = x;
                mipmap_regions[index].y = y;
                mipmap_regions[index].w = w;
                mipmap_regions[index].h = h;
            } while (*s++ != 0);
        } else if (strncmp(argv[argi], "-mipmaps-transparent-at=", 24) == 0) {
            char *s;
            mipmaps_transparent_at = strtoul(argv[argi]+24, &s, 10);
            if (*s) {
                fprintf(stderr, "Invalid argument for option"
                        " -mipmaps-transparent-at\n");
                goto usage;
            }
        } else if (strcmp(argv[argi], "-opaque-bitmap") == 0) {
            do_opaque_bitmap = 1;
        } else if (strncmp(argv[argi], "-outdir=", 8) == 0) {
            if (!argv[argi][8]) {
                fprintf(stderr, "Missing argument for option -outdir\n");
                goto usage;
            }
            outdir = &argv[argi][8];
        } else if (strcmp(argv[argi], "-psp") == 0) {
            psp = 1;
        } else if (strcmp(argv[argi], "-pvrtc2") == 0) {
            format = TEX_FORMAT_PVRTC2_RGBA;
        } else if (strcmp(argv[argi], "-pvrtc4") == 0) {
            format = TEX_FORMAT_PVRTC4_RGBA;
        } else if (strncmp(argv[argi], "-pvrtextool=", 12) == 0) {
            if (!argv[argi][12]) {
                fprintf(stderr, "Missing argument for option -pvrtextool\n");
                goto usage;
            }
            pvrtextool = &argv[argi][12];
        } else if (strncmp(argv[argi], "-resize=", 8) == 0) {
            if (sscanf(&argv[argi][8], "%ux%u", &resize_w, &resize_h) != 2) {
                fprintf(stderr, "Invalid argument for option -resize\n");
                goto usage;
            }
            if (resize_w == 0 || resize_h == 0) {
                fprintf(stderr, "Invalid argument for option -resize"
                        " (size is zero)\n");
                goto usage;
            }
        } else if (strncmp(argv[argi], "-scale=", 7) == 0) {
            char *s;
            float scale = strtof(argv[argi]+7, &s);
            if (*s) {
                fprintf(stderr, "Invalid argument for option -scale\n");
                goto usage;
            }
            if (scale < 1/65536.0f
             || scale >= 65536
             || (double)scale*65536 != floor((double)scale*65536)) {
                fprintf(stderr, "Invalid scale value; must be a positive"
                        " multiple of 1/65536 less than 65536\n");
                goto usage;
            }
            scale_fixed = (int32_t)floorf(scale*65536);
        } else if (strcmp(argv[argi], "-verbose") == 0) {
            verbose = 1;
        } else {
            goto usage;
        }
        argi++;
    }
    if (psp) {  // The PSP only supports up to 8 levels.
        num_mipmaps = ubound(num_mipmaps, 7);
    }
    if (!scale_fixed) {
        if (psp) {
            scale_fixed = 1<<15;  // 0.5
        } else {
            scale_fixed = 1<<16;  // 1.0
        }
    }

    if (argi >= argc) {
      usage:
        fprintf(stderr, "\n"
                "Usage: %s [options] file1.png [file2.png...]\n"
                "\n"
                "Options:\n"
                "\n"
                "-8 will quantize to 8bpp indexed textures.\n"
                "\n"
                "-alpha will write alpha-only textures, discarding all color\n"
                "information in the input files.\n"
                "\n"
                "-a=LO,HI specifies low and high alpha thresholds when using\n"
                "compressed texture formats.  Do not include a space between\n"
                "\"-a\" and the argument.\n"
                "\n"
                "-bgra will write 32bpp textures in BGRA pixel order instead\n"
                "of the default RGBA pixel order.  This does not affect 8bpp\n"
                "paletted textures.\n"
                "\n"
                "-crop=X,Y+WxH crops the input image to the given region\n"
                "before resizing and converting.\n"
                "\n"
                "-dxt1, -dxt3, and -dxt5 select S3TC DXTn-compressed output.\n"
                "The dxtcomp program must be available in the executable\n"
                "search path (see source code comments for details).\n"
                "\n"
                "-make-square (only valid in DXT or PVRTC output modes)\n"
                "forces the texture to be a square if it is not already\n"
                "square.  Append -center to the option name to center the\n"
                "original image in the final texture.\n"
                "\n"
                "-mipmaps will generate mipmaps for each texture; adding a\n"
                "number (like -mipmaps=2) limits the number of additional\n"
                "mipmaps to that number or fewer.\n"
                "\n"
                "-mipmap-regions=x:y:w:h[,...] will shrink the specified\n"
                "areas independently when generating mipmaps, to prevent\n"
                "adjacent subtextures from leaking into each other.\n"
                "\n"
                "-mipmaps-transparent-at=N will force mipmaps at level N and\n"
                "greater (relative size 1/2^N and smaller) to be completely\n"
                "transparent, to help avoid graphical glitches caused by\n"
                "degenerate geometry.\n"
                "\n"
                "-opaque-bitmap will generate a bitmap of opaque pixels to\n"
                "allow the program to read texture opacity data.\n"
                "\n"
                "-outdir=OUTDIR specifies the output directory for all files.\n"
                "\n"
                "-psp selects PSP output mode, with automatic resize to half\n"
                "size (unless -resize is given), scale factor 0.5 (unless\n"
                "-scale is given), data alignment, and swizzling.\n"
                "\n"
                "-pvrtc2 and -pvrtc4 select (respectively) 2bpp and 4bpp\n"
                "PVRTC-compressed output.  The PVRTexToolCLI program must\n"
                "be available in the executable search path, unless the\n"
                "-pvrtextool option is given.\n"
                "\n"
                "-pvrtextool=PATH specifies the path (including filename) of\n"
                "the PVRTexToolCLI program used for PVRTC texture compression.\n"
                "\n"
                "-resize=WxH resizes the input image to the given size\n"
                "before converting.  The scale factor is not affected.\n"
                "\n"
                "-scale=N gives the scale factor of the input image relative\n"
                "to the original texture.  For example, use -scale=0.5 if\n"
                "the input image has been shrunk by half; this would cause\n"
                "the texture size to be reported as twice the image size.\n"
                "\n",
                argv[0]);
        return 1;
    }

    for (; argi < argc; argi++) {
        const char *infile = argv[argi];

        Texture *texture = read_png(infile);
        if (!texture) {
            fprintf(stderr, "Failed to read %s\n", infile);
            return 1;
        }

        texture = convert_format(texture, infile);
        if (!texture) {
            return 1;
        }

        char pathbuf[1000];
        unsigned int pathlen;
        if (outdir) {
            const char *filename = strrchr(infile, '/');
            if (filename) {
                filename++;
            } else {
                filename = infile;
            }
            pathlen = snprintf(pathbuf, sizeof(pathbuf)-4, "%s/%s",
                               outdir, filename);
        } else {
            pathlen = snprintf(pathbuf, sizeof(pathbuf)-4, "%s", infile);
        }
        if (pathlen >= sizeof(pathbuf)-4) {
            fprintf(stderr, "Pathname buffer overflow on %s\n", infile);
            return 1;
        }
        if (stricmp(pathbuf + pathlen - 4, ".png") == 0) {
            pathbuf[pathlen-4] = 0;
            pathlen -= 4;
        }
        memcpy(pathbuf + pathlen, ".tex", 5);
        if (!write_tex(texture, pathbuf)) {
            fprintf(stderr, "Failed to write %s\n", pathbuf);
            return 1;
        }

        free_texture(texture);
    }

    return 0;
}

/*************************************************************************/
/************************** PNG input functions **************************/
/*************************************************************************/

/**
 * read_png:  Read a PNG file into a Texture data structure.
 *
 * [Parameters]
 *     path: Pathname of file to read.
 * [Return value]
 *     Texture, or NULL on error.
 */
static Texture *read_png(const char *path)
{
    /* We have to be able to free these on error, so we need volatile
     * declarations. */
    volatile FILE *f_volatile = NULL;
    volatile png_structp png_volatile = NULL;
    volatile png_infop info_volatile = NULL;
    volatile Texture *texture_volatile = NULL;
    volatile void *row_buffer_volatile = NULL;

    if (setjmp(png_jmpbuf) != 0) {
        /* libpng jumped back here with an error, so return the error. */
      error:  // Let's reuse it for our own error handling, too.
        free((void *)row_buffer_volatile);
        free((void *)texture_volatile);
        png_destroy_read_struct((png_structpp)&png_volatile,
                                (png_infopp)&info_volatile, NULL);
        if (f_volatile) {
            fclose((FILE *)f_volatile);
        }
        return NULL;
    }


    /* Open the requested file. */

    FILE *f = fopen(path, "rb");
    if (!f) {
        perror(path);
        return NULL;
    }
    f_volatile = f;

    /* Set up the PNG reader instance. */

    png_structp png = png_create_read_struct(
        PNG_LIBPNG_VER_STRING,
        (void *)path, png_error_callback, png_warning_callback);
    png_volatile = png;
    png_infop info = png_create_info_struct(png);
    info_volatile = info;
    png_init_io(png, f);

    /* Read the image information. */

    png_read_info(png, info);
    const unsigned int width      = png_get_image_width(png, info);
    const unsigned int height     = png_get_image_height(png, info);
    const unsigned int bit_depth  = png_get_bit_depth(png, info);
    const unsigned int color_type = png_get_color_type(png, info);
    if (png_get_interlace_type(png, info) != PNG_INTERLACE_NONE) {
        fprintf(stderr, "Interlaced images not supported\n");
        goto error;
    }
    if (bit_depth < 8) {
        fprintf(stderr, "Bit depth %d not supported\n", bit_depth);
        goto error;
    }

    /* Set up image transformation parameters. */

    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png);
    } else if (color_type == PNG_COLOR_TYPE_GRAY
            || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png);
    }
    if (bit_depth == 16) {
        png_set_strip_16(png);
    }
    if (png_get_valid(png, info, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png);
    }
    if (color_type == PNG_COLOR_TYPE_RGB
     || color_type == PNG_COLOR_TYPE_PALETTE
     || color_type == PNG_COLOR_TYPE_GRAY) {
        png_set_add_alpha(png, 0xFF, PNG_FILLER_AFTER);
    }
    png_read_update_info(png, info);

    /* Create the texture structure. */

    Texture *texture;
    const uint32_t struct_size = align_up(sizeof(*texture), 64);
    texture = malloc(struct_size + (width * height * 4));
    if (!texture) {
        fprintf(stderr, "Out of memory for texture (%ux%u, %u bytes)\n",
                width, height, struct_size + (width * height * 4));
        goto error;
    }
    texture->width         = width;
    texture->height        = height;
    texture->stride        = width;
    texture->format        = TEX_FORMAT_RGBA8888;
    texture->swizzled      = 0;
    texture->mipmaps       = 0;
    texture->palette       = NULL;
    texture->pixels        = (uint8_t *)texture + struct_size;
    texture->opaque_bitmap = NULL;

    texture_volatile = texture;

    /* Read the image in one row at a time. */

    uint8_t *row_buffer;
    uint32_t rowbytes = png_get_rowbytes(png, info);
    row_buffer = malloc(rowbytes);
    if (!row_buffer) {
        fprintf(stderr, "Out of memory for pixel read buffer (%u bytes)\n",
                rowbytes);
        goto error;
    }
    row_buffer_volatile = row_buffer;

    uint8_t *dest = texture->pixels;
    for (unsigned int y = 0; y < height; y++, dest += texture->stride * 4) {
        png_read_row(png, row_buffer, NULL);
        memcpy(dest, row_buffer, rowbytes);
    }

    row_buffer_volatile = NULL;
    free(row_buffer);

    /* Done!  Close down the PNG reader and return success. */

    png_read_end(png, NULL);
    png_destroy_read_struct(&png, &info, NULL);
    fclose(f);
    return texture;
}

/*-----------------------------------------------------------------------*/

/**
 * png_warning_callback, png_error_callback:  Error handling functions for
 * libpng.
 *
 * [Parameters]
 *     png: PNG reader structure.
 *     message: Warning or error message.
 * [Return value]
 *     None (png_error_callback() does not return).
 */
static void png_warning_callback(png_structp png, const char *message)
{
    fprintf(stderr, "libpng warning: %s: %s\n",
            (char *)png_get_error_ptr(png), message);
}

static void png_error_callback(png_structp png, const char *message)
{
    fprintf(stderr, "libpng error: %s: %s\n",
            (char *)png_get_error_ptr(png), message);
    longjmp(png_jmpbuf, 1);
}


/*************************************************************************/
/*********************** Texture output functions ************************/
/*************************************************************************/

/**
 * write_tex:  Write a SIL-format *.tex file for the given texture.
 *
 * [Parameters]
 *     texture: Texture to write.
 *     path: Pathname for new file.
 * [Return value]
 *     True on success, false on error.
 */
static int write_tex(Texture *texture, const char *path)
{
    TexFileHeader header;
    memcpy(header.magic, TEX_FILE_MAGIC, sizeof(header.magic));
    header.version       = TEX_FILE_VERSION;
    header.format        = texture->format;
    header.mipmaps       = texture->mipmaps;
    header.opaque_bitmap = (texture->opaque_bitmap != NULL) ? 1 : 0;
    header.width         = s16_to_be(texture->width);
    header.height        = s16_to_be(texture->height);
    header.scale_int     = s32_to_be(scale_fixed);
    header.pixels_offset = s32_to_be(psp ? align_up(sizeof(header), 64)
                                         : sizeof(header));
    header.pixels_size   = s32_to_be(0);  // Will be set later.
    header.bitmap_offset = s32_to_be(0);  // Will be set later if needed.
    header.bitmap_size   = s32_to_be(0);  // Will be set later if needed.

    FILE *f = fopen(path, "wb");
    if (!f) {
        perror(path);
        goto error_return;
    }

    if (fwrite(&header, sizeof(header), 1, f) != 1) {
        perror(path);
        goto error_close_file;
    }
    if (fseek(f, be_to_s32(header.pixels_offset), SEEK_SET) != 0) {
        perror(path);
        goto error_close_file;
    }

    uint32_t pixel_bytes_written = 0;

    if (texture->format == TEX_FORMAT_PALETTE8_RGBA8888
     || texture->format == TEX_FORMAT_PSP_PALETTE8_RGBA8888
     || texture->format == TEX_FORMAT_PSP_PALETTE8_RGBA8888_SWIZZLED) {
        if (fwrite(texture->palette, 4, 256, f) != 256) {
            perror(path);
            goto error_close_file;
        }
        pixel_bytes_written += 256*4;
    }

    unsigned int bpp;
    switch (texture->format) {
      case TEX_FORMAT_RGBA8888:
      case TEX_FORMAT_BGRA8888:
      case TEX_FORMAT_PSP_RGBA8888:
      case TEX_FORMAT_PSP_RGBA8888_SWIZZLED:
        bpp = 32;
        break;
      case TEX_FORMAT_RGB565:
      case TEX_FORMAT_RGBA5551:
      case TEX_FORMAT_RGBA4444:
      case TEX_FORMAT_BGR565:
      case TEX_FORMAT_BGRA5551:
      case TEX_FORMAT_BGRA4444:
      case TEX_FORMAT_PSP_RGB565:
      case TEX_FORMAT_PSP_RGBA5551:
      case TEX_FORMAT_PSP_RGBA4444:
      case TEX_FORMAT_PSP_RGB565_SWIZZLED:
      case TEX_FORMAT_PSP_RGBA5551_SWIZZLED:
      case TEX_FORMAT_PSP_RGBA4444_SWIZZLED:
        bpp = 16;
        break;
      case TEX_FORMAT_PALETTE8_RGBA8888:
      case TEX_FORMAT_A8:
      case TEX_FORMAT_PSP_PALETTE8_RGBA8888:
      case TEX_FORMAT_PSP_A8:
      case TEX_FORMAT_PSP_PALETTE8_RGBA8888_SWIZZLED:
      case TEX_FORMAT_PSP_A8_SWIZZLED:
      case TEX_FORMAT_S3TC_DXT3:
      case TEX_FORMAT_S3TC_DXT5:
        bpp = 8;
        break;
      case TEX_FORMAT_PVRTC2_RGBA:
      case TEX_FORMAT_PVRTC2_RGB:
        bpp = 2;
        break;
      case TEX_FORMAT_S3TC_DXT1:
      case TEX_FORMAT_PVRTC4_RGBA:
      case TEX_FORMAT_PVRTC4_RGB:
        bpp = 4;
        break;
      default:
        fprintf(stderr, "Unknown texture format %u, assuming 32bpp\n",
                texture->format);
        bpp = 32;
        break;
    }
    unsigned int width = texture->width, height = texture->height;
    unsigned int stride = texture->stride;
    const uint8_t *pixels = texture->pixels;
    for (unsigned int level = 0; level <= texture->mipmaps; level++) {
        unsigned int data_width, data_height;
        if (texture->format == TEX_FORMAT_PVRTC2_RGBA
         || texture->format == TEX_FORMAT_PVRTC4_RGBA
         || texture->format == TEX_FORMAT_PVRTC2_RGB
         || texture->format == TEX_FORMAT_PVRTC4_RGB) {
            data_width = lbound(width, 32/bpp);
            data_height = lbound(height, 8);
        } else if (texture->format == TEX_FORMAT_S3TC_DXT1
                || texture->format == TEX_FORMAT_S3TC_DXT3
                || texture->format == TEX_FORMAT_S3TC_DXT5) {
            data_width = lbound(width, 4);
            data_height = lbound(height, 4);
        } else {
            data_width = stride;
            data_height = texture->swizzled ? align_up(height, 8) : height;
        }
        if (fwrite(pixels, (data_width * data_height * bpp) / 8, 1, f) != 1) {
            perror(path);
            goto error_close_file;
        }
        pixel_bytes_written += (data_width * data_height * bpp) / 8;
        pixels += (data_width * data_height * bpp) / 8;
        width  = lbound(width/2, 1);
        height = lbound(height/2, 1);
        if (psp) {
            stride = align_up(stride/2, 128/bpp);
        } else {
            stride = lbound(stride/2, 1);
        }
    }
    header.pixels_size = s32_to_be(pixel_bytes_written);

    if (texture->opaque_bitmap) {
        const unsigned int rowsize = (texture->width+7) / 8;
        header.bitmap_offset = s32_to_be(ftell(f));
        header.bitmap_size = s32_to_be(rowsize * texture->height);
        if (fwrite(texture->opaque_bitmap, rowsize * texture->height, 1, f)
            != 1)
        {
            perror(path);
            goto error_close_file;
        }
    }

    if (fseek(f, 0, SEEK_SET) != 0
     || fwrite(&header, sizeof(header), 1, f) != 1) {
        perror(path);
        goto error_close_file;
    }

    fclose(f);
    return 1;

  error_close_file:
    fclose(f);
    remove(path);
  error_return:
    return 0;
}

/*************************************************************************/
/********** Format conversion and image manipulation functions ***********/
/*************************************************************************/

/**
 * convert_format:  Convert a 32bpp uncompressed texture to the format
 * specified by the command-line flags.  The input texture's memory is
 * either reused or freed.
 *
 * [Parameters]
 *     texture: Texture to convert.
 *     path: File pathname (for error messages).
 * [Return value]
 *     Converted texture, or NULL on error.
 */
static Texture *convert_format(Texture *texture, const char *path)
{
    if (format == TEX_FORMAT_A8) {
        /* Force all pixels' color data to white so subsequent operations
         * don't produce suboptimal results due to the color data (which
         * will just be ignored anyway). */
        for (int i = 0; i < texture->stride * texture->height; i++) {
            texture->pixels[i*4+0] = 255;
            texture->pixels[i*4+1] = 255;
            texture->pixels[i*4+2] = 255;
        }
    }

    if (crop_w && crop_h) {
        if ((int)(crop_x + crop_w) > texture->width
         || (int)(crop_y + crop_h) > texture->height) {
            fprintf(stderr, "%s: Crop rectangle (%u,%u+%ux%u) is outside"
                    " texture bounds (%dx%d)\n", path, crop_x, crop_y,
                    crop_w, crop_h, texture->width, texture->height);
            free_texture(texture);
            return 0;
        }
        if (!crop_texture(texture, crop_x, crop_y, crop_w, crop_h)) {
            fprintf(stderr, "%s: Shrinking failed\n", path);
            free_texture(texture);
            return 0;
        }
    }

    int resize_width = resize_w, resize_height = resize_h;
    if (!(resize_width && resize_height) && psp) {
        resize_width = lbound(texture->width/2, 1);
        resize_height = lbound(texture->height/2, 1);
    }
    if (resize_width && resize_height) {
        if ((int)resize_width > texture->width
         || (int)resize_height > texture->height) {
            fprintf(stderr, "%s: Expanding resize not currently supported\n",
                    path);
            free_texture(texture);
            return 0;
        }
        if (!shrink_texture(texture, resize_width, resize_height)) {
            fprintf(stderr, "%s: Resizing failed\n", path);
            free_texture(texture);
            return 0;
        }
    }

    if (make_square
        && (format == TEX_FORMAT_S3TC_DXT1
         || format == TEX_FORMAT_S3TC_DXT3
         || format == TEX_FORMAT_S3TC_DXT5
         || format == TEX_FORMAT_PVRTC2_RGBA
         || format == TEX_FORMAT_PVRTC4_RGBA
         || format == TEX_FORMAT_PVRTC2_RGB
         || format == TEX_FORMAT_PVRTC4_RGB))
    {
        /* Calculate the required size. */
        int new_width, new_height;
        for (new_width = 1; new_width < texture->width; new_width <<= 1) {}
        for (new_height = 1; new_height < texture->height; new_height <<= 1) {}
        if (new_width < new_height) {
            new_width = new_height;
        } else if (new_height < new_width) {
            new_height = new_width;
        }
        /* Check whether this would kill the compression ratio. */
        int max_factor;
        if (format == TEX_FORMAT_PVRTC2_RGBA
         || format == TEX_FORMAT_PVRTC2_RGB) {
            max_factor = 16;
        } else if (format == TEX_FORMAT_PVRTC4_RGBA
                || format == TEX_FORMAT_PVRTC4_RGB
                || format == TEX_FORMAT_S3TC_DXT1) {
            max_factor = 8;
        } else {
            max_factor = 4;
        }
        if (new_width * new_height
            >= texture->width * texture->height * max_factor)
        {
            fprintf(stderr, "%s: warning: expanding texture would waste space;"
                    " ignoring -make-square and writing as 32bpp RGBA\n",
                    path);
            format = TEX_FORMAT_RGBA8888;
        } else {
            /* It's safe to expand, so do so. */
            uint8_t fill_alpha = 0xFF;
            for (int i = 3; i < texture->width*texture->height*4; i += 4) {
                if (texture->pixels[i] != 0xFF) {
                    fill_alpha = 0;
                    break;
                }
            }
            const uint32_t alloc_width  = align_up(new_width, 4);
            const uint32_t alloc_height = align_up(new_height, 8);
            const uint32_t struct_size  = align_up(sizeof(*texture), 64);
            Texture *new_texture =
                malloc(struct_size + (alloc_width * alloc_height * 4));
            if (!new_texture) {
                fprintf(stderr, "%s: Out of memory\n", path);
                free_texture(texture);
                return 0;
            }
            *new_texture = *texture;
            new_texture->width = new_width;
            new_texture->height = new_height;
            new_texture->stride = new_width;
            new_texture->pixels = (uint8_t *)new_texture + struct_size;
            const int offset_x =
                make_square_center ? (new_width - texture->width) / 2 : 0;
            const int offset_y =
                make_square_center ? (new_height - texture->height) / 2 : 0;
            const uint8_t *src = texture->pixels;
            uint8_t *dest = new_texture->pixels;
            for (int y = 0; y < offset_y; y++) {
                for (int x = 0; x < new_width; x++) {
                    *dest++ = 0;
                    *dest++ = 0;
                    *dest++ = 0;
                    *dest++ = fill_alpha;
                }
            }
            for (int y = 0; y < texture->height; y++, src += texture->stride*4)
            {
                for (int x = 0; x < offset_x; x++) {
                    *dest++ = 0;
                    *dest++ = 0;
                    *dest++ = 0;
                    *dest++ = fill_alpha;
                }
                memcpy(dest, src, texture->width*4);
                dest += texture->width*4;
                for (int x = offset_x + texture->width; x < new_width; x++) {
                    *dest++ = 0;
                    *dest++ = 0;
                    *dest++ = 0;
                    *dest++ = fill_alpha;
                }
            }
            for (int y = offset_y + texture->height; y < new_height; y++) {
                for (int x = 0; x < new_width; x++) {
                    *dest++ = 0;
                    *dest++ = 0;
                    *dest++ = 0;
                    *dest++ = fill_alpha;
                }
            }
            free_texture(texture);
            texture = new_texture;
        }
    }

    uint32_t extra_pixels = 0;
    if (num_mipmaps > 0) {
        if ((texture->width & (texture->width - 1)) != 0
         || (texture->height & (texture->height - 1)) != 0) {
            /* X & (X-1) is zero iff X is either zero or a power of two. */
            fprintf(stderr, "%s: Not generating mipmaps (size %dx%d is not a"
                    " power of 2)\n", path, texture->width, texture->height);
        } else {
            texture = generate_mipmaps(texture, num_mipmaps, &extra_pixels);
            if (!texture) {
                fprintf(stderr, "%s: Failed to generate mipmaps\n", path);
                return 0;
            }
        }
    }

    const uint32_t total_pixels =
        texture->width * texture->height + extra_pixels;

    if (do_opaque_bitmap && !generate_opaque_bitmap(texture)) {
        fprintf(stderr, "%s: Failed to generate opaque bitmap\n", path);
        free_texture(texture);
        return 0;
    }

    if (format == TEX_FORMAT_PALETTE8_RGBA8888) {
        texture->palette = calloc(256, 4);
        if (!texture->palette) {
            fprintf(stderr, "%s: Out of memory\n", path);
            free_texture(texture);
            return 0;
        }
        generate_palette((uint32_t *)texture->pixels, total_pixels, 1,
                         total_pixels, texture->palette, 0, NULL);
        if (!quantize_texture(texture)) {
            fprintf(stderr, "%s: Color quantization failed\n", path);
            free_texture(texture);
            return 0;
        }
    } else if (format == TEX_FORMAT_A8) {
        for (uint32_t i = 0; i < total_pixels; i++) {
            texture->pixels[i] = texture->pixels[i*4+3];
        }
        texture->format = TEX_FORMAT_A8;
    } else if (format == TEX_FORMAT_S3TC_DXT1
            || format == TEX_FORMAT_S3TC_DXT3
            || format == TEX_FORMAT_S3TC_DXT5) {
        const int format_num = (format==TEX_FORMAT_S3TC_DXT1 ? 1 :
                                format==TEX_FORMAT_S3TC_DXT3 ? 3 : 5);
        if (!compress_dxt(texture, format_num)) {
            fprintf(stderr, "%s: Compression failed\n", path);
            free_texture(texture);
            return 0;
        }
    } else if (format == TEX_FORMAT_PVRTC2_RGBA
            || format == TEX_FORMAT_PVRTC4_RGBA
            || format == TEX_FORMAT_PVRTC2_RGB
            || format == TEX_FORMAT_PVRTC4_RGB) {
        const int bpp =
            (format==TEX_FORMAT_PVRTC2_RGBA || format==TEX_FORMAT_PVRTC2_RGB
             ? 2 : 4);
        if (!compress_pvrtc(texture, bpp)) {
            fprintf(stderr, "%s: Compression failed\n", path);
            free_texture(texture);
            return 0;
        }
    }

    if (psp) {
        if (!(texture = align_texture_psp(texture))) {
            fprintf(stderr, "%s: Failed to align pixel data\n", path);
            return 0;
        }
        if (!swizzle_texture(texture)) {
            fprintf(stderr, "%s: Swizzling failed\n", path);
            free_texture(texture);
            return 0;
        }
        if (format == TEX_FORMAT_RGBA8888) {
            texture->format = TEX_FORMAT_PSP_RGBA8888_SWIZZLED;
        } else if (format == TEX_FORMAT_RGB565) {
            texture->format = TEX_FORMAT_PSP_RGB565_SWIZZLED;
        } else if (format == TEX_FORMAT_RGBA5551) {
            texture->format = TEX_FORMAT_PSP_RGBA5551_SWIZZLED;
        } else if (format == TEX_FORMAT_RGBA4444) {
            texture->format = TEX_FORMAT_PSP_RGBA4444_SWIZZLED;
        } else if (format == TEX_FORMAT_PALETTE8_RGBA8888) {
            texture->format = TEX_FORMAT_PSP_PALETTE8_RGBA8888_SWIZZLED;
        } else if (format == TEX_FORMAT_A8) {
            texture->format = TEX_FORMAT_PSP_A8_SWIZZLED;
        } else {
            fprintf(stderr, "%s: Invalid texture format for PSP: 0x%02X\n",
                    path, texture->format);
        }
    }

    if (bgra && format == TEX_FORMAT_RGBA8888) {
        convert_rgba_to_bgra(texture);
        format = TEX_FORMAT_BGRA8888;
    }

    return texture;
}

/*-----------------------------------------------------------------------*/

/**
 * crop_texture:  Crop the given texture to the given region.  The texture
 * buffer is _not_ reallocated.
 *
 * [Parameters]
 *     texture: Texture to crop.
 *     left, top: Coordinates of upper-left corner of crop region, in pixels.
 *     width, height: Size of crop region, in pixels.
 * [Return value]
 *     True on success, false on error.
 */
static int crop_texture(Texture *texture,
                        int left, int top, int width, int height)
{
    const uint8_t *src = &texture->pixels[(top*texture->stride + left) * 4];
    uint8_t *dest = texture->pixels;
    for (int y = 0; y < height;
         y++, src += texture->stride*4, dest += width*4)
    {
        memmove(dest, src, width*4);
    }

    texture->width  = width;
    texture->height = height;
    texture->stride = width;

    return 1;
}

/*-----------------------------------------------------------------------*/

/**
 * shrink_texture:  Shrink the given texture to the given size.  The
 * texture buffer is _not_ reallocated.
 *
 * [Parameters]
 *     texture: Texture to shrink.
 *     new_width: New texture width, in pixels.
 *     new_height: New texture width, in pixels.
 * [Return value]
 *     True on success, false on error.
 */
static int shrink_texture(Texture *texture, int new_width, int new_height)
{
    new_width = ubound(new_width, texture->width);
    new_height = ubound(new_height, texture->height);
    if (new_width == texture->width && new_height == texture->height) {
        return 1;
    }

    void *tempbuf = malloc(new_width * new_height * 4);
    if (!tempbuf) {
        fprintf(stderr, "Out of memory for shrink buffer (%d bytes)\n",
                new_width * new_height * 4);
    }

    ZoomInfo *zi = zoom_init(texture->width, texture->height,
                             new_width, new_height,
                             4, texture->stride*4, new_width*4,
                             1, TCV_ZOOM_CUBIC_KEYS4);
    if (!zi) {
        fprintf(stderr, "zoom_init() failed\n");
        free(tempbuf);
        return 0;
    }
    zoom_process(zi, texture->pixels, tempbuf);
    zoom_free(zi);

    texture->width  = new_width;
    texture->height = new_height;
    texture->stride = new_width;
    memcpy(texture->pixels, tempbuf, new_width * new_height * 4);
    free(tempbuf);

    return 1;
}

/*-----------------------------------------------------------------------*/

/**
 * generate_mipmaps:  Generate the given number of mipmaps for the given
 * 32bpp texture, stopping if the mipmap size reaches 1x1.  The texture
 * buffer is reallocated by this function, and freed on error.
 *
 * [Parameters]
 *     tex: Texture to generate mipmaps for.
 *     mipmaps: Maximum number of mipmaps to generate.
 *     extra_pixels_ret: Pointer to variable to receive the number of
 *         pixels added, or NULL if not needed.
 * [Return value]
 *     New texture pointer, or NULL on error.
 */
static Texture *generate_mipmaps(Texture *texture, int mipmaps,
                                 uint32_t *extra_pixels_ret)
{
    /* Worst case is a 1x2^n texture, which will have mipmaps of size
     * 1x2^(n-1), 1x2^(n-2), ..., 1x4, 1x2, 1x1, for a total of 2^(n+1)-1
     * pixels. */
    const uint32_t struct_size = align_up(sizeof(*texture), 64);
    const uint32_t alloc_size =
        struct_size + (texture->stride * texture->height * 4) * 2;
    Texture *new_texture = realloc(texture, alloc_size);
    if (!new_texture) {
        fprintf(stderr, "Out of memory generating mipmaps\n");
        free_texture(texture);
        return NULL;
    }
    texture = new_texture;
    texture->pixels = (uint8_t *)texture + struct_size;

    if (extra_pixels_ret) {
        *extra_pixels_ret = 0;
    }

    unsigned int width  = texture->width;
    unsigned int height = texture->height;
    unsigned int stride = texture->stride;
    uint8_t *    pixels = texture->pixels;

    for (int level = 1; level <= mipmaps && (width > 1 || height > 1); level++) {

        const unsigned int old_width  = width;
        const unsigned int old_height = height;
        const unsigned int old_stride = stride;
        uint8_t * const    old_pixels = pixels;

        pixels += stride * height * 4;
        width  = lbound(width/2, 1);
        height = lbound(height/2, 1);
        stride = lbound(stride/2, 1);

        ZoomInfo *zi = zoom_init(old_width, old_height, width, height,
                                 4, old_stride*4, stride*4,
                                 1, TCV_ZOOM_CUBIC_KEYS4);
        if (!zi) {
            fprintf(stderr, "zoom_init() failed\n");
            free_texture(texture);
            return NULL;
        }
        zoom_process(zi, old_pixels, pixels);
        zoom_free(zi);

        for (int i = 0; i < num_mipmap_regions; i++) {
            if (mipmap_regions[i].w == 0 || mipmap_regions[i].h == 0) {
                /* Invalid or previously disabled. */
            } else if ((mipmap_regions[i].x % 2) != 0
                    || (mipmap_regions[i].y % 2) != 0
                    || (mipmap_regions[i].w % 2) != 0
                    || (mipmap_regions[i].h % 2) != 0) {
                /* Doesn't shrink to pixel boundaries, so disable the entry. */
                mipmap_regions[i].w = mipmap_regions[i].h = 0;
            } else {
                zi = zoom_init(mipmap_regions[i].w, mipmap_regions[i].h,
                               mipmap_regions[i].w/2, mipmap_regions[i].h/2,
                               4, old_stride*4, stride*4,
                               1, TCV_ZOOM_CUBIC_KEYS4);
                if (!zi) {
                    fprintf(stderr,
                            "zoom_init() failed for mipmap region %d\n", i);
                    free_texture(texture);
                    return NULL;
                }
                const uint8_t *src = old_pixels
                    + (mipmap_regions[i].y * old_stride
                       + mipmap_regions[i].x) * 4;
                uint8_t *dest = pixels
                    + ((mipmap_regions[i].y/2) * stride
                       + (mipmap_regions[i].x/2)) * 4;
                zoom_process(zi, src, dest);
                zoom_free(zi);
                mipmap_regions[i].x /= 2;
                mipmap_regions[i].y /= 2;
                mipmap_regions[i].w /= 2;
                mipmap_regions[i].h /= 2;
            }
        }  // for (i = 0; i < num_mipmap_regions; i++)

        if (mipmaps_transparent_at != 0 && level >= mipmaps_transparent_at) {
            /* Retain the color data (for inter-mipmap interpolation); just
             * clear the alpha channel. */
            for (uint32_t i = 0; i < stride*height; i++) {
                pixels[i*4+3] = 0;
            }
        }

        texture->mipmaps++;
        if (extra_pixels_ret) {
            *extra_pixels_ret += width * height;
        }

    }

    return texture;
}

/*-----------------------------------------------------------------------*/

/**
 * quantize_texture:  Convert the given texture to indexed-color 8bpp by
 * quantizing the color palette down to the 256 colors specified in
 * texture->palette.  The texture buffer is _not_ reallocated.
 *
 * [Parameters]
 *     texture: Texture to quantize.
 * [Return value]
 *     True on success, false on error.
 */
static int quantize_texture(Texture *texture)
{
    unsigned int width  = texture->width;
    unsigned int height = texture->height;
    unsigned int stride = texture->stride;
    uint32_t *pixels_in = (uint32_t *)texture->pixels;
    uint8_t *pixels_out = texture->pixels;

    for (unsigned int level = 0; level <= texture->mipmaps; level++) {
        if (!quantize_image(pixels_in, stride, pixels_out, stride,
                            width, height, texture->palette, 256)) {
            fprintf(stderr, "quantize_image() failed for level %u\n", level);
            return 0;
        }
        pixels_in  += stride * height;
        pixels_out += stride * height;
        width  = lbound(width/2, 1);
        height = lbound(height/2, 1);
        stride = lbound(stride/2, 1);
    }

    texture->format = TEX_FORMAT_PALETTE8_RGBA8888;
    return 1;
}

/*-----------------------------------------------------------------------*/

/**
 * generate_opaque_bitmap:  Generate a bitmap of opaque pixels for the
 * given texture.  For textures with mipmaps, the bitmap reflects the
 * pixels in the base image.  The bitmap is stored in a separately-
 * allocated buffer.
 *
 * [Parameters]
 *     tex: Texture for which to generate an opaque pixel bitmap.
 * [Return value]
 *     True on success, false on error.
 */
static int generate_opaque_bitmap(Texture *texture)
{
    const unsigned int width = texture->width;
    const unsigned int height = texture->height;
    const unsigned int rowsize = (width+7) / 8;

    texture->opaque_bitmap = calloc(rowsize * height, 1);
    if (!texture->opaque_bitmap) {
        fprintf(stderr, "Out of memory generating opaque bitmap (%u bytes)\n",
                rowsize * height);
        return 0;
    }

    const uint8_t *src = texture->pixels;
    const uint32_t stride = texture->stride;
    uint8_t *dest = texture->opaque_bitmap;
    switch (texture->format) {
      case TEX_FORMAT_RGBA8888: {
        for (unsigned int y = 0; y < height; y++, src += stride*4, dest += rowsize) {
            for (unsigned int x = 0; x < width; x++) {
                dest[x/8] |= (src[x*4+3]==255 ? 1 : 0) << (x%8);
            }
        }
        break;
      }
      case TEX_FORMAT_PALETTE8_RGBA8888: {
        const uint32_t * const palette = texture->palette;
        for (unsigned int y = 0; y < height; y++, src += stride, dest += rowsize) {
            for (unsigned int x = 0; x < width; x++) {
                dest[x/8] |= ((palette[src[x]]>>24)==255 ? 1 : 0) << (x%8);
            }
        }
        break;
      }
      case TEX_FORMAT_A8: {
        for (unsigned int y = 0; y < height; y++, src += stride, dest += rowsize) {
            for (unsigned int x = 0; x < width; x++) {
                dest[x/8] |= (src[x]==255 ? 1 : 0) << (x%8);
            }
        }
        break;
      }
      default:
        fprintf(stderr, "Can't generate opaque bitmap for format %u\n",
                texture->format);
        free(texture->opaque_bitmap);
        texture->opaque_bitmap = NULL;
        return 0;
    }

    return 1;
}

/*-----------------------------------------------------------------------*/

/**
 * align_texture_psp:  Align the given texture's pixel data for use on the
 * PSP (64-byte aligned, using 16-byte by 8-line blocks).  The texture
 * buffer is reallocated by this function, and freed on error.
 *
 * [Parameters]
 *     tex: Texture to modify.
 * [Return value]
 *     New texture pointer, or NULL on error.
 */
static Texture *align_texture_psp(Texture *texture)
{
    const unsigned int Bpp = (texture->format == TEX_FORMAT_PALETTE8_RGBA8888
                              || texture->format == TEX_FORMAT_A8) ? 1 : 4;
    const unsigned int block_width = 16 / Bpp;

    uint32_t total_pixel_bytes = 0;
    unsigned int width = texture->width;
    unsigned int height = texture->height;
    for (unsigned int level = 0; level <= texture->mipmaps; level++) {
        const unsigned int stride = align_up(width, block_width);
        total_pixel_bytes += stride * align_up(height, 8) * Bpp;
        width = lbound(width/2, 1);
        height = lbound(height/2, 1);
    }

    const uint32_t struct_size = align_up(sizeof(*texture), 64);
    const uint32_t alloc_size = struct_size + total_pixel_bytes;
    Texture *new_texture = malloc(alloc_size);
    if (!new_texture) {
        fprintf(stderr, "Out of memory generating mipmaps\n");
        free_texture(texture);
        return NULL;
    }
    *new_texture = *texture;
    new_texture->stride = align_up(texture->width, block_width);
    new_texture->pixels = (uint8_t *)new_texture + struct_size;

    const uint8_t *src = texture->pixels;
    uint8_t *dest = new_texture->pixels;
    width = texture->width;
    height = texture->height;
    unsigned int stride = texture->stride;
    for (unsigned int level = 0; level <= texture->mipmaps; level++) {
        const unsigned int new_stride = align_up(width, block_width);
        memset(dest, 0, new_stride * align_up(height, 8) * Bpp);

        for (unsigned int y = 0; y < height;
             y++, src += stride*Bpp, dest += new_stride*Bpp)
        {
            memcpy(dest, src, width*Bpp);
        }

        dest += new_stride * (align_up(height, 8) - height) * Bpp;
        width = lbound(width/2, 1);
        height = lbound(height/2, 1);
        stride = lbound(stride/2, 1);
    }

    free(texture);  // NOT free_texture(texture)! (this is just a realloc)
    return new_texture;
}

/*-----------------------------------------------------------------------*/

/**
 * swizzle_texture:  Swizzle the given texture's pixel data, which is
 * assumed to have been aligned with align_texture_psp().  The texture
 * buffer is _not_ reallocated.
 *
 * [Parameters]
 *     texture: Texture to swizzle.
 * [Return value]
 *     True on success, false on error.
 */
static int swizzle_texture(Texture *texture)
{
    unsigned int height = align_up(texture->height, 8);
    unsigned int stride = texture->stride;
    /* The pixels can be either 8bpp or 32bpp, but we process them as
     * 32bpp for speed.  (The swizzle block width is 16 bytes either way.) */
    uint32_t *pixels = (uint32_t *)texture->pixels;
    unsigned int stride_words = (texture->format == TEX_FORMAT_PALETTE8_RGBA8888
                                 || texture->format == TEX_FORMAT_A8
                                 ? stride/4 : stride);

    uint32_t *tempbuf = malloc(8 * (stride_words*4));
    if (!tempbuf) {
        fprintf(stderr, "Out of memory for temporary buffer (%u bytes)\n",
                8 * (stride_words*4));
        return 0;
    }

    for (unsigned int level = 0; level <= texture->mipmaps; level++) {
        const uint32_t *src = pixels;
        uint32_t *dest = pixels;

        for (unsigned int y = 0; y < height; y += 8, src += 8*stride_words) {
            memcpy(tempbuf, src, 8 * (stride_words*4));
            const uint32_t *tempsrc = tempbuf;
            for (unsigned int x = 0; x < stride_words; x += 4, tempsrc += 4) {
                const uint32_t *linesrc = tempsrc;
                for (unsigned int line = 0; line < 8;
                     line++, linesrc += stride_words, dest += 4)
                {
                    const uint32_t pixel0 = linesrc[0];
                    const uint32_t pixel1 = linesrc[1];
                    const uint32_t pixel2 = linesrc[2];
                    const uint32_t pixel3 = linesrc[3];
                    dest[0] = pixel0;
                    dest[1] = pixel1;
                    dest[2] = pixel2;
                    dest[3] = pixel3;
                }
            }
        }

        pixels += stride_words * height;
        height = align_up(height/2, 8);
        stride_words = align_up(stride_words/2, 4);
    }

    free(tempbuf);
    texture->swizzled = 1;
    return 1;
}

/*-----------------------------------------------------------------------*/

/**
 * compress_dxt:  Compress the given texture's pixel data using the given
 * S3TC DXTn compression method.  If the texture is fully opaque, DXT1 is
 * used regardless of the requested compression method.  The texture buffer
 * is _not_ reallocated.
 *
 * [Parameters]
 *     tex: Texture to compress.
 *     type: Compression type (1 = DXT1, 3 = DXT3, 5 = DXT5).
 * [Return value]
 *     True on success, false on error.
 */
static int compress_dxt(Texture *texture, int type)
{
    if (type != 1 && type != 3 && type != 5) {
        fprintf(stderr, "Invalid type for DXT: %d\n", type);
        return 0;
    }
    if (texture->format != TEX_FORMAT_RGBA8888) {
        fprintf(stderr, "Texture format must be RGBA8888 for DXT\n");
        return 0;
    }

    /* Check whether the texture data has any non-opaque pixels,
     * and set the texture format as appropriate. */
    int has_alpha = 0;
    for (int i = 3; i < texture->width * texture->height * 4; i += 4) {
        if (texture->pixels[i] != 0xFF) {
            has_alpha = 1;
            break;
        }
    }
    if (has_alpha) {
        if (type == 1) {
            fprintf(stderr, "warning: Conversion to DXT1 will drop alpha"
                    " channel\n");
        }
        texture->format = (type==1 ? TEX_FORMAT_S3TC_DXT1 :
                           type==3 ? TEX_FORMAT_S3TC_DXT3 :
                           TEX_FORMAT_S3TC_DXT5);
    } else {
        type = 1;
        texture->format = TEX_FORMAT_S3TC_DXT1;
    }

    /* Call out to dxtcomp to actually generate the data for each mipmap
     * level. */
    const unsigned int bpp = (type==1 ? 4 : 8);
    uint8_t *src = texture->pixels;
    uint8_t *dest = texture->pixels;
    unsigned int width = texture->width;
    unsigned int height = texture->height;
    uint8_t *temp = malloc(lbound(width,4) * lbound(height,4) * bpp/8 * 2);
    if (!temp) {
        fprintf(stderr, "Out of memory compressing to DXT\n");
        return 0;
    }
    for (unsigned int i = 0; i <= texture->mipmaps; i++) {
        if (!run_dxtcomp(src, width, height, type, temp)) {
            fprintf(stderr, "dxtcomp failed\n");
            return 0;
        }
        const unsigned int dxt_width = lbound(width, 4);
        const unsigned int dxt_height = lbound(height, 4);
        memcpy(dest, temp, (dxt_width * dxt_height * bpp) / 8);
        src += width * height * 4;
        dest += (dxt_width * dxt_height * bpp) / 8;
        width = lbound(width/2, 1);
        height = lbound(height/2, 1);
    }
    free(temp);

    return type;
}

/*----------------------------------*/

/**
 * run_dxtcomp:  Run the external dxtcomp program (assumed to be in the
 * executable path) to compress pixel data.  Note that DXTn data has a
 * minimum encoded size of 4x4, regardless of the actual image size, and
 * the caller must ensure enough output buffer space is available.
 *
 * [Parameters]
 *     src: Input pixel data, in RGBA8888 format (destroyed).
 *     width: Texture width, in pixels.
 *     height: Texture height, in pixels.
 *     type: DXTn compression type (1, 3, or 5).
 *     dest: Output data pointer.
 * [Return value]
 *     True on success, false on error.
 */
static int run_dxtcomp(const uint8_t *src, unsigned int width,
                       unsigned int height, unsigned int type, uint8_t *dest)
{
    const unsigned int bpp = (type==1 ? 4 : 8);
    char dir[1000], infile[1009], outfile[1009];

    /* Create a temporary directory for the dxtcomp input/output files.
     * We ignore a value of $TMPDIR containing an apostrophe because it'll
     * wreak havoc with our command line below. */
    const char *tmpdir = getenv("TMPDIR");
    if (!tmpdir || strchr(tmpdir, '\'') != NULL) {
        tmpdir = "/tmp";
    }
    if (snprintf(dir, sizeof(dir), "%s/pngtotexXXXXXX",
                 tmpdir) >= (int)sizeof(dir)) {
        fprintf(stderr, "Buffer overflow on temporary directory\n");
        goto error_return;
    }
    if (!mkdtemp(dir)) {
        fprintf(stderr, "Failed to create temporary directory (%s): %s\n",
                dir, strerror(errno));
        goto error_return;
    }
    /* These are guaranteed to fit. */
    snprintf(infile, sizeof(infile), "%s/in.rgba", dir);
    snprintf(outfile, sizeof(outfile), "%s/out.dxt", dir);

    /* Create a temporary buffer, expanding (with repetition) to the
     * minimum size for the selected format and forcing the alpha bytes to
     * full opacity if using DXT1 compression. */
    const unsigned int dxt_width = lbound(width, 4);
    const unsigned int dxt_height = lbound(height, 4);
    uint8_t *tempbuf = malloc(dxt_width * dxt_height * 4);
    if (!tempbuf) {
        fprintf(stderr, "Failed to allocate temporary buffer (%u bytes)\n",
                dxt_width * dxt_height * 4);
        goto error_rmdir;
    }
    if (width != dxt_width || height != dxt_height) {
        uint8_t *out = tempbuf;
        for (unsigned int y = 0; y < dxt_height; y++) {
            const uint8_t *row = src + (y % height) * width * 4;
            for (unsigned int x = 0; x < dxt_width; x++, out += 4) {
                const uint8_t r = row[(x % width) * 4 + 0];
                const uint8_t g = row[(x % width) * 4 + 1];
                const uint8_t b = row[(x % width) * 4 + 2];
                const uint8_t a = row[(x % width) * 4 + 3];
                out[0] = b;
                out[1] = g;
                out[2] = r;
                out[3] = (type==1 ? 255 : a);
            }
        }
    } else if (type == 1) {
        for (unsigned int i = 0; i < width*height*4; i += 4) {
            const uint8_t r = src[i+0];
            const uint8_t g = src[i+1];
            const uint8_t b = src[i+2];
            tempbuf[i+0] = b;
            tempbuf[i+1] = g;
            tempbuf[i+2] = r;
            tempbuf[i+3] = 255;
        }
        spread_border(tempbuf, width, height);
    } else {
        memcpy(tempbuf, src, width*height*4);
    }

    /* Write the pixel data as a raw RGBA file. */
    FILE *f = fopen(infile, "wb");
    if (!f) {
        fprintf(stderr, "Failed to create %s: %s\n", infile, strerror(errno));
        goto error_free_tempbuf;
    }
    if (fwrite(tempbuf, dxt_width*dxt_height*4, 1, f) != 1) {
        fprintf(stderr, "Failed to write data to %s: %s\n", infile,
                strerror(errno));
        fclose(f);
        goto error_remove_infile;
    }
    if (fclose(f) != 0) {
        fprintf(stderr, "Failed to close %s: %s\n", infile, strerror(errno));
        goto error_remove_infile;
    }

    /* Call out to dxtcomp. */
    char cmdbuf[4000];  // Long enough to avoid truncation.
    snprintf(cmdbuf, sizeof(cmdbuf), "dxtcomp -%d '%s' '%s' %u %u",
             type, infile, outfile, dxt_width, dxt_height);
    if (verbose) {
        fprintf(stderr, "Executing: %s\n", cmdbuf);
    }
    if (system(cmdbuf) != 0) {
        fprintf(stderr, "dxtcomp call failed\n");
        goto error_remove_outfile;
    }

    /* Read the compressed pixel data from the output file. */
    f = fopen(outfile, "r");
    if (!f) {
        fprintf(stderr, "Failed to open %s: %s\n", outfile, strerror(errno));
        goto error_remove_outfile;
    }
    if (fread(dest, (dxt_width * dxt_height * bpp) / 8, 1, f) != 1) {
        fprintf(stderr, "Failed to read data from %s\n", outfile);
        fclose(f);
        goto error_remove_outfile;
    }
    fclose(f);

    /* All done!  Clean up our files/buffers and return. */
    remove(outfile);
    remove(infile);
    rmdir(dir);
    free(tempbuf);
    return 1;

    /* Handle all error returns here so we don't leave junk behind. */
  error_remove_outfile:
    remove(outfile);
  error_remove_infile:
    remove(infile);
  error_free_tempbuf:
    free(tempbuf);
  error_rmdir:
    rmdir(dir);
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

/**
 * compress_pvrtc:  Compress the given texture's pixel data using the
 * PVRTC compression method.  The texture buffer is _not_ reallocated.
 *
 * [Parameters]
 *     tex: Texture to compress.
 *     bpp: Desired bits per pixel (2 or 4).
 * [Return value]
 *     True on success, false on error.
 */
static int compress_pvrtc(Texture *texture, unsigned int bpp)
{
    if (bpp != 2 && bpp != 4) {
        fprintf(stderr, "Invalid bits per pixel for PVRTC: %d\n", bpp);
        return 0;
    }
    if (texture->format != TEX_FORMAT_RGBA8888) {
        fprintf(stderr, "Texture format must be RGBA8888 for PVRTC\n");
        return 0;
    }

    /* Check whether the texture data has any non-opaque pixels,
     * and set the texture format as appropriate. */
    int has_alpha = 0;
    for (int i = 3; i < texture->width*texture->height*4; i += 4) {
        if (texture->pixels[i] != 0xFF) {
            has_alpha = 1;
            break;
        }
    }
    texture->format = (has_alpha
                   ? (bpp==2 ? TEX_FORMAT_PVRTC2_RGBA : TEX_FORMAT_PVRTC4_RGBA)
                   : (bpp==2 ? TEX_FORMAT_PVRTC2_RGB : TEX_FORMAT_PVRTC4_RGB));

    /* Call out to PVRTexToolCLI to actually generate the data for each
     * mipmap level. */
    uint8_t *src = texture->pixels;
    uint8_t *dest = texture->pixels;
    unsigned int width = texture->width;
    unsigned int height = texture->height;
    uint8_t *temp = malloc(lbound(width,16) * lbound(height,8) * bpp/8 * 2);
    if (!temp) {
        fprintf(stderr, "Out of memory compressing to PVRTC\n");
        return 0;
    }
    for (unsigned int i = 0; i <= texture->mipmaps; i++) {
        if (!run_pvrtextool(src, width, height, bpp, has_alpha, temp)) {
            fprintf(stderr, "PVRTC compression failed\n");
            return 0;
        }
        if (bpp == 4 && width >= 4 && height >= 4 && width == height) {
            fix_pvrtc4_alpha(src, temp, width, height);
        }
        const unsigned int pvr_width = lbound(width, 32/bpp);
        const unsigned int pvr_height = lbound(height, 8);
        memcpy(dest, temp, (pvr_width * pvr_height * bpp) / 8);
        src += width * height * 4;
        dest += (pvr_width * pvr_height * bpp) / 8;
        width = lbound(width/2, 1);
        height = lbound(height/2, 1);
    }
    free(temp);

    return 1;
}

/*----------------------------------*/

/**
 * run_pvrtextool:  Run the external PVRTexToolCLI program to compress
 * pixel data.  Note that PVRTC data has a minimum encoded size of 8x8 for
 * 4bpp or 16x8 for 2bpp, regardless of the actual image size, and the
 * caller must ensure enough output buffer space is available.
 *
 * [Parameters]
 *     src: Input pixel data, in RGBA8888 format (destroyed).
 *     width: Texture width, in pixels.
 *     height: Texture height, in pixels.
 *     bpp: Target bits per pixel (2 or 4).
 *     alpha: True if the texture uses the alpha channel, false if the
 *         texture is opaque.
 *     dest: Output data pointer.
 * [Return value]
 *     True on success, false on error.
 */
static int run_pvrtextool(const uint8_t *src, unsigned int width,
                          unsigned int height, unsigned int bpp, int alpha,
                          uint8_t *dest)
{
    char dir[1000], infile[1008], outfile[1009];

    /* Create a temporary directory for the PVRTexTool input/output files.
     * We ignore a value of $TMPDIR containing an apostrophe because it'll
     * wreak havoc with our command line below. */
    const char *tmpdir = getenv("TMPDIR");
    if (!tmpdir || strchr(tmpdir, '\'') != NULL) {
        tmpdir = "/tmp";
    }
    if (snprintf(dir, sizeof(dir), "%s/pngtotexXXXXXX",
                 tmpdir) >= (int)sizeof(dir)) {
        fprintf(stderr, "Buffer overflow on temporary directory\n");
        goto error_return;
    }
    if (!mkdtemp(dir)) {
        fprintf(stderr, "Failed to create temporary directory (%s): %s\n",
                dir, strerror(errno));
        goto error_return;
    }
    /* These are guaranteed to fit. */
    snprintf(infile, sizeof(infile), "%s/in.tga", dir);
    snprintf(outfile, sizeof(outfile), "%s/out.pvr", dir);

    /* Create a temporary buffer and massage the pixel data into BGR order,
     * expanding (with repetition) to the minimum size for the selected
     * format and deleting the alpha bytes if it's a non-alpha image. */
    const unsigned int pvr_width = lbound(width, 32/bpp);
    const unsigned int pvr_height = lbound(height, 32/bpp);
    uint8_t *tempbuf = malloc(pvr_width * pvr_height * (alpha ? 4 : 3));
    if (!tempbuf) {
        fprintf(stderr, "Failed to allocate temporary buffer (%u bytes)\n",
                pvr_width * pvr_height * (alpha ? 4 : 3));
        goto error_rmdir;
    }
    if (width != pvr_width || height != pvr_height) {
        uint8_t *out = tempbuf;
        for (unsigned int y = 0; y < pvr_height; y++) {
            const uint8_t *row = src + (y % height) * width * 4;
            for (unsigned int x = 0; x < pvr_width; x++, out += (alpha ? 4 : 3)) {
                const uint8_t r = row[(x % width) * 4 + 0];
                const uint8_t g = row[(x % width) * 4 + 1];
                const uint8_t b = row[(x % width) * 4 + 2];
                const uint8_t a = row[(x % width) * 4 + 3];
                out[0] = b;
                out[1] = g;
                out[2] = r;
                if (alpha) {
                    out[3] = a;
                }
            }
        }
    } else if (alpha) {
        for (unsigned int i = 0; i < width*height*4; i += 4) {
            const uint8_t r = src[i+0];
            const uint8_t g = src[i+1];
            const uint8_t b = src[i+2];
            const uint8_t a = src[i+3];
            tempbuf[i+0] = b;
            tempbuf[i+1] = g;
            tempbuf[i+2] = r;
            tempbuf[i+3] = a;
        }
        spread_border(tempbuf, width, height);
    } else {
        for (unsigned int i = 0, j = 0; i < width*height*4; i += 4, j += 3) {
            const uint8_t r = src[i+0];
            const uint8_t g = src[i+1];
            const uint8_t b = src[i+2];
            tempbuf[j+0] = b;
            tempbuf[j+1] = g;
            tempbuf[j+2] = r;
        }
    }

    /* Write the pixel data as a simple TGA file. */
    FILE *f = fopen(infile, "wb");
    if (!f) {
        fprintf(stderr, "Failed to create %s: %s\n", infile, strerror(errno));
        goto error_free_tempbuf;
    }
    uint8_t tga_header[18];
    memcpy(tga_header, "\0\0\2\0\0\0\0\0\0\0\0\0", 12);
    tga_header[12] = pvr_width >>0 & 0xFF;
    tga_header[13] = pvr_width >>8 & 0xFF;
    tga_header[14] = pvr_height>>0 & 0xFF;
    tga_header[15] = pvr_height>>8 & 0xFF;
    tga_header[16] = alpha ? 32 : 24;
    tga_header[17] = 0;
    if (fwrite(tga_header, 18, 1, f) != 1) {
        fprintf(stderr, "Failed to write header to %s: %s\n", infile,
                strerror(errno));
        fclose(f);
        goto error_remove_infile;
    }
    const unsigned int rowsize = alpha ? pvr_width*4 : pvr_width*3;
    /* PVRTexTool expects the data in bottom-up order. */
    for (int y = pvr_height-1; y >= 0; y--) {
        if (fwrite(tempbuf + y*rowsize, rowsize, 1, f) != 1) {
            fprintf(stderr, "Failed to write row %d data to %s: %s\n",
                    y, infile, strerror(errno));
            fclose(f);
            goto error_remove_infile;
        }
    }
    if (fclose(f) != 0) {
        fprintf(stderr, "Failed to close %s: %s\n", infile, strerror(errno));
        goto error_remove_infile;
    }

    /* Call out to PVRTexTool. */
    char cmdbuf[4000];  // Long enough to avoid truncation.
    snprintf(cmdbuf, sizeof(cmdbuf), "%s -f PVRTC1_%u%s -i '%s' -q pvrtc%s"
             " -o '%s'%s", pvrtextool, bpp, alpha ? "" : "_RGB",
             infile, hq ? "best" : "normal", outfile,
             verbose ? "" : " >/dev/null 2>&1");
    if (verbose) {
        fprintf(stderr, "Executing: %s\n", cmdbuf);
    }
    if (system(cmdbuf) != 0) {
        fprintf(stderr, "PVRTexToolCLI call failed%s\n",
                verbose ? "" : " (use -verbose to see errors)");
        if (strchr(pvrtextool, '/')) {
            fprintf(stderr, "Check that the path to the PVRTexToolCLI program"
                    " is correct.\n");
        } else {
            fprintf(stderr, "Check that the \"%s\" program can be found in"
                    " your PATH.\n", pvrtextool);
        }
        goto error_remove_outfile;
    }

    /* Read the compressed pixel data from the output file. */
    f = fopen(outfile, "r");
    if (!f) {
        fprintf(stderr, "Failed to open %s: %s\n", outfile, strerror(errno));
        goto error_remove_outfile;
    }
    uint8_t pvr_headerlen_buf[4];
    if (fread(pvr_headerlen_buf, 4, 1, f) != 1) {
        fprintf(stderr, "Failed to read header length from %s\n", outfile);
        fclose(f);
        goto error_remove_outfile;
    }
    uint32_t pvr_headerlen;
    if (memcmp(pvr_headerlen_buf, "PVR\3", 4) == 0) {
        /* PVR version 3 files have a fixed header size of 52 bytes plus
         * an arbitrary amount of metadata. */
        pvr_headerlen = 52;
        if (fseek(f, 48, SEEK_SET) != 0) {
            fprintf(stderr, "Failed to seek to metadata in %s: %s\n", outfile,
                    strerror(errno));
            fclose(f);
            goto error_remove_outfile;
        }
        if (fread(pvr_headerlen_buf, 4, 1, f) != 1) {
            fprintf(stderr, "Failed to read metadata length from %s\n",
                    outfile);
            fclose(f);
            goto error_remove_outfile;
        }
        pvr_headerlen += pvr_headerlen_buf[0]<< 0
                       | pvr_headerlen_buf[1]<< 8
                       | pvr_headerlen_buf[2]<<16
                       | pvr_headerlen_buf[3]<<24;
    } else {
        pvr_headerlen = pvr_headerlen_buf[0]<< 0
                      | pvr_headerlen_buf[1]<< 8
                      | pvr_headerlen_buf[2]<<16
                      | pvr_headerlen_buf[3]<<24;
    }
    if (fseek(f, pvr_headerlen, SEEK_SET) != 0) {
        fprintf(stderr, "Failed to seek to data in %s: %s\n", outfile,
                strerror(errno));
        fclose(f);
        goto error_remove_outfile;
    }
    if (fread(dest, (pvr_width * pvr_height * bpp) / 8, 1, f) != 1) {
        fprintf(stderr, "Failed to read data from %s\n", outfile);
        fclose(f);
        goto error_remove_outfile;
    }
    fclose(f);

    /* All done!  Clean up our files/buffers and return. */
    remove(outfile);
    remove(infile);
    rmdir(dir);
    free(tempbuf);
    return 1;

    /* Handle all error returns here so we don't leave junk behind. */
  error_remove_outfile:
    remove(outfile);
  error_remove_infile:
    remove(infile);
  error_free_tempbuf:
    free(tempbuf);
  error_rmdir:
    rmdir(dir);
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

/**
 * convert_rgba_to_bgra:  Convert the given texture (which must be in the
 * RGBA8888 format) to BGRA8888 format.  The texture buffer is _not_
 * reallocated.
 *
 * [Parameters]
 *     texture: Texture to convert.
 */
static void convert_rgba_to_bgra(Texture *texture)
{
    ASSERT(texture->format == TEX_FORMAT_RGBA8888);
    const int num_pixels = texture->width * texture->height;
    uint8_t *pixels = texture->pixels;
    for (int i = 0; i < num_pixels; i++) {
        uint8_t temp = pixels[i*4+0];
        pixels[i*4+0] = pixels[i*4+2];
        pixels[i*4+2] = temp;
    }
    texture->format = TEX_FORMAT_BGRA8888;
}

/*************************************************************************/
/******************** Miscellaneous utility functions ********************/
/*************************************************************************/

/**
 * spread_border:  Spread the border colors of the image throughout its
 * transparent region, to avoid "white halo" effect from texture
 * compression.
 *
 * [Parameters]
 *     pixels: Pixel buffer, in RGBA format.
 *     width: Image width, in pixels.
 *     height: Image height, in pixels.
 */
static void spread_border(uint8_t * const pixels, const unsigned int width,
                          const unsigned int height)
{
    uint8_t *saved_alpha = malloc(width*height);
    if (!saved_alpha) {
        fprintf(stderr, "Out of memory (can't allocate %u bytes)\n",
                width*height);
        exit(1);
    }
    {
        for (unsigned int i = 0; i < width*height; i++) {
            saved_alpha[i] = pixels[i*4+3];
        }
    }

    uint8_t *updated = calloc(width*height, 1);
    if (!updated) {
        fprintf(stderr, "Out of memory (can't allocate %u bytes)\n",
                width*height);
        exit(1);
    }
    int changed;
    do {
        changed = 0;
        for (unsigned int i = 0, y = 0; y < height; y++) {
            for (unsigned int x = 0; x < width; x++, i += 4) {
                if (pixels[i+3] == 0) {
                    unsigned int r = 0, g = 0, b = 0, weight = 0;
                    unsigned int j;
                    if (y > 0) {
                        if (x > 0) {
                            j = i - width*4 - 4;
                            if (pixels[j+3] != 0) {
                                const unsigned int w = pixels[j+3] * 7;
                                r += pixels[j+0] * w;
                                g += pixels[j+1] * w;
                                b += pixels[j+2] * w;
                                weight += w;
                            }
                        }
                        j = i - width*4;
                        if (pixels[j+3] != 0) {
                            const unsigned int w = pixels[j+3] * 10;
                            r += pixels[j+0] * w;
                            g += pixels[j+1] * w;
                            b += pixels[j+2] * w;
                            weight += w;
                        }
                        if (x < width-1) {
                            j = i - width*4 + 4;
                            if (pixels[j+3] != 0) {
                                const unsigned int w = pixels[j+3] * 7;
                                r += pixels[j+0] * w;
                                g += pixels[j+1] * w;
                                b += pixels[j+2] * w;
                                weight += w;
                            }
                        }
                    }
                    if (x > 0) {
                        j = i - 4;
                        if (pixels[j+3] != 0) {
                            const unsigned int w = pixels[j+3] * 10;
                            r += pixels[j+0] * w;
                            g += pixels[j+1] * w;
                            b += pixels[j+2] * w;
                            weight += w;
                        }
                    }
                    if (x < width-1) {
                        j = i + 4;
                        if (pixels[j+3] != 0) {
                            const unsigned int w = pixels[j+3] * 10;
                            r += pixels[j+0] * w;
                            g += pixels[j+1] * w;
                            b += pixels[j+2] * w;
                            weight += w;
                        }
                    }
                    if (y < height-1) {
                        if (x > 0) {
                            j = i + width*4 - 4;
                            if (pixels[j+3] != 0) {
                                const unsigned int w = pixels[j+3] * 7;
                                r += pixels[j+0] * w;
                                g += pixels[j+1] * w;
                                b += pixels[j+2] * w;
                                weight += w;
                            }
                        }
                        j = i + width*4;
                        if (pixels[j+3] != 0) {
                            const unsigned int w = pixels[j+3] * 10;
                            r += pixels[j+0] * w;
                            g += pixels[j+1] * w;
                            b += pixels[j+2] * w;
                            weight += w;
                        }
                        if (x < width-1) {
                            j = i + width*4 + 4;
                            if (pixels[j+3] != 0) {
                                const unsigned int w = pixels[j+3] * 7;
                                r += pixels[j+0] * w;
                                g += pixels[j+1] * w;
                                b += pixels[j+2] * w;
                                weight += w;
                            }
                        }
                    }
                    if (weight > 0) {
                        pixels[i+0] = (r + weight/2) / weight;
                        pixels[i+1] = (g + weight/2) / weight;
                        pixels[i+2] = (b + weight/2) / weight;
                        updated[i/4] = 1;
                        changed = 1;
                    }
                }  // if (pixels[i+3] == 0)
            }  // for (x = 0; x < width; x++, i += 4)
        }  // for (i = y = 0; y < height; y++)
        for (unsigned int i = 0; i < width*height; i++) {
            if (updated[i]) {
                pixels[i*4+3] = 1;  // Don't recalculate it next time.
            }
        }
    } while (changed);
    free(updated);

    {
        for (unsigned int i = 0; i < width*height; i++) {
            pixels[i*4+3] = saved_alpha[i];
        }
    }
    free(saved_alpha);
}

/*-----------------------------------------------------------------------*/

/**
 * fix_pvrtc4_alpha:  Correct the "ring" effect in PVRTexTool-compressed
 * textures by recalculating the modulation codes for each pixel using an
 * alpha-aware color-difference algorithm, and switching blocks to
 * punch-through alpha mode where necessary to ensure that transparent
 * pixels stay transparent.
 *
 * [Parameters]
 *     original: Original RGBA pixel data.
 *     compressed: PVRTC4-compressed pixel data.
 *     width: Texture width, in pixels.
 *     height: Texture height, in pixels (must be equal to width).
 */
static void fix_pvrtc4_alpha(const uint8_t *original, uint8_t *compressed,
                             unsigned int width, unsigned int height)
{
    const uint8_t *src = original;
    for (unsigned int y = 0; y < height/4; y++, src += (3*width)*4) {
        for (unsigned int x = 0; x < width/4; x++, src += 4*4) {
          redo:;

            /* Load the data words for this block and the 8 surrounding it. */

            uint32_t block_data[3][3];
            unsigned int yy;
            int block_y;
            for (yy = 0, block_y = (int)y-1; yy < 3; yy++, block_y++) {
                int real_y = (block_y + (height/4)) % (height/4);
                unsigned int xx;
                int block_x;
                for (xx = 0, block_x = (int)x-1; xx < 3; xx++, block_x++) {
                    int real_x = (block_x + (width/4)) % (width/4);
                    /* Inverted X and Y are intentional here -- apparently
                     * PVRTC puts Y in the lowest bit. */
                    const uint8_t *block =
                        compressed + 8*morton_index(real_y, real_x,
                                                    height/4, width/4);
                    block_data[yy][xx] = block[4] <<  0
                                       | block[5] <<  8
                                       | block[6] << 16
                                       | block[7] << 24;
                }
            }

            /* Extract the block mode for this block. */

            const unsigned int mode = block_data[1][1] & 1;

            /* Interpolate to find the effective color pairs for each
             * pixel in the block. */

            unsigned int rAs[4][4], gAs[4][4], bAs[4][4], aAs[4][4];
            unsigned int rBs[4][4], gBs[4][4], bBs[4][4], aBs[4][4];
            for (yy = 0; yy < 4; yy++) {
                const unsigned int yfrac = (yy+2) % 4;
                uint32_t *row0, *row1;
                if (yy < 2) {
                    row0 = block_data[0];
                    row1 = block_data[1];
                } else {
                    row0 = block_data[1];
                    row1 = block_data[2];
                }
                unsigned int rA00, gA00, bA00, aA00;
                unsigned int rA01, gA01, bA01, aA01;
                unsigned int rA10, gA10, bA10, aA10;
                unsigned int rA11, gA11, bA11, aA11;
                unsigned int rB00, gB00, bB00, aB00;
                unsigned int rB01, gB01, bB01, aB01;
                unsigned int rB10, gB10, bB10, aB10;
                unsigned int rB11, gB11, bB11, aB11;
                block_data_to_colors(row0[0],
                                     &rA00, &gA00, &bA00, &aA00,
                                     &rB00, &gB00, &bB00, &aB00);
                block_data_to_colors(row0[1],
                                     &rA01, &gA01, &bA01, &aA01,
                                     &rB01, &gB01, &bB01, &aB01);
                block_data_to_colors(row1[0],
                                     &rA10, &gA10, &bA10, &aA10,
                                     &rB10, &gB10, &bB10, &aB10);
                block_data_to_colors(row1[1],
                                     &rA11, &gA11, &bA11, &aA11,
                                     &rB11, &gB11, &bB11, &aB11);
                for (unsigned int xx = 0; xx < 4; xx++) {
                    const unsigned int xfrac = (xx+2) % 4;
                    const unsigned int weight00 = (4-xfrac)*(4-yfrac);
                    const unsigned int weight01 = (  xfrac)*(4-yfrac);
                    const unsigned int weight10 = (4-xfrac)*(  yfrac);
                    const unsigned int weight11 = (  xfrac)*(  yfrac);
                    if (xx == 2) {
                        row0++;
                        row1++;
                        block_data_to_colors(row0[0],
                                             &rA00, &gA00, &bA00, &aA00,
                                             &rB00, &gB00, &bB00, &aB00);
                        block_data_to_colors(row0[1],
                                             &rA01, &gA01, &bA01, &aA01,
                                             &rB01, &gB01, &bB01, &aB01);
                        block_data_to_colors(row1[0],
                                             &rA10, &gA10, &bA10, &aA10,
                                             &rB10, &gB10, &bB10, &aB10);
                        block_data_to_colors(row1[1],
                                             &rA11, &gA11, &bA11, &aA11,
                                             &rB11, &gB11, &bB11, &aB11);
                    }
                    rAs[yy][xx] = (rA00*weight00 + rA01*weight01
                                 + rA10*weight10 + rA11*weight11) / 16;
                    gAs[yy][xx] = (gA00*weight00 + gA01*weight01
                                 + gA10*weight10 + gA11*weight11) / 16;
                    bAs[yy][xx] = (bA00*weight00 + bA01*weight01
                                 + bA10*weight10 + bA11*weight11) / 16;
                    aAs[yy][xx] = (aA00*weight00 + aA01*weight01
                                 + aA10*weight10 + aA11*weight11) / 16;
                    rBs[yy][xx] = (rB00*weight00 + rB01*weight01
                                 + rB10*weight10 + rB11*weight11) / 16;
                    gBs[yy][xx] = (gB00*weight00 + gB01*weight01
                                 + gB10*weight10 + gB11*weight11) / 16;
                    bBs[yy][xx] = (bB00*weight00 + bB01*weight01
                                 + bB10*weight10 + bB11*weight11) / 16;
                    aBs[yy][xx] = (aB00*weight00 + aB01*weight01
                                 + aB10*weight10 + aB11*weight11) / 16;
                }
            }

            /* Recalculate the modulation values for this block, using a
             * color-difference algorithm that ensures all transparent
             * pixels stay transparent. */

            uint8_t *block =
                compressed + 8*morton_index(y, x, height/4, width/4);
            const uint8_t *src_row;
            for (src_row = src, yy = 0; yy < 4; yy++, src_row += width*4) {
                for (unsigned int xx = 0; xx < 4; xx++) {

                    /* Pull out colors A and B from the precomputed array. */
                    const unsigned int rA = rAs[yy][xx];
                    const unsigned int gA = gAs[yy][xx];
                    const unsigned int bA = bAs[yy][xx];
                    const unsigned int aA = aAs[yy][xx];
                    const unsigned int rB = rBs[yy][xx];
                    const unsigned int gB = gBs[yy][xx];
                    const unsigned int bB = bBs[yy][xx];
                    const unsigned int aB = aBs[yy][xx];

                    /* Colors C and D correspond to modulation values 01
                     * and 10, respectively. */
                    unsigned int rC, gC, bC, aC;
                    unsigned int rD, gD, bD, aD;
                    if (mode == 0) {
                        rC = (rA*5 + rB*3) / 8;
                        gC = (gA*5 + gB*3) / 8;
                        bC = (bA*5 + bB*3) / 8;
                        aC = (aA*5 + aB*3) / 8;
                        rD = (rA*3 + rB*5) / 8;
                        gD = (gA*3 + gB*5) / 8;
                        bD = (bA*3 + bB*5) / 8;
                        aD = (aA*3 + aB*5) / 8;
                    } else {  // mode == 1
                        rC = rD = (rA + rB) / 2;
                        gC = gD = (gA + gB) / 2;
                        bC = bD = (bA + bB) / 2;
                        aC = (aA + aB) / 2;
                        aD = 0;
                    }

                    /* Load the colors for this pixel. */
                    const unsigned int r = src_row[xx*4+0];
                    const unsigned int g = src_row[xx*4+1];
                    const unsigned int b = src_row[xx*4+2];
                    const unsigned int a = src_row[xx*4+3];

                    /* If this is a transparent pixel and none of the
                     * available colors is transparent, switch to block
                     * mode 1 and reprocess the block. */
                    if (a == 0 && aA != 0 && aB != 0 && aC != 0 && aD != 0) {
                        block[4] |= 1;
                        goto redo;
                    }

                    /* Find the best-fit modulation value for this pixel. */
                    const uint32_t dAsq = colordiff_sq(r,g,b,a, rA,gA,bA,aA);
                    const uint32_t dBsq = colordiff_sq(r,g,b,a, rB,gB,bB,aB);
                    const uint32_t dCsq = colordiff_sq(r,g,b,a, rC,gC,bC,aC);
                    const uint32_t dDsq = colordiff_sq(r,g,b,a, rD,gD,bD,aD);
                    uint8_t M;
                    if (dAsq <= dBsq && dAsq <= dCsq && dAsq <= dDsq) {
                        M = 0;
                    } else if (dBsq <= dAsq && dBsq <= dCsq && dBsq <= dDsq) {
                        M = 3;
                    } else if (dCsq <= dAsq && dCsq <= dBsq && dCsq <= dDsq) {
                        M = 1;
                    } else {
                        M = 2;
                    }

                    /* Replace the pixel's modulation value in the
                     * compressed data. */
                    block[yy] &= ~(3 << (xx*2));
                    block[yy] |= M << (xx*2);
                }
            }
        }
    }
}

/*----------------------------------*/

/**
 * morton_index:  Return the Morton (Z-order) index for the given X and Y
 * coordinates.  Helper function for do_pvrtc4().
 *
 * [Parameters]
 *     x, y: Coordinates to convert.
 *     w, h: Range (max+1) of the X and Y coordinates (must be powers of two).
 * [Return value]
 *     Corresponding Morton index.
 */
static CONST_FUNCTION int morton_index(unsigned int x, unsigned int y,
                                       unsigned int w, unsigned int h)
{
    int index = 0, shift = 0;
    while (w > 0 || h > 0) {
        if (w > 0) {
            index |= (x & 1) << shift;
            shift++;
            x >>= 1;
            w >>= 1;
        }
        if (h > 0) {
            index |= (y & 1) << shift;
            shift++;
            y >>= 1;
            h >>= 1;
        }
    }
    return index;
}

/*----------------------------------*/

/**
 * block_data_to_colors:  Extract the color components of the two colors
 * specified by the given block's data word.
 *
 * [Parameters]
 *     block_data: 32-bit block data word.
 *     rA, gA, bA, aA: Pointers to variables to receive the RGBA components
 *         of color A.
 *     rB, gB, bB, aB: Pointers to variables to receive the RGBA components
 *         of color B.
 */
static inline void block_data_to_colors(
    uint32_t block_data,
    unsigned int *rA, unsigned int *gA, unsigned int *bA, unsigned int *aA,
    unsigned int *rB, unsigned int *gB, unsigned int *bB, unsigned int *aB)
{
    const unsigned int color_A = block_data & 0xFFFE;
    const unsigned int color_B = block_data >> 16;
    if (color_A & 0x8000) {
        *rA = (color_A>>10 & 0x1F) * 33 / 4;
        *gA = (color_A>> 5 & 0x1F) * 33 / 4;
        *bA = ((color_A>>1 & 0x0F) << 1 | (color_A>>4 & 1)) * 33 / 4;
        *aA = 0xFF;
    } else {
        *rA = ((color_A>> 8 & 0xF) << 1 | (color_A>>11 & 1)) * 33 / 4;
        *gA = ((color_A>> 4 & 0xF) << 1 | (color_A>> 7 & 1)) * 33 / 4;
        *bA = ((color_A>> 1 & 0x7) << 2 | (color_A>> 2 & 3)) * 33 / 4;
        *aA = ((color_A>>12 & 0x7) << 1) * 17;
    }
    if (color_B & 0x8000) {
        *rB = (color_B>>10 & 0x1F) * 33 / 4;
        *gB = (color_B>> 5 & 0x1F) * 33 / 4;
        *bB = (color_B>> 0 & 0x1F) * 33 / 4;
        *aB = 0xFF;
    } else {
        *rB = ((color_B>> 8 & 0xF) << 1 | (color_B>>11 & 1)) * 33 / 4;
        *gB = ((color_B>> 4 & 0xF) << 1 | (color_B>> 7 & 1)) * 33 / 4;
        *bB = ((color_B>> 0 & 0xF) << 1 | (color_B>> 3 & 1)) * 33 / 4;
        *aB = ((color_B>>12 & 0x7) << 1) * 17;
    }
}

/*----------------------------------*/

/**
 * colordiff_sq:  Return the square of the 4-dimensional color difference
 * between the two given color values, taking into account alpha values.
 * (RGB component differences between low-alpha pixels are not as important
 * as between high-alpha pixels, but alpha component differences are always
 * important.)
 *
 * [Parameters]
 *     r1, g1, b1, a1: RGBA components of color 1.
 *     r2, g2, b2, a2: RGBA components of color 2.
 * [Return value]
 *     Square of color difference (unsigned 32-bit value).
 */
static CONST_FUNCTION inline uint32_t colordiff_sq(
    unsigned int r1, unsigned int g1, unsigned int b1, unsigned int a1,
    unsigned int r2, unsigned int g2, unsigned int b2, unsigned int a2)
{
    const int dr = (int)r1 - (int)r2;
    const int dg = (int)g1 - (int)g2;
    const int db = (int)b1 - (int)b2;
    const int da = (int)a1 - (int)a2;
    /* Divide each component's difference by 4 to avoid integer overflow.
     * (This is the same algorithm used by colordiff_sq() in quantize.c for
     * 8bpp color quantization.) */
    return ((uint32_t)(dr*dr) * (a1*a2+1)) / 4
         + ((uint32_t)(dg*dg) * (a1*a2+1)) / 4
         + ((uint32_t)(db*db) * (a1*a2+1)) / 4
         + ((uint32_t)(da*da) * (255*255+1)) / 4;
}

/*-----------------------------------------------------------------------*/

/**
 * free_texture:  Free all memory associated with the given texture.
 * Does nothing if texture == NULL.
 *
 * [Parameters]
 *     texture: Texture to free.
 */
static void free_texture(Texture *texture)
{
    if (texture) {
        free(texture->palette);
        free(texture->opaque_bitmap);
        free(texture);
    }
}

/*************************************************************************/
/*************************************************************************/
