package com.emgor.thewatch.camera

import android.content.ContentValues
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.provider.MediaStore
import android.util.Log
import android.view.WindowManager
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.camera.core.CameraSelector
import androidx.camera.core.ImageCapture
import androidx.camera.core.ImageCaptureException
import androidx.camera.core.Preview
import androidx.camera.core.UseCaseGroup
import androidx.camera.lifecycle.ProcessCameraProvider
import androidx.camera.video.MediaStoreOutputOptions
import androidx.camera.video.Quality
import androidx.camera.video.QualitySelector
import androidx.camera.video.Recorder
import androidx.camera.video.Recording
import androidx.camera.video.VideoCapture
import androidx.camera.video.VideoRecordEvent
import androidx.camera.view.PreviewView
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.rememberScrollState
import androidx.compose.ui.unit.sp
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.Slider
import androidx.compose.material3.Text
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.core.content.ContextCompat
import com.emgor.thewatch.ui.AppTheme
import java.text.SimpleDateFormat
import java.util.Locale

/**
 * App-hosted CameraX capture screen driven by watch BLE commands (see
 * [CameraController]). Shows a live, VISIBLE preview; photos and videos are saved
 * to the phone's gallery (MediaStore, DCIM/TheWatch). This is an overt, user-
 * initiated smartwatch shutter remote — the user taps their own watch and this
 * on-screen UI appears on their own phone.
 *
 * On Android 14 this VISIBLE Activity is the ONLY reliable place a capture can
 * happen: CAMERA is a "while-in-use" permission, so the app must be foreground to
 * capture. When the watch sends a command while the app is backgrounded, this
 * Activity is launched from the background via the SYSTEM_ALERT_WINDOW ("Display
 * over other apps") background-activity-launch exemption, and — if auto_shutter is
 * set — captures automatically once the preview binds and 3A settles.
 *
 * The VISIBLE Preview is REQUIRED: Samsung/One UI drives autofocus/auto-exposure
 * off the preview's repeating request, so without an on-screen preview shots come
 * out dark/unfocused.
 *
 * Commands (delivered via the launch Intent / onNewIntent):
 *   open    -> just bring this screen to the foreground
 *   capture -> capture in the CURRENT on-screen mode: PHOTO takes a photo,
 *              VIDEO starts/stops recording. The PHOTO/VIDEO toggle lives in
 *              this UI, so the watch only ever needs to say "capture".
 * Back-compat with older firmware:
 *   shutter -> capture a photo
 *   video   -> start/stop video recording (toggle)
 *   mode    -> toggle photo <-> video mode (affects on-screen UI + reported state)
 */
class CameraActivity : ComponentActivity() {

    private val TAG = "TheWatch/Cam"

    private var cameraProvider: ProcessCameraProvider? = null
    private var imageCapture: ImageCapture? = null
    private var videoCapture: VideoCapture<Recorder>? = null
    private var recording: Recording? = null
    private var camera: androidx.camera.core.Camera? = null

    // Command queued until the camera is bound (e.g. arrives before permission grant).
    @Volatile private var pendingCmd: String? = null
    // When true, the queued command should fire automatically after the preview
    // binds — with a short 3A settle delay so exposure/focus converge first.
    @Volatile private var pendingAutoShutter = false
    @Volatile private var bound = false

    private val mainHandler = android.os.Handler(android.os.Looper.getMainLooper())
    // ~500ms after bind before an auto-shutter so Samsung 3A (AE/AF) converges.
    private val autoShutterSettleMs = 500L

    // UI state (also mirrored back to the watch).
    private var videoMode by mutableStateOf(false)
    private var recActive by mutableStateOf(false)
    private var hasPerms by mutableStateOf(false)

    // Camera control UI state.
    private var lensFacing = CameraSelector.LENS_FACING_BACK
    private var zoomRatio by mutableStateOf(1f)
    private var minZoom by mutableStateOf(1f)
    private var maxZoom by mutableStateOf(1f)
    // 0 = Off, 1 = On, 2 = Auto
    private var flashMode by mutableStateOf(ImageCapture.FLASH_MODE_OFF)
    private var torchOn by mutableStateOf(false)
    // Focus-ring: screen px position + a token that changes each tap to retrigger.
    private var focusPoint by mutableStateOf<androidx.compose.ui.geometry.Offset?>(null)
    private var focusToken by mutableStateOf(0)
    // Latest capture thumbnail for the bottom-right gallery button.
    private var galleryThumb by mutableStateOf<android.graphics.Bitmap?>(null)
    private var galleryUri by mutableStateOf<android.net.Uri?>(null)

