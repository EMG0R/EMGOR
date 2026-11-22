package com.emgor.thewatch.service

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.provider.CalendarContract
import androidx.core.content.ContextCompat
import com.emgor.thewatch.ble.GbProtocol

/** Reads the next upcoming calendar event and builds a calendar line. */
object CalendarReporter {

    private const val WINDOW_MS = 14L * 24 * 60 * 60 * 1000 // look ahead 14 days

    fun line(context: Context): String? {
        val ctx = context.applicationContext
        if (ContextCompat.checkSelfPermission(ctx, Manifest.permission.READ_CALENDAR)
            != PackageManager.PERMISSION_GRANTED
        ) return null

        val now = System.currentTimeMillis()
        val end = now + WINDOW_MS
        val builder = CalendarContract.Instances.CONTENT_URI.buildUpon()
        builder.appendPath(now.toString())
        builder.appendPath(end.toString())
        val projection = arrayOf(
            CalendarContract.Instances.TITLE,
            CalendarContract.Instances.BEGIN,
            CalendarContract.Instances.EVENT_LOCATION,
            CalendarContract.Instances.ALL_DAY
        )
        // Only events that have not yet started; earliest first.
        val selection = "${CalendarContract.Instances.BEGIN} >= ?"
        val args = arrayOf(now.toString())
        val sort = "${CalendarContract.Instances.BEGIN} ASC"

        return try {
            ctx.contentResolver.query(builder.build(), projection, selection, args, sort)
                ?.use { c ->
                    if (!c.moveToFirst()) return null
                    val title = c.getString(0) ?: "(no title)"
                    val beginMs = c.getLong(1)
                    val loc = c.getString(2) ?: ""
                    GbProtocol.calendarLine(
                        title = title,
                        startEpochSeconds = beginMs / 1000L,
                        location = loc
                    )
                }
        } catch (e: Exception) {
            null
        }
    }
}
