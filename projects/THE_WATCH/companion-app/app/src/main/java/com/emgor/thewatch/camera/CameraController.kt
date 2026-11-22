package com.emgor.thewatch.camera

import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.provider.Settings
import android.util.Log
import android.widget.Toast
import androidx.core.app.NotificationCompat
import com.emgor.thewatch.R
import com.emgor.thewatch.TheWatchApp

/**
 * Bridges watch BLE camera commands to the VISIBLE [CameraActivity].
 *
 * Android 14 reality: CAMERA is a "while-in-use" permission — an app can only
 * capture while it is visibly foreground. A headless/background capture (a
 * camera-typed FGS or a service-owned CameraX) is architecturally impossible and
 * silently fails. So EVERY camera command goes through the app's own visible
 * CameraX Activity, which we launch from the background using the
 * SYSTEM_ALERT_WINDOW ("Display over other apps") background-activity-launch
 * exemption, and we capture IN that visible Activity.
 *
 * Commands from the watch (bare JSON on the NUS TX characteristic):
 *   {"t":"camera","n":"open"}     -> start/bring up the visible CameraActivity
 *   {"t":"camera","n":"capture"}  -> capture in the current PHOTO/VIDEO mode:
 *        - if CameraActivity is already foreground, fire the shutter directly;
 *        - otherwise launch CameraActivity with auto_shutter=true so it captures
 *          as soon as the preview is bound and 3A has settled.
 * Back-compat (older firmware): "shutter" = photo, "video" = toggle video,
 * "mode" = flip photo/video. The PHOTO/VIDEO mode lives in the Activity UI (see
 * [videoMode], toggled by the on-screen toggle) — the watch only says "capture".
 */
object CameraController {

    private const val TAG = "TheWatch/Cam"
    private const val FSI_NOTIF_ID = 4242
    private const val OVERLAY_PROMPT_NOTIF_ID = 4243

    const val EXTRA_CMD = "cmd"
    const val EXTRA_AUTO_SHUTTER = "auto_shutter"
    const val CMD_OPEN = "open"
    const val CMD_CAPTURE = "capture"
    // Back-compat aliases (older watch firmware).
    const val CMD_SHUTTER = "shutter"
    const val CMD_VIDEO = "video"
    const val CMD_MODE = "mode"

    /** Canonical PHOTO/VIDEO mode. Source of truth for what `capture` does.
     *  Written by the Activity's on-screen toggle; read for state reports. */
    @Volatile var videoMode = false

    /** True while [CameraActivity] is resumed/visible (has a live preview). */
    @Volatile var activityActive = false

    /** Set by WatchService: forwards a phone->watch cameraState line to the link. */
    @Volatile var onState: ((line: String) -> Unit)? = null

    private val valid = setOf(CMD_OPEN, CMD_CAPTURE, CMD_SHUTTER, CMD_VIDEO, CMD_MODE)

    /**
     * Handle one {"t":"camera","n":...} command from the watch.
     *
     * Launch priority (see FIX 1):
     *   1. Activity already foreground -> deliver command straight to it.
     *   2. Overlay ("Display over other apps") granted -> directly startActivity.
     *      This is the RELIABLE path; we do NOT post a full-screen-intent notif here.
     *   3. Overlay denied (last resort) -> try startActivity anyway, post the
     *      full-screen-intent notification (lock-screen fallback), AND loudly
     *      prompt the user to grant overlay so the camera can open reliably.
     */
    fun handleCommand(context: Context, name: String?) {
        val cmd = name ?: return
        if (cmd !in valid) { Log.w(TAG, "ignoring unknown camera cmd=$cmd"); return }
        val canOverlay = CameraPermissions.canDrawOverlays(context)
        val foreground = activityActive
        Log.i(TAG, "command received cmd=$cmd (foreground=$foreground " +
            "videoMode=$videoMode canDrawOverlays=$canOverlay)")

        // A capture-style command needs the auto-shutter only when we have to bring
        // the Activity up cold; if it is already foreground it fires immediately.
        val captureCmd = cmd == CMD_CAPTURE || cmd == CMD_SHUTTER || cmd == CMD_VIDEO

        // --- Reliable path: foreground OR overlay granted -> direct startActivity ---
        if (foreground || canOverlay) {
            val reason = if (foreground) "foreground" else "overlay-granted"
            Log.i(TAG, "launch decision=DIRECT ($reason) -> startActivity(CameraActivity), no FSI")
            startCameraActivity(context, cmd, autoShutter = !foreground && captureCmd)
            return
        }

        // --- Last-resort path: overlay denied AND not foreground ---
        // The direct BAL is likely blocked, so try it, then fall back to the
        // full-screen-intent notification (lock-screen only) and, crucially, prompt
        // the user to grant overlay — the camera can't reliably open without it.
        Log.w(TAG, "launch decision=FALLBACK (overlay denied, background) -> " +
            "attempt startActivity + full-screen-intent notif + loud overlay prompt")
        val intent = cameraIntent(context, cmd, autoShutter = captureCmd)
        val launched = runCatching { context.applicationContext.startActivity(intent) }
            .onFailure { Log.w(TAG, "direct startActivity blocked (expected, no overlay)", it) }
            .isSuccess
        Log.i(TAG, "fallback direct startActivity launched=$launched")
        launchViaFullScreenIntent(context.applicationContext, intent)
        promptEnableOverlay(context.applicationContext)
    }

    // ---- Activity launch (background-activity-launch via SYSTEM_ALERT_WINDOW) --

