/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/quickpng.c: Simple PNG image generator used for creating
 * save file icons from screen captures.
 */

#include "src/base.h"
#include "src/sysdep/psp/quickpng.h"

/*
 * quickpng.c -- quick PNG image generator
 *
 * Written by Andrew Church <achurch@achurch.org>
 * This source code is public domain.
 *
 * ==============================================
 *
 * Version history:
 *
 * Version 1.3b, 2013-01-21
 *   - Style tweaks; no functional changes.
 *
 * Version 1.3a, 2010-11-03
 *   - Fixed warnings from GCC about comparisons between signed and
 *     unsigned values; no functional changes.
 *
 * Version 1.3, 2010-03-26
 *   - Fixed an integer overflow in quickpng_rgb32_size() for environments
 *     in which the int type is 16 bits wide.
 *   - Minor comment cleanup.
 *
 * Version 1.2, 2008-02-08
 *   - Added the use_alpha parameter to quickpng_from_rgb32() and
 *     quickpng_rgb32_size().
 *
 * Version 1.1, 2007-12-08
 *   - Added quickpng_append_chunk().
 *   - Specified quickpng_from_rgb32()'s successful return value to be the
 *     length of the resultant data (in bytes).
 *
 * Version 1.0, 2007-12-04
 *   - Initial release.
 *
 * ==============================================
 *
 * This file implements a simple generator for PNG (Portable Network
 * Graphics) images, as defined in ISO/IEC 15948:2003.  The generator is
 * designed to be both independent of external libraries, and as fast and
 * compact as possible; however, no optimization or compression is
 * performed on the image data, so the resulting PNG file will be larger
 * than would be produced by typical image generation routines.
 *
 * Since the PNG standard does not (unfortunately) support uncompressed
 * images, this file instead generates a PNG stream with one uncompressed
 * "deflate" block per pixel row, incurring an overhead of five bytes per
 * row (in addition to the filter type byte).  In order to avoid block and
 * data lengths exceeding the "deflate" and PNG format limitations, the
 * image size is limited to 10,000 pixels or less in both dimensions.
 *
 * To create a PNG file from a 32-bit-per-pixel source image buffer, call:
 *     quickpng_from_rgb32(source_data, width, height, stride,
 *                         output_buffer, output_size,
 *                         bgr_order, alpha_first, use_alpha)
 * where:
 *     - "source_data" is a pointer to the first byte of the image data;
 *     - "width" and "height" specify the image size, in pixels;
 *     - "stride" specifies the image line length (the linear distance
 *          between two vertically adjacent pixels in memory), in pixels;
 *     - "output_buffer" is a pointer to the first byte of the buffer into
 *          which the generated PNG data will be stored;
 *     - "output_size" is the number of bytes available in the output
 *          buffer;
 *     - "bgr_order" is nonzero if the pixel data is in BGR format, zero if
 *          in RGB format; and
 *     - "alpha_first" is nonzero if the alpha byte of the pixel data
 *          precedes the color bytes, zero if it follows the color bytes.
 *          The alpha channel itself is ignored unless use_alpha is nonzero.
 *     - "use_alpha" is nonzero to create a PNG file with an alpha channel,
 *          zero to create a plain RGB PNG file.
 *
 * The following table shows the correspondence between supported data
 * formats and the appropriate values of "bgr_order" and "alpha_first"
 * (note that any nonzero value can be used in place of the value 1):
 *
 *     Byte order | bgr_order | alpha_first
 *     -----------+-----------+------------
 *        RGBA    |     0     |      0
 *        BGRA    |     1     |      0
 *        ARGB    |     0     |      1
 *        ABGR    |     1     |      1
 *
 * The size of the output data depends solely on the number of pixels in
 * the image, and can be calculated using the quickpng_rgb32_size()
 * function:
 *
 *     output_size = quickpng_rgb32_size(width, height);
 *
 * The quickpng_from_rgb32() function returns an unspecified nonzero value
 * if the PNG data was successfully generated, otherwise zero.  The
 * function will fail if and only if one or more of the following is true:
 *     - source_data == NULL
 *     - width <= 0 || width > 10000
 *     - height <= 0 || height > 10000
 *     - stride < width
 *     - output_buffer == NULL
 *     - output_size < quickpng_rgb32_size(width, height)
 * (Of course, the function may cause unexpected behavior or crash if the
 * source_data or output_buffer pointers point to invalid or undersized
 * memory regions.  Such is the excitement of working with a pointer-based
 * language like C.)
 *
 * If desired, additional chunks can be appended to the PNG file by calling
 * quickpng_append_chunk() for each chunk to be added.  The chunks are
 * inserted immediately before the final IEND chunk, so standard chunks
 * required to be placed before the image data (such as tRNS) cannot be
 * inserted in this manner.  This function is intended primarily for
 * application-specific ancilliary data.
 */

