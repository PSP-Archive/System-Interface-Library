/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/android/RunTestsActivity.java: Activity used to run the
 * built-in tests, subclassed from SILActivity.
 */

package com.example.sil_app;  // Replaced by the Makefile.

public class RunTestsActivity extends SILActivity {

/*************************************************************************/
/*************************************************************************/

@Override
public String getArgs()
{
    return "-test";
}

/*************************************************************************/
/*************************************************************************/

}  // class RunTestsActivity
