package com.emgor.thewatch.weather

import android.content.Context
import androidx.work.CoroutineWorker
import androidx.work.Constraints
import androidx.work.ExistingPeriodicWorkPolicy
import androidx.work.NetworkType
import androidx.work.PeriodicWorkRequestBuilder
import androidx.work.WorkManager
import androidx.work.WorkerParameters
import com.emgor.thewatch.ble.BleManager
import com.emgor.thewatch.data.Prefs
import java.util.concurrent.TimeUnit

/**
 * Belt-and-suspenders periodic weather refresh via WorkManager, in addition to
 * the in-service hourly loop. Runs even if the OS trims the process; if the BLE
 * link is up it pushes weather straight to the watch. WorkManager's minimum
 * period is 15 minutes, so we clamp there.
 */
class WeatherWorker(ctx: Context, params: WorkerParameters) : CoroutineWorker(ctx, params) {
    override suspend fun doWork(): Result {
        if (!BleManager.isReady) return Result.success() // nothing to deliver right now
        val ok = WeatherRepository.fetchAndSend(applicationContext) { line ->
            BleManager.sendLine(line)
        }
        return if (ok) Result.success() else Result.retry()
    }

    companion object {
        private const val NAME = "the_watch_weather"

        fun schedule(ctx: Context) {
            val minutes = Prefs.weatherMinutes(ctx).coerceAtLeast(15).toLong()
            val req = PeriodicWorkRequestBuilder<WeatherWorker>(minutes, TimeUnit.MINUTES)
                .setConstraints(
                    Constraints.Builder().setRequiredNetworkType(NetworkType.CONNECTED).build()
                )
                .build()
            WorkManager.getInstance(ctx)
                .enqueueUniquePeriodicWork(NAME, ExistingPeriodicWorkPolicy.UPDATE, req)
        }

        fun cancel(ctx: Context) {
            WorkManager.getInstance(ctx).cancelUniqueWork(NAME)
        }
    }
}
