package com.emgor.thewatch.media

import android.content.Context
import android.media.AudioAttributes
import android.media.AudioManager
import android.media.MediaPlayer
import android.media.RingtoneManager
import android.net.Uri
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.VibrationEffect
import android.os.Vibrator
import android.os.VibratorManager
import android.util.Log

/**
 * "Find my phone": rings the phone LOUD and unmissable on the ALARM stream so it
 * sounds through silent / vibrate / Do Not Disturb. Saves and restores the prior
 * alarm volume, loops an alarm tone, and vibrates. Auto-stops after a safety
 * timeout. Driven by {"t":"findPhone","n":bool} from the watch.
 */
object FindPhone {
    private const val TAG = "TheWatch/Find"
    private const val SAFETY_TIMEOUT_MS = 60_000L

    private var player: MediaPlayer? = null
    private var savedAlarmVolume: Int = -1
    private val main = Handler(Looper.getMainLooper())
    private val stopRunnable = Runnable { stop(null) }

    @Synchronized
    fun start(context: Context) {
        val ctx = context.applicationContext
        if (player != null) return // already ringing
        val am = ctx.getSystemService(Context.AUDIO_SERVICE) as AudioManager

        // Save current alarm volume, then blast it to max.
        savedAlarmVolume = am.getStreamVolume(AudioManager.STREAM_ALARM)
        val max = am.getStreamMaxVolume(AudioManager.STREAM_ALARM)
        runCatching { am.setStreamVolume(AudioManager.STREAM_ALARM, max, 0) }

        val uri: Uri = RingtoneManager.getActualDefaultRingtoneUri(ctx, RingtoneManager.TYPE_ALARM)
            ?: RingtoneManager.getActualDefaultRingtoneUri(ctx, RingtoneManager.TYPE_RINGTONE)
            ?: RingtoneManager.getDefaultUri(RingtoneManager.TYPE_ALARM)

        try {
            player = MediaPlayer().apply {
                setAudioAttributes(
                    AudioAttributes.Builder()
                        .setUsage(AudioAttributes.USAGE_ALARM) // ALARM usage plays through silent/DND
                        .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
                        .build()
                )
                setDataSource(ctx, uri)
                isLooping = true
                prepare()
                start()
            }
        } catch (e: Exception) {
            Log.e(TAG, "ring start failed", e)
        }

        vibrate(ctx)
        main.removeCallbacks(stopRunnable)
        main.postDelayed(stopRunnable, SAFETY_TIMEOUT_MS)
        Log.i(TAG, "ringing (savedAlarmVol=$savedAlarmVolume)")
    }

    @Synchronized
    fun stop(context: Context?) {
        main.removeCallbacks(stopRunnable)
        player?.let { runCatching { it.stop() }; runCatching { it.release() } }
        player = null
        // Restore prior alarm volume.
        val ctx = context?.applicationContext
        if (ctx != null && savedAlarmVolume >= 0) {
            val am = ctx.getSystemService(Context.AUDIO_SERVICE) as AudioManager
            runCatching { am.setStreamVolume(AudioManager.STREAM_ALARM, savedAlarmVolume, 0) }
        }
        savedAlarmVolume = -1
        stopVibrate(ctx)
        Log.i(TAG, "stopped")
    }

    private fun vibrator(ctx: Context): Vibrator? =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            val vm = ctx.getSystemService(Context.VIBRATOR_MANAGER_SERVICE) as? VibratorManager
            vm?.defaultVibrator
        } else {
            @Suppress("DEPRECATION")
            ctx.getSystemService(Context.VIBRATOR_SERVICE) as? Vibrator
        }

    private fun vibrate(ctx: Context) {
        val v = vibrator(ctx) ?: return
        val pattern = longArrayOf(0, 600, 400) // wait, buzz, gap — repeats
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            v.vibrate(VibrationEffect.createWaveform(pattern, 0))
        } else {
            @Suppress("DEPRECATION")
            v.vibrate(pattern, 0)
        }
    }

    private fun stopVibrate(ctx: Context?) {
        ctx ?: return
        runCatching { vibrator(ctx)?.cancel() }
    }
}