/*************************************************************************/
/*************************************************************************/

/*
 * Exported function declarations.  These may be removed to a separate
 * header file if appropriate for the application.
 */

/**
 * quickpng_from_rgb32:  Generate a PNG file from a 32-bit-per-pixel RGB
 * image (with 8 bits for each color component and 8 alpha or unused bits).
 *
 * Parameters:
 *       source_data: A pointer to the first byte of the image data.
 *             width: The width of the image, in pixels.
 *            height: The height of the image, in pixels.
 *            stride: The length of a pixel row's data, in pixels.
 *     output_buffer: A pointer to the first byte of the buffer into which
 *                       the generated PNG data will be stored.
 *       output_size: The number of bytes available in the output buffer.
 *         bgr_order: Nonzero if the pixel data is in BGR format; zero if
 *                       in RGB format.
 *       alpha_first: Nonzero if the alpha or unused byte of the pixel data
 *                       precedes the color bytes, zero if it follows the
 *                       color bytes.
 *         use_alpha: Nonzero to include the image's alpha channel in the
 *                       PNG file, zero to create an RGB-only PNG file.
 * Return value:
 *     The resultant PNG data length (nonzero, in bytes) on success, zero
 *     on failure.
 * Preconditions:
 *     If source_data is not NULL and both stride and height are positive,
 *        source_data points to at least (stride * height * 4) bytes of
 *        readable memory.
 *     If output_buffer is not NULL and output_size is positive,
 *        output_buffer points to at least output_size bytes of writable
 *        memory.
 * Postconditions:
 *     The function succeeds iff all of the following are true on entry:
 *        - source_data != NULL
 *        - width > 0 && width <= 10000
 *        - height > 0 && height <= 10000
 *        - stride >= width
 *        - output_buffer != NULL
 *        - output_size >= quickpng_rgb32_size(width, height)
 */
extern long quickpng_from_rgb32(const void *source_data,
                                int width, int height, int stride,
                                void *output_buffer, long output_size,
                                int bgr_order, int alpha_first, int use_alpha);

/**
 * quickpng_rgb32_size:  Return the size of the buffer necessary to store
 * the PNG data generated by quickpng_from_rgb32() for the specified image
 * size.
 *
 * It is an error to specify a nonpositive value or a value greater than
 * 10,000 for either width or height.
 *
 * Parameters:
 *         width: The width of the image, in pixels.
 *        height: The height of the image, in pixels.
 *     use_alpha: Whether or not to include an alpha channel.  (Use the same
 *                   value you intend to pass to quickpng_from_rgb32().)
 * Return value:
 *     The number of bytes required to hold the resulting PNG data, or an
 *     unspecified negative value on error.
 * Postconditions:
 *     If both width and height are within the range [1,10000], then the
 *     return value is positive.
 */
extern long quickpng_rgb32_size(int width, int height, int use_alpha);

