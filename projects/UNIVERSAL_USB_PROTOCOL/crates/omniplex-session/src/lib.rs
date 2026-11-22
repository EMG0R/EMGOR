//! `omniplex-session` — the session facade tying every layer together (plan
//! Task 5.5, design spec §7).
//!
//! Full flow: watch link → mDNS discovery → role election (higher instance
//! id dials) → QUIC dial/listen → HELLO/HelloAck on the first stream
//! (version negotiation; the dialer is the clock master) → session clock
//! spawn → channel muxer ready. Auto-reconnects after a disconnect.
//!
//! This is the API the CLI (and later the SDK, Phase 2) consumes.

use std::collections::HashMap;
use std::net::{Ipv6Addr, SocketAddr, SocketAddrV6};
use std::sync::{Arc, Mutex};

use tokio::sync::{mpsc, watch};
use tokio::task::JoinHandle;

use omniplex_clock::SessionClock;
use omniplex_discovery::{elect, Discovery, DiscoveryConfig};
use omniplex_metrics::MetricsHub;
use omniplex_mux::{ChannelRx, ChannelTx, Mux, MuxError, MuxEvent, Role as MuxRole};
use omniplex_transport::{
    QuinnListener, QuinnTransport, Transport, TransportError, TransportStream,
};
use omniplex_wire::{
    decode_frame, encode_frame, negotiate, ChannelDescriptor, ChannelId, FrameHeader, FrameType,
    Hello, HelloAck, SessionTimestamp, FLAG_END_OF_MESSAGE, FRAME_HEADER_LEN,
};

pub use omniplex_discovery::Role;

/// Number of session events buffered before the emitting tasks await.
const EVENT_BUFFER: usize = 64;

/// Stream priority used for the HELLO and clock streams (latency-critical
/// tier, matching the mux's `LatencyCritical = 100` mapping).
const CONTROL_STREAM_PRIORITY: i32 = 100;

/// Why a session (or connection attempt) ended.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum DisconnectReason {
    /// HELLO version ranges do not overlap (§10 clean failure).
    VersionMismatch,
    /// The transport reported link loss (cable pull, idle timeout, peer gone).
    LinkLost,
    /// The peer violated the handshake protocol.
    ProtocolError(String),
}

/// Events surfaced to the application (spec §7, §10, §11).
#[derive(Debug, Clone, PartialEq)]
pub enum SessionEvent {
    /// mDNS found a peer on the point-to-point link.
    PeerDiscovered,
    /// HELLO completed; the session is live at the negotiated version.
    Connected { version: u16 },
    /// The remote side opened a channel toward us. Retrieve its endpoints
    /// with [`Session::take_offered`].
    ChannelOffered(ChannelDescriptor),
    /// The connection ended. `Session::start` sessions auto-reconnect.
    Disconnected(DisconnectReason),
    /// The mux shrank a bulk channel's allocation (§10 degradation ladder).
    BandwidthPressure {
        channel: ChannelId,
        allocated_bps: u64,
    },
}

/// Configuration for [`Session::start`] / [`Session::from_transport`].
#[derive(Debug, Clone)]
pub struct SessionConfig {
    /// Random 128-bit instance id (role election + mDNS identity).
    pub instance_id: [u8; 16],
    /// Supported protocol versions `(min, max)`, announced in HELLO.
    pub version_range: (u16, u16),
    /// Local UDP bind address for the QUIC listener (`[::]:0` default).
    pub bind: SocketAddr,
    /// Channels pre-announced in the HELLO payload (may be empty).
    pub channels: Vec<ChannelDescriptor>,
}

impl Default for SessionConfig {
    fn default() -> Self {
        Self {
            instance_id: rand::random(),
            version_range: (1, 1),
            bind: SocketAddr::V6(SocketAddrV6::new(Ipv6Addr::UNSPECIFIED, 0, 0, 0)),
            channels: Vec::new(),
        }
    }
}

