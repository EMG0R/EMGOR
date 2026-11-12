package com.emgor.thewatch.media

import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.util.Log

/**
 * Generic audiobook/media sleep timer. Audible's own in-app sleep timer can't be
 * set externally (no API), so the timer lives here: on a
 * {"t":"sleeptimer","n":<minutes>} command from the watch we count down and, when
 * it elapses, PAUSE the active media session (via [onPause]).
 *
 * While active it emits a sleepState line (see [onTick]) every ~[TICK_MS] so the
 * watch can show a live countdown, plus on start and on cancel/fire.
 *
 * Semantics of the inbound `n` (minutes):
 *   n  > 0  -> start, or restart-to-new-value if already running
 *   n == 0  -> cancel (stop the countdown; do NOT pause)
 *
 * Runs on the main thread via a Handler; owned by the foreground service so it
 * survives as long as the link does.
 */
class SleepTimer(
    private val onPause: () -> Unit,
    private val onTick: (active: Boolean, remainingSeconds: Int) -> Unit,
) {
    private val handler = Handler(Looper.getMainLooper())
    private var deadlineElapsed = 0L
    @Volatile var active = false
        private set

    private val tickRunnable = object : Runnable {
        override fun run() {
            if (!active) return
            val remaining = remainingSeconds()
            if (remaining <= 0) { fire(); return }
            onTick(true, remaining)
            handler.postDelayed(this, TICK_MS)
        }
    }

    /** Handle a {"t":"sleeptimer","n":<minutes>} command. */
    fun handle(minutes: Int) {
        if (minutes <= 0) { cancel(); return }
        start(minutes)
    }

    fun start(minutes: Int) {
        handler.removeCallbacks(tickRunnable)
        active = true
        deadlineElapsed = SystemClock.elapsedRealtime() + minutes * 60_000L
        Log.i(TAG, "start: $minutes min (remaining=${remainingSeconds()}s)")
        onTick(true, remainingSeconds())
        handler.postDelayed(tickRunnable, TICK_MS)
    }

    fun cancel() {
        if (!active) { Log.i(TAG, "cancel: no timer running"); return }
        Log.i(TAG, "cancel")
        active = false
        handler.removeCallbacks(tickRunnable)
        onTick(false, 0)
    }

    fun stop() {
        active = false
        handler.removeCallbacks(tickRunnable)
    }

    private fun fire() {
        Log.i(TAG, "fired: pausing media")
        active = false
        handler.removeCallbacks(tickRunnable)
        runCatching { onPause() }.onFailure { Log.e(TAG, "onPause failed", it) }
        onTick(false, 0)
    }

    private fun remainingSeconds(): Int {
        val ms = deadlineElapsed - SystemClock.elapsedRealtime()
        return (ms / 1000L).toInt().coerceAtLeast(0)
    }

    companion object {
        private const val TAG = "TheWatch/Sleep"
        private const val TICK_MS = 5_000L
    }
}
