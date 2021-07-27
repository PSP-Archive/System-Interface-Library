/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * tools/zoom.h: Header for image scaling routine.  Borrowed from transcode
 * (GPL2+).
 */

#ifndef SIL_TOOLS_ZOOM_H
#define SIL_TOOLS_ZOOM_H

/*************************************************************************/
/*************************************************************************/

/* Filter IDs for tcv_zoom(): */
typedef enum {
    TCV_ZOOM_HERMITE = 1,
    TCV_ZOOM_BOX,
    TCV_ZOOM_TRIANGLE,
    TCV_ZOOM_BELL,
    TCV_ZOOM_B_SPLINE,
    TCV_ZOOM_LANCZOS3,
    TCV_ZOOM_MITCHELL,
    TCV_ZOOM_CUBIC_KEYS4,
    TCV_ZOOM_SINC8,
} TCVZoomFilter;

/* Internal data used by zoom_process(). (opaque to caller) */
typedef struct zoominfo ZoomInfo;

/* Create a ZoomInfo structure for the given parameters. */
ZoomInfo *zoom_init(int old_w, int old_h, int new_w, int new_h, int Bpp,
                    int old_stride, int new_stride, int alpha_mode,
                    TCVZoomFilter filter);

/* The resizing function itself. */
void zoom_process(const ZoomInfo *zi, const uint8_t *src, uint8_t *dest);

/* Free a ZoomInfo structure. */
void zoom_free(ZoomInfo *zi);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_TOOLS_ZOOM_H
