/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/android/sysfont.c: System font rendering functionality for
 * Android.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/android/internal.h"
#include "src/sysdep/opengl/opengl.h"
#include "src/texture.h"
#include "src/utility/utf8.h"

#include <android/bitmap.h>

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Data structure for Android fonts. */
struct SysFont {
    /* Java object pointers. */
    jclass class;       // SysFont class object (from Java).
    jobject instance;   // SysFont (Java) instance created for this object.

    /* Java method IDs for SysFont methods. */
    jmethodID height, baseline, ascent, descent, textAdvance, textWidth;
    jmethodID drawText;

};

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

SysFont *sys_sysfont_create(UNUSED const char *name, UNUSED float size,
                            int mem_flags)
{
    SysFont *font = mem_alloc(sizeof(*font), 0, mem_flags);
    if (UNLIKELY(!font)) {
        DLOG("Failed to allocate memory for SysFont structure");
        goto error_return;
    }

    JNIEnv *env = get_jni_env();
    const jclass class = get_class(".SysFont");
    ASSERT(class != 0, goto error_free_font);

    const jmethodID constructor = get_method(
        class, "<init>", "(Landroid/app/Activity;)V");
    font->height = (*env)->GetMethodID(env, class, "height", "(F)F");
    font->baseline = (*env)->GetMethodID(env, class, "baseline", "(F)F");
    font->ascent = (*env)->GetMethodID(env, class, "ascent", "(F)F");
    font->descent = (*env)->GetMethodID(env, class, "descent", "(F)F");
    font->textAdvance = get_method(
        class, "textAdvance", "(Ljava/lang/String;F)F");
    font->textWidth = get_method(class, "textWidth", "(Ljava/lang/String;F)F");
    font->drawText = get_method(
        class, "drawText", "(Ljava/lang/String;F)Landroid/graphics/Bitmap;");
    ASSERT(constructor, goto error_unref_class);
    ASSERT(font->height, goto error_unref_class);
    ASSERT(font->baseline, goto error_unref_class);
    ASSERT(font->ascent, goto error_unref_class);
    ASSERT(font->descent, goto error_unref_class);
    ASSERT(font->textAdvance, goto error_unref_class);
    ASSERT(font->textWidth, goto error_unref_class);
    ASSERT(font->drawText, goto error_unref_class);

    const jobject instance = (*env)->NewObject(
        env, class, constructor, android_activity->clazz);
    if (UNLIKELY(clear_exceptions(env)) || UNLIKELY(!instance)) {
        DLOG("Failed to create SysFont instance");
        goto error_unref_class;
    }

    font->instance = (*env)->NewGlobalRef(env, instance);
    (*env)->DeleteLocalRef(env, instance);
    if (UNLIKELY(!font->instance)) {
        clear_exceptions(env);
        DLOG("Failed to create global reference for SysFont instance");
        goto error_unref_class;
    }

    (*env)->DeleteLocalRef(env, class);
    return font;

  error_unref_class:
    clear_exceptions(env);
    (*env)->DeleteLocalRef(env, class);
  error_free_font:
    mem_free(font);
  error_return:
    return NULL;
}

/*-----------------------------------------------------------------------*/

void sys_sysfont_destroy(SysFont *font)
{
    JNIEnv *env = get_jni_env();
    (*env)->DeleteGlobalRef(env, font->instance);
    mem_free(font);
}

/*-----------------------------------------------------------------------*/

int sys_sysfont_native_size(UNUSED SysFont *font)
{
    return 0;  // Any size works fine.
}

/*-----------------------------------------------------------------------*/

void sys_sysfont_get_metrics(
    SysFont *font, float size, float *height_ret, float *baseline_ret,
    float *ascent_ret, float *descent_ret)
{
    JNIEnv *env = get_jni_env();

    if (height_ret) {
        *height_ret = (*env)->CallFloatMethod(
            env, font->instance, font->height, size);
        ASSERT(!clear_exceptions(env), *height_ret = 0);
    }
    if (baseline_ret) {
        /* Round up to match render behavior. */
        *baseline_ret = ceilf((*env)->CallFloatMethod(
                                  env, font->instance, font->baseline, size));
        ASSERT(!clear_exceptions(env), *baseline_ret = 0);
    }
    if (ascent_ret) {
        *ascent_ret = (*env)->CallFloatMethod(
            env, font->instance, font->ascent, size);
        ASSERT(!clear_exceptions(env), *ascent_ret = 0);
    }
    if (descent_ret) {
        *descent_ret = (*env)->CallFloatMethod(
            env, font->instance, font->descent, size);
        ASSERT(!clear_exceptions(env), *descent_ret = 0);
    }
}

/*-----------------------------------------------------------------------*/