/// The state of one live connection.
#[derive(Clone)]
struct Active {
    mux: Arc<Mux>,
    clock: SessionClock,
}

/// State shared between the [`Session`] handle and its background tasks.
struct Shared {
    metrics: MetricsHub,
    active: watch::Sender<Option<Active>>,
    /// Remote-initiated channels, stashed by the accept loop before the
    /// corresponding [`SessionEvent::ChannelOffered`] is emitted.
    offered: Mutex<HashMap<u16, (ChannelTx, ChannelRx)>>,
}

impl Shared {
    fn new() -> Self {
        Self {
            metrics: MetricsHub::new(),
            active: watch::channel(None).0,
            offered: Mutex::new(HashMap::new()),
        }
    }
}

/// A running Omniplex session. Dropping it stops all background tasks.
pub struct Session {
    shared: Arc<Shared>,
    task: JoinHandle<()>,
}

impl Drop for Session {
    fn drop(&mut self) {
        self.task.abort();
    }
}

impl Session {
    /// Full §7 flow: watch link → mDNS → elect → dial/listen
    /// ([`QuinnTransport`]) → HELLO/HelloAck (version negotiate,
    /// clock-master = dialer) → spawn [`SessionClock`] → [`Mux`] ready.
    /// Loops back to discovery after every disconnect (auto-reconnect).
    pub fn start(cfg: SessionConfig) -> (Session, mpsc::Receiver<SessionEvent>) {
        let (events_tx, events_rx) = mpsc::channel(EVENT_BUFFER);
        let shared = Arc::new(Shared::new());
        let task = tokio::spawn(supervise(cfg, shared.clone(), events_tx));
        (Session { shared, task }, events_rx)
    }

    /// Test/embedding constructor: run a session over an existing transport
    /// (e.g. a `LoopbackTransport`) with a fixed role, skipping discovery.
    /// No reconnect: after `Disconnected` the session is done.
    pub fn from_transport(
        transport: Arc<dyn Transport>,
        role: Role,
        cfg: SessionConfig,
    ) -> (Session, mpsc::Receiver<SessionEvent>) {
        let (events_tx, events_rx) = mpsc::channel(EVENT_BUFFER);
        let shared = Arc::new(Shared::new());
        let task = tokio::spawn({
            let shared = shared.clone();
            async move {
                let reason = run_connection(transport, role, &cfg, &shared, &events_tx).await;
                let _ = events_tx.send(SessionEvent::Disconnected(reason)).await;
            }
        });
        (Session { shared, task }, events_rx)
    }

    /// Open a channel toward the peer. Waits until the session is connected,
    /// then delegates to [`Mux::open_channel`].
    pub async fn open_channel(
        &self,
        desc: ChannelDescriptor,
    ) -> Result<(ChannelTx, ChannelRx), MuxError> {
        let active = self.wait_active().await;
        active.mux.open_channel(desc).await
    }

    /// Retrieve the endpoints of a remote-initiated channel previously
    /// announced via [`SessionEvent::ChannelOffered`].
    pub fn take_offered(&self, id: ChannelId) -> Option<(ChannelTx, ChannelRx)> {
        self.shared
            .offered
            .lock()
            .expect("offered lock")
            .remove(&id.0)
    }

    /// The current session clock.
    ///
    /// # Panics
    /// Panics if the session is not currently connected.
    pub fn clock(&self) -> SessionClock {
        self.shared
            .active
            .borrow()
            .as_ref()
            .expect("session not connected")
            .clock
            .clone()
    }

    /// The metrics hub (lives across reconnects).
    pub fn metrics(&self) -> MetricsHub {
        self.shared.metrics.clone()
    }

    /// True while a connection is established (HELLO done, mux live).
    pub fn is_connected(&self) -> bool {
        self.shared.active.borrow().is_some()
    }

    async fn wait_active(&self) -> Active {
        let mut rx = self.shared.active.subscribe();
        let guard = rx
            .wait_for(|a| a.is_some())
            .await
            .expect("session supervisor alive");
        guard.clone().expect("checked is_some")
    }
}

