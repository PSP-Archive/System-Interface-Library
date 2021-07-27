/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/android/Dialog.java: Wrapper for displaying an Android alert
 * dialog.
 */

/* Replace this with an appropriate package name for your program. */
package com.example.sil_app;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;

public class Dialog {

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Constructor arguments. */
private Activity activity;
private String title;
private String text;
private String button_yes;
private String button_no;
private String button_other;

/* The AlertDialog.Builder instance used to create the dialog. */
private AlertDialog.Builder builder;

/* The AlertDialog created for this instance. */
private AlertDialog dialog;

/* Code indicating the button pressed (returned from showAndWait()). */
private int result;

/*************************************************************************/
/*************************** Interface methods ***************************/
/*************************************************************************/

/**
 * Dialog:  Constructor for the Dialog class.
 *
 * [Parameters]
 *     activity: Activity with which the dialog is to be associated.
 *     title: Dialog title, or null for none.
 *     text: Dialog text, or null for none.
 *     button_yes: Text for the positive ("Yes"/"OK") button, or null to
 *         not show the button.
 *     button_no: Text for the negative ("No"/"Cancel") button, or null
 *         to not show the button.
 *     button_other: Text for the neutral button, or null to not show the
 *         button.
 */
Dialog(Activity activity, String title, String text,
       String button_yes, String button_no, String button_other)
{
    this.activity = activity;
    this.title = title;
    this.text = text;
    this.button_yes = button_yes;
    this.button_no = button_no;
    this.button_other = button_other;
}

/*-----------------------------------------------------------------------*/

/**
 * showAndWait:  Display this dialog and wait for a button to be pressed.
 *
 * [Return value]
 *     1 if the positive button was activated; 0 if the negative button was
 *     activated; -1 if the neutral button was activated.
 */
synchronized int showAndWait()
{
    dialog = null;
    activity.runOnUiThread(new Runnable() {public void run() {
        synchronized(Dialog.this) {
            setup();
            Dialog.this.notify();
        }
    }});
    try {
        wait();
    } catch (InterruptedException e) {}
    result = -2;
    activity.runOnUiThread(new Runnable() {public void run() {
        dialog.show();
    }});
    try {
        wait();
    } catch (InterruptedException e) {}
    return result;
}

/*************************************************************************/
/***************************** Local methods *****************************/
/*************************************************************************/

/**
 * setup:  Create the AlertDialog instance for this dialog.  Must be called
 * from the UI thread.
 */
private void setup()
{
    builder = new AlertDialog.Builder(activity);
    builder.setCancelable(false);
    if (title != null) {
        builder.setTitle(title);
    }
    if (text != null) {
        builder.setMessage(text);
    }
    if (button_yes != null) {
        builder.setPositiveButton(
            button_yes, new DialogInterface.OnClickListener() {
                public void onClick(DialogInterface dialog, int which) {
                    synchronized(Dialog.this) {
                        result = 1;
                        dialog.dismiss();
                        Dialog.this.notify();
                    }
                }
            }
        );
    }
    if (button_no != null) {
        builder.setNegativeButton(
            button_yes, new DialogInterface.OnClickListener() {
                public void onClick(DialogInterface dialog, int which) {
                    synchronized(Dialog.this) {
                        result = 0;
                        dialog.dismiss();
                        Dialog.this.notify();
                    }
                }
            }
        );
    }
    if (button_other != null) {
        builder.setNeutralButton(
            button_yes, new DialogInterface.OnClickListener() {
                public void onClick(DialogInterface dialog, int which) {
                    synchronized(Dialog.this) {
                        result = -1;
                        dialog.dismiss();
                        Dialog.this.notify();
                    }
                }
            }
        );
    }
    dialog = builder.create();
}

/*************************************************************************/
/*************************************************************************/

}  // class Dialog