/**
 * quickpng_append_chunk:  Append an arbitrary chunk to a PNG file,
 * inserting the chunk immediately before the trailing IEND chunk.  Fails
 * if there is not enough space in the PNG data buffer to insert the chunk
 * (there must be at least 12 bytes free in addition to the space required
 * by the chunk data itself).
 *
 * If the chunk to append will not contain any data (i.e., chunk_len == 0),
 * then chunk_data may be specified as NULL.
 *
 * Parameters:
 *          chunk_type: Pointer to a buffer containing the 4-byte chunk type.
 *          chunk_data: Pointer to the data for the chunk to append.
 *           chunk_len: Length of the chunk data, in bytes.
 *            png_data: Pointer to the PNG data buffer.
 *             png_len: Length of the current PNG data, in bytes.
 *     png_buffer_size: Size of the PNG data buffer, in bytes.
 * Return value:
 *     The resultant PNG data length (nonzero, in bytes) on success, zero
 *     on failure.
 * Preconditions:
 *     If chunk_type is not NULL, chunk_type points to at least 4 bytes of
 *        readable memory.
 *     If chunk_data is not NULL and chunk_len is positive, chunk_data
 *        points to at least chunk_len bytes of readable memory.
 *     If chunk_len is positive, its value can be represented in 32 bits.
 *     If png_data is not NULL and png_buffer_size is positive, png_data
 *        points to at least png_buffer_size bytes of readable and writable
 *        memory.
 */
extern long quickpng_append_chunk(const void *chunk_type,
                                  const void *chunk_data, long chunk_len,
                                  void *png_data, long png_len,
                                  long png_buffer_size);

/*************************************************************************/

/* Define the NULL symbol for convenience, if it's not already defined. */

#ifndef NULL
# define NULL 0
#endif


/* Local routine declarations: */

static void store_32bit_value(unsigned char *buffer, unsigned long value);
static void generate_crc_table(unsigned long crc_table[256]);
static unsigned long crc(unsigned char *data, long size,
                         const unsigned long crc_table[256]);
static unsigned long partial_adler32(unsigned long adler32,
                                     unsigned char *data, long size);


/* PNG header data (written before the image data): */

static const unsigned char png_header[0x2B] =
    "\x89PNG\x0D\x0A\x1A\x0A\x00\x00\x00\x0DIHDR"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x08\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00IDAT\x78\x01";

#define PNG_HEADER_OFFSET_IHDR      0x0C  /* Offset to the IHDR chunk */
#define PNG_HEADER_SIZE_IHDR        0x11  /* Size of the IHDR chunk */
#define PNG_HEADER_OFFSET_WIDTH     0x10  /* Offset to the image width */
#define PNG_HEADER_OFFSET_HEIGHT    0x14  /* Offset to the image height */
#define PNG_HEADER_OFFSET_FORMAT    0x19  /* Offset to the image format */
#define PNG_HEADER_OFFSET_IHDR_CRC  0x1D  /* Offset to the IHDR CRC */
#define PNG_HEADER_OFFSET_IDAT_SIZE 0x21  /* Offset to the IDAT chunk size */
#define PNG_HEADER_OFFSET_IDAT_DATA 0x29  /* Offset to the IDAT chunk data */


/* PNG trailer data (written after the image data and CRCs): */

static const unsigned char png_trailer[12] =
    "\x00\x00\x00\x00IEND\xAE\x42\x60\x82";

/*************************************************************************/
/*************************************************************************/

