import Foundation
import CoreBluetooth

/// Events surfaced to the coordinator (AppState). Delivered on the main queue.
protocol WatchBLEDelegate: AnyObject {
    func bleDidChangeConnection(_ connected: Bool)
    func bleDidReceive(_ inbound: GbProtocol.Inbound)
    func bleDidChangeState(_ description: String)
}

/// CoreBluetooth central for The Watch (Nordic UART Service peripheral).
///
/// - Scans for a peripheral whose advertised name starts "Bangle.js".
/// - Discovers NUS, subscribes to TX notify (6E400003), writes to RX (6E400002).
/// - Buffers inbound bytes and splits on '\n' (dropping '\r') into JSON lines.
/// - Auto-reconnects on drop.
final class WatchBLEManager: NSObject {

    // Nordic UART Service UUIDs.
    static let nusService = CBUUID(string: "6E400001-B5A3-F393-E0A9-E50E24DCCA9E")
    static let nusRX      = CBUUID(string: "6E400002-B5A3-F393-E0A9-E50E24DCCA9E") // write (central->watch)
    static let nusTX      = CBUUID(string: "6E400003-B5A3-F393-E0A9-E50E24DCCA9E") // notify (watch->central)

    weak var delegate: WatchBLEDelegate?

    private var central: CBCentralManager!
    private var peripheral: CBPeripheral?
    private var rxChar: CBCharacteristic?   // we WRITE here
    private var txChar: CBCharacteristic?   // we SUBSCRIBE here

    private var rxBuffer = Data()           // inbound line-assembly buffer
    private let queue = DispatchQueue(label: "com.emgor.thewatch.ble")

    private(set) var isConnected = false {
        didSet {
            if oldValue != isConnected {
                let c = isConnected
                DispatchQueue.main.async { self.delegate?.bleDidChangeConnection(c) }
            }
        }
    }

    /// True only when the GATT link is up AND both NUS characteristics are
    /// discovered — i.e. a write to RX will actually go out (not just queue).
    /// Read from other queues (bridge/health), so guarded by the BLE `queue`.
    var isReady: Bool {
        queue.sync { isConnected && rxChar != nil && txChar != nil }
    }

    /// Advertised local names starting with this are considered "our watch".
    private let namePrefix = "Bangle.js"

    // Pending outbound lines queued while (re)connecting; flushed once RX is ready.
    private var outbox: [Data] = []

    func start() {
        central = CBCentralManager(delegate: self, queue: queue)
    }

    /// Enqueue a framed line for the watch. Written with-response when possible
    /// (falls back to without-response for the WRITE_NR path the firmware exposes).
    func send(_ data: Data) {
        queue.async {
            guard self.isConnected, let p = self.peripheral, let rx = self.rxChar else {
                self.outbox.append(data)
                NSLog("[ble] queued %d bytes (RX not ready): %@",
                      data.count, Self.preview(data))
                return
            }
            NSLog("[ble] write %d bytes to RX: %@", data.count, Self.preview(data))
            self.writeChunked(data, to: p, char: rx)
        }
    }

    // MARK: - internal

    private func flushOutbox() {
        guard let p = peripheral, let rx = rxChar else { return }
        let pending = outbox; outbox.removeAll()
        if !pending.isEmpty { NSLog("[ble] flushing %d queued line(s) to RX", pending.count) }
        for d in pending { writeChunked(d, to: p, char: rx) }
    }

    /// Short printable preview of an outbound line for logging (trims framing noise).
    private static func preview(_ data: Data) -> String {
        let s = String(data: data, encoding: .utf8) ?? "<\(data.count) bytes>"
        let oneLine = s.replacingOccurrences(of: "\n", with: "\\n")
            .replacingOccurrences(of: "\r", with: "\\r")
        return oneLine.count > 120 ? String(oneLine.prefix(120)) + "…" : oneLine
    }

    /// Write respecting the negotiated ATT MTU. maximumWriteValueLength gives the
    /// safe payload per write; we chunk longer lines (e.g. notification bodies).
    private func writeChunked(_ data: Data, to p: CBPeripheral, char: CBCharacteristic) {
        let withResponse = char.properties.contains(.write)
        let type: CBCharacteristicWriteType = withResponse ? .withResponse : .withoutResponse
        let maxLen = max(20, p.maximumWriteValueLength(for: type))
        var offset = 0
        while offset < data.count {
            let end = min(offset + maxLen, data.count)
            p.writeValue(data.subdata(in: offset..<end), for: char, type: type)
            offset = end
        }
    }

