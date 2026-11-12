package com.emgor.thewatch.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material.icons.filled.Warning
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.emgor.thewatch.ble.BleManager
import com.emgor.thewatch.data.Prefs
import com.emgor.thewatch.weather.WeatherWorker

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun HomeScreen(
    refreshKey: Int,
    onOpenFilter: () -> Unit,
    onGrantNotificationAccess: () -> Unit,
    onGrantRuntime: () -> Unit,
    onGrantBattery: () -> Unit,
    onGrantDnd: () -> Unit,
    onGrantCamera: () -> Unit,
    onGrantOverlay: () -> Unit,
    onRestartService: () -> Unit,
) {
    val ctx = LocalContext.current
    val state by BleManager.state.collectAsState()
    val label by BleManager.deviceLabel.collectAsState()

    // Recomputed whenever refreshKey changes (e.g. returning from Settings).
    @Suppress("UNUSED_EXPRESSION") refreshKey
    val notifOk = Permissions.hasNotificationAccess(ctx)
    val btOk = Permissions.hasBluetooth(ctx)
    val locOk = Permissions.hasLocation(ctx)
    val postOk = Permissions.hasPostNotifications(ctx)
    val calOk = Permissions.hasCalendar(ctx)
    val battOk = Permissions.isIgnoringBatteryOptimizations(ctx)
    val dndOk = Permissions.hasDndAccess(ctx)
    val camOk = com.emgor.thewatch.camera.CameraPermissions.hasCamera(ctx) &&
        com.emgor.thewatch.camera.CameraPermissions.hasAudio(ctx)
    val overlayOk = com.emgor.thewatch.camera.CameraPermissions.canDrawOverlays(ctx)

    Scaffold(topBar = { TopAppBar(title = { Text("The Watch") }) }) { pad ->
        Column(
            Modifier
                .padding(pad)
                .fillMaxSize()
                .verticalScroll(rememberScrollState())
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            // ---- connection status ----
            Card(Modifier.fillMaxWidth()) {
                Column(Modifier.padding(16.dp)) {
                    Text("Connection", style = MaterialTheme.typography.titleMedium)
                    Spacer(Modifier.height(4.dp))
                    val statusText = when (state) {
                        BleManager.State.READY -> "Connected — ${label ?: "watch"}"
                        BleManager.State.CONNECTED -> "Connecting…"
                        BleManager.State.CONNECTING -> "Connecting…"
                        BleManager.State.SCANNING -> "Searching for watch…"
                        BleManager.State.IDLE -> "Not connected"
                    }
                    Text(
                        statusText,
                        style = MaterialTheme.typography.bodyLarge,
                        fontWeight = FontWeight.SemiBold,
                        color = if (state == BleManager.State.READY)
                            MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurface
                    )
                    Spacer(Modifier.height(8.dp))
                    OutlinedButton(onClick = onRestartService) { Text("Reconnect / restart service") }
                }
            }

            // ---- permissions ----
            Card(Modifier.fillMaxWidth()) {
                Column(Modifier.padding(16.dp)) {
                    Text("Permissions", style = MaterialTheme.typography.titleMedium)
                    Spacer(Modifier.height(8.dp))
                    PermRow("Notification access", notifOk, "Grant", onGrantNotificationAccess,
                        "Required to forward notifications & read media sessions.")
                    PermRow("Bluetooth + Location", btOk && locOk, "Grant", onGrantRuntime,
                        "Bluetooth to link the watch; location for weather (Open-Meteo).")
                    PermRow("Post notifications", postOk, "Grant", onGrantRuntime,
                        "Lets the app show its ongoing 'link' notification (Android 13+).")
                    PermRow("Calendar", calOk, "Grant", onGrantRuntime,
                        "Reads your next event to show it on the watch.")
                    PermRow("Ignore battery optimizations", battOk, "Allow", onGrantBattery,
                        "Keeps the connection alive in the background.")
                    PermRow("Do Not Disturb access", dndOk, "Allow", onGrantDnd,
                        "Lets Find-Phone ring loudly through silent / DND.")
                    PermRow("Camera + Microphone", camOk, "Grant", onGrantCamera,
                        "Lets the watch open the camera and take photos / videos.")
                    PermRow("Display over other apps", overlayOk, "Allow", onGrantOverlay,
                        "REQUIRED for the camera. On Android 14 the camera can only " +
                            "capture while its screen is visible, so the watch uses this " +
                            "\"Appear on top\" permission to open the camera screen when " +
                            "your phone is locked or in another app. Without it, a watch " +
                            "camera tap does nothing unless the app is already open.")
                }
            }

            // ---- Samsung / One UI keep-alive hints ----
            Card(Modifier.fillMaxWidth()) {
                Column(Modifier.padding(16.dp)) {
                    Text("Samsung / One UI setup", style = MaterialTheme.typography.titleMedium)
                    Spacer(Modifier.height(4.dp))
                    Text(
                        "One UI aggressively sleeps background apps and will kill the " +
                            "watch link. To keep it reliable:\n" +
                            "• Allow \"Ignore battery optimizations\" above.\n" +
                            "• Settings ▸ Battery ▸ Background usage limits ▸ Never " +
                            "sleeping apps ▸ add \"The Watch\".\n" +
                            "• Make sure \"Display over other apps\" above is ON so the " +
                            "watch can open the camera when the phone is locked.",
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.75f)
                    )
                }
            }

            // ---- weather ----
            WeatherCard()

            // ---- extras (battery / calendar to watch) ----
            ExtrasCard()

            // ---- notification filter ----
            Card(Modifier.fillMaxWidth()) {
                Column(Modifier.padding(16.dp)) {
                    Text("Notification filter", style = MaterialTheme.typography.titleMedium)
                    Spacer(Modifier.height(4.dp))
                    Text(
                        "All apps are forwarded by default. Mute individual apps here.",
                        style = MaterialTheme.typography.bodyMedium
                    )
                    Spacer(Modifier.height(8.dp))
                    Button(onClick = onOpenFilter) { Text("Manage app filter") }
                }
            }

            Spacer(Modifier.height(24.dp))
            Text(
                "Watch: Bangle.js EMGOR · Nordic UART · Open-Meteo",
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.6f)
            )
        }
    }
}