/**
 * quickpng_from_rgb32:  Generate a PNG file from a 32-bit-per-pixel RGB
 * image (with 8 bits for each color component and 8 alpha or unused bits).
 *
 * Parameters:
 *       source_data: A pointer to the first byte of the image data.
 *             width: The width of the image, in pixels.
 *            height: The height of the image, in pixels.
 *            stride: The length of a pixel row's data, in pixels.
 *     output_buffer: A pointer to the first byte of the buffer into which
 *                       the generated PNG data will be stored.
 *       output_size: The number of bytes available in the output buffer.
 *         bgr_order: Nonzero if the pixel data is in BGR format; zero if
 *                       in RGB format.
 *       alpha_first: Nonzero if the alpha or unused byte of the pixel data
 *                       precedes the color bytes, zero if it follows the
 *                       color bytes.
 *         use_alpha: Nonzero to include the image's alpha channel in the
 *                       PNG file, zero to create an RGB-only PNG file.
 * Return value:
 *     The resultant PNG data length (nonzero, in bytes) on success, zero
 *     on failure.
 * Preconditions:
 *     If source_data is not NULL and both stride and height are positive,
 *        source_data points to at least (stride * height * 4) bytes of
 *        readable memory.
 *     If output_buffer is not NULL and output_size is positive,
 *        output_buffer points to at least output_size bytes of readable
 *        and writable memory.
 * Postconditions:
 *     The function succeeds iff all of the following are true on entry:
 *        - source_data != NULL
 *        - width > 0 && width <= 10000
 *        - height > 0 && height <= 10000
 *        - stride >= width
 *        - output_buffer != NULL
 *        - output_size >= quickpng_rgb32_size(width, height, use_alpha)
 */
long quickpng_from_rgb32(const void *source_data,
                         int width, int height, int stride,
                         void *output_buffer, long output_size,
                         int bgr_order, int alpha_first, int use_alpha)
{
    const unsigned char *src_ptr;  /* Copy source pointer */
    unsigned char *dest_ptr;       /* Copy destination pointer */
    unsigned char *idat_ptr;       /* Pointer to the start of the IDAT data */
    unsigned char *idat_size_ptr;  /* Pointer to the IDAT chunk size field */
    unsigned long idat_size;       /* Size of the IDAT chunk data */
    unsigned long zlib_crc;        /* CRC value to append to the zlib data */
    unsigned long crc_table[256];  /* Lookup table for CRC computation */
    unsigned int Bpp;              /* Bytes per pixel (depends on use_alpha) */
    unsigned int i;
    int x, y;

    /* Check parameter validity.  This is the only place where the function
     * can fail.  Note that if execution reaches the quickpng_rgb32_size()
     * call, that call will always succeed, since width and height have
     * already been range-checked. */
    if (source_data == NULL
     || width <= 0 || width > 10000
     || height <= 0 || height > 10000
     || stride < width
     || output_buffer == NULL
     || output_size < quickpng_rgb32_size(width, height, use_alpha)) {
        return 0;
    }

    /* Calculate the number of bytes per pixel. */
    Bpp = (use_alpha ? 4 : 3);

    /* Generate the CRC lookup table. */
    generate_crc_table(crc_table);

    /* Copy the PNG header to the output buffer, inserting image-specific
     * values where appropriate, and save pointers to the IDAT size field
     * and chunk start. */
    dest_ptr = (unsigned char *)output_buffer;
    for (i = 0; i < sizeof(png_header); i++) {
        dest_ptr[i] = png_header[i];
    }
    store_32bit_value(&dest_ptr[PNG_HEADER_OFFSET_WIDTH], width);
    store_32bit_value(&dest_ptr[PNG_HEADER_OFFSET_HEIGHT], height);
    dest_ptr[PNG_HEADER_OFFSET_FORMAT] = (use_alpha ? 6 : 2);
    store_32bit_value(&dest_ptr[PNG_HEADER_OFFSET_IHDR_CRC],
                      crc(&dest_ptr[PNG_HEADER_OFFSET_IHDR],
                          PNG_HEADER_SIZE_IHDR, crc_table));
    idat_size_ptr = &dest_ptr[PNG_HEADER_OFFSET_IDAT_SIZE];
    idat_ptr = &dest_ptr[PNG_HEADER_OFFSET_IDAT_DATA];
    dest_ptr += sizeof(png_header);

    /* Copy the image data itself to the output buffer, inserting the
     * necessary overhead bytes. */
    src_ptr = (const unsigned char *)source_data;
    if (alpha_first) {
        src_ptr++;
    }
    zlib_crc = 1;
    for (y = 0; y < height; y++, src_ptr += (stride - width) * 4) {
        /* Save a pointer to the beginning of the "deflate" block's data,
         * so we can update the zlib CRC afterwards. */
        unsigned char *row_start = dest_ptr + 5;
        /* Precalculate the size of the block's data. */
        const unsigned int line_size = 1 + width*Bpp;
        /* Set up the block header and filter type. */
        dest_ptr[0] = (y == height-1) ? 0x01 : 0x00;
        dest_ptr[1] = (line_size >> 0) & 0xFF;
        dest_ptr[2] = (line_size >> 8) & 0xFF;
        dest_ptr[3] = ~dest_ptr[1];
        dest_ptr[4] = ~dest_ptr[2];
        dest_ptr[5] = 0;
        dest_ptr += 6;
        /* Copy the pixel data itself. */
        for (x = 0; x < width; x++, src_ptr += 4, dest_ptr += Bpp) {
            if (bgr_order) {
                dest_ptr[0] = src_ptr[2];
                dest_ptr[1] = src_ptr[1];
                dest_ptr[2] = src_ptr[0];
            } else {
                dest_ptr[0] = src_ptr[0];
                dest_ptr[1] = src_ptr[1];
                dest_ptr[2] = src_ptr[2];
            }
            if (use_alpha) {
                dest_ptr[3] = src_ptr[alpha_first ? -1 : 3];
            }
        }
        /* Update the zlib CRC. */
        zlib_crc = partial_adler32(zlib_crc, row_start, line_size);
    }

    /* Compute and store the image data length (note that the data length
     * includes the zlib CRC, which is appended to the image data as part
     * of png_trailer[] below). */
    idat_size = (dest_ptr + 4) - idat_ptr;
    store_32bit_value(idat_size_ptr, idat_size);

    /* Compute and store CRC values for the image data. */
    store_32bit_value(dest_ptr, zlib_crc);
    store_32bit_value(dest_ptr+4, crc(idat_ptr-4, idat_size+4, crc_table));
    dest_ptr += 8;

    /* Copy the PNG trailer to the output buffer. */
    for (i = 0; i < sizeof(png_trailer); i++) {
        dest_ptr[i] = png_trailer[i];
    }
    dest_ptr += sizeof(png_trailer);

    /* Return the final PNG data length. */
    return dest_ptr - (unsigned char *)output_buffer;
}