float sys_sysfont_char_advance(SysFont *font, int32_t ch, float size)
{
    char buf[7];
    if (ch < 1<<7) {
        buf[0] = ch;
        buf[1] = 0;
    } else if (ch < 1<<11) {
        buf[0] = 0xC0 | ch>>6;
        buf[1] = 0x80 | ((ch>>0) & 0x3F);
        buf[2] = 0;
    } else if (ch < 1<<16) {
        buf[0] = 0xE0 | ch>>12;
        buf[1] = 0x80 | ((ch>>6) & 0x3F);
        buf[2] = 0x80 | ((ch>>0) & 0x3F);
        buf[3] = 0;
    } else if (ch < 1<<21) {
        buf[0] = 0xF0 | ch>>18;
        buf[1] = 0x80 | ((ch>>12) & 0x3F);
        buf[2] = 0x80 | ((ch>> 6) & 0x3F);
        buf[3] = 0x80 | ((ch>> 0) & 0x3F);
        buf[4] = 0;
    } else if (ch < 1<<26) {
        buf[0] = 0xF8 | ch>>24;
        buf[1] = 0x80 | ((ch>>18) & 0x3F);
        buf[2] = 0x80 | ((ch>>12) & 0x3F);
        buf[3] = 0x80 | ((ch>> 6) & 0x3F);
        buf[4] = 0x80 | ((ch>> 0) & 0x3F);
        buf[5] = 0;
    } else {
        buf[0] = 0xFC | ch>>30;
        buf[1] = 0x80 | ((ch>>24) & 0x3F);
        buf[2] = 0x80 | ((ch>>18) & 0x3F);
        buf[3] = 0x80 | ((ch>>12) & 0x3F);
        buf[4] = 0x80 | ((ch>> 6) & 0x3F);
        buf[5] = 0x80 | ((ch>> 0) & 0x3F);
        buf[6] = 0;
    }
    return sys_sysfont_text_advance(font, buf, size);
}

/*-----------------------------------------------------------------------*/

float sys_sysfont_text_advance(SysFont *font, const char *str, float size)
{
    JNIEnv *env = get_jni_env();
    jstring j_str = (*env)->NewStringUTF(env, str);
    ASSERT(j_str != 0, clear_exceptions(env); return 0);
    const float advance = (*env)->CallFloatMethod(
        env, font->instance, font->textAdvance, j_str, size);
    (*env)->DeleteLocalRef(env, j_str);
    ASSERT(!clear_exceptions(env));
    return advance;
}

/*-----------------------------------------------------------------------*/

void sys_sysfont_get_text_bounds(
    SysFont *font, const char *str, float size, float *left_ret,
    float *right_ret)
{
    JNIEnv *env = get_jni_env();
    jstring j_str = (*env)->NewStringUTF(env, str);
    ASSERT(j_str != 0, clear_exceptions(env); *left_ret = 0; *right_ret = 0; return);
    const float width = (*env)->CallFloatMethod(
        env, font->instance, font->textWidth, j_str, size);
    (*env)->DeleteLocalRef(env, j_str);
    *left_ret = 0;
    *right_ret = width;
    ASSERT(!clear_exceptions(env), *right_ret = 0);
}

/*-----------------------------------------------------------------------*/

SysTexture *sys_sysfont_render(
    SysFont *font, const char *str, float size, float *origin_x_ret,
    float *origin_y_ret, float *advance_ret, float *scale_ret)
{
    JNIEnv *env = get_jni_env();
    jstring j_str = (*env)->NewStringUTF(env, str);
    ASSERT(j_str != 0, clear_exceptions(env); return 0);
    jobject bitmap = (*env)->CallObjectMethod(
        env, font->instance, font->drawText, j_str, size);
    (*env)->DeleteLocalRef(env, j_str);
    if (clear_exceptions(env) || !bitmap) {
        DLOG("Failed to render text (Java exception?)");
        return NULL;
    }

    AndroidBitmapInfo info;
    void *pixels;
    if (AndroidBitmap_getInfo(env, bitmap, &info) != 0) {
        DLOG("Failed to get bitmap info");
        (*env)->DeleteLocalRef(env, bitmap);
        return NULL;
    }
    if (info.format != ANDROID_BITMAP_FORMAT_A_8) {
        DLOG("Wrong format for bitmap (was %d, should be %d == ALPHA_8)",
             info.format, ANDROID_BITMAP_FORMAT_A_8);
        (*env)->DeleteLocalRef(env, bitmap);
        return NULL;
    }
    if (AndroidBitmap_lockPixels(env, bitmap, &pixels) != 0) {
        DLOG("Failed to get bitmap pixels");
        (*env)->DeleteLocalRef(env, bitmap);
        return NULL;
    }

    /* Need to flip the image upside down for the texture. */
    uint8_t *top, *bottom;
    for (top = pixels, bottom = top + ((info.height - 1) * info.stride);
         top < bottom;
         top += info.stride, bottom -= info.stride)
    {
        uint8_t temp[256];
        for (unsigned int x = 0; x < info.stride; x += sizeof(temp)) {
            const unsigned int copy = ubound(info.stride - x, sizeof(temp));
            memcpy(temp, &top[x], copy);
            memcpy(&top[x], &bottom[x], copy);
            memcpy(&bottom[x], temp, copy);
        }
    }

    SysTexture *texture = sys_texture_create(
        info.width, info.height, TEX_FORMAT_A8, 1, pixels, info.stride,
        (int32_t[]){0}, (int32_t[]){info.stride * info.height}, 1, 0, 0);
    AndroidBitmap_unlockPixels(env, bitmap);
    (*env)->DeleteLocalRef(env, bitmap);
    if (UNLIKELY(!texture)) {
        DLOG("Failed to create %ux%u texture for text: %s",
             info.width, info.height, str);
        return NULL;
    }

    float ascent;
    sys_sysfont_get_metrics(font, size, NULL, NULL, &ascent, NULL);
    *origin_x_ret = 0;
    *origin_y_ret = info.height - iroundf(ascent);
    *advance_ret = sys_sysfont_text_advance(font, str, size);
    *scale_ret = 1;

    return texture;
}

/*************************************************************************/
/*************************************************************************/