    private val permLauncher =
        registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) {
            hasPerms = CameraPermissions.hasCamera(this)
            if (hasPerms) startCamera() else {
                toast("Camera permission is required")
                CameraController.reportState(open = false, videoMode = videoMode, recording = false)
            }
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        // Show over the lock screen / turn the screen on so a watch tap can wake it.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O_MR1) {
            setShowWhenLocked(true); setTurnScreenOn(true)
        } else {
            @Suppress("DEPRECATION")
            window.addFlags(
                WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED or
                    WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON or
                    WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON
            )
        }
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        // We are now foreground; drop the transient full-screen launch notification.
        CameraController.clearLaunchNotification(this)

        // Adopt the app-wide canonical mode so the on-screen UI matches what a
        // headless "capture" would do.
        videoMode = CameraController.videoMode

        pendingCmd = intent?.getStringExtra(CameraController.EXTRA_CMD)
        pendingAutoShutter = intent?.getBooleanExtra(CameraController.EXTRA_AUTO_SHUTTER, false) ?: false
        Log.i(TAG, "onCreate cmd=$pendingCmd autoShutter=$pendingAutoShutter " +
            "hasCamera=${CameraPermissions.hasCamera(this)}")
        hasPerms = CameraPermissions.hasCamera(this)

        setContent {
            AppTheme {
                if (hasPerms) CameraUi() else PermissionUi()
            }
        }

        if (hasPerms) startCamera()
        else permLauncher.launch(CameraPermissions.runtimePermsToRequest())
    }

    override fun onNewIntent(intent: android.content.Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        CameraController.clearLaunchNotification(this)
        val cmd = intent.getStringExtra(CameraController.EXTRA_CMD) ?: return
        val auto = intent.getBooleanExtra(CameraController.EXTRA_AUTO_SHUTTER, false)
        Log.i(TAG, "onNewIntent cmd=$cmd autoShutter=$auto bound=$bound")
        if (bound) {
            // Already bound & framing: no settle delay needed, fire now.
            runCommand(cmd)
        } else {
            pendingCmd = cmd
            pendingAutoShutter = auto
        }
    }

    override fun onResume() {
        super.onResume()
        // Mark the preview as active so CameraController delivers captures straight
        // to this on-screen instance (live preview already framing).
        CameraController.activityActive = true
        videoMode = CameraController.videoMode
    }

    override fun onPause() {
        // No longer visible. A later capture command re-launches this Activity
        // (via the SYSTEM_ALERT_WINDOW BAL exemption) with auto_shutter.
        CameraController.activityActive = false
        super.onPause()
    }

    override fun onDestroy() {
        CameraController.activityActive = false
        mainHandler.removeCallbacksAndMessages(null)
        runCatching { recording?.stop() }
        recording = null
        runCatching { cameraProvider?.unbindAll() }
        CameraController.reportState(open = false, videoMode = videoMode, recording = false)
        super.onDestroy()
    }

    // ---- CameraX bring-up ----------------------------------------------------

    private lateinit var previewView: PreviewView

    private fun startCamera() {
        val future = ProcessCameraProvider.getInstance(this)
        future.addListener({
            val provider = runCatching { future.get() }.getOrNull() ?: run {
                toast("Camera unavailable"); return@addListener
            }
            cameraProvider = provider
            bindUseCases(provider)
        }, ContextCompat.getMainExecutor(this))
    }

    private fun bindUseCases(provider: ProcessCameraProvider) {
        val preview = Preview.Builder().build().also {
            if (::previewView.isInitialized) it.setSurfaceProvider(previewView.surfaceProvider)
        }
        val rotation = if (::previewView.isInitialized) previewView.display?.rotation
            ?: android.view.Surface.ROTATION_0 else android.view.Surface.ROTATION_0
        val ic = ImageCapture.Builder()
            .setCaptureMode(ImageCapture.CAPTURE_MODE_MINIMIZE_LATENCY)
            .setTargetRotation(rotation)
            .build().apply { flashMode = this@CameraActivity.flashMode }
        val recorder = Recorder.Builder()
            .setQualitySelector(QualitySelector.from(Quality.HD))
            .build()
        val vc = VideoCapture.withOutput(recorder)

        provider.unbindAll()
        // Prefer binding preview + photo + video together so both actions work in
        // either mode. Some devices can't run all three concurrently; fall back to
        // preview + photo (video disabled) rather than crashing.
        val selector = CameraSelector.Builder().requireLensFacing(lensFacing).build()
        val cam = runCatching {
            provider.bindToLifecycle(
                this, selector,
                UseCaseGroup.Builder()
                    .addUseCase(preview).addUseCase(ic).addUseCase(vc).build()
            )
        }.getOrNull()
        if (cam != null) {
            imageCapture = ic; videoCapture = vc; camera = cam
        } else {
            val cam2 = runCatching {
                provider.bindToLifecycle(this, selector, preview, ic)
            }.getOrNull()
            if (cam2 != null) {
                imageCapture = ic; videoCapture = null; camera = cam2
                Log.w(TAG, "video+photo concurrent bind unsupported; video disabled")
            } else { toast("Camera bind failed"); return }
        }
        // Observe zoom range/current so the slider + indicator reflect the camera.
        camera?.cameraInfo?.zoomState?.observe(this) { z ->
            minZoom = z.minZoomRatio; maxZoom = z.maxZoomRatio; zoomRatio = z.zoomRatio
        }
        camera?.cameraControl?.enableTorch(torchOn)

        bound = true
        loadLatestThumb()
        CameraController.reportState(open = true, videoMode = videoMode, recording = recActive)
        val cmd = pendingCmd
        val auto = pendingAutoShutter
        pendingCmd = null
        pendingAutoShutter = false
        if (cmd != null) {
            if (auto) {
                // Give CameraX's repeating preview request ~500ms so AE/AF converge
                // before we fire — otherwise the first shot is dark/unfocused.
                Log.i(TAG, "auto-shutter armed: settle ${autoShutterSettleMs}ms then $cmd")
                mainHandler.postDelayed({ runCommand(cmd) }, autoShutterSettleMs)
            } else {
                runCommand(cmd)
            }
        }
    }

    // ---- command execution ---------------------------------------------------

    private fun runCommand(cmd: String) {
        Log.i(TAG, "runCommand $cmd (videoMode=$videoMode rec=$recActive)")
        when (cmd) {
            CameraController.CMD_OPEN -> { /* already foreground */ }
            // Primary command: capture in whatever mode the on-screen UI is in.
            CameraController.CMD_CAPTURE -> if (videoMode) toggleVideo() else takePhoto()
            CameraController.CMD_MODE -> toggleMode()
            CameraController.CMD_SHUTTER -> takePhoto()
            CameraController.CMD_VIDEO -> toggleVideo()
            else -> Log.w(TAG, "unknown cmd=$cmd")
        }
        CameraController.reportState(open = true, videoMode = videoMode, recording = recActive)
    }

    private fun toggleMode() {
        // Don't switch away mid-recording.
        if (recActive) return
        videoMode = !videoMode
        // Keep the app-wide canonical mode in sync so a later headless capture
        // (watch tap while backgrounded) uses the mode the user last chose.
        CameraController.videoMode = videoMode
    }

    // ---- camera controls -----------------------------------------------------

    private fun flipCamera() {
        if (recActive) { toast("Stop recording to flip"); return }
        lensFacing = if (lensFacing == CameraSelector.LENS_FACING_BACK)
            CameraSelector.LENS_FACING_FRONT else CameraSelector.LENS_FACING_BACK
        torchOn = false
        val p = cameraProvider ?: return
        bound = false
        bindUseCases(p)
    }

    private fun setZoom(ratio: Float) {
        val clamped = ratio.coerceIn(minZoom, maxZoom)
        camera?.cameraControl?.setZoomRatio(clamped)
        zoomRatio = clamped
    }

    private fun pinchZoom(scaleFactor: Float) = setZoom(zoomRatio * scaleFactor)

    private fun focusAt(x: Float, y: Float, viewW: Int, viewH: Int) {
        val cc = camera?.cameraControl ?: return
        if (viewW <= 0 || viewH <= 0) return
        val factory = androidx.camera.core.SurfaceOrientedMeteringPointFactory(
            viewW.toFloat(), viewH.toFloat()
        )
        val point = factory.createPoint(x, y)
        val action = androidx.camera.core.FocusMeteringAction.Builder(point).build()
        runCatching { cc.startFocusAndMetering(action) }
        focusPoint = androidx.compose.ui.geometry.Offset(x, y)
        focusToken += 1
    }

    private fun cycleFlash() {
        flashMode = when (flashMode) {
            ImageCapture.FLASH_MODE_OFF -> ImageCapture.FLASH_MODE_ON
            ImageCapture.FLASH_MODE_ON -> ImageCapture.FLASH_MODE_AUTO
            else -> ImageCapture.FLASH_MODE_OFF
        }
        imageCapture?.flashMode = flashMode
    }

    private fun toggleTorch() {
        torchOn = !torchOn
        camera?.cameraControl?.enableTorch(torchOn)
    }

    // ---- gallery thumbnail (bottom-right) ------------------------------------

    /** Query MediaStore for the most recent image in DCIM/TheWatch. */
    private fun loadLatestThumb() {
        Thread {
            runCatching {
                val proj = arrayOf(MediaStore.Images.Media._ID)
                val sel: String?; val args: Array<String>?
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                    sel = "${MediaStore.Images.Media.RELATIVE_PATH} LIKE ?"
                    args = arrayOf("%DCIM/TheWatch%")
                } else { sel = null; args = null }
                contentResolver.query(
                    MediaStore.Images.Media.EXTERNAL_CONTENT_URI, proj, sel, args,
                    "${MediaStore.Images.Media.DATE_ADDED} DESC"
                )?.use { c ->
                    if (c.moveToFirst()) {
                        val id = c.getLong(0)
                        val uri = android.content.ContentUris.withAppendedId(
                            MediaStore.Images.Media.EXTERNAL_CONTENT_URI, id
                        )
                        val bmp = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                            contentResolver.loadThumbnail(
                                uri, android.util.Size(160, 160), null
                            )
                        } else {
                            @Suppress("DEPRECATION")
                            MediaStore.Images.Thumbnails.getThumbnail(
                                contentResolver, id,
                                MediaStore.Images.Thumbnails.MINI_KIND, null
                            )
                        }
                        runOnUiThread { galleryThumb = bmp; galleryUri = uri }
                    }
                }
            }.onFailure { Log.w(TAG, "loadLatestThumb failed", it) }
        }.start()
    }

    /** Open the native gallery viewing the latest capture. */
    private fun openGallery() {
        val uri = galleryUri ?: run { toast("No photos yet"); return }
        val view = android.content.Intent(android.content.Intent.ACTION_VIEW).apply {
            setDataAndType(uri, "image/*")
            addFlags(android.content.Intent.FLAG_GRANT_READ_URI_PERMISSION)
            addFlags(android.content.Intent.FLAG_ACTIVITY_NEW_TASK)
        }
        runCatching { startActivity(view) }
            .onFailure { toast("No gallery app found") }
    }

    private fun timestamp(): String =
        SimpleDateFormat("yyyyMMdd_HHmmss", Locale.US).format(System.currentTimeMillis())

    private fun takePhoto() {
        val ic = imageCapture ?: run { Log.e(TAG, "capture start ABORT: camera not ready"); toast("Camera not ready"); return }
        Log.i(TAG, "capture start: photo")
        val name = "IMG_${timestamp()}"
        val values = ContentValues().apply {
            put(MediaStore.MediaColumns.DISPLAY_NAME, name)
            put(MediaStore.MediaColumns.MIME_TYPE, "image/jpeg")
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                put(MediaStore.MediaColumns.RELATIVE_PATH, "DCIM/TheWatch")
            }
        }
        val output = ImageCapture.OutputFileOptions.Builder(
            contentResolver, MediaStore.Images.Media.EXTERNAL_CONTENT_URI, values
        ).build()
        ic.takePicture(output, ContextCompat.getMainExecutor(this),
            object : ImageCapture.OnImageSavedCallback {
                override fun onImageSaved(r: ImageCapture.OutputFileResults) {
                    Log.i(TAG, "capture saved: ${r.savedUri}")
                    toast("Photo saved"); flashHint(); loadLatestThumb()
                }
                override fun onError(e: ImageCaptureException) {
                    Log.e(TAG, "capture failed: photo", e); toast("Photo failed")
                }
            })
    }

    private fun toggleVideo() {
        val rec = recording
        if (rec != null) { Log.i(TAG, "capture: stopping video"); rec.stop(); recording = null; return } // onFinalize flips recActive
        val vc = videoCapture ?: run { Log.e(TAG, "capture start ABORT: video unsupported"); toast("Video not supported on this device"); return }
        Log.i(TAG, "capture start: video")
        videoMode = true
        val name = "VID_${timestamp()}"
        val values = ContentValues().apply {
            put(MediaStore.MediaColumns.DISPLAY_NAME, name)
            put(MediaStore.MediaColumns.MIME_TYPE, "video/mp4")
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                put(MediaStore.MediaColumns.RELATIVE_PATH, "DCIM/TheWatch")
            }
        }
        val opts = MediaStoreOutputOptions.Builder(
            contentResolver, MediaStore.Video.Media.EXTERNAL_CONTENT_URI
        ).setContentValues(values).build()

        var pending = vc.output.prepareRecording(this, opts)
        val audioGranted = ContextCompat.checkSelfPermission(
            this, android.Manifest.permission.RECORD_AUDIO
        ) == PackageManager.PERMISSION_GRANTED
        if (audioGranted) pending = pending.withAudioEnabled()

        recording = pending.start(ContextCompat.getMainExecutor(this)) { event ->
            when (event) {
                is VideoRecordEvent.Start -> {
                    recActive = true
                    CameraController.reportState(true, videoMode, true)
                    toast(if (audioGranted) "Recording…" else "Recording (no mic)…")
                }
                is VideoRecordEvent.Finalize -> {
                    recActive = false
                    recording = null
                    if (event.hasError()) {
                        Log.e(TAG, "capture failed: video ${event.error}"); toast("Video failed")
                    } else { Log.i(TAG, "capture saved: ${event.outputResults.outputUri}"); toast("Video saved") }
                    CameraController.reportState(true, videoMode, false)
                }
                else -> {}
            }
        }
    }

    private fun flashHint() { /* reserved for a shutter flash animation */ }

    private fun toast(m: String) =
        runOnUiThread { Toast.makeText(this, m, Toast.LENGTH_SHORT).show() }

    // ---- UI ------------------------------------------------------------------

    @androidx.compose.runtime.Composable
    private fun CameraUi() {
        Box(Modifier.fillMaxSize().background(Color.Black)) {
            // Live preview with pinch-to-zoom (ScaleGestureDetector) and
            // tap-to-focus (GestureDetector single tap).
            AndroidView(
                factory = { ctx ->
                    PreviewView(ctx).also { pv ->
                        previewView = pv
                        val scale = android.view.ScaleGestureDetector(ctx,
                            object : android.view.ScaleGestureDetector.SimpleOnScaleGestureListener() {
                                override fun onScale(d: android.view.ScaleGestureDetector): Boolean {
                                    pinchZoom(d.scaleFactor); return true
                                }
                            })
                        val tap = android.view.GestureDetector(ctx,
                            object : android.view.GestureDetector.SimpleOnGestureListener() {
                                override fun onSingleTapUp(e: android.view.MotionEvent): Boolean {
                                    focusAt(e.x, e.y, pv.width, pv.height); return true
                                }
                            })
                        pv.setOnTouchListener { v, ev ->
                            scale.onTouchEvent(ev); tap.onTouchEvent(ev)
                            v.performClick(); true
                        }
                        cameraProvider?.let { p -> bindUseCases(p) }
                    }
                },
                modifier = Modifier.fillMaxSize()
            )

            // Focus ring at the last tap point (auto-hides).
            focusPoint?.let { pt ->
                val density = LocalDensity.current
                LaunchedEffect(focusToken) {
                    kotlinx.coroutines.delay(800); focusPoint = null
                }
                val xdp = with(density) { pt.x.toDp() }
                val ydp = with(density) { pt.y.toDp() }
                Box(
                    Modifier
                        .offset(x = xdp - 32.dp, y = ydp - 32.dp)
                        .size(64.dp)
                        .border(2.dp, Color.White, CircleShape)
                )
            }

            // Top row: flash / torch / flip.
            Row(
                Modifier.align(Alignment.TopCenter).fillMaxWidth().padding(16.dp),
                horizontalArrangement = Arrangement.SpaceEvenly
            ) {
                Button(onClick = { cycleFlash() }) {
                    Text("Flash: " + when (flashMode) {
                        ImageCapture.FLASH_MODE_ON -> "On"
                        ImageCapture.FLASH_MODE_AUTO -> "Auto"
                        else -> "Off"
                    })
                }
                Button(onClick = { toggleTorch() }) { Text(if (torchOn) "Torch On" else "Torch") }
                Button(onClick = { flipCamera() }) { Text("Flip") }
            }

            // REC indicator (top-center, only while recording).
            if (recActive) {
                Text(
                    "● REC",
                    color = Color(0xFFFF4136),
                    fontWeight = FontWeight.Bold,
                    modifier = Modifier.align(Alignment.TopCenter).padding(top = 70.dp)
                )
            }

            // Zoom preset chips (little translucent squares: .6x / 1x / 2x ...).
            // Pinch-to-zoom still works too; active preset highlighted purple.
            // Zoom preset chips: .6 / 1 / 2 / 5 / 10 / 20 / 50 / 100 up to the camera's
            // REAL max, plus a chip at the exact max. Horizontally scrollable so they all
            // fit; pinch-to-zoom reaches the same max. The ceiling is whatever this
            // device's CameraX exposes (maxZoom) — maxed out.
            val presets = remember(minZoom, maxZoom) {
                val list = buildList {
                    if (minZoom <= 0.7f) add(0.6f)
                    add(1f)
                    for (z in listOf(2f, 5f, 10f, 20f, 30f, 50f, 100f))
                        if (z <= maxZoom + 0.01f) add(z)
                }.toMutableList()
                if (maxZoom > (list.lastOrNull() ?: 1f) + 0.5f) list.add(maxZoom)
                list.filter { it in minZoom..maxZoom }.ifEmpty { listOf(1f) }
            }
            // Raised above the corner buttons (gallery / mode) so it never overlaps
            // them, and smaller chips so they fit well.
            Row(
                Modifier.align(Alignment.BottomCenter).fillMaxWidth()
                    .horizontalScroll(rememberScrollState())
                    .padding(start = 12.dp, end = 12.dp, bottom = 96.dp),
                horizontalArrangement = Arrangement.spacedBy(8.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                presets.forEach { p ->
                    // Wider active tolerance at high zoom so a preset stays lit.
                    val active = kotlin.math.abs(zoomRatio - p) < (p * 0.08f + 0.15f)
                    Box(
                        Modifier.size(34.dp).clip(RoundedCornerShape(7.dp))
                            .background(if (active) Color(0xCCB14DFF) else Color(0x66000000))
                            .border(
                                1.dp,
                                if (active) Color(0xFFC77DFF) else Color(0x88FFFFFF),
                                RoundedCornerShape(7.dp)
                            )
                            .clickable { setZoom(p) },
                        contentAlignment = Alignment.Center
                    ) {
                        Text(
                            if (p < 1f) ".6x" else "${kotlin.math.round(p).toInt()}x",
                            color = Color.White, fontWeight = FontWeight.Bold, fontSize = 11.sp
                        )
                    }
                }
            }

            // Gallery thumbnail (bottom-LEFT) -> opens native gallery.
            Box(
                Modifier.align(Alignment.BottomStart).padding(24.dp)
                    .size(56.dp).clip(RoundedCornerShape(12.dp))
                    .background(Color(0x66000000))
                    .border(1.dp, Color(0x88FFFFFF), RoundedCornerShape(12.dp))
                    .clickable { openGallery() },
                contentAlignment = Alignment.Center
            ) {
                val thumb = galleryThumb
                if (thumb != null) {
                    Image(
                        bitmap = thumb.asImageBitmap(),
                        contentDescription = "Open gallery",
                        contentScale = ContentScale.Crop,
                        modifier = Modifier.fillMaxSize()
                    )
                } else {
                    Text("🖼", color = Color.White)
                }
            }

            // PHOTO/VIDEO toggle (bottom-RIGHT square, same size as gallery) —
            // source of truth for what a watch "capture" does.
            Box(
                Modifier.align(Alignment.BottomEnd).padding(24.dp)
                    .size(56.dp).clip(RoundedCornerShape(12.dp))
                    .background(if (videoMode) Color(0xCCB14DFF) else Color(0x66000000))
                    .border(1.dp, Color(0xFFC77DFF), RoundedCornerShape(12.dp))
                    .clickable { toggleMode() },
                contentAlignment = Alignment.Center
            ) {
                Text(if (videoMode) "VID" else "PIC", color = Color.White, fontWeight = FontWeight.Bold)
            }
        }
    }

    @androidx.compose.runtime.Composable
    private fun PermissionUi() {
        Column(
            Modifier.fillMaxSize().background(Color.Black).padding(24.dp),
            verticalArrangement = Arrangement.Center,
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Text("Camera permission needed", color = Color.White, fontWeight = FontWeight.Bold)
            Spacer(Modifier.height(12.dp))
            Text(
                "Allow Camera (and Microphone for video) to use the watch shutter.",
                color = Color.White
            )
            Spacer(Modifier.height(16.dp))
            Button(onClick = { permLauncher.launch(CameraPermissions.runtimePermsToRequest()) }) {
                Text("Grant")
            }
            Spacer(Modifier.height(8.dp))
            Button(onClick = { finish() }) { Text("Close") }
        }
    }
}