/*************************************************************************/

/**
 * quickpng_rgb32_size:  Return the size of the buffer necessary to store
 * the PNG data generated by quickpng_from_rgb32() for the specified image
 * size.
 *
 * It is an error to specify a nonpositive value or a value greater than
 * 10,000 for either width or height.
 *
 * Parameters:
 *         width: The width of the image, in pixels.
 *        height: The height of the image, in pixels.
 *     use_alpha: Whether or not to include an alpha channel.  (Use the same
 *                   value you intend to pass to quickpng_from_rgb32().)
 * Return value:
 *     The number of bytes required to hold the resulting PNG data, or an
 *     unspecified negative value on error.
 * Postconditions:
 *     If both width and height are within the range [1,10000], then the
 *     return value is positive.
 */
long quickpng_rgb32_size(int width, int height, int use_alpha)
{
    if (width <= 0 || width > 10000 || height <= 0 || height > 10000) {
        return -1;
    }

    /* Note that this calculation cannot overflow: the maximum value is
     * roughly 4 * 10^8, well within the range of a signed 32-bit integer. */
    unsigned int Bpp = (use_alpha ? 4 : 3);
    return sizeof(png_header)                               /* File header */
         + (((long)width*Bpp + 6) * (long)height)            /* Image data */
         + 8 + sizeof(png_trailer);    /* Image data CRCs and file trailer */
}

/*************************************************************************/

