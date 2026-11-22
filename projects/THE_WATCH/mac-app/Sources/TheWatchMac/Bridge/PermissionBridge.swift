import Foundation
import Network

/// Result of a remote approval round-trip.
struct PermResult {
    let approve: Bool
    let scope: String       // "once" | "session" | "always" (forwarded verbatim from the watch's permResp)
    let timedOut: Bool
}

/// The coordinator implements this so the bridge can drive the watch.
protocol PermissionResponder: AnyObject {
    /// Register a permission request, send a permReq to the watch, and BLOCK until
    /// it is resolved (watch tap, menu-bar approve/deny, cancel, or ~60s timeout).
    /// Called on a background connection queue, so blocking is fine.
    ///
    /// `onRegister` is invoked synchronously with the assigned request id the moment
    /// the request is registered — BEFORE this call blocks — so the bridge can wire
    /// client-disconnect cancellation to that id. `onRegister(nil)` means the request
    /// could not be registered (e.g. watch not connected) and won't block.
    func requestPermission(title: String, body: String, origin: String,
                           onRegister: (Int32?) -> Void) -> PermResult
    /// Cancel a still-pending request (the requesting HTTP client went away). Resolves
    /// its waiter and sends a `permCancel` to the watch. No-op if already resolved.
    func cancelPermission(id: Int32)
    /// Snapshot for GET /health.
    func healthStatus() -> [String: Any]
}

/// Localhost-only HTTP server exposing the permission-approval bridge.
///
/// Contract (see README for the full spec another agent integrates against):
///   POST /permission  { "title": "...", "body": "...", "origin": "mac" }
///        -> blocks until the watch replies (or ~60s timeout)
///        -> { "approve": bool, "scope": "once"|"session"|"always", "timedOut": bool }
///   GET  /health      -> { "connected": bool, "watch": "...", ... }
///
/// Bound to 127.0.0.1 only (loopback), so it is never reachable off-box and does
/// not trip the Local Network TCC prompt.
final class PermissionBridge {

    weak var responder: PermissionResponder?
    let port: UInt16

    private var listener: NWListener?
    private let queue = DispatchQueue(label: "com.emgor.thewatch.httpbridge",
                                      attributes: .concurrent)

    init(port: UInt16 = 8765) {
        self.port = port
    }

    func start() {
        do {
            let params = NWParameters.tcp
            params.allowLocalEndpointReuse = true
            // Bind to loopback only.
            params.requiredLocalEndpoint = NWEndpoint.hostPort(
                host: NWEndpoint.Host("127.0.0.1"),
                port: NWEndpoint.Port(rawValue: port)!)
            let listener = try NWListener(using: params)
            listener.newConnectionHandler = { [weak self] conn in
                self?.handle(conn)
            }
            listener.start(queue: queue)
            self.listener = listener
            NSLog("[bridge] listening on 127.0.0.1:\(port)")
        } catch {
            NSLog("[bridge] failed to start on port \(port): \(error)")
        }
    }

    // MARK: - connection handling

    private func handle(_ conn: NWConnection) {
        conn.start(queue: queue)
        receive(conn, buffer: Data())
    }

    private func receive(_ conn: NWConnection, buffer: Data) {
        conn.receive(minimumIncompleteLength: 1, maximumLength: 64 * 1024) { [weak self] data, _, isComplete, error in
            guard let self else { return }
            var buf = buffer
            if let data { buf.append(data) }

            if let req = HTTPRequest.parse(buf) {
                // Full request in hand — route it (possibly blocking on the watch).
                self.queue.async { self.route(req, on: conn) }
                return
            }
            if isComplete || error != nil {
                self.respond(conn, status: 400, json: ["error": "bad request"])
                return
            }
            // Need more bytes.
            self.receive(conn, buffer: buf)
        }
    }

