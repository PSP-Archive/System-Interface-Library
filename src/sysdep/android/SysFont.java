/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/android/SysFont.java: Routines for drawing text using the
 * Android system font.
 */

/* Replace this with an appropriate package name for your program. */
package com.example.sil_app;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;

public class SysFont {

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Activity with which this object is associated. */
private Activity activity;

/* Paint object used for text maniplation. */
private Paint text_paint;

/* Current font size applied to text_paint. */
private float cur_size;

/* Font metrics for font size cur_size. */
private Paint.FontMetrics metrics;

/*************************************************************************/
/*************************** Interface methods ***************************/
/*************************************************************************/

/**
 * SysFont:  Constructor for the SysFont class.
 *
 * [Parameters]
 *     activity: Activity with which this object is associated.
 */
SysFont(Activity activity)
{
    this.activity = activity;
    text_paint = null;
    cur_size = 0;
    metrics = null;
}

/*-----------------------------------------------------------------------*/

/**
 * height, baseline, ascent, descent:  Return the line height, baseline
 * position, ascent, or descent of the system font at the given size.
 *
 * [Parameters]
 *     size: Font size (nominal height), in pixels.
 * [Return value]
 *     Requested font metric, in pixels.
 */
float height(float size)
{
    setSize(size);
    return Math.abs(metrics.ascent) + Math.abs(metrics.descent)
         + Math.abs(metrics.leading);
}
float baseline(float size) {setSize(size); return Math.abs(metrics.ascent);}
float ascent(float size) {setSize(size); return Math.abs(metrics.top);}
float descent(float size) {setSize(size); return Math.abs(metrics.bottom);}

/*-----------------------------------------------------------------------*/

/**
 * textAdvance:  Return the horizontal advance for the given text in the
 * system font at the given size.
 *
 * [Parameters]
 *     text: Text to measure.
 *     size: Font size.
 * [Return value]
 *     Horizontal advance, in pixels.
 */
float textAdvance(String text, float size)
{
    setSize(size);
    return internalTextAdvance(text);
}

private float internalTextAdvance(final String text)
{
    float[] advances = new float[text.length()];
    int num_advances = text_paint.getTextWidths(text, advances);
    float ret_advance = 0;
    for (int i = 0; i < num_advances; i++) {
        ret_advance += advances[i];
    }
    return ret_advance;
}

/*-----------------------------------------------------------------------*/

/**
 * textWidth:  Return the width of the given text as it would be rendered
 * in the system font at the given size.
 *
 * [Parameters]
 *     text: Text to measure.
 *     size: Font size.
 * [Return value]
 *     Text width, in pixels.
 */
float textWidth(String text, float size)
{
    setSize(size);
    return internalTextWidth(text);
}

private float internalTextWidth(final String text)
{
    return text_paint.measureText(text);
}

/*-----------------------------------------------------------------------*/

/**
 * drawText:  Return an alpha-only Bitmap object with the given text drawn
 * in the given size.  The text baseline is located at a Y coordinate of
 * ceil(baseline(size)) in the returned bitmap.
 *
 * [Parameters]
 *     text: Text to draw.
 *     size: Font size.
 * [Return value]
 *     Bitmap containing text.
 */
Bitmap drawText(String text, float size)
{
    setSize(size);
    return internalDrawText(text);
}

private Bitmap internalDrawText(final String text)
{
    int width = (int)Math.ceil(text_paint.measureText(text));
    int baseline = (int)Math.ceil(Math.abs(metrics.top));
    int height = baseline + (int)Math.ceil(Math.abs(metrics.bottom));
    Bitmap ret_bitmap =
        Bitmap.createBitmap(width, height, Bitmap.Config.ALPHA_8);
    ret_bitmap.eraseColor(Color.TRANSPARENT);
    Canvas canvas = new Canvas(ret_bitmap);
    canvas.drawText(text, 0, baseline, text_paint);
    return ret_bitmap;
}

/*************************************************************************/
/***************************** Local methods *****************************/
/*************************************************************************/

/**
 * setSize:  Set the font size to use for text operations.  Does nothing
 * if the given size is already the current size (and thus is safe, in
 * performance terms, to call repeatedly with the same value).
 *
 * [Parameters]
 *     size: Font size, in pixels.
 */
private void setSize(float size)
{
    if (cur_size == size) {
        return;
    }
    cur_size = size;
    if (text_paint == null) {
        text_paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    }
    text_paint.setTextSize(cur_size);
    metrics = text_paint.getFontMetrics();
}

/*************************************************************************/
/*************************************************************************/

}  // class SysFont
