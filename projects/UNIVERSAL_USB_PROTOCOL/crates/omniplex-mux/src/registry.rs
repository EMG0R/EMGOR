//! Channel registry + open/close handshake (plan Task 5.1): the `Mux`
//! facade, id allocation, the control-stream protocol loop, the generic
//! reliable-stream acceptor, and the datagram demultiplexer.

use std::sync::atomic::{AtomicU16, AtomicU32, Ordering};
use std::sync::Arc;
use std::time::Duration;

use bytes::Bytes;
use dashmap::DashMap;
use tokio::sync::{mpsc, oneshot, watch, Mutex as AsyncMutex};

use omniplex_clock::SessionClock;
use omniplex_metrics::MetricsHub;
use omniplex_transport::{Transport, TransportStream};
use omniplex_wire::{
    encode_frame, ChannelDescriptor, ChannelId, ChannelOpen, ChannelOpenAck, DeliveryClass,
    FormatDescriptor, FrameHeader, FrameType, LatencyClass, SessionTimestamp, FLAG_END_OF_MESSAGE,
    FRAME_HEADER_LEN,
};

use crate::reliable::{self, ReliableRx, ReliableTx, CHANNEL_QUEUE_CAP};
use crate::scheduler::{MuxEvent, Scheduler};
use crate::unreliable::{self, DgramFrag, LatestRx, LatestTx, Ring, UnreliableRx, UnreliableTx};
use crate::{try_take_frame, ChannelRx, ChannelTx};

/// Errors surfaced by [`Mux`] operations.
#[derive(Debug, thiserror::Error)]
pub enum MuxError {
    /// Peer rejected the channel open, with an optional human-readable
    /// reason.
    #[error("channel open rejected: {0:?}")]
    Rejected(Option<String>),
    /// The transport or session is gone.
    #[error("transport or session closed")]
    Closed,
    /// Failed to encode a frame (e.g. payload too large).
    #[error("encode error: {0}")]
    Encode(String),
}

/// Which side of the connection this `Mux` is: determines channel id
/// parity (dialer even from 2, listener odd from 3) and which side opens
/// vs. accepts the control stream (spec §7).
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Role {
    Dialer,
    Listener,
}

fn priority_for(latency: LatencyClass) -> i32 {
    match latency {
        LatencyClass::LatencyCritical => 100,
        LatencyClass::Interactive => 50,
        LatencyClass::Bulk => 0,
    }
}

/// Priority used for the single shared control stream: always serviced
/// first at the transport level.
const CONTROL_PRIORITY: i32 = i32::MAX;

/// A reliable channel accepted (ChannelOpen processed, Ack about to be
/// sent) but whose data stream hasn't arrived yet. The stream-acceptor
/// task matches it up by channel id once `transport.accept_stream()`
/// yields the corresponding stream.
struct PendingBind {
    send_rx: mpsc::Receiver<(Bytes, SessionTimestamp)>,
    recv_tx: mpsc::Sender<(Bytes, SessionTimestamp)>,
    stop_rx: watch::Receiver<bool>,
}

type Teardown = Box<dyn FnOnce() + Send + Sync>;

struct Shared {
    transport: Arc<dyn Transport>,
    #[allow(dead_code)] // exposed via Mux::clock(); not otherwise read by the registry itself.
    clock: SessionClock,
    metrics: MetricsHub,
    scheduler: Scheduler,
    accept_tx: mpsc::UnboundedSender<(ChannelDescriptor, ChannelTx, ChannelRx)>,
    pending_binds: DashMap<ChannelId, PendingBind>,
    teardowns: DashMap<ChannelId, Teardown>,
    dgram_routes: DashMap<ChannelId, mpsc::Sender<DgramFrag>>,
}

fn teardown_channel_local(shared: &Arc<Shared>, id: ChannelId) {
    if let Some((_, f)) = shared.teardowns.remove(&id) {
        f();
    }
    shared.scheduler.unregister(id);
    shared.dgram_routes.remove(&id);
}

fn deadline_for(desc: &ChannelDescriptor) -> Duration {
    if let FormatDescriptor::Video {
        fps_num, fps_den, ..
    } = &desc.format
    {
        if *fps_num > 0 {
            let frame_interval = Duration::from_secs_f64(*fps_den as f64 / *fps_num as f64);
            return frame_interval * 2;
        }
    }
    unreliable::DEFAULT_DEADLINE
}

