/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/android/InputDialog.java: Wrapper for displaying an Android
 * dialog to get a single line of text input from the user.
 */

/* Replace this with an appropriate package name for your program. */
package com.example.sil_app;

import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.text.Editable;
import android.view.KeyEvent;
import android.view.View;
import android.widget.EditText;
import android.widget.TextView;

public class InputDialog {

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Constructor arguments. */
private Activity activity;
private String title;
private String text;

/* The android.activity.Dialog created for this instance. */
private Dialog dialog;

/* The EditText widget used for text input. */
private EditText input_widget;

/* Flag indicating whether the dialog has closed.  If true, this.text will
 * be set to the text entered, or null if the dialog was cancelled. */
private boolean finished;

/*************************************************************************/
/*************************** Interface methods ***************************/
/*************************************************************************/

/**
 * InputDialog:  Constructor for the InputDialog class.
 *
 * [Parameters]
 *     activity: Activity with which the dialog is to be associated.
 *     title: Dialog title, or null for none.
 *     text: Initial input text, or null for none.
 */
InputDialog(Activity activity, String title, String text)
{
    this.activity = activity;
    this.title = title;
    this.text = text;
    this.finished = false;
}

/*-----------------------------------------------------------------------*/

/**
 * show:  Display this dialog.
 */
synchronized void show()
{
    dialog = null;
    activity.runOnUiThread(new Runnable() {public void run() {
        synchronized(InputDialog.this) {
            setup();
            InputDialog.this.notify();
        }
    }});
    try {
        wait();
    } catch (InterruptedException e) {}

    finished = false;
    activity.runOnUiThread(new Runnable() {public void run() {
        dialog.show();
    }});
}

/*-----------------------------------------------------------------------*/

/**
 * isFinished:  Return whether the dialog has received input from the user.
 *
 * [Return value]
 *     True if the dialog has closed (user pressed Enter or cancelled).
 */
synchronized boolean isFinished()
{
    return finished;
}

/*-----------------------------------------------------------------------*/

/**
 * getText:  Return the text entered into the dialog.  May only be called
 * when isFinished() returns true; if called at any other time, behavior
 * is undefined.
 *
 * [Return value]
 *     Text entered by user, or null if the dialog was cancelled.
 */
synchronized String getText()
{
    return text;
}

/*-----------------------------------------------------------------------*/

/**
 * dismiss:  Close this dialog.
 */
synchronized void dismiss()
{
    if (dialog != null) {
        activity.runOnUiThread(new Runnable() {public void run() {
            InputDialog.this.dialog.dismiss();
            InputDialog.this.dialog = null;
        }});
    }
}

/*************************************************************************/
/***************************** Local methods *****************************/
/*************************************************************************/

/**
 * setup:  Create the android.activity.Dialog instance for this dialog.
 * Must be called from the UI thread.
 */
private void setup()
{
    dialog = new Dialog(activity);
    dialog.setTitle(title);
    dialog.setCancelable(true);

    input_widget = new EditText(activity);
    input_widget.setSingleLine();
    if (text != null) {
        input_widget.setText(text, TextView.BufferType.EDITABLE);
    }
    input_widget.setOnEditorActionListener(
        new TextView.OnEditorActionListener() {
            public boolean onEditorAction(TextView widget, int actionID,
                                          KeyEvent event) {
                InputDialog.this.onFinished();
                return true;
            }
        });
    input_widget.addOnAttachStateChangeListener(
        new View.OnAttachStateChangeListener() {
            public void onViewAttachedToWindow(View view) {}
            public void onViewDetachedFromWindow(View view) {
                InputDialog.this.onFinished();
            }
        });
    dialog.setContentView(input_widget);
}

/*-----------------------------------------------------------------------*/

/**
 * onFinished:  Callback function called when the user confirms the input
 * string.
 */
private synchronized void onFinished()
{
    text = input_widget.getText().toString();
    finished = true;
}

/*************************************************************************/
/*************************************************************************/

}  // class InputDialog
