/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/ge-util/matrix.c: 3D coordinate transformation matrix
 * manipulation routines for the GE utility library.
 */

#include "src/base.h"
#include "src/math.h"
#include "src/sysdep/psp/ge-util.h"
#include "src/sysdep/psp/ge-util/ge-const.h"
#include "src/sysdep/psp/ge-util/ge-local.h"
#include "src/sysdep/psp/internal.h"

/*************************************************************************/
/*************************************************************************/

void ge_set_projection_matrix(const Matrix4f *matrix)
{
    if (UNLIKELY(!matrix)) {
        DLOG("matrix == NULL");
        return;
    }
    CHECK_GELIST(17);

    internal_add_command(GECMD_PROJ_START, 0);
    const float *m = &matrix->_11;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            internal_add_commandf(GECMD_PROJ_UPLOAD, m[i*4+j]);
        }
    }
}

/*-----------------------------------------------------------------------*/

void ge_set_view_matrix(const Matrix4f *matrix)
{
    if (UNLIKELY(!matrix)) {
        DLOG("matrix == NULL");
        return;
    }
    CHECK_GELIST(13);

    internal_add_command(GECMD_VIEW_START, 0);
    const float *m = &matrix->_11;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 3; j++) {
            internal_add_commandf(GECMD_VIEW_UPLOAD, m[i*4+j]);
        }
    }
}

/*-----------------------------------------------------------------------*/

void ge_set_model_matrix(const Matrix4f *matrix)
{
    if (UNLIKELY(!matrix)) {
        DLOG("matrix == NULL");
        return;
    }
    CHECK_GELIST(13);

    internal_add_command(GECMD_MODEL_START, 0);
    const float *m = &matrix->_11;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 3; j++) {
            internal_add_commandf(GECMD_MODEL_UPLOAD, m[i*4+j]);
        }
    }
}

/*-----------------------------------------------------------------------*/

void ge_set_texture_matrix(const Matrix4f *matrix)
{
    if (UNLIKELY(!matrix)) {
        DLOG("matrix == NULL");
        return;
    }
    CHECK_GELIST(13);

    internal_add_command(GECMD_TEXTURE_START, 0);
    const float *m = &matrix->_11;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 3; j++) {
            internal_add_commandf(GECMD_TEXTURE_UPLOAD, m[i*4+j]);
        }
    }
}

/*************************************************************************/
/*************************************************************************/