@Composable
private fun PermRow(
    title: String,
    granted: Boolean,
    action: String,
    onAction: () -> Unit,
    subtitle: String,
) {
    Row(
        Modifier.fillMaxWidth().padding(vertical = 6.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Icon(
            if (granted) Icons.Filled.CheckCircle else Icons.Filled.Warning,
            contentDescription = null,
            tint = if (granted) Color(0xFF2FBF71) else Color(0xFFE0A800)
        )
        Spacer(Modifier.height(0.dp))
        Column(Modifier.weight(1f).padding(horizontal = 12.dp)) {
            Text(title, fontWeight = FontWeight.Medium)
            Text(
                subtitle,
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.6f)
            )
        }
        if (!granted) OutlinedButton(onClick = onAction) { Text(action) }
        else Text("OK", color = Color(0xFF2FBF71), fontWeight = FontWeight.SemiBold)
    }
}

@Composable
private fun WeatherCard() {
    val ctx = LocalContext.current
    var units by remember { mutableStateOf(Prefs.units(ctx)) }
    var mode by remember { mutableStateOf(Prefs.weatherMode(ctx)) }
    var minutes by remember { mutableStateOf(Prefs.weatherMinutes(ctx).toString()) }
    var lat by remember { mutableStateOf(Prefs.manualLat(ctx).let { if (it.isNaN()) "" else it.toString() }) }
    var lon by remember { mutableStateOf(Prefs.manualLon(ctx).let { if (it.isNaN()) "" else it.toString() }) }
    var place by remember { mutableStateOf(Prefs.manualName(ctx)) }

    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp)) {
            Text("Weather", style = MaterialTheme.typography.titleMedium)
            Spacer(Modifier.height(8.dp))

            Text("Display units", style = MaterialTheme.typography.bodyMedium)
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                FilterChip(selected = units == "F", onClick = {
                    units = "F"; Prefs.setUnits(ctx, "F")
                }, label = { Text("°F") })
                FilterChip(selected = units == "C", onClick = {
                    units = "C"; Prefs.setUnits(ctx, "C")
                }, label = { Text("°C") })
            }
            Spacer(Modifier.height(8.dp))

            Text("Location source", style = MaterialTheme.typography.bodyMedium)
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                FilterChip(selected = mode == "phone", onClick = {
                    mode = "phone"; Prefs.setWeatherMode(ctx, "phone")
                }, label = { Text("Phone location") })
                FilterChip(selected = mode == "manual", onClick = {
                    mode = "manual"; Prefs.setWeatherMode(ctx, "manual")
                }, label = { Text("Manual") })
            }

            if (mode == "manual") {
                Spacer(Modifier.height(8.dp))
                OutlinedTextField(
                    value = lat, onValueChange = { lat = it },
                    label = { Text("Latitude") }, modifier = Modifier.fillMaxWidth()
                )
                OutlinedTextField(
                    value = lon, onValueChange = { lon = it },
                    label = { Text("Longitude") }, modifier = Modifier.fillMaxWidth()
                )
                OutlinedTextField(
                    value = place, onValueChange = { place = it },
                    label = { Text("Place name (shown on watch)") },
                    modifier = Modifier.fillMaxWidth()
                )
                OutlinedButton(onClick = {
                    val la = lat.toDoubleOrNull(); val lo = lon.toDoubleOrNull()
                    if (la != null && lo != null) Prefs.setManualLocation(ctx, la, lo, place)
                }) { Text("Save location") }
            }

            Spacer(Modifier.height(8.dp))
            OutlinedTextField(
                value = minutes,
                onValueChange = {
                    minutes = it.filter { c -> c.isDigit() }
                    minutes.toIntOrNull()?.let { m ->
                        Prefs.setWeatherMinutes(ctx, m.coerceAtLeast(15))
                        WeatherWorker.schedule(ctx)
                    }
                },
                label = { Text("Update interval (minutes, min 15)") },
                modifier = Modifier.fillMaxWidth()
            )
        }
    }
}

@Composable
private fun ExtrasCard() {
    val ctx = LocalContext.current
    var battery by remember { mutableStateOf(Prefs.sendBattery(ctx)) }
    var calendar by remember { mutableStateOf(Prefs.sendCalendar(ctx)) }
    Card(Modifier.fillMaxWidth()) {
        Column(Modifier.padding(16.dp)) {
            Text("Send to watch", style = MaterialTheme.typography.titleMedium)
            Spacer(Modifier.height(4.dp))
            Text(
                "Phone battery % and your next calendar event. (Needs a small watch " +
                    "firmware follow-up to display these.)",
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.6f)
            )
            Spacer(Modifier.height(8.dp))
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                Text("Phone battery %", Modifier.weight(1f))
                Switch(checked = battery, onCheckedChange = {
                    battery = it; Prefs.setSendBattery(ctx, it)
                })
            }
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                Text("Next calendar event", Modifier.weight(1f))
                Switch(checked = calendar, onCheckedChange = {
                    calendar = it; Prefs.setSendCalendar(ctx, it)
                })
            }
        }
    }
}