    private func startScan() {
        guard central.state == .poweredOn else { return }
        isConnected = false
        // Scan filtered by the NUS service UUID (advertised by the watch).
        central.scanForPeripherals(withServices: [Self.nusService],
                                   options: [CBCentralManagerScanOptionAllowDuplicatesKey: false])
        DispatchQueue.main.async { self.delegate?.bleDidChangeState("Scanning…") }
    }
}

// MARK: - CBCentralManagerDelegate
extension WatchBLEManager: CBCentralManagerDelegate {

    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .poweredOn:
            startScan()
        case .poweredOff:
            DispatchQueue.main.async { self.delegate?.bleDidChangeState("Bluetooth off") }
        case .unauthorized:
            DispatchQueue.main.async { self.delegate?.bleDidChangeState("Bluetooth not authorized") }
        default:
            DispatchQueue.main.async { self.delegate?.bleDidChangeState("Bluetooth unavailable") }
        }
    }

    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral,
                        advertisementData: [String: Any], rssi RSSI: NSNumber) {
        // Match on advertised local name prefix (falls back to peripheral.name).
        let advName = (advertisementData[CBAdvertisementDataLocalNameKey] as? String)
            ?? peripheral.name ?? ""
        guard advName.hasPrefix(namePrefix) else { return }

        self.peripheral = peripheral
        central.stopScan()
        DispatchQueue.main.async { self.delegate?.bleDidChangeState("Connecting to \(advName)…") }
        central.connect(peripheral, options: nil)
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        peripheral.delegate = self
        rxBuffer.removeAll()
        peripheral.discoverServices([Self.nusService])
        DispatchQueue.main.async { self.delegate?.bleDidChangeState("Discovering services…") }
    }

    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral,
                        error: Error?) {
        startScan()
    }

    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral,
                        error: Error?) {
        isConnected = false
        rxChar = nil; txChar = nil
        outbox.removeAll()   // stale lines from the previous session must not replay
        DispatchQueue.main.async { self.delegate?.bleDidChangeState("Disconnected — rescanning") }
        // Auto-reconnect, belt-and-braces:
        //  1) a direct reconnect to the known peripheral — completes the instant the
        //     watch re-advertises (survives a watch reboot with the same identifier);
        //  2) also resume scanning so we still find it if it comes back advertising
        //     under a fresh identifier. didDiscover/didConnect re-discover services &
        //     characteristics before we mark the link ready again.
        central.connect(peripheral, options: nil)
        startScan()
    }
}

// MARK: - CBPeripheralDelegate
extension WatchBLEManager: CBPeripheralDelegate {

    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard let svc = peripheral.services?.first(where: { $0.uuid == Self.nusService }) else { return }
        peripheral.discoverCharacteristics([Self.nusRX, Self.nusTX], for: svc)
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService,
                    error: Error?) {
        for c in service.characteristics ?? [] {
            if c.uuid == Self.nusTX {
                txChar = c
                peripheral.setNotifyValue(true, for: c)   // subscribe to watch->central notifies
            } else if c.uuid == Self.nusRX {
                rxChar = c
            }
        }
        if rxChar != nil && txChar != nil {
            isConnected = true
            DispatchQueue.main.async { self.delegate?.bleDidChangeState("Connected") }
            flushOutbox()
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic,
                    error: Error?) {
        guard characteristic.uuid == Self.nusTX, let value = characteristic.value else { return }
        rxBuffer.append(value)
        // Split on '\n'; the firmware sends bare JSON + "\r\n".
        while let nl = rxBuffer.firstIndex(of: 0x0A) {
            let lineData = rxBuffer.subdata(in: rxBuffer.startIndex..<nl)
            rxBuffer.removeSubrange(rxBuffer.startIndex...nl)
            let cleaned = lineData.filter { $0 != 0x0D }   // drop '\r'
            guard !cleaned.isEmpty, let line = String(data: cleaned, encoding: .utf8) else { continue }
            if let inbound = GbProtocol.parseInbound(line) {
                DispatchQueue.main.async { self.delegate?.bleDidReceive(inbound) }
            }
        }
        // Guard against a runaway buffer if a '\n' never arrives.
        if rxBuffer.count > 8192 { rxBuffer.removeAll() }
    }
}
