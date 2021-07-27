/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/debug.h: Internal header for debugging routines.
 */

#ifndef SIL_SRC_DEBUG_H
#define SIL_SRC_DEBUG_H

#include "SIL/debug.h"  // Include the public header.

#ifdef DEBUG  // To the end of the file.

/*************************************************************************/
/*************************************************************************/

/* Constants for debug_cpu_record_phase(). */
typedef enum DebugCPUPhase {
    /* Time spent actively making render calls. */
    DEBUG_CPU_RENDER_START = 0,
    DEBUG_CPU_RENDER_END,
    /* Time spent on non-render processing (anything between
     * graphics_finish_frame() and the next graphics_start_frame()). */
    DEBUG_CPU_PROCESS_START,
    DEBUG_CPU_PROCESS_END,
    /* Time spent waiting for the graphics hardware to finish performing
     * all commands from the previous frame. */
    DEBUG_CPU_GPU_WAIT_START,
    DEBUG_CPU_GPU_WAIT_END,
} DebugCPUPhase;

/*-----------------------------------------------------------------------*/

/**
 * debug_init:  Initialize the debug interface.  This function always
 * succeeds.
 */
extern void debug_init(void);

/**
 * debug_cleanup:  Clean up debug interface resources.
 */
extern void debug_cleanup(void);

/**
 * debug_record_cpu_phase:  Indicate that processing has entered the given
 * phase.  These functions are called by graphics_start_frame() and
 * graphics_finish_frame() to record CPU usage information.
 *
 * In order for the debug interface to function properly, this function
 * _must_ be called with DEBUG_CPU_RENDER_END immediately before
 * terminating rendering for the current frame.
 *
 * [Parameters]
 *     phase: DEBUG_CPU_* constant indicating state change to record.
 */
extern void debug_record_cpu_phase(DebugCPUPhase phase);

/*-----------------------------------------------------------------------*/

#ifdef SIL_INCLUDE_TESTS

/**
 * TEST_debug_set_frame_period:  Set the frame period used by the CPU meter.
 *
 * [Parameters]
 *     period: Frame period to use, or 0 to use the system frame period.
 */
extern void TEST_debug_set_frame_period(float period);

/**
 * TEST_debug_capture_frame:  Capture the specified portion of the display
 * after performing all debug rendering for the current frame.  If called
 * multiple times during the same frame, only the last call is honored.
 * Sample usage:
 *
 *     TEST_debug_capture_frame(0, 0, width, height, pixels);
 *     graphics_finish_frame();
 *     // Pixel data is now valid.
 *
 * This serves as a substitute for graphics_read_pixels() in debug
 * interface tests, since that function cannot reliably be used after
 * graphics_finish_frame() but graphics_finish_frame() must be called to
 * trigger debug rendering.
 *
 * This function is only defined when tests are included.
 *
 * [Parameters]
 *     x, y, w, h: Display region to capture (like graphics_read_pixels()).
 *     pixels: Pixel buffer into which to store captured data, or NULL to
 *         cancel a pending capture.
 */
extern void TEST_debug_capture_frame(int x, int y, int w, int h, void *pixels);

#endif  // SIL_INCLUDE_TESTS

/*************************************************************************/
/*************************************************************************/

#endif  // DEBUG
#endif  // SIL_SRC_DEBUG_H
