/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/utility/strformat.c: String formatting routine (like snprintf()).
 */

#include "src/base.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/utility/strformat.h"
#include "src/utility/utf8.h"

/*
 * This source file defines the strformat() and vstrformat() functions.
 * These functions are for most purposes identical to snprintf() and
 * vsnprintf() respectively, but they do not require the stdio library,
 * which can reduce code bloat in environments with which stdio does not
 * mesh well (such as Windows with its multitude of runtime libraries).
 * These functions also understand UTF-8 characters, and in particular:
 *    - %c will convert to a multi-byte UTF-8 sequence for character
 *      values greater than 127.
 *    - If the string is truncated due to running out of buffer space, it
 *      will not be truncated in the middle of a multibyte UTF-8 character.
 *
 * As an additional feature, strformat() can use the "+" modifier on the
 * numeric tokens %d, %[eE], %[fF], %[gG], and %i to output characters in
 * a locale-specific style.  See the documentation of the
 * strformat_enable_fullwidth() function for details.
 *
 * Defining the preprocessor symbol SIL_STRFORMAT_USE_FLOATS will cause
 * the %[eE], %[fF], and %[gG] tokens to use single-precision rather than
 * double-precision arithmetic, for more efficient code on systems without
 * native support for double precision.
 *
 * ======== Details of supported format tokens ========
 *
 * The following format modifiers are supported:
 *
 *    "0": Use "0" characters rather than spaces to pad the field.  When
 *         used with a numeric argument, the sign or space prefix (for
 *         negative values or with the " " or "+" modifiers) will be
 *         output before the padding characters.  Must precede any other
 *         specifier except " " or "+"; may not be used with "-".
 *
 *    "-": Left-justify the field (the default is to right-justify).  Must
 *         precede any other specifier except " " or "+"; may not be used
 *         with "0".
 *
 *    " ": For %d, %[eE], %[fF], %[gG], and %i, prepend a space to
 *         nonnegative values.  Must precede any other specifier except
 *         "0", "-", or "+".
 *
 *    "+": For %d, %[eE], %[fF], %[gG], and %i, do one of the following:
 *            - If strformat_enable_fullwidth() has been called with a
 *              true argument, convert output characters to fullwidth
 *              ASCII (Unicode U+FFxx) when fullwidth output has been
 *              enabled with strformat_set_fullwidth(), and leave the
 *              output unchanged otherwise.
 *            - If strformat_enable_fullwidth() has not been called or
 *              has been called with a false argument, prepend a "+" to
 *              nonnegative values.
 *         Must precede any other specifier except "0", "-", or " "; if
 *         used with " " when strformat_enable_fullwidth() has not been
 *         enabled, this specifier takes precedence over " ".  Note that
 *         using this specifier with " " may trigger compiler warnings.
 *
 *    "width.prec": Specifies the width and precision of the field.  The
 *         "width" value gives the minimum number of characters to write;
 *         if the number of characters produced by the output of the
 *         format token is less than this value, the text will be padded
 *         until the specified number of characters have been written.
 *         The "precision" value has different meanings depending on the
 *         particular format.  Either value may be given as "*", which
 *         causes an "int" argument to be read from the argument stream
 *         and used as the respective value.
 *
 *    "h": For integer formats, indicates that the next argument is "short"
 *         rather than "int".  Ignored for other formats.  Must immediately
 *         precede the format specifier.
 *
 *    "hh": For integer formats, indicates that the next argument is "char"
 *          rather than "int".  Ignored for other formats.  Must
 *          immediately precede the format specifier.
 *
 *    "j": For integer formats, indicates that the next argument is
 *         "intmax_t" rather than "int".  Ignored for other formats.
 *         Must immediately precede the format specifier.
 *
 *    "l": (the lowercase letter L) For integer formats, indicates that
 *         the next argument is "long" rather than "int".  Ignored for
 *         other formats.  Must immediately precede the format specifier.
 *
 *    "ll": For integer formats, indicates that the next argument is
 *          "long long" rather than "int"; for floating-point formats,
 *          indicates that the next argument is "long double" rather than
 *          "double".  Ignored for other formats.  Must immediately precede
 *          the format specifier.
 *
 *    "L": Synonym for "ll".
 *
 *    "t": For integer formats, indicates that the next argument is
 *         "ptrdiff_t" rather than "int".  Ignored for other formats.
 *         Must immediately precede the format specifier.
 *
 *    "z": For integer formats, indicates that the next argument is
 *         "size_t" rather than "int".  Ignored for other formats.
 *         Must immediately precede the format specifier.
 *
 * The following base format specifiers are supported:
 *
 *    "%": Outputs a literal "%" character.  All modifiers are ignored.
 *
 *    "c": Reads an int argument and outputs the character with the
 *         argument's code.  If the value of the argument is greater
 *         than 127, the character is output as a UTF-8 sequence.  The
 *         precision modifier is ignored.
 *
 *    "d", "i": Reads a signed int, long, or long long argument, and
 *              prints the decimal representation of that argument.  The
 *              precision modifier specifies the minimum number of digits
 *              to print.
 *
 *    "e", "E": Reads a double argument, and prints the argument in
 *              exponential form ("M.MMMe+EE").  The precision modifier
 *              specifies the number of digits after the decimal point in
 *              the mantissa (default 6).  The exponent is always printed
 *              using at least two digits, inserting a leading zero if
 *              necessary.  Infinite values are  printed as "inf"; NaNs are
 *              printed as "nan".  %E causes all lowercase letters to be
 *              printed in uppercase ("...E+", "INF", "NAN").
 *
 *    "f", "F": Reads a double argument, and prints the argument in
 *              standard (non-exponential) form.  The precision modifier
 *              specifies the number of digits after the decimal point
 *              (default 6).  %F causes all lowercase letters to be printed
 *              in uppercase.
 *
 *    "g", "G": Reads a double argument, and prints with the number of
 *              significant digits specified by the precision (default 6).
 *              If the exponent of the number is less than -4 or greater
 *              than or equal to the precision, the number is printed in
 *              the "e" format; otherwise, it is printed in the "f" format.
 *              %G causes all lowercase letters to be printed in uppercase.
 *
 *    "o": Reads an unsigned int, long, or long long argument, and prints
 *         the octal representation of that argument.  The precision
 *         modifier specifies the minimum number of digits to print.
 *
 *    "p": Reads a pointer argument, and prints the pointer address using
 *         the format "0x%X".
 *
 *    "s": Reads a pointer argument, and prints the null-terminated
 *         string stored at the address pointed to by the argument.
 *         The precision modifier specifies the maximum number of bytes
 *         (_not_ characters) to print.  The string is parsed for UTF-8
 *         validity, and invalid characters (including any UTF-8 sequence
 *         which crosses the precision limit) are skipped.
 *
 *    "u": Reads an unsigned int, long, or long long argument, and prints
 *         the decimal representation of that argument.  The precision
 *         modifier specifies the minimum number of digits to print.
 *
 *    "x", "X": Reads an unsigned int, long, or long long argument, and
 *              prints the hexadecimal representation of that argument.
 *              Digits greater than 9 are printed with lowercase letters
 *              for "%x" and uppercase letters for "%X".  The precision
 *              modifier specifies the minimum number of digits to print.
 *
 * ======== Differences from C99 snprintf() ========
 *
 * The following modifiers and format specifiers defined by C99 are _not_
 * supported by this function:
 *
 *    Modifiers: "#" (However, this is accepted and ignored.)
 *    Format specifiers: "a", "A", "n"
 *
 * Floating-point arguments of "long double" type (the "ll"/"L" modifier)
 * are truncated to "double", or to "float" if SIL_STRFORMAT_USE_FLOATS is
 * defined, before formatting.  This may cause over- or underflow of the
 * value, resulting in "inf" or "0" for values of very large (but finite)
 * or small magnitude.
 *
 * The "%c" specifier always behaves as though it was preceded by an "l"
 * modifier and operating in a UTF-8 locale.  (Adding an explicit "l"
 * modifier is allowed but has no effect.)
 *
 * The "l" modifier on "%s" is ignored; all strings are treated as UTF-8.
 *
 * The "n$" argument index modifier, which is not part of C99 but is
 * specified by POSIX (for example, "%2$d" to print the second format
 * argument as a decimal number), is not supported or accepted; the "$"
 * will be treated as an invalid format specifier and cause the token to
 * be appended literally to the output string.  (Note that, because POSIX
 * does not allow mixing of explicit and implicit argument indices, a
 * well-formed POSIX format string using explicit indices will not cause
 * any variadic arguments to be misinterpreted, because all format tokens
 * which would consume an argument will contain a "$" and will thus be
 * treated as literal text instead of format tokens.)
 *
 * ======== Limitations ========
 *
 * The following limitations are present in this implementation:
 *
 *    - Width and precision values larger than 10000 are silently reduced
 *      to 10000.
 *
 *    - Numeric values are truncated at approximately 100 bytes of output
 *      (not including field-width padding).
 */

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* True to use the "+" modifier for optional fullwidth character output. */
static uint8_t plus_as_fullwidth = 0;

