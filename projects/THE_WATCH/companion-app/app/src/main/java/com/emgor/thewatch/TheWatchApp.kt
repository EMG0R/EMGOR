package com.emgor.thewatch

import android.app.Application
import android.app.NotificationChannel
import android.app.NotificationManager
import android.os.Build

class TheWatchApp : Application() {
    override fun onCreate() {
        super.onCreate()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val nm = getSystemService(NotificationManager::class.java)
            val ch = NotificationChannel(
                CHANNEL_SERVICE,
                getString(R.string.notif_channel_service),
                NotificationManager.IMPORTANCE_LOW
            ).apply {
                description = getString(R.string.notif_channel_service_desc)
                setShowBadge(false)
            }
            nm.createNotificationChannel(ch)

            // High-importance channel for the camera full-screen launch intent.
            // Must be IMPORTANCE_HIGH for the system to honour setFullScreenIntent().
            val cam = NotificationChannel(
                CHANNEL_CAMERA,
                getString(R.string.notif_channel_camera),
                NotificationManager.IMPORTANCE_HIGH
            ).apply {
                description = getString(R.string.notif_channel_camera_desc)
                setShowBadge(false)
            }
            nm.createNotificationChannel(cam)
        }
    }

    companion object {
        const val CHANNEL_SERVICE = "watch_link"
        const val CHANNEL_CAMERA = "watch_camera"
    }
}