    private fun cameraIntent(context: Context, cmd: String, autoShutter: Boolean): Intent =
        Intent(context.applicationContext, CameraActivity::class.java).apply {
            putExtra(EXTRA_CMD, cmd)
            putExtra(EXTRA_AUTO_SHUTTER, autoShutter)
            // NEW_TASK is required to start an Activity from a non-Activity context;
            // the Activity is singleTask so repeats route via onNewIntent.
            addFlags(
                Intent.FLAG_ACTIVITY_NEW_TASK or
                    Intent.FLAG_ACTIVITY_REORDER_TO_FRONT
            )
        }

    /** The reliable path: direct startActivity, no full-screen-intent notification. */
    private fun startCameraActivity(context: Context, cmd: String, autoShutter: Boolean) {
        val intent = cameraIntent(context, cmd, autoShutter)
        val launched = runCatching { context.applicationContext.startActivity(intent) }
            .onFailure { Log.e(TAG, "startActivity failed unexpectedly", it) }
            .isSuccess
        Log.i(TAG, "startCameraActivity cmd=$cmd autoShutter=$autoShutter launched=$launched")
        // Belt-and-braces: if the direct launch somehow failed even with overlay/
        // foreground, drop to the FSI notif + overlay prompt so the user isn't stuck.
        if (!launched) {
            Log.w(TAG, "direct launch failed -> FSI fallback + overlay prompt")
            launchViaFullScreenIntent(context.applicationContext, intent)
            if (!CameraPermissions.canDrawOverlays(context)) promptEnableOverlay(context.applicationContext)
        }
    }

    /**
     * Loudly prompt the user to grant "Display over other apps" — without it the
     * watch can't reliably open the camera from the background. Posts a high-
     * importance notification that deep-links to the overlay settings, and also
     * attempts a toast.
     */
    private fun promptEnableOverlay(appCtx: Context) {
        // Best-effort toast (may be suppressed in the background on some OS builds).
        runCatching {
            android.os.Handler(android.os.Looper.getMainLooper()).post {
                Toast.makeText(
                    appCtx,
                    "Enable \"Display over other apps\" so the watch can open the camera",
                    Toast.LENGTH_LONG
                ).show()
            }
        }
        try {
            val settings = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                Intent(
                    Settings.ACTION_MANAGE_OVERLAY_PERMISSION,
                    Uri.parse("package:${appCtx.packageName}")
                ).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            } else {
                appCtx.packageManager.getLaunchIntentForPackage(appCtx.packageName)
                    ?: Intent()
            }
            val flags = PendingIntent.FLAG_UPDATE_CURRENT or
                (if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) PendingIntent.FLAG_IMMUTABLE else 0)
            val pi = PendingIntent.getActivity(appCtx, 1, settings, flags)
            val notif = NotificationCompat.Builder(appCtx, TheWatchApp.CHANNEL_CAMERA)
                .setContentTitle("Watch camera can't open")
                .setContentText("Tap to allow \"Display over other apps\" so a watch tap can open the camera.")
                .setStyle(
                    NotificationCompat.BigTextStyle().bigText(
                        "The watch tried to open the camera, but \"Display over other apps\" " +
                            "is off. Tap here to enable it — then the camera will open " +
                            "reliably on every watch tap."
                    )
                )
                .setSmallIcon(R.drawable.ic_stat_watch)
                .setPriority(NotificationCompat.PRIORITY_HIGH)
                .setContentIntent(pi)
                .setAutoCancel(true)
                .build()
            val nm = appCtx.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
            nm.notify(OVERLAY_PROMPT_NOTIF_ID, notif)
            Log.i(TAG, "posted overlay-permission prompt notification")
        } catch (e: Exception) {
            Log.e(TAG, "overlay prompt failed", e)
        }
    }

    private fun launchViaFullScreenIntent(appCtx: Context, activityIntent: Intent) {
        try {
            val flags = PendingIntent.FLAG_UPDATE_CURRENT or
                (if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) PendingIntent.FLAG_IMMUTABLE else 0)
            val pi = PendingIntent.getActivity(appCtx, 0, activityIntent, flags)
            val notif = NotificationCompat.Builder(appCtx, TheWatchApp.CHANNEL_CAMERA)
                .setContentTitle("Watch camera")
                .setContentText("Opening camera…")
                .setSmallIcon(R.drawable.ic_stat_watch)
                .setPriority(NotificationCompat.PRIORITY_HIGH)
                .setCategory(NotificationCompat.CATEGORY_CALL)
                .setFullScreenIntent(pi, true)
                .setContentIntent(pi)
                .setAutoCancel(true)
                .setTimeoutAfter(5_000)
                .build()
            val nm = appCtx.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
            nm.notify(FSI_NOTIF_ID, notif)
        } catch (e: Exception) {
            Log.e(TAG, "full-screen intent launch failed", e)
        }
    }

    /** Called by CameraActivity on state change; relays to the watch. */
    fun reportState(open: Boolean, videoMode: Boolean, recording: Boolean) {
        val line = com.emgor.thewatch.ble.GbProtocol.cameraStateLine(open, videoMode, recording)
        runCatching { onState?.invoke(line) }.onFailure { Log.e(TAG, "reportState", it) }
    }

    /** Dismiss the transient full-screen launch notification (called once shown). */
    fun clearLaunchNotification(context: Context) {
        runCatching {
            val nm = context.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
            nm.cancel(FSI_NOTIF_ID)
        }
    }
}
