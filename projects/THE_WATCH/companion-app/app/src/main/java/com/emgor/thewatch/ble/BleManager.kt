package com.emgor.thewatch.ble

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.BluetoothLeScanner
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.os.Build
import android.os.Handler
import android.os.HandlerThread
import android.os.ParcelUuid
import android.util.Log
import com.emgor.thewatch.data.Prefs
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import java.util.ArrayDeque
import java.util.UUID

/**
 * BLE CENTRAL that maintains a persistent GATT connection to the watch (a
 * peripheral running a Nordic UART Service). Scans by NUS service UUID / name,
 * connects, subscribes to TX notifications, and writes lines to RX. Auto-reconnects.
 *
 * Singleton so both [com.emgor.thewatch.service.WatchService] and the
 * NotificationListenerService can share the one live connection.
 */
@SuppressLint("MissingPermission") // callers ensure BLUETOOTH_CONNECT/SCAN are granted
object BleManager {

    private const val TAG = "TheWatch/BLE"

    val NUS_SERVICE: UUID = UUID.fromString("6E400001-B5A3-F393-E0A9-E50E24DCCA9E")
    val NUS_RX: UUID = UUID.fromString("6E400002-B5A3-F393-E0A9-E50E24DCCA9E") // write
    val NUS_TX: UUID = UUID.fromString("6E400003-B5A3-F393-E0A9-E50E24DCCA9E") // notify
    private val CCCD: UUID = UUID.fromString("00002902-0000-1000-8000-00805F9B34FB")
    private const val NAME_PREFIX = "Bangle.js"

    enum class State { IDLE, SCANNING, CONNECTING, CONNECTED, READY }

    private val _state = MutableStateFlow(State.IDLE)
    val state: StateFlow<State> = _state
    private val _deviceLabel = MutableStateFlow<String?>(null)
    val deviceLabel: StateFlow<String?> = _deviceLabel

    /** Called for every inbound (watch -> phone) message. Set by WatchService. */
    @Volatile var onInbound: ((GbProtocol.Inbound) -> Unit)? = null
    /** Called once the link is fully READY (subscribed). Used to push initial state. */
    @Volatile var onReady: (() -> Unit)? = null

    private lateinit var appCtx: Context
    private var adapter: BluetoothAdapter? = null
    private var scanner: BluetoothLeScanner? = null
    private var gatt: BluetoothGatt? = null
    private var rxChar: BluetoothGattCharacteristic? = null

    @Volatile private var wanted = false          // user wants us connected
    @Volatile private var mtu = 23
    private val rxLine = StringBuilder()

    // Dedicated thread for pacing/reconnect timers (GATT callbacks arrive on a
    // binder thread; we keep all queue mutations on this handler).
    private val ht = HandlerThread("ble-worker").apply { start() }
    private val h = Handler(ht.looper)

    // Serial GATT operation queue (Android allows one outstanding op at a time).
    private val ops = ArrayDeque<() -> Unit>()
    @Volatile private var opInFlight = false

    fun init(context: Context) {
        if (::appCtx.isInitialized) return
        appCtx = context.applicationContext
        val bm = appCtx.getSystemService(Context.BLUETOOTH_SERVICE) as? BluetoothManager
        adapter = bm?.adapter
    }

    fun start() {
        wanted = true
        h.post { ensureConnected() }
    }

    fun stop() {
        wanted = false
        h.post {
            stopScan()
            gatt?.let { runCatching { it.disconnect() }; runCatching { it.close() } }
            gatt = null
            rxChar = null
            setState(State.IDLE, null)
        }
    }

    val isReady: Boolean get() = _state.value == State.READY

    // ---- connection bring-up -------------------------------------------------

    private fun ensureConnected() {
        val a = adapter ?: return
        if (!a.isEnabled) { setState(State.IDLE, null); scheduleRetry(); return }
        if (_state.value == State.CONNECTED || _state.value == State.READY ||
            _state.value == State.CONNECTING
        ) return
        startScan()
    }