/// Aborts the wrapped task when dropped (so cancelling a connection future
/// tears down its helper tasks).
struct AbortOnDrop(JoinHandle<()>);

impl Drop for AbortOnDrop {
    fn drop(&mut self) {
        self.0.abort();
    }
}

/// `Session::start` supervisor: discovery + connect + reconnect loop.
async fn supervise(cfg: SessionConfig, shared: Arc<Shared>, events: mpsc::Sender<SessionEvent>) {
    let listener = match QuinnListener::bind(cfg.bind) {
        Ok(l) => l,
        Err(err) => {
            tracing::error!(%err, "failed to bind QUIC listener");
            return;
        }
    };
    let udp_port = match listener.local_addr() {
        Ok(addr) => addr.port(),
        Err(err) => {
            tracing::error!(%err, "failed to read listener address");
            return;
        }
    };
    let mut discovery = Discovery::start(DiscoveryConfig {
        instance_id: cfg.instance_id,
        version_range: cfg.version_range,
        udp_port,
    });

    loop {
        let peer = discovery.next_peer().await;
        if events.send(SessionEvent::PeerDiscovered).await.is_err() {
            return; // application dropped the event receiver
        }
        let role = elect(cfg.instance_id, peer.instance_id);
        tracing::info!(?role, addr = %peer.addr, "peer discovered");

        let connected: Result<QuinnTransport, TransportError> = match role {
            Role::Dialer => QuinnTransport::dial(SocketAddr::V6(peer.addr)).await,
            Role::Listener => listener.accept().await,
        };
        let transport: Arc<dyn Transport> = match connected {
            Ok(t) => Arc::new(t),
            Err(err) => {
                tracing::warn!(%err, "connection attempt failed; back to discovery");
                continue;
            }
        };

        let reason = run_connection(transport, role, &cfg, &shared, &events).await;
        tracing::info!(?reason, "session disconnected");
        if events
            .send(SessionEvent::Disconnected(reason))
            .await
            .is_err()
        {
            return;
        }
        // Loop: auto-reconnect on the next discovery event.
    }
}

/// Drive one connection to completion; returns why it ended.
async fn run_connection(
    transport: Arc<dyn Transport>,
    role: Role,
    cfg: &SessionConfig,
    shared: &Arc<Shared>,
    events: &mpsc::Sender<SessionEvent>,
) -> DisconnectReason {
    let (version, clock, _hello_stream) = match establish(&transport, role, cfg).await {
        Ok(v) => v,
        Err(HandshakeError::VersionMismatch) => return DisconnectReason::VersionMismatch,
        Err(HandshakeError::Transport(_)) => return DisconnectReason::LinkLost,
        Err(HandshakeError::Protocol(msg)) => return DisconnectReason::ProtocolError(msg),
    };

    let mux_role = match role {
        Role::Dialer => MuxRole::Dialer,
        Role::Listener => MuxRole::Listener,
    };
    let mux = Arc::new(Mux::new(
        transport.clone(),
        clock.clone(),
        shared.metrics.clone(),
        mux_role,
    ));
    // `send` is a no-op (and drops the value!) when there are zero live
    // receivers, which is exactly the initial state here (`Shared::new`
    // discards the constructor's receiver). `send_replace` updates the
    // watched value unconditionally, so `Session::clock`/`is_connected`
    // (which read via `borrow`, not a subscription) see it immediately.
    shared.active.send_replace(Some(Active {
        mux: mux.clone(),
        clock,
    }));
    let _ = events.send(SessionEvent::Connected { version }).await;

    // Accept loop: stash remote-initiated channels, announce them.
    let _accept_guard = AbortOnDrop(tokio::spawn({
        let mux = mux.clone();
        let shared = shared.clone();
        let events = events.clone();
        async move {
            loop {
                let (desc, tx, rx) = mux.accept_channel().await;
                shared
                    .offered
                    .lock()
                    .expect("offered lock")
                    .insert(desc.id.0, (tx, rx));
                if events
                    .send(SessionEvent::ChannelOffered(desc))
                    .await
                    .is_err()
                {
                    return;
                }
            }
        }
    }));

    // Forward mux bandwidth-pressure events to the application (§10).
    let _pressure_guard = AbortOnDrop(tokio::spawn({
        let mut mux_events = mux.subscribe_events();
        let events = events.clone();
        async move {
            loop {
                let ev = match mux_events.recv().await {
                    Ok(ev) => ev,
                    // Lagged: a burst of events was dropped; keep going.
                    Err(tokio::sync::broadcast::error::RecvError::Lagged(_)) => continue,
                    Err(tokio::sync::broadcast::error::RecvError::Closed) => return,
                };
                let MuxEvent::BandwidthPressure {
                    channel,
                    allocated_bps,
                } = ev;
                if events
                    .send(SessionEvent::BandwidthPressure {
                        channel,
                        allocated_bps,
                    })
                    .await
                    .is_err()
                {
                    return;
                }
            }
        }
    }));

    // Run until the link dies (§10: closed() resolves on link loss).
    let _err = transport.closed().await;
    shared.active.send_replace(None);
    DisconnectReason::LinkLost
}