/**
 * quickpng_append_chunk:  Append an arbitrary chunk to a PNG file,
 * inserting the chunk immediately before the trailing IEND chunk.  Fails
 * if there is not enough space in the PNG data buffer to insert the chunk
 * (there must be at least 12 bytes free in addition to the space required
 * by the chunk data itself).
 *
 * If the chunk to append will not contain any data (i.e., chunk_len == 0),
 * then chunk_data may be specified as NULL.
 *
 * Parameters:
 *          chunk_type: Pointer to a buffer containing the 4-byte chunk type.
 *          chunk_data: Pointer to the data for the chunk to append.
 *           chunk_len: Length of the chunk data, in bytes.
 *            png_data: Pointer to the PNG data buffer.
 *             png_len: Length of the current PNG data, in bytes.
 *     png_buffer_size: Size of the PNG data buffer, in bytes.
 * Return value:
 *     The resultant PNG data length (nonzero, in bytes) on success, zero
 *     on failure.
 * Preconditions:
 *     If chunk_type is not NULL, chunk_type points to at least 4 bytes of
 *        readable memory.
 *     If chunk_data is not NULL and chunk_len is positive, chunk_data
 *        points to at least chunk_len bytes of readable memory.
 *     If png_data is not NULL and png_buffer_size is positive, png_data
 *        points to at least png_buffer_size bytes of readable and writable
 *        memory.
 */
long quickpng_append_chunk(const void *chunk_type,
                           const void *chunk_data, long chunk_len,
                           void *png_data, long png_len,
                           long png_buffer_size)
{
    const unsigned char *src_ptr;
    unsigned char *dest_ptr;
    unsigned char *dest_chunk_ptr;  /* Points to the chunk in the PNG buffer */
    unsigned long crc_table[256];   /* Lookup table for CRC computation */
    long i;

    /* Check for obvious parameter errors. */
    if (chunk_type == NULL
     || (chunk_data == NULL && chunk_len > 0)
     || chunk_len < 0
     || png_data == NULL
     || png_len < (long)sizeof(png_trailer)
     || png_buffer_size < png_len) {
        return 0;
    }

    /* Check whether we have enough room for the new chunk.  This check is
     * done in two steps to avoid integer overflow in pathological cases. */
    if (png_buffer_size - png_len < (long)sizeof(png_trailer)
     || png_buffer_size - (png_len + (long)sizeof(png_trailer)) < chunk_len) {
        return 0;
    }

    /* Check that the existing PNG data ends with an IEND chunk. */
    src_ptr = png_trailer;
    dest_ptr = (unsigned char *)png_data + (png_len - sizeof(png_trailer));
    for (i = 0; i < (long)sizeof(png_trailer); i++) {
        if (src_ptr[i] != dest_ptr[i]) {
            return 0;
        }
    }

    /* Generate the CRC lookup table. */
    generate_crc_table(crc_table);

    /* Write the new chunk over the existing IEND chunk. */
    store_32bit_value(dest_ptr, chunk_len);
    dest_ptr += 4;
    dest_chunk_ptr = dest_ptr;
    dest_ptr[0] = ((const char *)chunk_type)[0];
    dest_ptr[1] = ((const char *)chunk_type)[1];
    dest_ptr[2] = ((const char *)chunk_type)[2];
    dest_ptr[3] = ((const char *)chunk_type)[3];
    dest_ptr += 4;
    if (chunk_len > 0) {
        src_ptr = (const unsigned char *)chunk_data;
        for (i = 0; i < chunk_len; i++) {
            dest_ptr[i] = src_ptr[i];
        }
    }
    dest_ptr += chunk_len;
    store_32bit_value(dest_ptr, crc(dest_chunk_ptr, chunk_len+4, crc_table));
    dest_ptr += 4;

    /* Append a new IEND chunk. */
    src_ptr = png_trailer;
    for (i = 0; i < (long)sizeof(png_trailer); i++) {
        dest_ptr[i] = src_ptr[i];
    }
    dest_ptr += sizeof(png_trailer);

    /* Return the new PNG data length. */
    return dest_ptr - (unsigned char *)png_data;
}