    private func route(_ req: HTTPRequest, on conn: NWConnection) {
        switch (req.method, req.path) {
        case ("GET", "/health"):
            let status = responder?.healthStatus() ?? ["connected": false]
            respond(conn, status: 200, json: status)

        case ("POST", "/permission"):
            guard let obj = try? JSONSerialization.jsonObject(with: req.body) as? [String: Any] else {
                respond(conn, status: 400, json: ["error": "invalid JSON body"]); return
            }
            let title  = (obj["title"] as? String) ?? "Approve?"
            let body   = (obj["body"] as? String) ?? ""
            let origin = (obj["origin"] as? String) ?? "mac"
            guard let responder = responder else {
                respond(conn, status: 503, json: ["approve": false, "scope": "once",
                                                   "timedOut": true, "error": "no watch coordinator"])
                return
            }
            // Register + block up to ~60s. If THIS client disconnects while we're
            // blocked (e.g. the Claude window is closed / cancelled), the watch card
            // must be dismissed — so wire the connection's cancel/failure to
            // cancelPermission(id). Thread-safe: cancelPermission is a no-op once the
            // request is resolved by any other path.
            let cancelBox = CancelBox()
            let result = responder.requestPermission(title: title, body: body, origin: origin) { id in
                guard let id else { return }
                cancelBox.id = id
                conn.stateUpdateHandler = { [weak responder] state in
                    switch state {
                    case .cancelled, .failed:
                        if let cid = cancelBox.take() { responder?.cancelPermission(id: cid) }
                    default:
                        break
                    }
                }
            }
            // We're about to respond ourselves; stop treating our own conn.cancel()
            // (inside respond) as a client disconnect.
            _ = cancelBox.take()
            conn.stateUpdateHandler = nil
            respond(conn, status: 200, json: ["approve": result.approve,
                                              "scope": result.scope,
                                              "timedOut": result.timedOut])

        default:
            respond(conn, status: 404, json: ["error": "not found"])
        }
    }

    private func respond(_ conn: NWConnection, status: Int, json: [String: Any]) {
        let bodyData = (try? JSONSerialization.data(withJSONObject: json)) ?? Data("{}".utf8)
        let reason: String = [200: "OK", 400: "Bad Request", 404: "Not Found",
                              503: "Service Unavailable"][status] ?? "OK"
        var head = "HTTP/1.1 \(status) \(reason)\r\n"
        head += "Content-Type: application/json\r\n"
        head += "Content-Length: \(bodyData.count)\r\n"
        head += "Connection: close\r\n\r\n"
        var out = Data(head.utf8)
        out.append(bodyData)
        conn.send(content: out, completion: .contentProcessed { _ in
            conn.cancel()
        })
    }
}

/// Thread-safe one-shot holder for a pending request's id. `take()` returns the id
/// exactly once (nil thereafter), so a client-disconnect fires cancelPermission at
/// most once and never after we've decided to respond ourselves.
private final class CancelBox {
    private let lock = NSLock()
    private var _id: Int32?
    var id: Int32? {
        get { lock.lock(); defer { lock.unlock() }; return _id }
        set { lock.lock(); _id = newValue; lock.unlock() }
    }
    func take() -> Int32? {
        lock.lock(); defer { lock.unlock() }
        let v = _id; _id = nil; return v
    }
}

/// Minimal HTTP/1.1 request parser (headers + Content-Length body).
private struct HTTPRequest {
    let method: String
    let path: String
    let body: Data

    static func parse(_ data: Data) -> HTTPRequest? {
        // Find end of headers.
        let sep = Data("\r\n\r\n".utf8)
        guard let range = data.range(of: sep) else { return nil }
        let headerData = data.subdata(in: data.startIndex..<range.lowerBound)
        guard let headerStr = String(data: headerData, encoding: .utf8) else { return nil }
        let lines = headerStr.components(separatedBy: "\r\n")
        guard let requestLine = lines.first else { return nil }
        let comps = requestLine.split(separator: " ")
        guard comps.count >= 2 else { return nil }
        let method = String(comps[0])
        let path = String(comps[1])

        // Content-Length → ensure the full body has arrived.
        var contentLength = 0
        for line in lines.dropFirst() {
            let parts = line.split(separator: ":", maxSplits: 1)
            if parts.count == 2, parts[0].lowercased().trimmingCharacters(in: .whitespaces) == "content-length" {
                contentLength = Int(parts[1].trimmingCharacters(in: .whitespaces)) ?? 0
            }
        }
        let bodyStart = range.upperBound
        let available = data.distance(from: bodyStart, to: data.endIndex)
        if available < contentLength { return nil }   // wait for more
        let body = data.subdata(in: bodyStart..<data.index(bodyStart, offsetBy: contentLength))
        return HTTPRequest(method: method, path: path, body: body)
    }
}
