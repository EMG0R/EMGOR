package com.emgor.thewatch

import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.provider.Settings
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import com.emgor.thewatch.service.WatchService
import com.emgor.thewatch.ui.AppFilterScreen
import com.emgor.thewatch.ui.AppTheme
import com.emgor.thewatch.ui.HomeScreen

class MainActivity : ComponentActivity() {

    // Bumped on resume + after permission results so the UI re-reads grant state.
    private var refresh by mutableIntStateOf(0)

    private val permissionLauncher =
        registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) {
            refresh++
            maybeStartService() // (re)start now that BT/location may be granted
        }

    private val cameraPermissionLauncher =
        registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) {
            refresh++
        }

    /**
     * Android 14 requires HOLDING a connectedDevice-type permission (e.g.
     * BLUETOOTH_CONNECT) before a connectedDevice foreground service may call
     * startForeground(). So we only start the service once Bluetooth is granted.
     */
    private fun maybeStartService() {
        if (com.emgor.thewatch.ui.Permissions.hasBluetooth(this)) WatchService.start(this)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        lifecycle.addObserver(LifecycleEventObserver { _, event ->
            if (event == Lifecycle.Event.ON_RESUME) refresh++
        })

        // Start the link once Bluetooth is granted (also (re)starts after grants).
        maybeStartService()

        setContent {
            AppTheme {
                var screen by remember { mutableStateOf("home") }
                // read refresh so recomposition happens when it changes
                @Suppress("UNUSED_EXPRESSION") refresh
                when (screen) {
                    "filter" -> AppFilterScreen(onBack = { screen = "home" })
                    else -> HomeScreen(
                        refreshKey = refresh,
                        onOpenFilter = { screen = "filter" },
                        onGrantNotificationAccess = { openNotificationAccess() },
                        onGrantRuntime = { permissionLauncher.launch(com.emgor.thewatch.ui.Permissions.runtimePermsToRequest()) },
                        onGrantBattery = { requestBatteryExemption() },
                        onGrantDnd = { openDndAccess() },
                        onGrantCamera = {
                            cameraPermissionLauncher.launch(
                                com.emgor.thewatch.camera.CameraPermissions.runtimePermsToRequest()
                            )
                        },
                        onGrantOverlay = { openOverlaySettings() },
                        onRestartService = { maybeStartService() },
                    )
                }
            }
        }
    }

    private fun openNotificationAccess() {
        startActivity(Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS))
    }

    private fun openDndAccess() {
        startActivity(Intent(Settings.ACTION_NOTIFICATION_POLICY_ACCESS_SETTINGS))
    }

    private fun openOverlaySettings() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            val i = Intent(
                Settings.ACTION_MANAGE_OVERLAY_PERMISSION,
                Uri.parse("package:$packageName")
            )
            runCatching { startActivity(i) }.onFailure {
                startActivity(Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION))
            }
        }
    }

    private fun requestBatteryExemption() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            val i = Intent(Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS)
            i.data = Uri.parse("package:$packageName")
            runCatching { startActivity(i) }.onFailure {
                startActivity(Intent(Settings.ACTION_IGNORE_BATTERY_OPTIMIZATION_SETTINGS))
            }
        }
    }
}