/* True to use fullwidth characters for %+[deEfFgGi]. */
static uint8_t fullwidth_enabled = 0;

/*************************************************************************/
/**************************** Helper routines ****************************/
/*************************************************************************/

/**
 * append:  Append a string to the output buffer.  The string is assumed
 * to be valid UTF-8.
 *
 * [Parameters]
 *     buf: Output buffer.
 *     size: Size of output buffer, in bytes.
 *     total_ptr: Pointer to variable holding the total number of bytes
 *         written.
 *     full_ptr: Pointer to flag indicating whether the buffer is full.
 *     str: String to append.
 *     len: Length of string to append, in bytes.
 */
static inline void append(char *buf, int size, int *total_ptr, int *full_ptr,
                          const char *str, int len)
{
    static const unsigned char utf8_bytes[256] = {
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
        2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
        3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,6,6,6,6,
    };

    const unsigned char *s = (const unsigned char *)(str);
    const unsigned char *top = s + (len);
    while (s < top) {
        if (*full_ptr) {
            break;
        }
        const int bytes = utf8_bytes[*s];
        if (*total_ptr + bytes >= size) {
            *full_ptr = 1;
            /* Don't try to write to a buffer of size zero! */
            if (LIKELY(size > 0)) {
                buf[*total_ptr] = '\0';
            }
            break;
        }
        for (int i = 0; i < bytes; i++) {
            buf[(*total_ptr)+i] = s[i];
        }
        *total_ptr += bytes;
        s += bytes;
    }
    *total_ptr += top - s;
}

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int strformat(char *buf, int size, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    const int res = vstrformat(buf, size, format, args);
    va_end(args);
    return res;
}

