/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/ge-util/ge-const.h: Header defining internal constants
 * for the GE utility library.
 */

#ifndef SIL_SRC_SYSDEP_PSP_GE_UTIL_GE_CONST_H
#define SIL_SRC_SYSDEP_PSP_GE_UTIL_GE_CONST_H

/*************************************************************************/
/*************************************************************************/

/* GE instruction opcodes. */

typedef enum GECommand {
    GECMD_NOP           = 0x00, // No operation
    GECMD_VERTEX_POINTER= 0x01, // Vertex pointer (lower 24 bits)
    GECMD_INDEX_POINTER = 0x02, // Index pointer (lower 24 bites)
    /* 0x03 undefined */
    GECMD_DRAW_PRIMITIVE= 0x04, // Draw primitives
    GECMD_DRAW_BEZIER   = 0x05, // Draw Bezier curves
    GECMD_DRAW_SPLINE   = 0x06, // Draw splines
    GECMD_TEST_BBOX     = 0x07, // Test against a bounding box
    GECMD_JUMP          = 0x08, // Unconditional jump to address (lower 24 bits)
    GECMD_COND_JUMP     = 0x09, // Conditional jump to address (lower 24 bits)
    GECMD_CALL          = 0x0A, // Call subroutine (lower 24 bits)
    GECMD_RETURN        = 0x0B, // Return from subroutine
    GECMD_END           = 0x0C, // Stop processing
    /* 0x0D undefined */
    GECMD_SIGNAL        = 0x0E, // Send a signal to the CPU
    GECMD_FINISH        = 0x0F, // Wait for drawing operations to finish

    GECMD_ADDRESS_BASE  = 0x10, // Upper 8 bits for address instructions
    /* 0x11 undefined */
    GECMD_VERTEX_FORMAT = 0x12, // Vertex data format
    GECMD_UNKNOWN_13    = 0x13, // psp_doc: Offset Address (BASE)
    GECMD_UNKNOWN_14    = 0x14, // psp_doc: Origin Address (BASE)
    GECMD_DRAWAREA_LOW  = 0x15, // Upper-left coordinates of drawing area
    GECMD_DRAWAREA_HIGH = 0x16, // Lower-right coordinates of drawing area
    GECMD_ENA_LIGHTING  = 0x17, // Enable/disable all lighting effects
    GECMD_ENA_LIGHT0    = 0x18, // Enable/disable light source 0
    GECMD_ENA_LIGHT1    = 0x19, // Enable/disable light source 1
    GECMD_ENA_LIGHT2    = 0x1A, // Enable/disable light source 2
    GECMD_ENA_LIGHT3    = 0x1B, // Enable/disable light source 3
    GECMD_ENA_ZCLIP     = 0x1C, // Enable/disable depth clipping
    GECMD_ENA_FACE_CULL = 0x1D, // Enable/disable face culling
    GECMD_ENA_TEXTURE   = 0x1E, // Enable/disable texturing
    GECMD_ENA_FOG       = 0x1F, // Enable/disable fog

    GECMD_ENA_DITHER    = 0x20, // Enable/disable dithering
    GECMD_ENA_BLEND     = 0x21, // Enable/disable blending
    GECMD_ENA_ALPHA_TEST= 0x22, // Enable/disable alpha testing
    GECMD_ENA_DEPTH_TEST= 0x23, // Enable/disable depth testing
    GECMD_ENA_STENCIL   = 0x24, // Enable/disable stencil operations
    GECMD_ENA_ANTIALIAS = 0x25, // Enable/disable antialiasing
    GECMD_ENA_PATCH_CULL= 0x26,
    GECMD_ENA_COLOR_TEST= 0x27, // Enable/disable color testing
    GECMD_ENA_LOGIC_OP  = 0x28, // Enable/disable logic operations
    /* 0x29 undefined */
    GECMD_BONE_OFFSET   = 0x2A,
    GECMD_BONE_UPLOAD   = 0x2B,
    GECMD_MORPH_0       = 0x2C,
    GECMD_MORPH_1       = 0x2D,
    GECMD_MORPH_2       = 0x2E,
    GECMD_MORPH_3       = 0x2F,

    GECMD_MORPH_4       = 0x30,
    GECMD_MORPH_5       = 0x31,
    GECMD_MORPH_6       = 0x32,
    GECMD_MORPH_7       = 0x33,
    /* 0x34 undefined */
    /* 0x35 undefined */
    GECMD_PATCH_SUBDIV  = 0x36,
    GECMD_PATCH_PRIM    = 0x37,
    GECMD_PATCH_FRONT   = 0x38,
    /* 0x39 undefined */
    GECMD_MODEL_START   = 0x3A, // Start setting the model matrix
    GECMD_MODEL_UPLOAD  = 0x3B, // Set the next element in the model matrix
    GECMD_VIEW_START    = 0x3C, // Start setting the view matrix
    GECMD_VIEW_UPLOAD   = 0x3D, // Set the next element in the view matrix
    GECMD_PROJ_START    = 0x3E, // Start setting the projection matrix
    GECMD_PROJ_UPLOAD   = 0x3F, // Set the next element in the projection matrix

    GECMD_TEXTURE_START = 0x40, // Start setting the texture matrix
    GECMD_TEXTURE_UPLOAD= 0x41, // Set the next element in the texture matrix
    GECMD_XSCALE        = 0x42, // X coordinate scale factor
    GECMD_YSCALE        = 0x43, // Y coordinate scale factor
    GECMD_ZSCALE        = 0x44, // Z coordinate scale factor
    GECMD_XPOS          = 0x45, // X coordinate of window space origin
    GECMD_YPOS          = 0x46, // Y coordinate of window space origin
    GECMD_ZPOS          = 0x47, // Z coordinate of window space origin
    GECMD_USCALE        = 0x48, // Texture U coordinate scale factor
    GECMD_VSCALE        = 0x49, // Texture V coordinate scale factor
    GECMD_UOFFSET       = 0x4A, // Texture U coordinate offset
    GECMD_VOFFSET       = 0x4B, // Texture V coordinate offset
    GECMD_XOFFSET       = 0x4C, // X offset of upper-left corner of screen
    GECMD_YOFFSET       = 0x4D, // Y offset of upper-left corner of screen
    /* 0x4E undefined */
    /* 0x4F undefined */

    GECMD_SHADE_MODE    = 0x50,
    GECMD_REV_NORMALS   = 0x51, // Invert normal vector signs
    /* 0x52 undefined */
    GECMD_COLOR_MATERIAL= 0x53, // psp_doc: Color Material
    GECMD_EMISSIVE_COLOR= 0x54,
    GECMD_AMBIENT_COLOR = 0x55,
    GECMD_DIFFUSE_COLOR = 0x56,
    GECMD_SPECULAR_COLOR= 0x57,
    GECMD_AMBIENT_ALPHA = 0x58,
    /* 0x59 undefined */
    /* 0x5A undefined */
    GECMD_SPECULAR_POWER= 0x5B,
    GECMD_LIGHT_AMBCOLOR= 0x5C,
    GECMD_LIGHT_AMBALPHA= 0x5D,
    GECMD_LIGHT_MODEL   = 0x5E,
    GECMD_LIGHT0_TYPE   = 0x5F,

    GECMD_LIGHT1_TYPE   = 0x60,
    GECMD_LIGHT2_TYPE   = 0x61,
    GECMD_LIGHT3_TYPE   = 0x62,
    GECMD_LIGHT0_XPOS   = 0x63,
    GECMD_LIGHT0_YPOS   = 0x64,
    GECMD_LIGHT0_ZPOS   = 0x65,
    GECMD_LIGHT1_XPOS   = 0x66,
    GECMD_LIGHT1_YPOS   = 0x67,
    GECMD_LIGHT1_ZPOS   = 0x68,
    GECMD_LIGHT2_XPOS   = 0x69,
    GECMD_LIGHT2_YPOS   = 0x6A,
    GECMD_LIGHT2_ZPOS   = 0x6B,
    GECMD_LIGHT3_XPOS   = 0x6C,
    GECMD_LIGHT3_YPOS   = 0x6D,
    GECMD_LIGHT3_ZPOS   = 0x6E,
    GECMD_LIGHT0_XDIR   = 0x6F,

    GECMD_LIGHT0_YDIR   = 0x70,
    GECMD_LIGHT0_ZDIR   = 0x71,
    GECMD_LIGHT1_XDIR   = 0x72,
    GECMD_LIGHT1_YDIR   = 0x73,
    GECMD_LIGHT1_ZDIR   = 0x74,
    GECMD_LIGHT2_XDIR   = 0x75,
    GECMD_LIGHT2_YDIR   = 0x76,
    GECMD_LIGHT2_ZDIR   = 0x77,
    GECMD_LIGHT3_XDIR   = 0x78,
    GECMD_LIGHT3_YDIR   = 0x79,
    GECMD_LIGHT3_ZDIR   = 0x7A,
    GECMD_LIGHT0_CATT   = 0x7B, // Constant attenuation factor for light 0
    GECMD_LIGHT0_LATT   = 0x7C, // Linear attenuation factor for light 0
    GECMD_LIGHT0_QATT   = 0x7D, // Quadratic attenuation factor for light 0
    GECMD_LIGHT1_CATT   = 0x7E, // Constant attenuation factor for light 1
    GECMD_LIGHT1_LATT   = 0x7F, // Linear attenuation factor for light 1

    GECMD_LIGHT1_QATT   = 0x80, // Quadratic attenuation factor for light 1
    GECMD_LIGHT2_CATT   = 0x81, // Constant attenuation factor for light 2
    GECMD_LIGHT2_LATT   = 0x82, // Linear attenuation factor for light 2
    GECMD_LIGHT2_QATT   = 0x83, // Quadratic attenuation factor for light 2
    GECMD_LIGHT3_CATT   = 0x84, // Constant attenuation factor for light 3
    GECMD_LIGHT3_LATT   = 0x85, // Linear attenuation factor for light 3
    GECMD_LIGHT3_QATT   = 0x86, // Quadratic attenuation factor for light 3
    GECMD_LIGHT0_SPOTEXP= 0x87, // Spotlight exponent for light 0
    GECMD_LIGHT1_SPOTEXP= 0x88, // Spotlight exponent for light 1
    GECMD_LIGHT2_SPOTEXP= 0x89, // Spotlight exponent for light 2
    GECMD_LIGHT3_SPOTEXP= 0x8A, // Spotlight exponent for light 3
    GECMD_LIGHT0_SPOTLIM= 0x8B, // Spotlight limit for light 0
    GECMD_LIGHT1_SPOTLIM= 0x8C, // Spotlight limit for light 1
    GECMD_LIGHT2_SPOTLIM= 0x8D, // Spotlight limit for light 2
    GECMD_LIGHT3_SPOTLIM= 0x8E, // Spotlight limit for light 3
    GECMD_LIGHT0_ACOL   = 0x8F, // Ambient color for light 0

    GECMD_LIGHT0_DCOL   = 0x90, // Diffuse color for light 0
    GECMD_LIGHT0_SCOL   = 0x91, // Specular color for light 0
    GECMD_LIGHT1_ACOL   = 0x92, // Ambient color for light 1
    GECMD_LIGHT1_DCOL   = 0x93, // Diffuse color for light 1
    GECMD_LIGHT1_SCOL   = 0x94, // Specular color for light 1
    GECMD_LIGHT2_ACOL   = 0x95, // Ambient color for light 2
    GECMD_LIGHT2_DCOL   = 0x96, // Diffuse color for light 2
    GECMD_LIGHT2_SCOL   = 0x97, // Specular color for light 2
    GECMD_LIGHT3_ACOL   = 0x98, // Ambient color for light 3
    GECMD_LIGHT3_DCOL   = 0x99, // Diffuse color for light 3
    GECMD_LIGHT3_SCOL   = 0x9A, // Specular color for light 3
    GECMD_FACE_ORDER    = 0x9B, // Vertex direction for face culling (CW/CCW)
    GECMD_DRAW_ADDRESS  = 0x9C, // Draw buffer address (low 24 bits)
    GECMD_DRAW_STRIDE   = 0x9D, // Draw buffer address (high 8 bits) and stride
    GECMD_DEPTH_ADDRESS = 0x9E, // Depth buffer address (low 24 bits)
    GECMD_DEPTH_STRIDE  = 0x9F, // Depth buffer address (high 8 bits) and stride

    GECMD_TEX0_ADDRESS  = 0xA0, // Texture level 0 address (low 24 bits)
    GECMD_TEX1_ADDRESS  = 0xA1, // Texture level 1 address (low 24 bits)
    GECMD_TEX2_ADDRESS  = 0xA2, // Texture level 2 address (low 24 bits)
    GECMD_TEX3_ADDRESS  = 0xA3, // Texture level 3 address (low 24 bits)
    GECMD_TEX4_ADDRESS  = 0xA4, // Texture level 4 address (low 24 bits)
    GECMD_TEX5_ADDRESS  = 0xA5, // Texture level 5 address (low 24 bits)
    GECMD_TEX6_ADDRESS  = 0xA6, // Texture level 6 address (low 24 bits)
    GECMD_TEX7_ADDRESS  = 0xA7, // Texture level 7 address (low 24 bits)
    GECMD_TEX0_STRIDE   = 0xA8, // Texture level 0 address (high 8b) and stride
    GECMD_TEX1_STRIDE   = 0xA9, // Texture level 1 address (high 8b) and stride
    GECMD_TEX2_STRIDE   = 0xAA, // Texture level 2 address (high 8b) and stride
    GECMD_TEX3_STRIDE   = 0xAB, // Texture level 3 address (high 8b) and stride
    GECMD_TEX4_STRIDE   = 0xAC, // Texture level 4 address (high 8b) and stride
    GECMD_TEX5_STRIDE   = 0xAD, // Texture level 5 address (high 8b) and stride
    GECMD_TEX6_STRIDE   = 0xAE, // Texture level 6 address (high 8b) and stride
    GECMD_TEX7_STRIDE   = 0xAF, // Texture level 7 address (high 8b) and stride

    GECMD_CLUT_ADDRESS_L= 0xB0, // CLUT address (low 24 bits)
    GECMD_CLUT_ADDRESS_H= 0xB1, // CLUT address (high 8 bits)
    GECMD_COPY_S_ADDRESS= 0xB2, // Copy source address (low 24 bits)
    GECMD_COPY_S_STRIDE = 0xB3, // Copy source address (high 8 bits) and stride
    GECMD_COPY_D_ADDRESS= 0xB4, // Copy destination address (low 24 bits)
    GECMD_COPY_D_STRIDE = 0xB5, // Copy dest. address (high 8 bits) and stride
    /* 0xB6 undefined */
    /* 0xB7 undefined */
    GECMD_TEX0_SIZE     = 0xB8, // Texture level 0 size
    GECMD_TEX1_SIZE     = 0xB9, // Texture level 1 size
    GECMD_TEX2_SIZE     = 0xBA, // Texture level 2 size
    GECMD_TEX3_SIZE     = 0xBB, // Texture level 3 size
    GECMD_TEX4_SIZE     = 0xBC, // Texture level 4 size
    GECMD_TEX5_SIZE     = 0xBD, // Texture level 5 size
    GECMD_TEX6_SIZE     = 0xBE, // Texture level 6 size
    GECMD_TEX7_SIZE     = 0xBF, // Texture level 7 size

    GECMD_TEXTURE_MAP   = 0xC0, // Environment mapping mode and data source
    GECMD_TEXTURE_MATSEL= 0xC1, // Texture matrix row selection
    GECMD_TEXTURE_MODE  = 0xC2, // Texturing mode
    GECMD_TEXTURE_PIXFMT= 0xC3, // Texture data format
    GECMD_CLUT_LOAD     = 0xC4, // Load CLUT data
    GECMD_CLUT_MODE     = 0xC5, // CLUT color format and shift/mask values
    GECMD_TEXTURE_FILTER= 0xC6, // Texture mag/min filters
    GECMD_TEXTURE_WRAP  = 0xC7, // Texture coordinate wrapping modes
    GECMD_TEXTURE_BIAS  = 0xC8, // Mipmap selection mode and bias
    GECMD_TEXTURE_FUNC  = 0xC9, // Texturing function
    GECMD_TEXTURE_COLOR = 0xCA, // Texture color for blend-mode texturing
    GECMD_TEXTURE_FLUSH = 0xCB, // Clear texture cache
    GECMD_COPY_SYNC     = 0xCC, // Wait for data copying to complete
    GECMD_FOG_LIMIT     = 0xCD, // Fog depth limit
    GECMD_FOG_RANGE     = 0xCE, // Fog depth range
    GECMD_FOG_COLOR     = 0xCF, // Fog color

    GECMD_TEXTURE_SLOPE = 0xD0, // Slope for mipmap selection
    /* 0xD1 undefined */
    GECMD_FRAME_PIXFMT  = 0xD2, // Pixel format for draw buffer
    GECMD_CLEAR_MODE    = 0xD3, // Depth/draw buffer clear flags
    GECMD_CLIP_MIN      = 0xD4, // Clip region low coordinates
    GECMD_CLIP_MAX      = 0xD5, // Clip region high coordinates
    GECMD_CLIP_NEAR     = 0xD6, // Clip region near depth value
    GECMD_CLIP_FAR      = 0xD7, // Clip region far depth value
    GECMD_COLORTEST_FUNC= 0xD8, // Color test function
    GECMD_COLORTEST_REF = 0xD9, // Color test reference value
    GECMD_COLORTEST_MASK= 0xDA, // Color test bitmask
    GECMD_ALPHATEST     = 0xDB, // Alpha test function, ref value, and bitmask
    GECMD_STENCILTEST   = 0xDC, // Stencil test function, ref value, and bitmask
    GECMD_STENCIL_OP    = 0xDD, // Stencil operation to perform
    GECMD_DEPTHTEST     = 0xDE, // Depth test function
    GECMD_BLEND_FUNC    = 0xDF, // Blend function

    GECMD_BLEND_SRCFIX  = 0xE0, // Blend constant for source
    GECMD_BLEND_DSTFIX  = 0xE1, // Blend constant for destination
    GECMD_DITHER0       = 0xE2, // Dithering matrix (row 0)
    GECMD_DITHER1       = 0xE3, // Dithering matrix (row 1)
    GECMD_DITHER2       = 0xE4, // Dithering matrix (row 2)
    GECMD_DITHER3       = 0xE5, // Dithering matrix (row 3)
    GECMD_LOGIC_OP      = 0xE6, // Logic operation for color values
    GECMD_DEPTH_MASK    = 0xE7, // Write mask for depth buffer
    GECMD_COLOR_MASK    = 0xE8, // Bitmask for writing color values
    GECMD_ALPHA_MASK    = 0xE9, // Bitmask for writing alpha values
    GECMD_COPY          = 0xEA, // Start data copy
    GECMD_COPY_S_POS    = 0xEB, // Upper-left source coordinates for copy
    GECMD_COPY_D_POS    = 0xEC, // Upper-left destination coordinates for copy
    /* 0xED undefined */
    GECMD_COPY_SIZE     = 0xEE, // Size of region to copy
    /* 0xEF undefined */

    GECMD_UNKNOWN_F0    = 0xF0,
    GECMD_UNKNOWN_F1    = 0xF1,
    GECMD_UNKNOWN_F2    = 0xF2,
    GECMD_UNKNOWN_F3    = 0xF3,
    GECMD_UNKNOWN_F4    = 0xF4,
    GECMD_UNKNOWN_F5    = 0xF5,
    GECMD_UNKNOWN_F6    = 0xF6,
    GECMD_UNKNOWN_F7    = 0xF7,
    GECMD_UNKNOWN_F8    = 0xF8,
    GECMD_UNKNOWN_F9    = 0xF9,
    /* 0xFA-0xFF undefined */
} GECommand;

/*************************************************************************/

/* Clear flags for GECMD_CLEAR_MODE. */
#define GECLEAR_OFF     0x0000  // Normal rendering (clear mode disabled)
#define GECLEAR_ON      0x0001  // Clear mode enabled
#define GECLEAR_DRAW    0x0100  // Clear the frame buffer
#define GECLEAR_STENCIL 0x0200  // Clear the stencil buffer
#define GECLEAR_DEPTH   0x0400  // Clear the depth buffer

/* Vertex order constant for GECMD_FACE_ORDER. */
typedef enum GEVertexOrder {
    GEVERT_CCW = 0,
    GEVERT_CW  = 1,
} GEVertexOrder;

/* Signal handler behaviors. */
typedef enum GESignalBehavior {
    GESIG_SUSPEND = 1,   // Stop until callback returns.
    GESIG_CONTINUE = 2,  // Continue without waiting for callback to return.
} GESignalBehavior;

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_PSP_GE_UTIL_GE_CONST_H
