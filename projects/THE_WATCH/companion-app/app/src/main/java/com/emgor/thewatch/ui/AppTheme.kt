package com.emgor.thewatch.ui

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

// Matches the watch theme: deep dark blue-violet bg + bright purple accents.
private val Purple = Color(0xFFB14DFF)        // bright purple accent (watch hand color)
private val PurpleBright = Color(0xFFC77DFF)  // lighter purple
private val BgViolet = Color(0xFF0A0820)      // deep dark violet (watch THEME_COL_BG)
private val SurfaceViolet = Color(0xFF1A0630) // slightly lifted surface
private val OnViolet = Color(0xFFFFFFFF)      // white text

private val PurpleDark = darkColorScheme(
    primary = Purple,
    onPrimary = Color(0xFF0A0820),
    secondary = PurpleBright,
    onSecondary = Color(0xFF0A0820),
    background = BgViolet,
    surface = SurfaceViolet,
    surfaceVariant = Color(0xFF28104E),
    onBackground = OnViolet,
    onSurface = OnViolet,
    onSurfaceVariant = Color(0xFFCBB6E6),
)

@Composable
fun AppTheme(content: @Composable () -> Unit) {
    // Always dark purple/violet, regardless of system light/dark.
    MaterialTheme(colorScheme = PurpleDark, content = content)
}
