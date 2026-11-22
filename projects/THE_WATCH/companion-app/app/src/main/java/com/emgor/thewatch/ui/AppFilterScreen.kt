package com.emgor.thewatch.ui

import android.content.Intent
import android.content.pm.PackageManager
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material3.Divider
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.produceState
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import com.emgor.thewatch.data.Prefs
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

private data class AppRow(val pkg: String, val label: String)

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun AppFilterScreen(onBack: () -> Unit) {
    val ctx = LocalContext.current

    val apps by produceState(initialValue = emptyList<AppRow>()) {
        value = withContext(Dispatchers.IO) { loadLauncherApps(ctx) }
    }
    // Local mutable copy of the muted set for instant toggle feedback.
    val muted = remember { mutableStateListOf<String>().apply { addAll(Prefs.mutedPackages(ctx)) } }

    Scaffold(topBar = {
        TopAppBar(
            title = { Text("App filter") },
            navigationIcon = {
                IconButton(onClick = onBack) {
                    Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Back")
                }
            }
        )
    }) { pad ->
        Column(Modifier.padding(pad).fillMaxSize()) {
            Text(
                "Toggle an app OFF to stop forwarding its notifications to the watch. " +
                    "Default: all apps forwarded.",
                style = MaterialTheme.typography.bodyMedium,
                modifier = Modifier.padding(16.dp)
            )
            Divider()
            LazyColumn(Modifier.fillMaxSize()) {
                items(apps, key = { it.pkg }) { app ->
                    val isMuted = muted.contains(app.pkg)
                    Row(
                        Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 12.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text(app.label, Modifier.weight(1f))
                        Switch(
                            checked = !isMuted, // ON = forwarding
                            onCheckedChange = { forwarding ->
                                val nowMuted = !forwarding
                                Prefs.setMuted(ctx, app.pkg, nowMuted)
                                if (nowMuted) muted.add(app.pkg) else muted.remove(app.pkg)
                            }
                        )
                    }
                    Divider()
                }
            }
        }
    }
}

private fun loadLauncherApps(ctx: android.content.Context): List<AppRow> {
    val pm = ctx.packageManager
    val intent = Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_LAUNCHER)
    val resolved = pm.queryIntentActivities(intent, 0)
    return resolved
        .mapNotNull { ri ->
            val pkg = ri.activityInfo?.packageName ?: return@mapNotNull null
            if (pkg == ctx.packageName) return@mapNotNull null
            AppRow(pkg, ri.loadLabel(pm).toString())
        }
        .distinctBy { it.pkg }
        .sortedBy { it.label.lowercase() }
}