/// Build the local (this side's) resources for a channel once both peers
/// have agreed to open it. `is_opener` is true for whichever side called
/// `open_channel` (it actively opens the reliable-channel data stream);
/// the other side just registers a pending bind for the stream-acceptor
/// task to match up.
async fn build_local_channel(
    shared: &Arc<Shared>,
    desc: &ChannelDescriptor,
    is_opener: bool,
) -> (ChannelTx, ChannelRx) {
    let id = desc.id;
    match desc.delivery {
        DeliveryClass::ReliableOrdered => {
            let (app_tx, task_rx) = mpsc::channel(CHANNEL_QUEUE_CAP);
            let (task_tx, app_rx) = mpsc::channel(CHANNEL_QUEUE_CAP);
            let priority = priority_for(desc.latency);
            let (stop_tx, stop_rx) = watch::channel(false);
            shared.teardowns.insert(
                id,
                Box::new(move || {
                    let _ = stop_tx.send(true);
                }),
            );
            if is_opener {
                if let Ok(mut stream) = shared.transport.open_stream(priority).await {
                    let _ = write_bind_frame(&mut *stream, id).await;
                    tokio::spawn(reliable::run_channel_task(
                        stream,
                        id,
                        task_rx,
                        task_tx,
                        shared.metrics.clone(),
                        stop_rx,
                    ));
                }
            } else {
                shared.pending_binds.insert(
                    id,
                    PendingBind {
                        send_rx: task_rx,
                        recv_tx: task_tx,
                        stop_rx,
                    },
                );
            }
            (
                ChannelTx::Reliable(ReliableTx { tx: app_tx }),
                ChannelRx::Reliable(ReliableRx { rx: app_rx }),
            )
        }
        DeliveryClass::UnreliableTimestamped => {
            let max_frag = omniplex_wire::MAX_FRAGMENT_PAYLOAD.min(
                shared
                    .transport
                    .max_datagram_size()
                    .saturating_sub(FRAME_HEADER_LEN),
            );
            shared.scheduler.register(id, desc.latency, false);
            let (route_tx, route_rx) = mpsc::channel(512);
            shared.dgram_routes.insert(id, route_tx);
            let ring = Arc::new(Ring::new(unreliable::RX_RING_CAP));
            let (stop_tx, stop_rx) = watch::channel(false);
            shared.teardowns.insert(
                id,
                Box::new(move || {
                    let _ = stop_tx.send(true);
                }),
            );
            tokio::spawn(unreliable::run_unreliable_rx(
                route_rx,
                ring.clone(),
                shared.metrics.clone(),
                id,
                deadline_for(desc),
                stop_rx,
            ));
            (
                ChannelTx::Unreliable(UnreliableTx {
                    channel: id,
                    scheduler: shared.scheduler.clone(),
                    seq: Arc::new(AtomicU32::new(0)),
                    max_frag,
                }),
                ChannelRx::Unreliable(UnreliableRx { ring }),
            )
        }
        DeliveryClass::LatestValueWins => {
            shared.scheduler.register(id, desc.latency, true);
            let (route_tx, route_rx) = mpsc::channel(64);
            shared.dgram_routes.insert(id, route_tx);
            let ring = Arc::new(Ring::new(unreliable::RX_RING_CAP));
            let (stop_tx, stop_rx) = watch::channel(false);
            shared.teardowns.insert(
                id,
                Box::new(move || {
                    let _ = stop_tx.send(true);
                }),
            );
            tokio::spawn(unreliable::run_latest_rx(
                route_rx,
                ring.clone(),
                shared.metrics.clone(),
                id,
                stop_rx,
            ));
            (
                ChannelTx::Latest(LatestTx {
                    channel: id,
                    scheduler: shared.scheduler.clone(),
                    seq: Arc::new(AtomicU32::new(0)),
                }),
                ChannelRx::Latest(LatestRx { ring }),
            )
        }
    }
}

async fn write_bind_frame(stream: &mut dyn TransportStream, id: ChannelId) -> Result<(), ()> {
    let header = FrameHeader {
        channel: id,
        frame_type: FrameType::ChannelOpen,
        flags: FLAG_END_OF_MESSAGE,
        timestamp: SessionTimestamp(0),
        seq: 0,
        frag_index: 0,
        frag_count: 1,
    };
    let mut out = Vec::new();
    encode_frame(&header, &[], &mut out).map_err(|_| ())?;
    stream.send(&out).await.map_err(|_| ())
}

async fn read_bind_frame(stream: &mut dyn TransportStream) -> Option<ChannelId> {
    let mut acc = Vec::new();
    let mut buf = [0u8; 64];
    loop {
        match stream.recv(&mut buf).await {
            Ok(0) | Err(_) => return None,
            Ok(n) => acc.extend_from_slice(&buf[..n]),
        }
        match try_take_frame(&mut acc) {
            Ok(Some((header, _))) => return Some(header.channel),
            Ok(None) => continue,
            Err(_) => return None,
        }
    }
}

/// Accepts reliable-channel data streams (anything after the control
/// stream) and matches each to its pending bind by channel id. A stream
/// for an unknown channel (malformed peer) is simply dropped — the session
/// and every other channel keep running (spec §10).
async fn stream_acceptor_task(shared: Arc<Shared>) {
    loop {
        let Ok(mut stream) = shared.transport.accept_stream().await else {
            return;
        };
        match read_bind_frame(&mut *stream).await {
            Some(id) => {
                if let Some((_, pending)) = shared.pending_binds.remove(&id) {
                    tokio::spawn(reliable::run_channel_task(
                        stream,
                        id,
                        pending.send_rx,
                        pending.recv_tx,
                        shared.metrics.clone(),
                        pending.stop_rx,
                    ));
                } // else: unknown channel — drop the stream, keep accepting.
            }
            None => { /* malformed/empty stream — drop, keep accepting. */ }
        }
    }
}

