package com.emgor.thewatch.service

import android.app.Notification
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.util.Log
import androidx.core.app.NotificationCompat
import androidx.lifecycle.LifecycleService
import androidx.lifecycle.lifecycleScope
import com.emgor.thewatch.MainActivity
import com.emgor.thewatch.R
import com.emgor.thewatch.TheWatchApp
import com.emgor.thewatch.ble.BleManager
import com.emgor.thewatch.ble.GbProtocol
import com.emgor.thewatch.camera.CameraController
import com.emgor.thewatch.data.Prefs
import com.emgor.thewatch.media.FindPhone
import com.emgor.thewatch.media.MediaMonitor
import com.emgor.thewatch.media.SleepTimer
import com.emgor.thewatch.weather.WeatherRepository
import com.emgor.thewatch.weather.WeatherWorker
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

/**
 * Persistent foreground service. Owns the BLE link (via BleManager), routes
 * inbound watch messages, and periodically pushes time / weather / battery /
 * calendar. Runs as a connectedDevice foreground service so Android keeps it alive.
 */
class WatchService : LifecycleService() {

    private val main = Handler(Looper.getMainLooper())
    private lateinit var media: MediaMonitor
    private lateinit var sleepTimer: SleepTimer
    private var ticker: Job? = null

    // Last-sent timestamps (ms) for the periodic scheduler.
    private var lastWeather = 0L
    private var lastTime = 0L
    private var lastBattery = 0L
    private var lastCalendar = 0L

    override fun onCreate() {
        super.onCreate()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            startForeground(
                NOTIF_ID, buildNotification(),
                ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE
            )
        } else {
            startForeground(NOTIF_ID, buildNotification())
        }

        BleManager.init(this)
        media = MediaMonitor(this) { line -> BleManager.sendLine(line) }
        BleManager.onInbound = { msg -> main.post { handleInbound(msg) } }
        BleManager.onReady = { main.post { onLinkReady() } }
        // Camera screen reports its live state back to the watch through the link.
        // All capture happens in the VISIBLE CameraActivity (Android 14: CAMERA is
        // a while-in-use permission) — no headless/service capture path exists.
        CameraController.onState = { line -> BleManager.sendLine(line) }

        // Audiobook sleep timer: counts down, pauses the active media session on
        // elapse, and streams a live sleepState countdown back to the watch.
        sleepTimer = SleepTimer(
            onPause = { media.pauseActive() },
            onTick = { active, remaining ->
                BleManager.sendLine(GbProtocol.sleepStateLine(active, remaining))
            }
        )

        // Reflect connection state in the ongoing notification.
        lifecycleScope.launch {
            BleManager.state.collect { updateNotification() }
        }