/*-----------------------------------------------------------------------*/

int vstrformat(char *buf, int size, const char *format, va_list args)
{
    int total = 0;  // Total number of bytes in the result string.
    int full = 0;   // Is the output buffer full?


    /* First check parameters. */
    if (format == NULL) {
        DLOG("format == NULL");
        return 0;
    }
    if (size < 0) {
        DLOG("size (%d) < 0", size);
        return 0;
    }
    if (buf == NULL) {
        size = 0;  // Make sure we don't try to write into the buffer.
    }

    while (*format) {

        if (*format != '%') {
            /* It's not a format token, so write the character directly. */
            const int charlen = utf8_charlen(format);
            if (charlen > 0) {
                append(buf, size, &total, &full, format, charlen);
                format += charlen;
            } else {
                format++;
            }
            continue;
        }

        format++;

        const char * const start = format; // Beginning of token (for restart).
        int left_justify = 0;   // Left-justification ("%-") flag.
        const char *pad = " ";  // Padding character (may be a UTF-8 sequence).
        int fullwidth_pad = 0;  // Flag for using fullwidth ASCII for padding.
        int plus_flag = 0;      // Flag for "+" modifier.
        int space_flag = 0;     // Flag for " " modifier.
        int width = -1;         // Field width (-1 = unspecified).
        int prec = -1;          // Precision (-1 = unspecified).
        enum {HH,H,M,L,LL,SIZE,PTRDIFF,INTMAX}
            dsize = M;          // Argument data size (M = default).
        char type = 0;          // Base format specifier (0 = not yet found).
        char tmpbuf[128];       // Temporary buffer for formatting numbers.
        const char *prefix=NULL;// String (sign/base) to print before zero-pad.
        const char *data = NULL;// Unpadded string data for this argument.
        int datalen = -1;       // Length of "data" string in bytes.

        /* (1) Parse the format token. */

        while (!type) {
            const char c = *format++;
            switch (c) {

              /* (1.1) Check for end-of-string in the middle of the token. */

              case '\0':
                /* We hit the end of the string before the end of the token,
                 * so treat the token as invalid and write it out directly.
                 * We also jump here if any other parsing error occurs. */
              invalid_format:
                append(buf, size, &total, &full, "%", 1);
                format = start;
                goto restart;  // "continue" the outermost loop

              /* (1.2) Check for flags like "-" and "0", which are only
               *       accepted before any field width, precision, or data
               *       size.  We also detect "#" and skip over it without
               *       error (though we don't do anything for it). */

              case '-':
                if (width >= 0 || prec >= 0 || dsize != M) {
                    goto invalid_format;
                }
                left_justify = 1;
                pad = " ";  // Override "0" (C99 7.19.6.6).
                break;

              case ' ':
                if (width >= 0 || prec >= 0 || dsize != M) {
                    goto invalid_format;
                }
                space_flag = 1;
                break;

              case '+':
                if (width >= 0 || prec >= 0 || dsize != M) {
                    goto invalid_format;
                }
                plus_flag = 1;
                break;

              case '#':
                if (width >= 0 || prec >= 0 || dsize != M) {
                    goto invalid_format;
                }
                break;

              case '0':
                if (width < 0 && prec < 0 && dsize == M) {
                    if (!left_justify) {
                        pad = "0";
                    }
                    break;
                }
                /* A "0" could also be part of a field width or precision
                 * value, so fall through to width/precision handling. */
                FALLTHROUGH;

              /* (1.3) Parse field width and precision values. */

              case '1':
              case '2':
              case '3':
              case '4':
              case '5':
              case '6':
              case '7':
              case '8':
              case '9':
                if (dsize != M) {
                    /* We've already seen a data size specifier, so this
                     * character is invalid. */
                    goto invalid_format;
                }
                if (prec >= 0) {
                    prec = (prec*10) + (c - '0');
                    if (prec > 10000) {  // Treat this as overflow.
                        prec = 10000;
                    }
                } else {
                    if (width < 0) {  // I.e., this is the first digit.
                        width = 0;
                    }
                    width = (width*10) + (c - '0');
                    if (width > 10000) {
                        width = 10000;
                    }
                }
                break;

              case '.':
                if (dsize != M) {
                    goto invalid_format;
                }
                if (prec >= 0) {
                    /* Only one "." is allowed. */
                    goto invalid_format;
                }
                prec = 0;  // Start reading the precision.
                break;

              case '*': {
                int val;
                if (dsize != M) {
                    goto invalid_format;
                }
                if (prec > 0 || (prec < 0 && width >= 0)) {
                    /* Not allowed in the middle of a number. */
                    goto invalid_format;
                }
                val = va_arg(args, int);
                if (prec == 0) {
                    prec = bound(val, 0, 10000);
                } else {
                    if (val < 0) {
                        left_justify = 1;
                        val = -val;
                    }
                    width = ubound(val, 10000);
                }
                break;
              }  // case '*'

              /* (1.4) Check for data size modifiers. */

              case 'h':
                if (dsize == M) {
                    dsize = H;
                } else if (dsize == H) {
                    dsize = HH;
                } else {
                    goto invalid_format;
                }
                break;

              case 'j':
                if (dsize == M) {
                    dsize = INTMAX;
                } else {
                    goto invalid_format;
                }
                break;

              case 'l':
                if (dsize == M) {
                    dsize = L;
                } else if (dsize == L) {
                    dsize = LL;
                } else {
                    goto invalid_format;
                }
                break;

              case 'L':
                if (dsize == M) {
                    /* C99 says that behavior of e.g. "%llf" and "%Ld" are
                     * undefined, so we just make "L" and "ll" synonyms. */
                    dsize = LL;
                } else {
                    goto invalid_format;
                }
                break;

              case 't':
                if (dsize == M) {
                    dsize = PTRDIFF;
                } else {
                    goto invalid_format;
                }
                break;

              case 'z':
                if (dsize == M) {
                    dsize = SIZE;
                } else {
                    goto invalid_format;
                }
                break;

              /* (1.5) Anything else must be a format specifier. */

              default:
                type = c;
                break;

            }  // switch (*format++)
        }  // while (!type)

        /* (2) Read and process argument data based on the format token. */

        switch (type) {

          default:
            DLOG("Invalid format character %c", type);
            goto invalid_format;

          case '%':
            data = "%";
            datalen = 1;
            break;

          case 'c': {
            unsigned int val = va_arg(args, unsigned int);
            if (val < 0x80) {
                tmpbuf[0] = val;
                datalen = 1;
            } else if (val < 0x800) {
                tmpbuf[0] = 0xC0 | ((val>>6) & 0x1F);
                tmpbuf[1] = 0x80 | ((val>>0) & 0x3F);
                datalen = 2;
            } else if (val < 0x10000) {
                tmpbuf[0] = 0xE0 | ((val>>12) & 0x0F);
                tmpbuf[1] = 0x80 | ((val>> 6) & 0x3F);
                tmpbuf[2] = 0x80 | ((val>> 0) & 0x3F);
                datalen = 3;
            } else if (val < 0x200000) {
                tmpbuf[0] = 0xF0 | ((val>>18) & 0x07);
                tmpbuf[1] = 0x80 | ((val>>12) & 0x3F);
                tmpbuf[2] = 0x80 | ((val>> 6) & 0x3F);
                tmpbuf[3] = 0x80 | ((val>> 0) & 0x3F);
                datalen = 4;
            } else if (val < 0x4000000) {
                tmpbuf[0] = 0xF8 | ((val>>24) & 0x03);
                tmpbuf[1] = 0x80 | ((val>>18) & 0x3F);
                tmpbuf[2] = 0x80 | ((val>>12) & 0x3F);
                tmpbuf[3] = 0x80 | ((val>> 6) & 0x3F);
                tmpbuf[4] = 0x80 | ((val>> 0) & 0x3F);
                datalen = 5;
            } else {
                tmpbuf[0] = 0xFC | ((val>>30) & 0x03);
                tmpbuf[1] = 0x80 | ((val>>24) & 0x3F);
                tmpbuf[2] = 0x80 | ((val>>18) & 0x3F);
                tmpbuf[3] = 0x80 | ((val>>12) & 0x3F);
                tmpbuf[4] = 0x80 | ((val>> 6) & 0x3F);
                tmpbuf[5] = 0x80 | ((val>> 0) & 0x3F);
                datalen = 6;
            }
            data = tmpbuf;
            break;
          }  // case 'c'

          case 's': {
            data = va_arg(args, const char *);
            if (!data) {
                data = "(null)";
            }
            datalen = strlen(data);
            if (prec >= 0) {
                datalen = ubound(datalen, prec);
            }
            break;
          }  // case 's'

          case 'd':
          case 'i':
          case 'u': {

            const int use_fullwidth =
                plus_flag && plus_as_fullwidth && fullwidth_enabled;
            int pos;

            /* Change the padding character to fullwidth if requested. */

            if (use_fullwidth) {
                fullwidth_pad = 1;
            }

            /* Convert the value to text.  Processing values larger than
             * "long" may be slow, so handle those separately from int-sized
             * or long-sized values. */

            if (dsize == LL  // Assume sizeof(long long) > sizeof(long).
#if SIZE_MAX > ULONG_MAX
             || dsize == SIZE
#endif
#if PTRDIFF_MAX > LONG_MAX
             || dsize == PTRDIFF
#endif
             || dsize == INTMAX) {  // sizeof(intmax_t) >= sizeof(long long)

                uintmax_t val;
                int isneg;
                if (type == 'u') {
#if SIZE_MAX > ULONG_MAX
                    if (dsize == SIZE) {
                        val = va_arg(args, size_t);
                    } else
#endif
#if PTRDIFF_MAX > LONG_MAX
                    if (dsize == PTRDIFF) {
                        val = (uintmax_t)va_arg(args, ptrdiff_t);
                    } else
#endif
                    if (dsize == INTMAX) {
                        val = va_arg(args, uintmax_t);
                    } else {  // dsize == LL
                        val = va_arg(args, unsigned long long);
                    }
                    isneg = 0;
                } else {
                    intmax_t sval;
#if SIZE_MAX > ULONG_MAX
                    if (dsize == SIZE) {
                        /* It would have been nice if C99 included ssize_t
                         * in addition to size_t.  Oh well, do what we can.
                         * This (and similar code below) assumes two's-
                         * complement representation of negative values
                         * and wraparound on overflow of signed integers. */
                        val = va_arg(args, size_t);
                        sval = (intmax_t)val;
# if SIZE_MAX < UINTMAX_MAX
                        if (val >= (size_t)1 << (sizeof(size_t)*8-1)) {
                            sval -= (intmax_t)1 << (sizeof(size_t)*8);
                        }
# endif
                    } else
#endif
#if PTRDIFF_MAX > LONG_MAX
                    if (dsize == PTRDIFF) {
                        sval = va_arg(args, ptrdiff_t);
                    } else
#endif
                    if (dsize == INTMAX) {
                        sval = va_arg(args, intmax_t);
                    } else {  // dsize == LL
                        sval = va_arg(args, long long);
                    }
                    if (sval >= 0) {
                        isneg = 0;
                        val = sval;
                    } else {
                        isneg = 1;
                        val = -sval;
                    }
                }
                pos = sizeof(tmpbuf);
                do {
                    const int digit = val % 10;
                    val /= 10;
                    ASSERT(pos >= 1, pos = 1);
                    tmpbuf[--pos] = '0' + digit;
                } while (val != 0);
                if (isneg) {
                    prefix = "-";
                } else if (type != 'u' && plus_flag && !plus_as_fullwidth) {
                    prefix = "+";
                } else if (type != 'u' && space_flag) {
                    prefix = " ";
                }

            } else {  // sizeof(type) <= sizeof(long)

                unsigned long val;
                int isneg;
                if (type == 'u') {
#if !(SIZE_MAX > ULONG_MAX)
                    if (dsize == SIZE) {
                        val = va_arg(args, size_t);
                    } else
#endif
#if !(PTRDIFF_MAX > LONG_MAX)
                    if (dsize == PTRDIFF) {
                        val = (unsigned long)va_arg(args, ptrdiff_t);
                    } else
#endif
                    if (dsize == L) {
                        val = va_arg(args, unsigned long);
                    } else {  // dsize <= M
                        val = va_arg(args, unsigned int);
                        if (dsize == H) {
                            val = (unsigned short)val;
                        } else if (dsize == HH) {
                            val = (unsigned char)val;
                        }
                    }
                    isneg = 0;
                } else {
                    long sval;
#if !(SIZE_MAX > ULONG_MAX)
                    if (dsize == SIZE) {
                        val = va_arg(args, size_t);
                        sval = (long)val;
# if SIZE_MAX < ULONG_MAX
                        if (val >= (size_t)1 << (sizeof(size_t)*8-1)) {
                            sval -= 1L << (sizeof(size_t)*8);
                        }
# endif
                    } else
#endif
#if !(PTRDIFF_MAX > LONG_MAX)
                    if (dsize == PTRDIFF) {
                        sval = va_arg(args, ptrdiff_t);
                    } else
#endif
                    if (dsize == L) {
                        sval = va_arg(args, long);
                    } else {
                        sval = va_arg(args, int);
                        if (dsize == H) {
                            sval = (short)sval;
                        } else if (dsize == HH) {
                            sval = (signed char)sval;
                        }
                    }
                    if (sval >= 0) {
                        isneg = 0;
                        val = sval;
                    } else {
                        isneg = 1;
                        val = -sval;
                    }
                }
                pos = sizeof(tmpbuf);
                do {
                    const unsigned long newval = val / 10;
                    const int digit = val - newval*10;
                    ASSERT(pos >= 1, pos = 1);
                    tmpbuf[--pos] = '0' + digit;
                    val = newval;
                } while (val != 0);
                if (isneg) {
                    prefix = "-";
                } else if (type != 'u' && plus_flag && !plus_as_fullwidth) {
                    prefix = "+";
                } else if (type != 'u' && space_flag) {
                    prefix = " ";
                }

            }  // if (dsize == LL)

            data = &tmpbuf[pos];
            datalen = sizeof(tmpbuf) - pos;

            /* Convert the formatted string to fullwidth if requested. */

            if (use_fullwidth) {
                /* Hard assertion because this should never happen (tmpbuf
                 * is large enough to hold even 128-bit integers). */
                ASSERT(datalen <= (int)(sizeof(tmpbuf)/3),
                       datalen = (int)(sizeof(tmpbuf)/3));
                unsigned int inpos = sizeof(tmpbuf) - datalen;
                unsigned int outpos = 0;
                while (inpos < sizeof(tmpbuf)) {
                    const unsigned char c = tmpbuf[inpos++] - 0x20;
                    tmpbuf[outpos++] = 0xEF;
                    tmpbuf[outpos++] = 0xBC | (c >> 6);
                    tmpbuf[outpos++] = 0x80 | (c & 0x3F);
                }
                data = tmpbuf;
                datalen = outpos;
                if (prefix) {
                    if (*prefix == '-') {
                        prefix = "－";
                    } else {  // Must have been " ".
                        prefix = "　";
                    }
                }
            }

            break;

          }  // case 'd', 'i', 'u'

          case 'o':
          case 'p':
          case 'x':
          case 'X': {

            const unsigned int shift = (type=='o' ? 3 : 4);
            const char * const digits = (type=='x' ? "0123456789abcdef"
                                                   : "0123456789ABCDEF");
            int pos = sizeof(tmpbuf);

            /* Since non-decimal bases are likely to be much less common
             * than decimal ones, we don't bother optimizing for integer
             * size here. */
            uintmax_t val;

            if (type == 'p') {
                val = (uintptr_t)va_arg(args, const void *);
                if (val == 0) {
                    /* For %p, if the pointer is NULL, print "(null)"
                     * instead of a numeric zero value. */
                    data = "(null)";
                    datalen = strlen(data);
                    break;
                }
            } else if (dsize == INTMAX) {
                val = va_arg(args, uintmax_t);
            } else if (dsize == SIZE) {
                val = va_arg(args, size_t);
            } else if (dsize == PTRDIFF) {
                val = (uintmax_t)va_arg(args, ptrdiff_t);
            } else if (dsize == LL) {
                val = va_arg(args, unsigned long long);
            } else if (dsize == L) {
                val = va_arg(args, unsigned long);
            } else {
                val = va_arg(args, unsigned int);
                if (dsize == H) {
                    val = (unsigned short)val;
                } else if (dsize == HH) {
                    val = (unsigned char)val;
                }
            }
            do {
                const unsigned int digit = val & ((1<<shift) - 1);
                val >>= shift;
                ASSERT(pos >= 1, pos = 1);
                tmpbuf[--pos] = digits[digit];
            } while (val != 0);

            if (type == 'p') {
                ASSERT(pos >= 2, pos = 2);
                tmpbuf[--pos] = 'x';
                tmpbuf[--pos] = '0';
            }

            data = &tmpbuf[pos];
            datalen = sizeof(tmpbuf) - pos;
            break;

          }  // case 'o', 'p', 'x', 'X'

          case 'e':
          case 'E':
          case 'f':
          case 'F':
          case 'g':
          case 'G': {

            const int use_fullwidth =
                plus_flag && plus_as_fullwidth && fullwidth_enabled;
            if (prec < 0) {
                prec = 6;  // Default precision.
            } else if (prec == 0 && type == 'g') {
                prec = 1;  // Can't have 0 significant digits!
            }

            int uppercase = 0;
            if (type == 'E' || type == 'F' || type == 'G') {
                uppercase = 1;
                type += 'a' - 'A';
            }

            /* Change the padding character to fullwidth if requested. */

            if (use_fullwidth) {
                if (*pad == '0') {
                    pad = "０";
                } else {
                    pad = "　";
                }
            }

#ifdef SIL_STRFORMAT_USE_FLOATS
# define DOUBLE_FLOAT float
# define FABS_DF fabsf
# define FLOOR_DF floorf
# define IFLOOR_DF ifloorf
# define FMOD_DF fmodf
# define LOG10_DF log10f
# define POW_DF powf
#else
# define DOUBLE_FLOAT double
# define FABS_DF fabs
# define FLOOR_DF floor
# define IFLOOR_DF ifloor
# define FMOD_DF fmod
# define LOG10_DF log10
# define POW_DF pow
#endif

            /* Check for NaNs and infinities first, and handle them
             * specially. */

            DOUBLE_FLOAT val;
            if (dsize == LL) {
                val = (DOUBLE_FLOAT) va_arg(args, long double);
            } else {
                val = va_arg(args, double);
            }
            if (isnan(val)) {
                data = (uppercase
                        ? (use_fullwidth ? "ＮＡＮ" : "NAN")
                        : (use_fullwidth ? "ｎａｎ" : "nan"));
                datalen = strlen(data);
                break;
            } else if (isinf(val)) {
                if (val < 0) {
                    data = (uppercase
                            ? (use_fullwidth ? "－ＩＮＦ" : "-INF")
                            : (use_fullwidth ? "－ｉｎｆ" : "-inf"));
                    datalen = strlen(data);
                } else {
                    data = (uppercase
                            ? (use_fullwidth ? "ＩＮＦ" : "INF")
                            : (use_fullwidth ? "ｉｎｆ" : "inf"));
                    datalen = strlen(data);
                }
                break;
            }

            /* Save the sign and convert to absolute value for simplicity
             * of processing. */

            int isneg = (val < 0);
            val = FABS_DF(val);

            /* Round off the last digit.  (We skip the entire rounding block
             * if the value is zero to avoid the more complex logic required
             * to handle zero properly.) */

            int prec_adjust = 0;
            if (val != 0) {
                if (type == 'e') {
                    prec_adjust = IFLOOR_DF(LOG10_DF(val));
                } else if (type == 'g') {
                    prec_adjust = IFLOOR_DF(LOG10_DF(val)) + 1;
                }
                DOUBLE_FLOAT round_temp = 0.5;
                round_temp *= POW_DF(10, -(prec - prec_adjust));
                val += round_temp;
                /* We may have rounded up to another digit (e.g. 0.95 -> 1.0)
                 * so recalculate the precision adjustment for %g. */
                if (type == 'g') {
                    prec_adjust = IFLOOR_DF(LOG10_DF(val)) + 1;
                }
            }

            /* Choose which output format (exponential or standard) to use,
             * and for "%g", adjust the precision to the number of decimal
             * places to output. */

#ifdef SIL_STRFORMAT_USE_FLOATS
            const float g_std_min = 1.0e-4f;
#else
            const double g_std_min = 1.0e-4;
#endif
            const int exp_format = (
                type == 'e'
                || (type == 'g' && ((val > 0 && val < g_std_min)
                                    || prec - prec_adjust < 0)));
            if (type == 'g') {
                if (exp_format) {
                    prec--;  // Always 1 digit before the decimal point.
                } else {
                    prec -= prec_adjust;
                }
            }

            /* If printing in exponential format, calculate the exponent
             * and shift the value so it is in the range [1.0,10.0). */

            int exponent = 0;
            if (exp_format && val > 0) {
                exponent = IFLOOR_DF(LOG10_DF(val));
                val *= POW_DF(10, -exponent);
            }

            /* Convert the value. */

            DOUBLE_FLOAT val_trunc = FLOOR_DF(val);
            DOUBLE_FLOAT val_frac  = val - val_trunc;
            int pos = sizeof(tmpbuf);
            if (exp_format) {
                pos -= (abs(exponent) >= 100 ? 5 : 4);
            }
            tmpbuf[--pos] = 0;
            do {
                const int digit = IFLOOR_DF(FMOD_DF(val_trunc, 10));
                tmpbuf[--pos] = '0' + digit;
                val_trunc = FLOOR_DF(val_trunc / 10);
            } while (val_trunc != 0 && pos >= 1);
            if (pos > 0) {
                memmove(tmpbuf, &tmpbuf[pos], strlen(&tmpbuf[pos])+1);
            }
            pos = strlen(tmpbuf);
            const int limit = sizeof(tmpbuf);
            /* Leave space for "e+NNN" if necessary. */
            const int val_limit = limit - (exp_format ? 5 : 0);
            int have_decimal = 0;
            if (prec > 0) {
                have_decimal = 1;
                ASSERT(pos < val_limit, pos = val_limit-1);
                tmpbuf[pos++] = '.';
                while (prec > 0 && pos < val_limit) {
                    val_frac *= 10;
                    const int digit = IFLOOR_DF(val_frac);
                    tmpbuf[pos++] = '0' + digit;
                    val_frac -= digit;
                    prec--;
                }
            }

            /* For %g, delete all trailing zeros in the fractional part.
             * If we delete the entire fractional part, also delete the
             * leftover decimal point. */
            if (type == 'g' && have_decimal) {
                ASSERT(pos >= 1, pos = 1);  // '.', at least, is present.
                while (tmpbuf[pos-1] == '0') {
                    pos--;
                    ASSERT(pos >= 1, pos = 1; break);
                }
                if (tmpbuf[pos-1] == '.') {
                    pos--;
                }
            }

            if (exp_format) {
                tmpbuf[pos++] = uppercase ? 'E' : 'e';
                tmpbuf[pos++] = (exponent < 0) ? '-' : '+';
                if (abs(exponent) >= 100) {
                    tmpbuf[pos++] = '0' + (abs(exponent)/100 % 10);
                }
                tmpbuf[pos++] = '0' + (abs(exponent)/10 % 10);
                tmpbuf[pos++] = '0' + (abs(exponent) % 10);
                ASSERT(pos <= limit, pos = limit);
            }

            data = tmpbuf;
            datalen = pos;

            if (isneg) {
                prefix = "-";
            } else if (plus_flag && !plus_as_fullwidth) {
                prefix = "+";
            } else if (space_flag) {
                prefix = " ";
            }

            /* Check for fullwidth characters. */

            if (use_fullwidth) {
                /* Convert the formatted string to fullwidth. */
                if (datalen > (int)(sizeof(tmpbuf)/3)) {
                    datalen = sizeof(tmpbuf)/3;
                }
                int inpos = datalen;
                int outpos = datalen * 3;
                datalen = outpos;
                while (inpos > 0) {
                    const unsigned char c = tmpbuf[--inpos] - 0x20;
                    tmpbuf[--outpos] = 0x80 | (c & 0x3F);
                    tmpbuf[--outpos] = 0xBC | (c >> 6);
                    tmpbuf[--outpos] = 0xEF;
                }
                if (prefix) {
                    if (*prefix == '-') {
                        prefix = "－";
                    } else {
                        prefix = "　";  // Must have been " ".
                    }
                }
            }

            break;

          }  // case 'e', 'E', 'f', 'F', 'g', 'G'

        }  // switch (type)

        /* (3) Copy the formatted text into the output buffer, applying
         *     padding as appropriate. */

        /* (3.1) Sanity-check the data pointer and length. */

        ASSERT(data != NULL, data = "(ERROR)"; datalen = strlen(data));
        ASSERT(datalen >= 0, data = "(ERROR)"; datalen = strlen(data));

        /* (3.2) Count the number of UTF-8 characters in the string and
         *       any prefix. */

        const char *s = data;
        int slen = datalen;
        int datachars = 0;
        while (slen > 0) {
            const int charlen = utf8_charlen(s);
            if (charlen > 0) {
                if (charlen <= datalen) {
                    datachars++;
                    s += charlen;
                    slen -= charlen;
                } else {
                    slen = 0;
                }
            } else {
                s++;
                slen--;
            }
        }

        int prefixchars = 0;
        if (prefix) {
            s = prefix;
            slen = strlen(s);
            while (slen > 0) {
                const int charlen = utf8_charlen(s);
                /* prefix is only set to known strings, so we should never
                 * have invalid characters. */
                ASSERT(charlen > 0, break);
                ASSERT(charlen <= slen, break);
                prefixchars++;
                s += charlen;
                slen -= charlen;
            }
        }

        /* (3.3) Write out the string and any padding necessary. */

        if ((strcmp(pad, "0") == 0 || strcmp(pad, "０") == 0) && prefix) {
            append(buf, size, &total, &full, prefix, strlen(prefix));
            prefix = NULL;
        }

        if (fullwidth_pad) {
            if (*pad == '0') {
                pad = "０";
            } else {
                pad = "　";
            }
        }
        width -= prefixchars + datachars;
        const int padlen = strlen(pad);
        if (!left_justify) {
            for (; width > 0; width--) {
                append(buf, size, &total, &full, pad, padlen);
            }
        }

        if (prefix) {
            append(buf, size, &total, &full, prefix, strlen(prefix));
            prefix = NULL;
        }

        while (datalen > 0) {
            const int charlen = utf8_charlen(data);
            if (charlen > 0) {
                if (charlen <= datalen) {
                    append(buf, size, &total, &full, data, charlen);
                    data += charlen;
                    datalen -= charlen;
                } else {
                    datalen = 0;
                }
            } else {
                data++;
                datalen--;
            }
        }

        if (left_justify) {
            for (; width > 0; width--) {
                append(buf, size, &total, &full, pad, padlen);
            }
        }

      restart:;
    }  // while (*format)

    /* Add a null byte to terminate the output string, unless we already
     * filled up the buffer (in which case a null byte was added at that
     * time). */

    if (size > 0 && !full) {
        ASSERT(total < size, total = size-1);
        buf[total] = '\0';
    }

    /* Return the total (untruncated) output string length. */

    return total;
}

