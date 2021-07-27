/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/android/Downloader-dummy.java: Dummy implementation of the
 * Downloader class used when APK expansion file support is disabled.
 */

/* Replace this with an appropriate package name for your program. */
package com.example.sil_app;

public class Downloader {

public static String getFilePath(android.content.Context context, int index) {
    return null;
}

}
