package com.emgor.thewatch.camera

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import android.provider.Settings
import androidx.core.content.ContextCompat

/** Permission checks specific to the app-hosted camera feature. */
object CameraPermissions {

    fun hasCamera(ctx: Context): Boolean =
        granted(ctx, Manifest.permission.CAMERA)

    fun hasAudio(ctx: Context): Boolean =
        granted(ctx, Manifest.permission.RECORD_AUDIO)

    /** True if we can save captures to MediaStore (Android 10+ scoped storage: always). */
    fun hasStorage(ctx: Context): Boolean =
        Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q ||
            granted(ctx, Manifest.permission.WRITE_EXTERNAL_STORAGE)

    /** CAMERA is mandatory; RECORD_AUDIO/storage are handled gracefully if missing. */
    fun ready(ctx: Context): Boolean = hasCamera(ctx)

    /** "Display over other apps" — the reliable background-activity-launch exemption. */
    fun canDrawOverlays(ctx: Context): Boolean =
        Build.VERSION.SDK_INT < Build.VERSION_CODES.M || Settings.canDrawOverlays(ctx)

    /** Runtime permissions requested together by the camera "Grant" button. */
    fun runtimePermsToRequest(): Array<String> {
        val list = mutableListOf(
            Manifest.permission.CAMERA,
            Manifest.permission.RECORD_AUDIO,
        )
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.P) {
            list.add(Manifest.permission.WRITE_EXTERNAL_STORAGE)
        }
        return list.toTypedArray()
    }

    private fun granted(ctx: Context, perm: String): Boolean =
        ContextCompat.checkSelfPermission(ctx, perm) == PackageManager.PERMISSION_GRANTED
}