        media.start()
        BleManager.start()
        WeatherWorker.schedule(this)
        startTicker()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        super.onStartCommand(intent, flags, startId)
        return START_STICKY // keep running / restart if killed
    }

    override fun onDestroy() {
        ticker?.cancel()
        media.stop()
        BleManager.onInbound = null
        BleManager.onReady = null
        CameraController.onState = null
        runCatching { sleepTimer.stop() }
        FindPhone.stop(this)
        super.onDestroy()
    }

    // ---- inbound routing (watch -> phone) -----------------------------------

    private fun handleInbound(msg: GbProtocol.Inbound) {
        when (msg.type) {
            "music" -> media.handleCommand(msg.json.optString("n"))
            "camera" -> CameraController.handleCommand(this, msg.json.optString("n"))
            "sleeptimer" -> {
                // {"t":"sleeptimer","n":<minutes>} — n>0 start/restart, n==0 cancel.
                val minutes = msg.json.optInt("n", 10)
                Log.i("TheWatch/Sleep", "sleeptimer cmd minutes=$minutes")
                sleepTimer.handle(minutes)
            }
            "findPhone" -> {
                if (msg.json.optBoolean("n")) FindPhone.start(this) else FindPhone.stop(this)
            }
            "notify" -> {
                // {"t":"notify","n":"DISMISS"|"REPLY","id":..,"msg":..}
                val n = msg.json.optString("n")
                val id = msg.json.optInt("id", 0)
                if (n == "DISMISS") WatchNotificationListener.dismissById(id)
                // REPLY would require a RemoteInput action; left for a follow-up.
            }
            else -> Log.d(TAG, "unhandled inbound type=${msg.type}")
        }
    }

    // ---- on link ready: push a full snapshot --------------------------------

    private fun onLinkReady() {
        val now = System.currentTimeMillis()
        BleManager.sendLine(GbProtocol.setTimeLine())
        lastTime = now
        // Backfill the watch with the phone's currently-active notifications so it
        // matches the phone immediately on connect (not just future live posts).
        WatchNotificationListener.resync()
        media.pushNow()
        sendBattery(); lastBattery = now
        sendCalendar(); lastCalendar = now
        // Weather off the main thread (network + geocode).
        lifecycleScope.launch(Dispatchers.IO) {
            WeatherRepository.fetchAndSend(this@WatchService) { BleManager.sendLine(it) }
        }
        lastWeather = now
    }

    // ---- periodic scheduler --------------------------------------------------

    private fun startTicker() {
        ticker?.cancel()
        ticker = lifecycleScope.launch {
            while (isActive) {
                if (BleManager.isReady) tick()
                delay(60_000)
            }
        }
    }

    private fun tick() {
        val now = System.currentTimeMillis()
        val weatherMs = Prefs.weatherMinutes(this).coerceAtLeast(15) * 60_000L
        if (now - lastWeather >= weatherMs) {
            lastWeather = now
            lifecycleScope.launch(Dispatchers.IO) {
                WeatherRepository.fetchAndSend(this@WatchService) { BleManager.sendLine(it) }
            }
        }
        if (now - lastTime >= 60 * 60_000L) { lastTime = now; BleManager.sendLine(GbProtocol.setTimeLine()) }
        if (now - lastBattery >= 10 * 60_000L) { lastBattery = now; sendBattery() }
        if (now - lastCalendar >= 30 * 60_000L) { lastCalendar = now; sendCalendar() }
    }

    private fun sendBattery() {
        if (!Prefs.sendBattery(this)) return
        BatteryReporter.line(this)?.let { BleManager.sendLine(it) }
    }

    private fun sendCalendar() {
        if (!Prefs.sendCalendar(this)) return
        CalendarReporter.line(this)?.let { BleManager.sendLine(it) }
    }

    // ---- foreground notification --------------------------------------------

    private fun buildNotification(): Notification {
        val pi = PendingIntent.getActivity(
            this, 0, Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT
        )
        val text = when (BleManager.state.value) {
            BleManager.State.READY -> "Connected to ${BleManager.deviceLabel.value ?: "watch"}"
            BleManager.State.CONNECTED -> "Connecting…"
            BleManager.State.CONNECTING -> "Connecting…"
            BleManager.State.SCANNING -> "Searching for watch…"
            BleManager.State.IDLE -> "Idle"
        }
        return NotificationCompat.Builder(this, TheWatchApp.CHANNEL_SERVICE)
            .setContentTitle("The Watch")
            .setContentText(text)
            .setSmallIcon(R.drawable.ic_stat_watch)
            .setOngoing(true)
            .setContentIntent(pi)
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .build()
    }

    private fun updateNotification() {
        val nm = getSystemService(Context.NOTIFICATION_SERVICE) as android.app.NotificationManager
        nm.notify(NOTIF_ID, buildNotification())
    }

    companion object {
        private const val TAG = "TheWatch/Svc"
        private const val NOTIF_ID = 42

        fun start(ctx: Context) {
            val i = Intent(ctx, WatchService::class.java)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                ctx.startForegroundService(i)
            } else {
                ctx.startService(i)
            }
        }

        fun stop(ctx: Context) {
            ctx.stopService(Intent(ctx, WatchService::class.java))
        }
    }
}
