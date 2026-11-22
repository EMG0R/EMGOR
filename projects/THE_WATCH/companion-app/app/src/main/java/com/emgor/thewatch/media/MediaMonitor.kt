package com.emgor.thewatch.media

import android.content.ComponentName
import android.content.Context
import android.media.AudioManager
import android.media.MediaMetadata
import android.media.session.MediaController
import android.media.session.MediaSessionManager
import android.media.session.PlaybackState
import android.util.Log
import com.emgor.thewatch.ble.GbProtocol
import com.emgor.thewatch.service.WatchNotificationListener
import java.util.Locale

/**
 * Bridges the phone's active media session to the watch:
 *  - OUT: pushes musicinfo (title/artist/album) + musicstate (play/pause/pos).
 *  - IN : executes {"t":"music","n":...} commands from the watch on the active
 *         session (play/pause/next/previous/volumeup/volumedown).
 *
 * Uses MediaSessionManager, which requires the app to hold Notification Access
 * (the same grant the NotificationListenerService uses).
 */
class MediaMonitor(
    private val context: Context,
    private val send: (String) -> Unit
) {
    private val appCtx = context.applicationContext
    private val msm =
        appCtx.getSystemService(Context.MEDIA_SESSION_SERVICE) as MediaSessionManager
    private val listenerComponent =
        ComponentName(appCtx, WatchNotificationListener::class.java)

    private var controller: MediaController? = null

    private val sessionsListener =
        MediaSessionManager.OnActiveSessionsChangedListener { list -> rebind(list) }

    private val controllerCallback = object : MediaController.Callback() {
        override fun onMetadataChanged(metadata: MediaMetadata?) { pushInfo(); pushState() }
        override fun onPlaybackStateChanged(state: PlaybackState?) { pushState() }
        override fun onSessionDestroyed() { rebind(null) }
    }

    fun start() {
        try {
            msm.addOnActiveSessionsChangedListener(sessionsListener, listenerComponent)
            rebind(msm.getActiveSessions(listenerComponent))
        } catch (e: SecurityException) {
            Log.w(TAG, "no notification access yet for media sessions", e)
        }
    }

    fun stop() {
        runCatching { msm.removeOnActiveSessionsChangedListener(sessionsListener) }
        controller?.unregisterCallback(controllerCallback)
        controller = null
    }

    /** Push current now-playing (called on connect/ready too). */
    fun pushNow() { pushInfo(); pushState() }

    /**
     * Pause the active media session — used by the sleep timer when it elapses.
     * No-op (logged) if there is no active session.
     */
    fun pauseActive() {
        val tc = controller?.transportControls
        if (tc == null) { Log.w(TAG, "sleep timer fired but no active media session to pause"); return }
        Log.i(TAG, "sleep timer: pausing active media session")
        tc.pause()
    }

    private fun rebind(list: List<MediaController>?) {
        controller?.unregisterCallback(controllerCallback)
        controller = pickBest(list)
        controller?.registerCallback(controllerCallback)
        pushInfo(); pushState()
    }

    private fun pickBest(list: List<MediaController>?): MediaController? {
        if (list.isNullOrEmpty()) return null
        // Prefer a session that is actively playing; else the first.
        return list.firstOrNull {
            it.playbackState?.state == PlaybackState.STATE_PLAYING
        } ?: list.first()
    }

    private fun pushInfo() {
        val c = controller ?: return
        val md = c.metadata ?: return
        val title = md.getString(MediaMetadata.METADATA_KEY_TITLE) ?: ""
        val artist = md.getString(MediaMetadata.METADATA_KEY_ARTIST)
            ?: md.getString(MediaMetadata.METADATA_KEY_ALBUM_ARTIST) ?: ""
        val album = md.getString(MediaMetadata.METADATA_KEY_ALBUM) ?: ""
        if (title.isEmpty() && artist.isEmpty()) return
        send(
            GbProtocol.musicInfoLine(
                artist = artist,
                album = album,
                track = title,
                audiobook = isAudiobook(c)
            )
        )
    }

    /**
     * True when the active media session belongs to an audiobook/podcast app.
     * Primary signal is the controller's package name (allowlist + substring
     * hints); Audible is the headline case. Metadata can't be trusted across
     * apps, so we key off the package which is stable and cheap.
     */
    private fun isAudiobook(c: MediaController): Boolean {
        val pkg = c.packageName?.lowercase(Locale.US) ?: return false
        if (pkg in AUDIOBOOK_PACKAGES) return true
        return AUDIOBOOK_HINTS.any { pkg.contains(it) }
    }

    private fun pushState() {
        val ps = controller?.playbackState ?: return
        val playing = ps.state == PlaybackState.STATE_PLAYING
        val posSec = (ps.position / 1000L).toInt().coerceAtLeast(0)
        send(GbProtocol.musicStateLine(playing = playing, positionSec = posSec))
    }

    // ---- incoming control from the watch ------------------------------------

    fun handleCommand(n: String) {
        val tc = controller?.transportControls
        when (n) {
            "play" -> tc?.play()
            "pause" -> tc?.pause()
            "next" -> tc?.skipToNext()
            "previous" -> tc?.skipToPrevious()
            "rewind" -> seekRelative(-30_000L)
            "forward" -> seekRelative(30_000L)
            "volumeup" -> adjustVolume(true)
            "volumedown" -> adjustVolume(false)
            else -> Log.w(TAG, "unknown music cmd: $n")
        }
    }

    /**
     * Seek the active session by [deltaMs] (negative = back). Prefers precise
     * seekTo(pos + delta) clamped to >= 0; falls back to the session's advertised
     * ACTION_REWIND / ACTION_FAST_FORWARD if it does not support seekTo.
     */
    private fun seekRelative(deltaMs: Long) {
        val c = controller ?: run { Log.w(TAG, "seek: no active session"); return }
        val tc = c.transportControls
        val ps = c.playbackState
        val actions = ps?.actions ?: 0L
        val canSeek = actions and PlaybackState.ACTION_SEEK_TO != 0L
        when {
            ps != null && canSeek -> {
                val target = (ps.position + deltaMs).coerceAtLeast(0L)
                tc.seekTo(target)
            }
            deltaMs < 0 && actions and PlaybackState.ACTION_REWIND != 0L -> tc.rewind()
            deltaMs > 0 && actions and PlaybackState.ACTION_FAST_FORWARD != 0L -> tc.fastForward()
            ps != null -> {
                // Best-effort seek even if the flag isn't advertised.
                tc.seekTo((ps.position + deltaMs).coerceAtLeast(0L))
            }
            else -> Log.w(TAG, "seek: no playback state")
        }
    }

    private fun adjustVolume(up: Boolean) {
        val am = appCtx.getSystemService(Context.AUDIO_SERVICE) as AudioManager
        val dir = if (up) AudioManager.ADJUST_RAISE else AudioManager.ADJUST_LOWER
        am.adjustStreamVolume(AudioManager.STREAM_MUSIC, dir, AudioManager.FLAG_SHOW_UI)
    }

    companion object {
        private const val TAG = "TheWatch/Media"

        /** Known audiobook/podcast player package names (exact match). */
        private val AUDIOBOOK_PACKAGES = setOf(
            "com.audible.application",              // Audible
            "com.audible.playstore",               // Audible (some regions)
            "com.google.android.apps.books",       // Play Books (audiobooks)
            "com.google.android.apps.podcasts",    // Google Podcasts
            "com.storytel.reader",                 // Storytel
            "com.libro.fm",                        // Libro.fm
            "com.overdrive.mobile.android.libby",  // Libby (OverDrive)
            "au.com.shiftyjelly.pocketcasts",      // Pocket Casts
            "com.bambuna.podcastaddict",           // Podcast Addict
            "de.danoeh.antennapod",                // AntennaPod
            "com.podcast.podcasts",                // Podcasts
            "fm.castbox.audiobook.radio.podcast",  // Castbox
            "com.acast.player",                    // Acast
            "com.spotify.podcast",                 // Spotify podcast host (rare)
        )

        /** Substring hints for packages not explicitly listed above. */
        private val AUDIOBOOK_HINTS = listOf(
            "audible", "audiobook", "podcast", "librivox",
            "storytel", "overdrive", "libby", "antennapod", "pocketcast"
        )
    }
}