    private fun startScan() {
        val a = adapter ?: return
        val s = a.bluetoothLeScanner ?: return
        scanner = s
        setState(State.SCANNING, _deviceLabel.value)
        val filters = listOf(
            ScanFilter.Builder().setServiceUuid(ParcelUuid(NUS_SERVICE)).build()
        )
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()
        runCatching { s.startScan(filters, settings, scanCallback) }
            .onFailure { Log.e(TAG, "startScan failed", it); scheduleRetry() }
        // Safety timeout: if we don't find it, cycle the scan.
        h.postDelayed(rescanTimeout, 20_000)
    }

    private val rescanTimeout = Runnable {
        if (wanted && _state.value == State.SCANNING) {
            stopScan(); scheduleRetry()
        }
    }

    private fun stopScan() {
        h.removeCallbacks(rescanTimeout)
        runCatching { scanner?.stopScan(scanCallback) }
    }

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val dev = result.device ?: return
            val name = result.scanRecord?.deviceName ?: runCatching { dev.name }.getOrNull()
            val last = Prefs.lastDevice(appCtx)
            val matchesName = name?.startsWith(NAME_PREFIX) == true
            val matchesAddr = last != null && dev.address == last
            val hasNus = result.scanRecord?.serviceUuids?.any { it.uuid == NUS_SERVICE } == true
            if (matchesName || matchesAddr || hasNus) {
                stopScan()
                Prefs.setLastDevice(appCtx, dev.address)
                connectTo(dev, name ?: dev.address)
            }
        }

        override fun onScanFailed(errorCode: Int) {
            Log.e(TAG, "scan failed: $errorCode")
            scheduleRetry()
        }
    }

    private fun connectTo(dev: BluetoothDevice, label: String) {
        setState(State.CONNECTING, label)
        _deviceLabel.value = label
        clearOps()
        gatt = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            dev.connectGatt(appCtx, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
        } else {
            dev.connectGatt(appCtx, false, gattCallback)
        }
    }

    private fun scheduleRetry(delayMs: Long = 4_000) {
        if (!wanted) return
        h.postDelayed({ if (wanted) ensureConnected() }, delayMs)
    }

    // ---- GATT callbacks ------------------------------------------------------

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                setState(State.CONNECTED, _deviceLabel.value)
                // Small settle delay improves reliability of discovery on some phones.
                h.postDelayed({ runCatching { g.discoverServices() } }, 250)
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                Log.w(TAG, "disconnected status=$status")
                cleanupGatt()
                if (wanted) scheduleRetry()
            }
        }

        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) { g.disconnect(); return }
            val svc = g.getService(NUS_SERVICE)
            if (svc == null) { Log.e(TAG, "NUS not found"); g.disconnect(); return }
            rxChar = svc.getCharacteristic(NUS_RX)
            val tx = svc.getCharacteristic(NUS_TX)
            if (rxChar == null || tx == null) { g.disconnect(); return }
            // Ask for a large MTU so short commands fit in one write.
            runCatching { g.requestMtu(247) }
        }

        override fun onMtuChanged(g: BluetoothGatt, mtuValue: Int, status: Int) {
            mtu = if (status == BluetoothGatt.GATT_SUCCESS) mtuValue else 23
            Log.i(TAG, "MTU=$mtu")
            enableTxNotifications(g)
        }

        override fun onDescriptorWrite(g: BluetoothGatt, d: BluetoothGattDescriptor, status: Int) {
            if (d.uuid == CCCD) {
                setState(State.READY, _deviceLabel.value)
                onReady?.invoke()
            }
            opDone()
        }

        override fun onCharacteristicWrite(
            g: BluetoothGatt, c: BluetoothGattCharacteristic, status: Int
        ) {
            opDone()
        }

        // API < 33 notification delivery
        @Deprecated("Deprecated in Java")
        override fun onCharacteristicChanged(g: BluetoothGatt, c: BluetoothGattCharacteristic) {
            if (c.uuid == NUS_TX) onTxBytes(c.value)
        }

        // API 33+ notification delivery
        override fun onCharacteristicChanged(
            g: BluetoothGatt, c: BluetoothGattCharacteristic, value: ByteArray
        ) {
            if (c.uuid == NUS_TX) onTxBytes(value)
        }
    }

    private fun enableTxNotifications(g: BluetoothGatt) {
        val tx = g.getService(NUS_SERVICE)?.getCharacteristic(NUS_TX) ?: return
        g.setCharacteristicNotification(tx, true)
        val cccd = tx.getDescriptor(CCCD) ?: return
        enqueue {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                g.writeDescriptor(cccd, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)
            } else {
                @Suppress("DEPRECATION")
                cccd.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                @Suppress("DEPRECATION")
                g.writeDescriptor(cccd)
            }
        }
    }

    private fun cleanupGatt() {
        clearOps()
        runCatching { gatt?.close() }
        gatt = null
        rxChar = null
        mtu = 23
        rxLine.setLength(0)
        setState(if (wanted) State.SCANNING else State.IDLE, _deviceLabel.value)
    }

    // ---- inbound line assembly ----------------------------------------------

    private fun onTxBytes(bytes: ByteArray?) {
        bytes ?: return
        for (b in bytes) {
            val ch = b.toInt().toChar()
            when (ch) {
                '\n' -> { flushLine(); }
                '\r' -> { /* skip */ }
                else -> {
                    rxLine.append(ch)
                    if (rxLine.length > 4000) rxLine.setLength(0)
                }
            }
        }
    }

    private fun flushLine() {
        val line = rxLine.toString()
        rxLine.setLength(0)
        if (line.isBlank()) return
        Log.d(TAG, "RX <- $line")
        val msg = GbProtocol.parseInbound(line) ?: return
        runCatching { onInbound?.invoke(msg) }.onFailure { Log.e(TAG, "onInbound", it) }
    }

    // ---- outbound writes -----------------------------------------------------

    /** Queue a full protocol line (already framed) for delivery to the watch's RX. */
    fun sendLine(line: String) {
        val g = gatt ?: return
        val rx = rxChar ?: return
        val data = line.toByteArray(Charsets.UTF_8)
        val chunk = if (mtu > 23) (mtu - 3) else 20
        var i = 0
        while (i < data.size) {
            val end = minOf(i + chunk, data.size)
            val part = data.copyOfRange(i, end)
            enqueue { writeChunk(g, rx, part) }
            i = end
        }
    }

    private fun writeChunk(g: BluetoothGatt, rx: BluetoothGattCharacteristic, part: ByteArray) {
        val ok = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            g.writeCharacteristic(rx, part, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT) ==
                BluetoothGatt.GATT_SUCCESS
        } else {
            @Suppress("DEPRECATION")
            run {
                rx.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
                rx.value = part
                g.writeCharacteristic(rx)
            }
        }
        if (!ok) opDone() // don't stall the queue if the write couldn't be issued
    }

    // ---- serial op queue -----------------------------------------------------

    private fun enqueue(op: () -> Unit) {
        h.post {
            ops.add(op)
            drain()
        }
    }

    private fun drain() {
        if (opInFlight) return
        val op = ops.pollFirst() ?: return
        opInFlight = true
        runCatching { op() }.onFailure { Log.e(TAG, "op", it); opDone() }
        // Watchdog: never let a lost callback wedge the queue forever.
        h.postDelayed(opWatchdog, 3_000)
    }

    private val opWatchdog = Runnable { if (opInFlight) opDone() }

    private fun opDone() {
        h.post {
            h.removeCallbacks(opWatchdog)
            opInFlight = false
            drain()
        }
    }

    private fun clearOps() {
        h.post { ops.clear(); opInFlight = false; h.removeCallbacks(opWatchdog) }
    }

    private fun setState(s: State, label: String?) {
        _state.value = s
        if (label != null) _deviceLabel.value = label
    }
}