/*-----------------------------------------------------------------------*/

int strformat_check(char *buf, int size, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    const int res = vstrformat_check(buf, size, format, args);
    va_end(args);
    return res;
}

/*-----------------------------------------------------------------------*/

int vstrformat_check(char *buf, int size, const char *format, va_list args)
{
    const int res = vstrformat(buf, size, format, args);
    ASSERT(res >= 0, return 0);  // We never return a negative value.
    return res < size;
}

/*-----------------------------------------------------------------------*/

int strformat_append(char **bufptr, int *lenptr, int mem_flags,
                     const char *format, ...)
{
    va_list args;
    va_start(args, format);
    const int res = vstrformat_append(bufptr, lenptr, mem_flags, format, args);
    va_end(args);
    return res;
}

/*-----------------------------------------------------------------------*/

int vstrformat_append(char **bufptr, int *lenptr, int mem_flags,
                      const char *format, va_list args)
{
    if (UNLIKELY(!bufptr)) {
        DLOG("bufptr == NULL");
        return 0;
    }
    if (UNLIKELY(!lenptr)) {
        DLOG("lenptr == NULL");
        return 0;
    }
    if (UNLIKELY(!format)) {
        DLOG("format == NULL");
        return 0;
    }

    va_list args_copy;
    va_copy(args_copy, args);
    const int append_len = vstrformat(NULL, 0, format, args_copy);
    va_end(args_copy);

    const int newlen = *lenptr + append_len;
    char *newbuf = mem_realloc(*bufptr, newlen+1, mem_flags);
    if (UNLIKELY(!newbuf)) {
        DLOG("Failed to expand string buffer to %d bytes", newlen+1);
        return 0;
    }

    ASSERT(vstrformat(newbuf+(*lenptr), append_len+1, format, args)
           == append_len);

    *bufptr = newbuf;
    *lenptr = newlen;
    return 1;
}

/*-----------------------------------------------------------------------*/

char *strformat_alloc(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    char *res = vstrformat_alloc(format, args);
    va_end(args);
    return res;
}

/*-----------------------------------------------------------------------*/

char *vstrformat_alloc(const char *format, va_list args)
{
    char *buf = NULL;
    int len = 0;
    if (!vstrformat_append(&buf, &len, 0, format, args)) {
        return NULL;
    }
    return buf;
}

/*-----------------------------------------------------------------------*/

void strformat_enable_fullwidth(int enable)
{
    plus_as_fullwidth = (enable != 0);
}

/*-----------------------------------------------------------------------*/

void strformat_set_fullwidth(int fullwidth)
{
    fullwidth_enabled = (fullwidth != 0);
}

/*************************************************************************/
/*************************************************************************/
