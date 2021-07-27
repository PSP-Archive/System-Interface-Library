/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/android/Downloader.java: APK expansion file download handling.
 */

/* Replace this with an appropriate package name for your program. */
package com.example.sil_app;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Bundle;
import android.os.Messenger;
import android.provider.Settings;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.ProgressBar;
import android.widget.TextView;
import com.google.android.vending.expansion.downloader.DownloadProgressInfo;
import com.google.android.vending.expansion.downloader.DownloaderClientMarshaller;
import com.google.android.vending.expansion.downloader.DownloaderServiceMarshaller;
import com.google.android.vending.expansion.downloader.Helpers;
import com.google.android.vending.expansion.downloader.IDownloaderClient;
import com.google.android.vending.expansion.downloader.IDownloaderService;
import com.google.android.vending.expansion.downloader.IStub;
import com.google.android.vending.expansion.downloader.impl.DownloadInfo;
import com.google.android.vending.expansion.downloader.impl.DownloadsDB;

public class Downloader extends Activity implements IDownloaderClient {

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Flag: Is the download paused? */
private boolean paused;
/* Flag: Has the download failed? */
private boolean failed;

/* Download UI elements. */
private ProgressBar progress_bar;
private TextView status_message, progress_fraction, progress_percent;
private TextView average_speed, time_left;
private View mobile_data_dialog;
private Button pause_button, cancel_button;

/* Things needed by the downloader framework. */
private IDownloaderService downloader_service;
private IStub downloader_client_stub;

/*************************************************************************/
/****************** Information query methods (static) *******************/
/*************************************************************************/

/**
 * getFilePath:  Return the filesystem path of the given downloaded file.
 *
 * [Parameters]
 *     context: Context in which the method is being called.
 *     index: 0 to query the main file, 1 to query the patch file.
 * [Return value]
 *     Pathname, or null if the given file does not exist.
 */
public static String getFilePath(Context context, int index)
{
    DownloadsDB db = DownloadsDB.getDB(context.getApplicationContext());
    DownloadInfo info[] = db.getDownloads();
    if (info == null || index < 0 || index >= info.length) {
        return null;
    }
    return android.os.Environment.getExternalStorageDirectory()
        + "/Android/obb/" + context.getPackageName()
        + "/" + info[index].mFileName;
}

/*************************************************************************/
/*********************** Activity callback methods ***********************/
/*************************************************************************/

@Override
public void onCreate(Bundle savedInstanceState)
{
    super.onCreate(savedInstanceState);

    Intent intent = new Intent(this, getClass())
        .setFlags(Intent.FLAG_ACTIVITY_NEW_TASK |
                  Intent.FLAG_ACTIVITY_CLEAR_TOP)
        .setAction(getIntent().getAction());
    if (getIntent().getCategories() != null) {
        for (String category : getIntent().getCategories()) {
            intent.addCategory(category);
        }
    }
    PendingIntent pendingIntent = PendingIntent.getActivity(
        this, 0, intent, PendingIntent.FLAG_UPDATE_CURRENT);
    int result;
    try {
        result = DownloaderClientMarshaller.startDownloadServiceIfRequired(
            this, pendingIntent, DownloaderService.class);
    } catch (NameNotFoundException e) {
        e.printStackTrace();
        throw new RuntimeException("Impossible: failed to find own class");
    }
    if (result == DownloaderClientMarshaller.NO_DOWNLOAD_REQUIRED) {
        Log.i(Constants.SIL_PLATFORM_ANDROID_DLOG_LOG_TAG,
              "Data already downloaded, starting app...");
        switchToApp();
        return;
    }

    downloader_client_stub = DownloaderClientMarshaller.CreateStub(
        this, DownloaderService.class);
    paused = false;

    setContentView(R.layout.downloader);
    progress_bar = (ProgressBar)findViewById(R.id.progress_bar);
    status_message = (TextView)findViewById(R.id.status_message);
    progress_fraction = (TextView)findViewById(R.id.progress_fraction);
    progress_percent = (TextView)findViewById(R.id.progress_percent);
    average_speed = (TextView)findViewById(R.id.average_speed);
    time_left = (TextView)findViewById(R.id.time_remaining);
    mobile_data_dialog = findViewById(R.id.mobile_data_dialog);
    pause_button = (Button)findViewById(R.id.pause_button);
    cancel_button = (Button)findViewById(R.id.cancel_button);

    progress_bar.setIndeterminate(true);

    pause_button.setOnClickListener(new View.OnClickListener() {
        @Override public void onClick(View view) {
            if (downloader_service != null) {
                paused = !paused;
                if (paused) {
                    downloader_service.requestPauseDownload();
                } else {
                    downloader_service.requestContinueDownload();
                }
                updatePauseButtonText();
            }
        }});

    cancel_button.setOnClickListener(new View.OnClickListener() {
        @Override public void onClick(View view) {
            if (downloader_service != null) {
                downloader_service.requestAbortDownload();
            }
        }});

    Button wifi_settings_button =
        (Button)findViewById(R.id.wifi_settings_button);
    wifi_settings_button.setOnClickListener(new View.OnClickListener() {
        @Override public void onClick(View v) {
            startActivity(new Intent(Settings.ACTION_WIFI_SETTINGS));
        }});

    Button resume_on_mobile_button =
        (Button)findViewById(R.id.resume_on_mobile_button);
    resume_on_mobile_button.setOnClickListener(new View.OnClickListener() {
        @Override public void onClick(View view) {
            downloader_service.setDownloadFlags(
                IDownloaderService.FLAGS_DOWNLOAD_OVER_CELLULAR);
            downloader_service.requestContinueDownload();
            mobile_data_dialog.setVisibility(View.GONE);
        }});

}

/*-----------------------------------------------------------------------*/

@Override
protected void onStart()
{
    super.onStart();
    if (downloader_client_stub != null) {
        downloader_client_stub.connect(this);
    }
}

/*-----------------------------------------------------------------------*/

@Override
protected void onStop()
{
    if (downloader_client_stub != null) {
        downloader_client_stub.disconnect(this);
    }
    super.onStop();
}

/*************************************************************************/
/******************* DownloaderClient callback methods *******************/
/*************************************************************************/

@Override
public void onServiceConnected(Messenger messenger)
{
    /* The sample code for the downloader library calls this logic a
     * "critical implementation detail". */
    downloader_service = DownloaderServiceMarshaller.CreateProxy(messenger);
    downloader_service.onClientUpdated(downloader_client_stub.getMessenger());
}

/*-----------------------------------------------------------------------*/

@Override
public void onDownloadProgress(DownloadProgressInfo progress)
{
    /* Don't use the library's helper because (1) it uses binary units and
     * (2) it doesn't use user-friendly precision. */
    average_speed.setText(prettyPrintBytes(
        progress.mCurrentSpeed /* bytes/msec */ * 1000, true));

    time_left.setText(getString(
        R.string.download_time_left,
        Helpers.getTimeRemaining(progress.mTimeRemaining)));

    /* ProgressBar values must be ints, so shift values down until they fit. */
    long bytes_total = progress.mOverallTotal;
    long bytes_so_far = progress.mOverallProgress;
    while (bytes_total > 2147483647) {
        bytes_total /= 2;
        bytes_so_far /= 2;
    }
    if (bytes_so_far > 0) {
        progress_bar.setMax((int)bytes_total);
        progress_bar.setProgress((int)bytes_so_far);
    } else {
        progress_bar.setIndeterminate(true);
    }
    progress_percent.setText(
        Long.toString((bytes_so_far * 100) / bytes_total) + "%");
    progress_fraction.setText(
        prettyPrintBytes(progress.mOverallProgress, false)
        + " / " +
        prettyPrintBytes(progress.mOverallTotal, false));
}

/*-----------------------------------------------------------------------*/

@Override
public void onDownloadStateChanged(int state)
{
    int stringID;
    switch (state) {
      case STATE_IDLE:
        stringID = R.string.download_state_idle;
        break;
      case STATE_FETCHING_URL:
        stringID = R.string.download_state_fetching_url;
        break;
      case STATE_CONNECTING:
        stringID = R.string.download_state_connecting;
        break;
      case STATE_DOWNLOADING:
        stringID = R.string.download_state_downloading;
        break;
      case STATE_COMPLETED:
        stringID = R.string.download_state_completed;
        break;
      case STATE_PAUSED_NETWORK_UNAVAILABLE:
        stringID = R.string.download_state_paused_network_unavailable;
        break;
      case STATE_PAUSED_BY_REQUEST:
        stringID = R.string.download_state_paused_by_request;
        break;
      case STATE_PAUSED_WIFI_DISABLED_NEED_CELLULAR_PERMISSION:
      case STATE_PAUSED_WIFI_DISABLED:
        stringID = R.string.download_state_paused_wifi_disabled;
        break;
      case STATE_PAUSED_NEED_CELLULAR_PERMISSION:
      case STATE_PAUSED_NEED_WIFI:
        stringID = R.string.download_state_paused_wifi_unavailable;
        break;
      case STATE_PAUSED_ROAMING:
        stringID = R.string.download_state_paused_roaming;
        break;
      case STATE_PAUSED_NETWORK_SETUP_FAILURE:
        stringID = R.string.download_state_paused_network_setup_failure;
        break;
      case STATE_PAUSED_SDCARD_UNAVAILABLE:
        stringID = R.string.download_state_paused_sdcard_unavailable;
        break;
      case STATE_FAILED_UNLICENSED:
        stringID = R.string.download_state_failed_unlicensed;
        break;
      case STATE_FAILED_FETCHING_URL:
        stringID = R.string.download_state_failed_fetching_url;
        break;
      case STATE_FAILED_SDCARD_FULL:
        stringID = R.string.download_state_failed_sdcard_full;
        break;
      case STATE_FAILED_CANCELED:
        stringID = R.string.download_state_failed_canceled;
        break;
      case STATE_FAILED:
        stringID = R.string.download_state_failed;
        break;
      default:
        stringID = Helpers.getDownloaderStringResourceIDFromState(state);
        break;
    }

    status_message.setText(getString(stringID));

    if (state == STATE_COMPLETED) {
        Log.i(Constants.SIL_PLATFORM_ANDROID_DLOG_LOG_TAG,
              "Download completed, starting app...");
        switchToApp();
        return;
    }

    paused = !(state == STATE_IDLE ||
               state == STATE_CONNECTING ||
               state == STATE_FETCHING_URL ||
               state == STATE_DOWNLOADING);
    failed = (state == STATE_FAILED_UNLICENSED ||
              state == STATE_FAILED_FETCHING_URL ||
              state == STATE_FAILED_SDCARD_FULL ||
              state == STATE_FAILED_CANCELED ||
              state == STATE_FAILED);
    updatePauseButtonText();
    if (paused && !failed && state != STATE_COMPLETED) {
        cancel_button.setVisibility(View.VISIBLE);
    } else {
        cancel_button.setVisibility(View.GONE);
    }

    if (state == STATE_PAUSED_NEED_CELLULAR_PERMISSION
     || state == STATE_PAUSED_WIFI_DISABLED_NEED_CELLULAR_PERMISSION) {
        mobile_data_dialog.setVisibility(View.VISIBLE);
    } else {
        mobile_data_dialog.setVisibility(View.GONE);
    }
}

/*************************************************************************/
/************************* Local helper methods **************************/
/*************************************************************************/

/**
 * switchToApp:  Start the main app activity, terminating this one.
 */
private void switchToApp()
{
    startActivity(new Intent().setClassName(
        this, getPackageName() + ".SILActivity"));
    finish();
}

/*-----------------------------------------------------------------------*/

/**
 * updatePauseButtonText:  Set the text on the pause button according to
 * the current pause state.
 */
private void updatePauseButtonText()
{
    if (paused) {
        if (failed) {
            pause_button.setText(R.string.download_button_retry);
        } else {
            pause_button.setText(R.string.download_button_resume);
        }
    } else {
        pause_button.setText(R.string.download_button_pause);
    }
}

/*-----------------------------------------------------------------------*/

/**
 * prettyPrintBytes:  Return a human-friendly string representing the
 * given number of bytes or bytes-per-second.
 *
 * [Parameters]
 *     value: Value to pretty-print.
 *     is_rate: True if value is in units of bytes per second, false if in
 *         units of bytes.
 * [Return value]
 *     Pretty-printed string.
 */
private String prettyPrintBytes(double value, boolean is_rate)
{
    if (is_rate && value > 0 && value < 10) {
        value = 10;  // Don't report less than 0.01 kBps unless stalled.
    }

    int string_k = is_rate ? R.string.download_kBps : R.string.download_kB;
    int string_M = is_rate ? R.string.download_MBps : R.string.download_MB;
    int string_G = is_rate ? R.string.download_GBps : R.string.download_GB;
    if (value < 9995.0) {
        return getString(string_k, String.format("%.2f", value/1e3));
    } else if (value < 99950.0) {
        return getString(string_k, String.format("%.1f", value/1e3));
    } else if (value < 999500.0) {
        return getString(string_k, String.format("%.0f", value/1e3));
    } else if (value < 9995000.0) {
        return getString(string_M, String.format("%.2f", value/1e6));
    } else if (value < 99950000.0) {
        return getString(string_M, String.format("%.1f", value/1e6));
    } else if (value < 999500000.0) {
        return getString(string_M, String.format("%.0f", value/1e6));
    } else if (value < 9995000000.0) {
        return getString(string_G, String.format("%.2f", value/1e9));
    } else if (value < 99950000000.0) {
        return getString(string_G, String.format("%.1f", value/1e9));
    } else {
        return getString(string_G, String.format("%.0f", value/1e9));
    }
}

/*************************************************************************/
/*********************** Downloader service class ************************/
/*************************************************************************/

public static class DownloaderService extends
    com.google.android.vending.expansion.downloader.impl.DownloaderService
{
    public static class AlarmReceiver extends BroadcastReceiver {
        @Override public void onReceive(Context context, Intent intent) {
            try {
                DownloaderClientMarshaller.startDownloadServiceIfRequired(
                    context, intent, DownloaderService.class);
            } catch (NameNotFoundException e) {
                e.printStackTrace();
            }
        }
    }

    @Override public String getPublicKey() {
        return Constants.DOWNLOADER_BASE64_PUBLIC_KEY;
    }

    @Override public byte[] getSALT() {
        return Constants.DOWNLOADER_SALT;
    }

    @Override public String getAlarmReceiverClassName() {
        return AlarmReceiver.class.getName();
    }
}

/*************************************************************************/
/*************************************************************************/

}  // class Downloader