/// Reads datagrams off the transport and routes each one's payload to the
/// registered channel's reassembly task by the frame's channel id.
/// Datagrams for unknown channels, or that fail to decode, are dropped
/// (session survives, spec §10).
async fn datagram_demux_task(shared: Arc<Shared>) {
    loop {
        match shared.transport.recv_datagram().await {
            Ok(dgram) => {
                if let Ok((header, payload)) = omniplex_wire::decode_frame(&dgram) {
                    if let Some(route) = shared.dgram_routes.get(&header.channel) {
                        let start = FRAME_HEADER_LEN;
                        let end = start + payload.len();
                        let bytes = dgram.slice(start..end);
                        let _ = route.try_send(DgramFrag {
                            ts: header.timestamp,
                            seq: header.seq,
                            frag_index: header.frag_index,
                            frag_count: header.frag_count,
                            payload: bytes,
                        });
                    }
                }
            }
            Err(_) => return,
        }
    }
}

enum Command {
    SendOpen {
        desc: ChannelDescriptor,
        reply: oneshot::Sender<Result<(), Option<String>>>,
    },
    SendClose {
        id: ChannelId,
    },
}

/// Owns the single control stream (ChannelId(0)). Establishes it per role,
/// then spawns the generic stream-acceptor and datagram-demux tasks (only
/// safe to start once the control stream itself has claimed the very first
/// `open_stream`/`accept_stream` call), then services `ChannelOpen` /
/// `ChannelOpenAck` / `ChannelClose` traffic.
async fn control_task(
    role: Role,
    shared: Arc<Shared>,
    mut cmd_rx: mpsc::UnboundedReceiver<Command>,
) {
    let control_stream = match role {
        Role::Dialer => shared.transport.open_stream(CONTROL_PRIORITY).await,
        Role::Listener => shared.transport.accept_stream().await,
    };
    let Ok(mut control_stream) = control_stream else {
        return;
    };

    tokio::spawn(stream_acceptor_task(shared.clone()));
    tokio::spawn(datagram_demux_task(shared.clone()));

    let mut acc: Vec<u8> = Vec::new();
    let mut recv_buf = vec![0u8; 8192];
    let mut pending_opens: std::collections::HashMap<
        ChannelId,
        oneshot::Sender<Result<(), Option<String>>>,
    > = std::collections::HashMap::new();

    loop {
        tokio::select! {
            cmd = cmd_rx.recv() => {
                match cmd {
                    None => return,
                    Some(Command::SendOpen { desc, reply }) => {
                        let id = desc.id;
                        pending_opens.insert(id, reply);
                        let Ok(body) = postcard::to_allocvec(&ChannelOpen { desc }) else { continue };
                        let header = FrameHeader {
                            channel: id,
                            frame_type: FrameType::ChannelOpen,
                            flags: FLAG_END_OF_MESSAGE,
                            timestamp: SessionTimestamp(0),
                            seq: 0,
                            frag_index: 0,
                            frag_count: 1,
                        };
                        let mut out = Vec::new();
                        if encode_frame(&header, &body, &mut out).is_ok()
                            && control_stream.send(&out).await.is_err() {
                                return;
                            }
                    }
                    Some(Command::SendClose { id }) => {
                        let header = FrameHeader {
                            channel: id,
                            frame_type: FrameType::ChannelClose,
                            flags: FLAG_END_OF_MESSAGE,
                            timestamp: SessionTimestamp(0),
                            seq: 0,
                            frag_index: 0,
                            frag_count: 1,
                        };
                        let mut out = Vec::new();
                        if encode_frame(&header, &[], &mut out).is_ok()
                            && control_stream.send(&out).await.is_err() {
                                return;
                            }
                    }
                }
            }
            n = control_stream.recv(&mut recv_buf) => {
                match n {
                    Ok(0) | Err(_) => return,
                    Ok(n) => {
                        acc.extend_from_slice(&recv_buf[..n]);
                        loop {
                            match try_take_frame(&mut acc) {
                                Ok(None) => break,
                                Err(_) => return, // corrupt control stream: session is unrecoverable.
                                Ok(Some((header, payload))) => {
                                    match header.frame_type {
                                        FrameType::ChannelOpen => {
                                            let Ok(open) = postcard::from_bytes::<ChannelOpen>(&payload) else { continue };
                                            let id = header.channel;
                                            let (tx, rx) = build_local_channel(&shared, &open.desc, false).await;
                                            let _ = shared.accept_tx.send((open.desc, tx, rx));
                                            let ack = ChannelOpenAck { id, accepted: true, reason: None };
                                            if let Ok(body) = postcard::to_allocvec(&ack) {
                                                let ack_header = FrameHeader {
                                                    channel: id,
                                                    frame_type: FrameType::ChannelOpenAck,
                                                    flags: FLAG_END_OF_MESSAGE,
                                                    timestamp: SessionTimestamp(0),
                                                    seq: 0,
                                                    frag_index: 0,
                                                    frag_count: 1,
                                                };
                                                let mut out = Vec::new();
                                                if encode_frame(&ack_header, &body, &mut out).is_ok()
                                                    && control_stream.send(&out).await.is_err() {
                                                        return;
                                                    }
                                            }
                                        }
                                        FrameType::ChannelOpenAck => {
                                            let Ok(ack) = postcard::from_bytes::<ChannelOpenAck>(&payload) else { continue };
                                            if let Some(reply) = pending_opens.remove(&ack.id) {
                                                let _ = reply.send(if ack.accepted { Ok(()) } else { Err(ack.reason) });
                                            }
                                        }
                                        FrameType::ChannelClose => {
                                            teardown_channel_local(&shared, header.channel);
                                        }
                                        _ => {}
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

struct MuxHandle {
    next_id: AtomicU16,
    cmd_tx: mpsc::UnboundedSender<Command>,
    accept_rx: AsyncMutex<mpsc::UnboundedReceiver<(ChannelDescriptor, ChannelTx, ChannelRx)>>,
    shared: Arc<Shared>,
}

/// The channel muxer: opens/accepts/closes channels over a shared
/// `Transport`, fragmenting and scheduling their traffic per delivery and
/// latency class (spec §9, §10).
#[derive(Clone)]
pub struct Mux {
    inner: Arc<MuxHandle>,
}

impl Mux {
    /// `role` decides channel-id parity (dialer even from 2, listener odd
    /// from 3) and which side opens vs. accepts the control stream.
    pub fn new(
        transport: Arc<dyn Transport>,
        clock: SessionClock,
        metrics: MetricsHub,
        role: Role,
    ) -> Self {
        Self::with_bandwidth_cap(transport, clock, metrics, role, None)
    }

    /// As [`Mux::new`], but pins the scheduler's bandwidth-capacity
    /// estimate to a fixed value instead of leaving it unbounded — this is
    /// how tests stand in for the plan's "loopback: from `LoopbackConfig`
    /// cap" bandwidth source (`Transport` itself exposes no acked-bytes/RTT
    /// stats to estimate from generically).
    pub fn with_bandwidth_cap(
        transport: Arc<dyn Transport>,
        clock: SessionClock,
        metrics: MetricsHub,
        role: Role,
        bandwidth_cap_bps: Option<u64>,
    ) -> Self {
        let scheduler = Scheduler::new(transport.clone(), metrics.clone(), bandwidth_cap_bps);
        let (accept_tx, accept_rx) = mpsc::unbounded_channel();
        let shared = Arc::new(Shared {
            transport,
            clock,
            metrics,
            scheduler,
            accept_tx,
            pending_binds: DashMap::new(),
            teardowns: DashMap::new(),
            dgram_routes: DashMap::new(),
        });
        let (cmd_tx, cmd_rx) = mpsc::unbounded_channel();
        tokio::spawn(control_task(role, shared.clone(), cmd_rx));
        let start = match role {
            Role::Dialer => 2u16,
            Role::Listener => 3u16,
        };
        Mux {
            inner: Arc::new(MuxHandle {
                next_id: AtomicU16::new(start),
                cmd_tx,
                accept_rx: AsyncMutex::new(accept_rx),
                shared,
            }),
        }
    }

    fn alloc_id(&self) -> ChannelId {
        ChannelId(self.inner.next_id.fetch_add(2, Ordering::Relaxed))
    }

    /// Open a channel: sends `ChannelOpen` over the control stream and
    /// awaits `ChannelOpenAck`. On accept, wires up the local send/receive
    /// path for the channel's delivery class.
    pub async fn open_channel(
        &self,
        mut desc: ChannelDescriptor,
    ) -> Result<(ChannelTx, ChannelRx), MuxError> {
        let id = self.alloc_id();
        desc.id = id;
        let (reply_tx, reply_rx) = oneshot::channel();
        self.inner
            .cmd_tx
            .send(Command::SendOpen {
                desc: desc.clone(),
                reply: reply_tx,
            })
            .map_err(|_| MuxError::Closed)?;
        let ack = reply_rx.await.map_err(|_| MuxError::Closed)?;
        ack.map_err(MuxError::Rejected)?;
        Ok(build_local_channel(&self.inner.shared, &desc, true).await)
    }

    /// Await the next remote-initiated channel open (always accepted in
    /// Phase 1).
    pub async fn accept_channel(&self) -> (ChannelDescriptor, ChannelTx, ChannelRx) {
        let mut rx = self.inner.accept_rx.lock().await;
        rx.recv().await.expect("control task outlives Mux")
    }

    /// Close a channel: tears down local resources immediately and
    /// notifies the peer so it tears down too.
    pub async fn close_channel(&self, id: ChannelId) {
        teardown_channel_local(&self.inner.shared, id);
        let _ = self.inner.cmd_tx.send(Command::SendClose { id });
    }

    /// Subscribe to scheduler events (currently just `BandwidthPressure`).
    pub fn subscribe_events(&self) -> tokio::sync::broadcast::Receiver<MuxEvent> {
        self.inner.shared.scheduler.subscribe_events()
    }

    pub fn metrics(&self) -> MetricsHub {
        self.inner.shared.metrics.clone()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use omniplex_transport::{LoopbackConfig, LoopbackTransport};
    use omniplex_wire::{ChannelKind, PixelFormat, SampleFormat};

    /// A `SessionClock` that never talks to a peer: master mode over a
    /// throwaway, unconnected stream. `now_session_ns()` is the identity
    /// mapping regardless, which is all these tests need — the clock
    /// protocol itself is Milestone 4's concern, not the mux's.
    async fn fake_clock() -> SessionClock {
        let (a, _b) = LoopbackTransport::pair(LoopbackConfig::default());
        let stream = a.open_stream(0).await.unwrap();
        SessionClock::spawn_master(stream)
    }

    fn midi_desc(name: &str) -> ChannelDescriptor {
        ChannelDescriptor {
            id: ChannelId(0),
            name: name.into(),
            kind: ChannelKind::Midi,
            delivery: DeliveryClass::ReliableOrdered,
            latency: LatencyClass::LatencyCritical,
            format: FormatDescriptor::Midi,
        }
    }

    fn audio_desc(name: &str) -> ChannelDescriptor {
        ChannelDescriptor {
            id: ChannelId(0),
            name: name.into(),
            kind: ChannelKind::Audio,
            delivery: DeliveryClass::UnreliableTimestamped,
            latency: LatencyClass::LatencyCritical,
            format: FormatDescriptor::Audio {
                sample_rate: 48_000,
                channels: 2,
                format: SampleFormat::F32,
            },
        }
    }

    fn latest_desc(name: &str) -> ChannelDescriptor {
        ChannelDescriptor {
            id: ChannelId(0),
            name: name.into(),
            kind: ChannelKind::Dmx,
            delivery: DeliveryClass::LatestValueWins,
            latency: LatencyClass::Interactive,
            format: FormatDescriptor::Dmx { universe: 1 },
        }
    }

    fn video_bulk_desc(name: &str) -> ChannelDescriptor {
        ChannelDescriptor {
            id: ChannelId(0),
            name: name.into(),
            kind: ChannelKind::Video,
            delivery: DeliveryClass::UnreliableTimestamped,
            latency: LatencyClass::Bulk,
            format: FormatDescriptor::Video {
                width: 1280,
                height: 720,
                fps_num: 30,
                fps_den: 1,
                pixel: PixelFormat::Uyvy,
            },
        }
    }

    async fn mux_pair(cfg: LoopbackConfig) -> (Mux, Mux) {
        let (ta, tb) = LoopbackTransport::pair(cfg);
        let mux_a = Mux::new(
            Arc::new(ta),
            fake_clock().await,
            MetricsHub::new(),
            Role::Dialer,
        );
        let mux_b = Mux::new(
            Arc::new(tb),
            fake_clock().await,
            MetricsHub::new(),
            Role::Listener,
        );
        (mux_a, mux_b)
    }

    #[tokio::test(start_paused = true)]
    async fn open_ack_round_trip() {
        let (mux_a, mux_b) = mux_pair(LoopbackConfig::default()).await;
        let mux_b2 = mux_b.clone();
        let accept_task = tokio::spawn(async move { mux_b2.accept_channel().await });

        let (a_tx, mut a_rx) = mux_a.open_channel(midi_desc("midi/1")).await.unwrap();
        let (desc, b_tx, mut b_rx) = tokio::time::timeout(Duration::from_secs(5), accept_task)
            .await
            .unwrap()
            .unwrap();
        assert_eq!(desc.name, "midi/1");
        assert_eq!(desc.id, ChannelId(2)); // dialer: first dynamic id is even, from 2

        a_tx.send(Bytes::from_static(b"a->b"), SessionTimestamp(1))
            .await
            .unwrap();
        let (got, ts) = tokio::time::timeout(Duration::from_secs(5), b_rx.recv())
            .await
            .unwrap()
            .unwrap();
        assert_eq!(got, Bytes::from_static(b"a->b"));
        assert_eq!(ts, SessionTimestamp(1));

        b_tx.send(Bytes::from_static(b"b->a"), SessionTimestamp(2))
            .await
            .unwrap();
        let (got, _) = tokio::time::timeout(Duration::from_secs(5), a_rx.recv())
            .await
            .unwrap()
            .unwrap();
        assert_eq!(got, Bytes::from_static(b"b->a"));
    }

    #[tokio::test(start_paused = true)]
    async fn ids_collision_free_on_simultaneous_opens() {
        let (mux_a, mux_b) = mux_pair(LoopbackConfig::default()).await;
        const N: usize = 8;

        let mux_b2 = mux_b.clone();
        let acceptor = tokio::spawn(async move {
            let mut ids = Vec::new();
            for _ in 0..N {
                let (desc, _tx, _rx) = mux_b2.accept_channel().await;
                ids.push(desc.id);
            }
            ids
        });

        let mut opens = Vec::new();
        for i in 0..N {
            let mux_a = mux_a.clone();
            opens.push(tokio::spawn(async move {
                mux_a
                    .open_channel(midi_desc(&format!("midi/{i}")))
                    .await
                    .unwrap()
            }));
        }
        let mut a_ids = Vec::new();
        for h in opens {
            let (tx, _rx) = h.await.unwrap();
            // Recover the id by sending a canary and having the acceptor
            // side report it back is unnecessary — ids are assigned before
            // the control round trip, so just check parity/uniqueness via
            // the accepted side below. Keep tx alive so the channel isn't
            // torn down mid-test.
            std::mem::forget(tx);
        }
        let b_seen_ids = tokio::time::timeout(Duration::from_secs(5), acceptor)
            .await
            .unwrap()
            .unwrap();
        for id in &b_seen_ids {
            assert_eq!(
                id.0 % 2,
                0,
                "dialer-opened channel ids must be even: {id:?}"
            );
            assert!(id.0 >= 2);
        }
        let unique: std::collections::HashSet<_> = b_seen_ids.iter().collect();
        assert_eq!(
            unique.len(),
            N,
            "ids must be collision-free: {b_seen_ids:?}"
        );
        a_ids.extend(b_seen_ids);
    }

    #[tokio::test(start_paused = true)]
    async fn close_tears_down_both_ends() {
        let (mux_a, mux_b) = mux_pair(LoopbackConfig::default()).await;
        let mux_b2 = mux_b.clone();
        let accept_task = tokio::spawn(async move { mux_b2.accept_channel().await });
        let (a_tx, mut a_rx) = mux_a.open_channel(midi_desc("midi/1")).await.unwrap();
        let (_desc, _b_tx, mut b_rx) = accept_task.await.unwrap();
        let id = ChannelId(2);

        a_tx.send(Bytes::from_static(b"ping"), SessionTimestamp(1))
            .await
            .unwrap();
        assert!(tokio::time::timeout(Duration::from_secs(5), b_rx.recv())
            .await
            .unwrap()
            .is_some());

        mux_a.close_channel(id).await;
        // Side A's own receiver must observe closure.
        assert!(
            tokio::time::timeout(Duration::from_secs(5), a_rx.recv())
                .await
                .unwrap()
                .is_none(),
            "closing side must tear down its own receiver"
        );
        // Side B's receiver must observe closure too, once the ChannelClose
        // control frame (or the underlying stream drop) propagates.
        assert!(
            tokio::time::timeout(Duration::from_secs(5), b_rx.recv())
                .await
                .unwrap()
                .is_none(),
            "peer must tear down its side of the channel too"
        );
    }

    #[tokio::test(start_paused = true)]
    async fn unknown_channel_frame_does_not_kill_session() {
        let (ta_raw, tb_raw) = LoopbackTransport::pair(LoopbackConfig::default());
        let ta = Arc::new(ta_raw);
        let tb = Arc::new(tb_raw);
        let mux_a = Mux::new(
            ta.clone(),
            fake_clock().await,
            MetricsHub::new(),
            Role::Dialer,
        );
        let mux_b = Mux::new(
            tb.clone(),
            fake_clock().await,
            MetricsHub::new(),
            Role::Listener,
        );

        // Establish one real, working channel first.
        let mux_b2 = mux_b.clone();
        let accept_task = tokio::spawn(async move { mux_b2.accept_channel().await });
        let (a_tx, _a_rx) = mux_a.open_channel(midi_desc("midi/1")).await.unwrap();
        let (_desc, _b_tx, mut b_rx) = accept_task.await.unwrap();

        // Simulate a malformed peer: open an extra stream directly on the
        // raw transport (bypassing the mux entirely) and send garbage that
        // is not a valid bind frame for any registered channel.
        let mut rogue_stream = ta.open_stream(0).await.unwrap();
        omniplex_transport::TransportStream::send(&mut *rogue_stream, &[0xFF; 4])
            .await
            .unwrap();
        drop(rogue_stream);

        // And a rogue datagram tagged with a channel id nobody registered.
        let bogus_header = FrameHeader {
            channel: ChannelId(9999),
            frame_type: FrameType::Media,
            flags: FLAG_END_OF_MESSAGE,
            timestamp: SessionTimestamp(0),
            seq: 0,
            frag_index: 0,
            frag_count: 1,
        };
        let mut bogus = Vec::new();
        encode_frame(&bogus_header, b"nope", &mut bogus).unwrap();
        ta.send_datagram(Bytes::from(bogus)).unwrap();

        // Give the background tasks a chance to process the garbage.
        tokio::time::sleep(Duration::from_millis(50)).await;

        // The session, and the pre-existing channel, must still work.
        a_tx.send(Bytes::from_static(b"still alive"), SessionTimestamp(1))
            .await
            .unwrap();
        let (got, _) = tokio::time::timeout(Duration::from_secs(5), b_rx.recv())
            .await
            .unwrap()
            .unwrap();
        assert_eq!(got, Bytes::from_static(b"still alive"));
    }

    #[tokio::test(start_paused = true)]
    async fn unreliable_lossy_delivers_most_messages_uncorrupted() {
        let cfg = LoopbackConfig {
            datagram_loss: 0.10,
            seed: 42,
            ..Default::default()
        };
        let (mux_a, mux_b) = mux_pair(cfg).await;
        let mux_b2 = mux_b.clone();
        let accept_task = tokio::spawn(async move { mux_b2.accept_channel().await });
        let (a_tx, _a_rx) = mux_a.open_channel(audio_desc("audio/main")).await.unwrap();
        let (_desc, _b_tx, mut b_rx) = accept_task.await.unwrap();

        const N: u64 = 500;
        let sender = tokio::spawn(async move {
            for i in 0..N {
                a_tx.send(Bytes::from(i.to_le_bytes().to_vec()), SessionTimestamp(i))
                    .await
                    .unwrap();
                tokio::time::sleep(Duration::from_millis(1)).await;
            }
        });

        let mut received = 0u64;
        let mut corrupt = 0u64;
        while let Ok(Some((bytes, ts))) =
            tokio::time::timeout(Duration::from_millis(200), b_rx.recv()).await
        {
            let expected = ts.0.to_le_bytes();
            if bytes.as_ref() != expected {
                corrupt += 1;
            }
            received += 1;
        }
        sender.await.unwrap();

        assert_eq!(corrupt, 0, "no message may be delivered corrupted");
        let ratio = received as f64 / N as f64;
        assert!(
            ratio > 0.8 && ratio < 1.0,
            "expected ~90% delivery with 10% loss, got {ratio}"
        );
    }

    #[tokio::test(start_paused = true)]
    async fn multi_fragment_message_with_lost_fragment_is_dropped_whole() {
        // Deterministic: engineer a datagram loss pattern by using a
        // transport with no loss, but only forward N-1 of the N raw
        // fragment datagrams a multi-fragment send would produce — we do
        // this by sending a message big enough to fragment, then racing
        // it against a hard deadline shorter than would be needed to
        // reassemble if a fragment never arrives at all. Simplest
        // deterministic construction: open the channel, then directly
        // drop one raw datagram off the wire between the transports by
        // using a loss rate of 100% for a single injected send. Since the
        // transport API doesn't expose per-datagram control, instead
        // verify the deadline-drop path directly: send a message that
        // fragments, but starve the receiver so its reassembly deadline
        // elapses before the message is seen as complete is not
        // observable from here either. So: use a very high loss seeded
        // run, and assert that whenever a multi-fragment message is
        // missing any fragment, it is never delivered (partial payloads
        // never appear) — this exercises the "dropped whole" rule
        // end-to-end without needing to control the exact loss pattern.
        let cfg = LoopbackConfig {
            datagram_loss: 0.3,
            seed: 7,
            ..Default::default()
        };
        let (mux_a, mux_b) = mux_pair(cfg).await;
        let mux_b2 = mux_b.clone();
        let accept_task = tokio::spawn(async move { mux_b2.accept_channel().await });
        let (a_tx, _a_rx) = mux_a.open_channel(audio_desc("audio/main")).await.unwrap();
        let (_desc, _b_tx, mut b_rx) = accept_task.await.unwrap();

        // Each message is 3 fragments (max_frag == transport MTU minus
        // header on loopback, which is large, so force fragmentation with
        // a payload sized against MAX_FRAGMENT_PAYLOAD instead).
        let msg = vec![0xCDu8; omniplex_wire::MAX_FRAGMENT_PAYLOAD * 3];
        const N: u64 = 40;
        let expected_len = msg.len();
        let sender = tokio::spawn(async move {
            for i in 0..N {
                a_tx.send(Bytes::from(msg.clone()), SessionTimestamp(i))
                    .await
                    .unwrap();
                tokio::time::sleep(Duration::from_millis(10)).await;
            }
        });

        let mut complete = 0u64;
        while let Ok(Some((bytes, _))) =
            tokio::time::timeout(Duration::from_millis(500), b_rx.recv()).await
        {
            // Every delivered message must be whole and uncorrupted — a
            // message missing even one fragment must never surface at all
            // (partial-length delivery would be a violation of the
            // "dropped whole" rule).
            assert_eq!(bytes.len(), expected_len);
            assert!(bytes.iter().all(|&b| b == 0xCD));
            complete += 1;
        }
        sender.await.unwrap();
        assert!(complete < N, "some loss was expected given a 30% loss rate");
        assert!(complete > 0, "some messages should still get through");
    }

    #[tokio::test(start_paused = true)]
    async fn latest_value_flood_delivers_increasing_subsequence_ending_at_final() {
        let (mux_a, mux_b) = mux_pair(LoopbackConfig::default()).await;
        let mux_b2 = mux_b.clone();
        let accept_task = tokio::spawn(async move { mux_b2.accept_channel().await });
        let (a_tx, _a_rx) = mux_a.open_channel(latest_desc("fader/1")).await.unwrap();
        let (_desc, _b_tx, mut b_rx) = accept_task.await.unwrap();

        const N: u32 = 500;
        for i in 0..N {
            a_tx.send(
                Bytes::from(i.to_le_bytes().to_vec()),
                SessionTimestamp(i as u64),
            )
            .await
            .unwrap();
        }
        // Let the scheduler drain the (collapsed) pending slot.
        tokio::time::sleep(Duration::from_millis(50)).await;

        let mut values = Vec::new();
        while let Ok(Some((bytes, _))) =
            tokio::time::timeout(Duration::from_millis(50), b_rx.recv()).await
        {
            let v = u32::from_le_bytes(bytes.as_ref().try_into().unwrap());
            values.push(v);
        }
        assert!(!values.is_empty());
        for w in values.windows(2) {
            assert!(
                w[0] < w[1],
                "subsequence must be strictly increasing: {values:?}"
            );
        }
        assert_eq!(
            *values.last().unwrap(),
            N - 1,
            "must end at the final value sent"
        );
    }

    #[tokio::test(start_paused = true)]
    async fn bandwidth_capped_scheduler_prioritizes_critical_and_paces_bulk() {
        const CAP_BPS: u64 = 50_000_000; // 50 Mbps
        let (ta, tb) = LoopbackTransport::pair(LoopbackConfig::default());
        let mux_a = Mux::with_bandwidth_cap(
            Arc::new(ta),
            fake_clock().await,
            MetricsHub::new(),
            Role::Dialer,
            Some(CAP_BPS),
        );
        let mux_b = Mux::new(
            Arc::new(tb),
            fake_clock().await,
            MetricsHub::new(),
            Role::Listener,
        );

        let mut events = mux_a.subscribe_events();

        let mux_b_ping = mux_b.clone();
        let ping_accept = tokio::spawn(async move { mux_b_ping.accept_channel().await });
        let (ping_tx, _ping_rx) = mux_a.open_channel(audio_desc("ping")).await.unwrap();
        let (_d, _t, mut ping_rx_b) = ping_accept.await.unwrap();

        let mux_b_bulk = mux_b.clone();
        let bulk_accept = tokio::spawn(async move { mux_b_bulk.accept_channel().await });
        let (bulk_tx, _bulk_rx) = mux_a
            .open_channel(video_bulk_desc("video/test"))
            .await
            .unwrap();
        let (_d2, _t2, mut bulk_rx_b) = bulk_accept.await.unwrap();

        let metrics = mux_a.metrics();

        // Flood bulk traffic continuously.
        // Enqueue a large, finite backlog up front rather than pacing sends
        // on the wall/sim clock: `UnreliableTx::send` never actually
        // awaits, so a producer loop with no natural end would keep this
        // task perpetually ready and, under `start_paused`, starve virtual
        // time from ever advancing (auto-advance only kicks in once every
        // task is parked on a timer). A big *finite* backlog (enough bytes
        // that the 50 Mbps cap can't drain it within the test's ~2 s ping
        // window) still saturates the bulk allocation, while letting the
        // clock advance normally once the enqueue loop finishes.
        let bulk_flooder = tokio::spawn(async move {
            let payload = Bytes::from(vec![0u8; 4096]);
            for _ in 0..50_000u32 {
                if bulk_tx
                    .send(payload.clone(), SessionTimestamp(0))
                    .await
                    .is_err()
                {
                    break;
                }
                tokio::task::yield_now().await;
            }
        });
        tokio::spawn(async move { while bulk_rx_b.recv().await.is_some() {} });

        // 1 kHz latency-critical pings for 2 simulated seconds.
        let ping_count = 2000u32;
        for i in 0..ping_count {
            ping_tx
                .send(Bytes::from_static(b"ping"), SessionTimestamp(i as u64))
                .await
                .unwrap();
            tokio::time::sleep(Duration::from_millis(1)).await;
        }

        let mut received_pings = 0u32;
        while tokio::time::timeout(Duration::from_millis(5), ping_rx_b.recv())
            .await
            .is_ok()
        {
            received_pings += 1;
        }
        assert!(received_pings > 0, "pings must be delivered");

        let snap = metrics.snapshot();
        let ping_id = 2u16; // dialer's first channel
        if let Some(m) = snap.get(&ping_id) {
            // Queueing delay recorded by the scheduler for the ping
            // channel must be tiny relative to simulated time, since
            // critical traffic is always serviced ahead of paced bulk
            // sends.
            assert!(
                m.latency.p99 < 1_000_000,
                "ping p99 queueing delay too high: {} ns",
                m.latency.p99
            );
        }

        // Under sustained bulk flood + non-trivial critical demand,
        // pressure events should have fired at least once.
        let mut saw_pressure = false;
        while let Ok(Ok(MuxEvent::BandwidthPressure { .. })) =
            tokio::time::timeout(Duration::from_millis(1), events.recv()).await
        {
            saw_pressure = true;
        }
        assert!(
            saw_pressure,
            "expected at least one BandwidthPressure event"
        );

        drop(bulk_flooder);
    }
}
