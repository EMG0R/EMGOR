// bridge_client.js
// Browser-side client for septagraph.tools.sept_bridge (Task 2.5): a thin
// wrapper over WebSocket that speaks binary frames only (the bridge relays
// them byte-for-byte to/from UDP -- no framing, no JSON, just the raw OSC
// packet bytes produced by sept_codec.js's encode* functions), with
// auto-reconnect.
//
// Usage:
//   import { connect } from "./bridge_client.js";
//   const bridge = connect("ws://127.0.0.1:8766");
//   bridge.onPacket((bytes) => { ... decode(bytes) ... });
//   bridge.send(encodeHit({...}));
//
// `connect()` returns immediately; the underlying socket connects
// asynchronously and reconnects on close/error with backoff. `send()` is a
// no-op (silently dropped, fast-plane semantics: never queue) while the
// socket isn't open.

const DEFAULT_MIN_BACKOFF_MS = 250;
const DEFAULT_MAX_BACKOFF_MS = 5000;

export function connect(url, options = {}) {
  const minBackoff = options.minBackoffMs ?? DEFAULT_MIN_BACKOFF_MS;
  const maxBackoff = options.maxBackoffMs ?? DEFAULT_MAX_BACKOFF_MS;

  let ws = null;
  let closedByUser = false;
  let backoff = minBackoff;
  let reconnectTimer = null;
  const packetHandlers = [];
  const stateHandlers = [];

  function emitState(state) {
    for (const cb of stateHandlers) {
      try { cb(state); } catch (e) { console.error("[bridge_client] state handler error", e); }
    }
  }

  function scheduleReconnect() {
    if (closedByUser) return;
    if (reconnectTimer !== null) return;
    reconnectTimer = setTimeout(() => {
      reconnectTimer = null;
      openSocket();
    }, backoff);
    backoff = Math.min(backoff * 2, maxBackoff);
  }

  function openSocket() {
    if (closedByUser) return;
    let socket;
    try {
      socket = new WebSocket(url);
    } catch (e) {
      console.error("[bridge_client] failed to open WebSocket", e);
      scheduleReconnect();
      return;
    }
    socket.binaryType = "arraybuffer";

    socket.onopen = () => {
      backoff = minBackoff;
      emitState("open");
    };

    socket.onmessage = (ev) => {
      if (!(ev.data instanceof ArrayBuffer)) return; // ignore any text frames
      const bytes = new Uint8Array(ev.data);
      for (const cb of packetHandlers) {
        try { cb(bytes); } catch (e) { console.error("[bridge_client] packet handler error", e); }
      }
    };

    socket.onclose = () => {
      emitState("closed");
      if (ws === socket) ws = null;
      scheduleReconnect();
    };

    socket.onerror = () => {
      // onclose always follows onerror for a WebSocket; reconnect is
      // scheduled there. Nothing to do here beyond letting it surface.
    };

    ws = socket;
  }

  openSocket();

  return {
    /** Send raw packet bytes (Uint8Array or ArrayBuffer). Dropped silently
     * if not currently connected -- fast plane never queues. */
    send(bytes) {
      if (ws === null || ws.readyState !== WebSocket.OPEN) return false;
      try {
        ws.send(bytes);
        return true;
      } catch (e) {
        console.error("[bridge_client] send failed", e);
        return false;
      }
    },
    /** Register a callback invoked with a Uint8Array for every relayed
     * binary frame (both UDP->WS directions: manifests on 7701, and any
     * --tap'd controller traffic on 7700). */
    onPacket(cb) {
      packetHandlers.push(cb);
      return () => {
        const i = packetHandlers.indexOf(cb);
        if (i >= 0) packetHandlers.splice(i, 1);
      };
    },
    /** Register a callback invoked with "open" or "closed" on connection
     * state changes -- useful for a UI status pill. */
    onState(cb) {
      stateHandlers.push(cb);
      return () => {
        const i = stateHandlers.indexOf(cb);
        if (i >= 0) stateHandlers.splice(i, 1);
      };
    },
    isOpen() {
      return ws !== null && ws.readyState === WebSocket.OPEN;
    },
    close() {
      closedByUser = true;
      if (reconnectTimer !== null) {
        clearTimeout(reconnectTimer);
        reconnectTimer = null;
      }
      if (ws !== null) {
        ws.close();
        ws = null;
      }
    },
  };
}