/*************************************************************************/
/*************************************************************************/

/**
 * store_32bit_value:  Store a 32-bit value in a buffer, using PNG (big-
 * endian) byte order.
 *
 * Parameters:
 *     buffer: Pointer to the buffer.
 *      value: Value to store in the buffer.
 * Return value:
 *     None.
 * Preconditions:
 *     buffer != NULL
 *     buffer points to at least 4 bytes of writable memory.
 */
static void store_32bit_value(unsigned char *buffer, unsigned long value)
{
    buffer[0] = (value >> 24) & 0xFF;
    buffer[1] = (value >> 16) & 0xFF;
    buffer[2] = (value >>  8) & 0xFF;
    buffer[3] = (value >>  0) & 0xFF;
}

/*************************************************************************/

/**
 * generate_crc_table:  Generate the CRC table used for computing chunk CRC
 * values in PNG files.  Based on the sample code in ISO/IEC 15948:2003
 * appendix D.
 *
 * Parameters:
 *     crc_table: Pointer to the table into which the generated values are
 *                   stored.
 * Return value:
 *     None.
 * Preconditions:
 *     crc_table != NULL
 */
static void generate_crc_table(unsigned long crc_table[256])
{
    int byte;
    for (byte = 0; byte < 256; byte++) {
        unsigned long byte_crc = (unsigned long)byte;
        int bitnum;
        for (bitnum = 0; bitnum < 8; bitnum++) {
            const unsigned long bit = byte_crc & 1;
            byte_crc >>= 1;
            if (bit) {
                byte_crc ^= 0xEDB88320UL;
            }
        }
        crc_table[byte] = byte_crc;
    }
}

/*************************************************************************/

/**
 * crc:  Return the PNG-style CRC for a given data stream.
 *
 * Parameters:
 *          data: Pointer to the data stream.
 *          size: Number of bytes of data in the data stream.
 *     crc_table: Pointer to the CRC lookup table generated by
 *                   generate_crc_table().
 * Return value:
 *     The CRC of the data stream.
 * Preconditions:
 *     data != NULL
 *     size >= 0
 *     crc_table != NULL
 *     "data" points to at least "size" bytes of readable memory.
 */
static unsigned long crc(unsigned char *data, long size,
                         const unsigned long crc_table[256])
{
    unsigned long crc_inverted = 0xFFFFFFFFUL;
    long index;
    for (index = 0; index < size; index++) {
        crc_inverted = crc_table[(crc_inverted ^ data[index]) & 0xFF]
                       ^ (crc_inverted >> 8);
    }
    return crc_inverted ^ 0xFFFFFFFFUL;
}

/*************************************************************************/

/**
 * partial_adler32:  Return the running Adler32 CRC for a given part of a
 * data stream.
 *
 * Parameters:
 *     adler32: The running Adler32 CRC for the data stream.  Must be
 *                 initialized to 1 before the first call for a given data
 *                 stream.
 *        data: Pointer to the part of the data stream to process.
 *        size: Number of bytes of data to process.
 * Return value:
 *     The Adler32 CRC of the data stream through the data provided.
 * Preconditions:
 *     data != NULL
 *     size >= 0
 *     "data" points to at least "size" bytes of readable memory.
 */
static unsigned long partial_adler32(unsigned long adler32,
                                     unsigned char *data, long size)
{
    unsigned long s1 = adler32 & 0xFFFF;
    unsigned long s2 = (adler32 >> 16) & 0xFFFF;
    long index;
    for (index = 0; index < size; index++) {
        s1 = (s1 + data[index]) % 65521;
        s2 = (s2 + s1)          % 65521;
    }
    return s2<<16 | s1;
}

/*************************************************************************/

/* End of quickpng.c */