#[derive(Debug, thiserror::Error)]
enum HandshakeError {
    #[error("transport: {0}")]
    Transport(#[from] TransportError),
    #[error("no common protocol version")]
    VersionMismatch,
    #[error("protocol error: {0}")]
    Protocol(String),
}

/// HELLO/HelloAck + session-clock spawn on a fresh connection (§7).
///
/// Both sides send `Hello` on the first stream (dialer opens, listener
/// accepts) and negotiate independently — `negotiate` is symmetric, so a
/// version mismatch is detected cleanly on both sides. On success the
/// dialer (clock master) confirms with `HelloAck`, then the follower
/// (listener) opens the dedicated clock stream and starts probing.
///
/// Returns the negotiated version, the running clock, and the HELLO stream
/// (kept alive for the lifetime of the connection).
async fn establish(
    transport: &Arc<dyn Transport>,
    role: Role,
    cfg: &SessionConfig,
) -> Result<(u16, SessionClock, Box<dyn TransportStream>), HandshakeError> {
    let mut hello_stream = match role {
        Role::Dialer => transport.open_stream(CONTROL_STREAM_PRIORITY).await?,
        Role::Listener => transport.accept_stream().await?,
    };

    let hello = Hello {
        version_min: cfg.version_range.0,
        version_max: cfg.version_range.1,
        instance_id: cfg.instance_id,
        channels: cfg.channels.clone(),
    };
    let payload = postcard::to_allocvec(&hello)
        .map_err(|e| HandshakeError::Protocol(format!("encode HELLO: {e}")))?;
    send_control(&mut *hello_stream, FrameType::Hello, &payload).await?;

    let (header, payload) = recv_control(&mut *hello_stream).await?;
    if header.frame_type != FrameType::Hello {
        return Err(HandshakeError::Protocol(format!(
            "expected HELLO, got {:?}",
            header.frame_type
        )));
    }
    let peer: Hello = postcard::from_bytes(&payload)
        .map_err(|e| HandshakeError::Protocol(format!("decode HELLO: {e}")))?;

    let Some(version) = negotiate(cfg.version_range, (peer.version_min, peer.version_max)) else {
        return Err(HandshakeError::VersionMismatch);
    };

    match role {
        Role::Dialer => {
            // Dialer is the clock master and confirms the negotiated version.
            let ack = HelloAck {
                version,
                clock_master: true,
                channels: cfg.channels.clone(),
            };
            let payload = postcard::to_allocvec(&ack)
                .map_err(|e| HandshakeError::Protocol(format!("encode HelloAck: {e}")))?;
            send_control(&mut *hello_stream, FrameType::HelloAck, &payload).await?;
        }
        Role::Listener => {
            let (header, payload) = recv_control(&mut *hello_stream).await?;
            if header.frame_type != FrameType::HelloAck {
                return Err(HandshakeError::Protocol(format!(
                    "expected HelloAck, got {:?}",
                    header.frame_type
                )));
            }
            let ack: HelloAck = postcard::from_bytes(&payload)
                .map_err(|e| HandshakeError::Protocol(format!("decode HelloAck: {e}")))?;
            if ack.version != version {
                return Err(HandshakeError::Protocol(format!(
                    "peer confirmed version {} but we negotiated {version}",
                    ack.version
                )));
            }
            if !ack.clock_master {
                return Err(HandshakeError::Protocol(
                    "dialer must be clock master".into(),
                ));
            }
        }
    }

    // Dedicated clock stream (ChannelId(1) tier, latency-critical): the
    // follower sends probes so it opens the stream; the master answers.
    let clock = match role {
        Role::Dialer => {
            let stream = transport.accept_stream().await?;
            SessionClock::spawn_master(stream)
        }
        Role::Listener => {
            let stream = transport.open_stream(CONTROL_STREAM_PRIORITY).await?;
            SessionClock::spawn_follower(stream)
        }
    };

    Ok((version, clock, hello_stream))
}

/// Send one control frame (header + postcard payload) on a stream.
async fn send_control(
    stream: &mut dyn TransportStream,
    frame_type: FrameType,
    payload: &[u8],
) -> Result<(), HandshakeError> {
    let header = FrameHeader {
        channel: ChannelId::CONTROL,
        frame_type,
        flags: FLAG_END_OF_MESSAGE,
        timestamp: SessionTimestamp(0),
        seq: 0,
        frag_index: 0,
        frag_count: 1,
    };
    let mut out = Vec::with_capacity(FRAME_HEADER_LEN + payload.len());
    encode_frame(&header, payload, &mut out)
        .map_err(|e| HandshakeError::Protocol(e.to_string()))?;
    stream.send(&out).await?;
    Ok(())
}

/// Receive one full control frame from a stream.
async fn recv_control(
    stream: &mut dyn TransportStream,
) -> Result<(FrameHeader, Vec<u8>), HandshakeError> {
    let mut buf = vec![0u8; FRAME_HEADER_LEN];
    read_exact(stream, &mut buf).await?;
    // payload_len is the trailing u16 of the fixed header (little-endian).
    let payload_len =
        u16::from_le_bytes([buf[FRAME_HEADER_LEN - 2], buf[FRAME_HEADER_LEN - 1]]) as usize;
    buf.resize(FRAME_HEADER_LEN + payload_len, 0);
    read_exact(stream, &mut buf[FRAME_HEADER_LEN..]).await?;
    let (header, payload) =
        decode_frame(&buf).map_err(|e| HandshakeError::Protocol(e.to_string()))?;
    Ok((header, payload.to_vec()))
}

async fn read_exact(
    stream: &mut dyn TransportStream,
    buf: &mut [u8],
) -> Result<(), HandshakeError> {
    let mut filled = 0;
    while filled < buf.len() {
        let n = stream.recv(&mut buf[filled..]).await?;
        if n == 0 {
            return Err(TransportError::Closed.into());
        }
        filled += n;
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn default_config_is_sane() {
        let cfg = SessionConfig::default();
        assert_eq!(cfg.version_range, (1, 1));
        assert!(cfg.channels.is_empty());
        assert_ne!(cfg.instance_id, [0u8; 16], "instance id should be random");
        assert_eq!(cfg.bind.port(), 0);
    }

    #[test]
    fn disconnect_reason_equality() {
        assert_eq!(
            DisconnectReason::VersionMismatch,
            DisconnectReason::VersionMismatch
        );
        assert_ne!(
            DisconnectReason::VersionMismatch,
            DisconnectReason::LinkLost
        );
    }
}
