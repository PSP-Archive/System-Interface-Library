/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/resource/package-pkg.h: Header defining the custom PKG format for
 * package files.
 */

#ifndef SIL_SRC_RESOURCE_PACKAGE_PKG_H
#define SIL_SRC_RESOURCE_PACKAGE_PKG_H

/*
 * The PKG ("PacKaGe") file format is a simple file format for wrapping up
 * multiple data files into a single package.  This file defines the data
 * format and hash function used by PKG files, and also declares utility
 * functions to generate package module instances for reading PKG files.
 *
 * The basic format of a PKG file is as follows:
 *   - Package header (struct PKGHeader).
 *   - File index (struct PKGIndexEntry * 1 for each file in the package),
 *     sorted by pathname hash, then by lowercased (y/A-Z/a-z/) pathname
 *     for files with the same pathname hash.
 *   - File pathname buffer, containing the pathname of each file (in
 *     arbitrary order) as a null-terminated string.
 *   - File data.  The start of each file may be freely adjusted to any
 *     desired alignment as long as the file's index entry points to the
 *     correct offset.
 *
 * All numeric values in the package header and file index are stored in
 * big-endian format; for example, the 32-bit value 0x12345678 would be
 * stored as the four bytes 0x12 0x34 0x56 0x78.  The utility macros
 * PKG_HEADER_SWAP_BYTES() and PKG_INDEX_SWAP_BYTES() can be used to
 * change the byte order of a PKGHeader or PKGIndexEntry structure,
 * respectively, between big-endian and native order.
 *
 * Note that PKG files use 32-bit offset and size values, so PKG files
 * cannot grow beyond approximately 4GB in size.
 *
 * To use a PKG file with the SIL resource management functionality,
 * create a package module instance with pkg_create_module() and register
 * the instance using resource_register_package().  The module instance
 * should be destroyed with pkg_destroy_module() after being unregistered.
 */

EXTERN_C_BEGIN

struct PackageModuleInfo;

/*************************************************************************/
/******************** File format and related macros *********************/
/*************************************************************************/

/* File header structure. */

typedef struct PKGHeader PKGHeader;
struct PKGHeader {
    char magic[4];        // Must be PKG_MAGIC.
    uint16_t header_size; // Size of this header (sizeof PKGHeader).
    uint16_t entry_size;  // Size of a file index entry (sizeof PKGIndexEntry).
    uint32_t entry_count; // Number of file index entries (== number of files).
    uint32_t name_size;   // Size of the pathname data buffer.
};

#define PKG_MAGIC  "PKG\012"


/* File index entry structure. */

typedef struct PKGIndexEntry PKGIndexEntry;
struct PKGIndexEntry {
    uint32_t hash;
    uint32_t nameofs_flags; // Lower 24 bits: Offset within pathname buffer
                            //    (in bytes) of this file's pathname.
                            // Upper 8 bits: Flags (PKGF_*, defined below).
    uint32_t offset;    // Offset within PKG file (in bytes) of file's data.
    uint32_t datalen;   // Length (in bytes) of file's data, as stored.
    uint32_t filesize;  // Size (in bytes) of file's data, after decompression.
};

/* Macro for extracting the pathname buffer offset from the nameofs_flags
 * field. */
#define PKG_NAMEOFS(nameofs_flags) ((nameofs_flags) & 0x00FFFFFF)

/* Flags for the nameofs_flags field. */
#define PKGF_DEFLATED  (1<<24)  // Compressed using the "deflate" method.

/*-----------------------------------------------------------------------*/

/**
 * PKG_HEADER_SWAP_BYTES:  Swap the byte order of numeric fields in a
 * PKGHeader structure between native byte order and big-endian order.
 *
 * [Parameters]
 *     header: PKGHeader structure to convert (note: not a pointer).
 */
#define PKG_HEADER_SWAP_BYTES(header) do {                  \
    (header).header_size = be_to_u16((header).header_size); \
    (header).entry_size = be_to_u16((header).entry_size);   \
    (header).entry_count = be_to_u32((header).entry_count); \
    (header).name_size = be_to_u32((header).name_size);     \
} while (0)


/**
 * PKG_HEADER_SWAP_BYTES:  Swap the byte order of numeric fields in an
 * array of PKGIndexEntry structures between native byte order and
 * big-endian order.
 *
 * [Parameters]
 *     index: Pointer to PKGIndexEntry array to convert.
 *     count: Number of array entries to convert.
 */
#define PKG_INDEX_SWAP_BYTES(index,count) do {  \
    uint32_t *_ptr32 = (uint32_t *)(index);     \
    uint32_t *_limit = _ptr32 + ((count) * (sizeof(PKGIndexEntry)/4)); \
    while (_ptr32 < _limit) {                   \
        *_ptr32 = be_to_u32(*_ptr32);           \
        _ptr32++;                               \
    }                                           \
} while (0)

/*-----------------------------------------------------------------------*/

/**
 * pkg_hash:  Pathname hash function used for the PKG file index.
 *
 * [Parameters]
 *     path: Pathname to hash.
 * [Return value]
 *     Hash value.
 */
static inline PURE_FUNCTION uint32_t pkg_hash(const char *path)
{
#ifndef IN_TOOL
    PRECOND(path != NULL, return 0);
#endif

    /* For each byte, rotate the hash value right 5 bits and XOR with the
     * byte value (with uppercase alphabetic characters converted to
     * lowercase).  This is reasonably fast, and seems to produce good
     * hash value distribution with real data sets. */
    uint32_t hash = 0;
    while (*path) {
        uint32_t c = *path++;
        if (c >= 'A' && c <= 'Z') {
            c += 0x20;
        }
        hash = hash<<27 | hash>>5;
        hash ^= c;
    }
    return hash;
}

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_SRC_RESOURCE_PACKAGE_PKG_H
