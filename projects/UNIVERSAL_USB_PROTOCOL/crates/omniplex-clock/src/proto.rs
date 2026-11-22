//! Clock protocol loop over a dedicated transport stream (plan Task 4.2).
//!
//! Master answers `ClockProbe` with `ClockReply`; follower sends a probe
//! every 100 ms on the dedicated clock stream (ChannelId(1),
//! latency-critical) and feeds the discipline filter. The master's session
//! time IS its local monotonic ns. The `(offset, drift, reference)` mapping
//! is published via `arc_swap::ArcSwap` so `now_session_ns()` is a lock-free
//! read (audio-hot-path safe).

use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Arc;
use std::time::Duration;

use arc_swap::ArcSwap;
use omniplex_transport::TransportStream;
use omniplex_wire::{
    decode_frame, encode_frame, ChannelId, ClockProbe, ClockReply, FrameHeader, FrameType,
    SessionTimestamp,
};

use crate::filter::{ClockFilter, ClockSample, ClockStats};
use crate::time::local_now_ns;

/// Injectable local monotonic clock (ns). Production uses
/// [`local_now_ns`]; tests inject tokio-paused or drift-scaled clocks.
pub type LocalClock = Arc<dyn Fn() -> u64 + Send + Sync>;

/// Dedicated clock channel per the plan (0 = control, 1 = clock).
const CLOCK_CHANNEL: ChannelId = ChannelId(1);
const PROBE_INTERVAL: Duration = Duration::from_millis(100);
const REPLY_TIMEOUT: Duration = Duration::from_millis(90);
/// Fixed wire header size: u16 + u8 + u8 + u64 + u32 + u16 + u16 + u16.
const HEADER_LEN: usize = 22;
/// Byte offset of the little-endian u16 `payload_len` field in the header.
const PAYLOAD_LEN_OFFSET: usize = 20;
const FLAG_END_OF_MESSAGE: u8 = 0x01;

/// Published lock-free mapping: `session(local) = base_session + (local - base_local) * rate`.
#[derive(Clone, Copy, Debug)]
struct Mapping {
    locked: bool,
    base_local: u64,
    base_session: f64,
    rate: f64,
}

struct Published {
    mapping: Mapping,
    stats: ClockStats,
}

struct Inner {
    published: Arc<ArcSwap<Published>>,
    clock: LocalClock,
    /// Highest session time ever returned; keeps `now_session_ns` monotonic.
    last_out: AtomicU64,
    task: tokio::task::JoinHandle<()>,
}

impl Drop for Inner {
    fn drop(&mut self) {
        self.task.abort();
    }
}

/// Handle to a running session clock (master or follower role).
#[derive(Clone)]
pub struct SessionClock {
    inner: Arc<Inner>,
}

impl SessionClock {
    /// Master: answers `ClockProbe` with `ClockReply`. The master's session
    /// time is its own local monotonic ns.
    pub fn spawn_master(stream: Box<dyn TransportStream>) -> Self {
        Self::spawn_master_with_clock(stream, Arc::new(local_now_ns))
    }

    /// Follower: probes every 100 ms, feeds the filter, publishes the
    /// mapping lock-free.
    pub fn spawn_follower(stream: Box<dyn TransportStream>) -> Self {
        Self::spawn_follower_with_clock(stream, Arc::new(local_now_ns))
    }

    /// [`Self::spawn_master`] with an injected local clock (for tests).
    pub fn spawn_master_with_clock(stream: Box<dyn TransportStream>, clock: LocalClock) -> Self {
        let published = Arc::new(ArcSwap::from_pointee(Published {
            // Identity mapping: master session time == master local time.
            mapping: Mapping {
                locked: true,
                base_local: 0,
                base_session: 0.0,
                rate: 1.0,
            },
            stats: ClockStats::default(),
        }));
        let task_clock = clock.clone();
        let task = tokio::spawn(async move {
            master_loop(stream, task_clock).await;
        });
        SessionClock {
            inner: Arc::new(Inner {
                published,
                clock,
                last_out: AtomicU64::new(0),
                task,
            }),
        }
    }

    /// [`Self::spawn_follower`] with an injected local clock (for tests).
    pub fn spawn_follower_with_clock(stream: Box<dyn TransportStream>, clock: LocalClock) -> Self {
        let published = Arc::new(ArcSwap::from_pointee(Published {
            mapping: Mapping {
                locked: false,
                base_local: 0,
                base_session: 0.0,
                rate: 1.0,
            },
            stats: ClockStats::default(),
        }));
        let task_published = published.clone();
        let task_clock = clock.clone();
        let task = tokio::spawn(async move {
            follower_loop(stream, task_clock, task_published).await;
        });
        SessionClock {
            inner: Arc::new(Inner {
                published,
                clock,
                last_out: AtomicU64::new(0),
                task,
            }),
        }
    }

    /// Current session time in ns. Lock-free (ArcSwap load + atomics), safe
    /// to call from the audio hot path. `None` until the clock is locked
    /// (master is locked from the start). Monotonic.
    pub fn now_session_ns(&self) -> Option<u64> {
        let published = self.inner.published.load();
        let m = published.mapping;
        if !m.locked {
            return None;
        }
        let now = (self.inner.clock)();
        let v = m.base_session + (now as f64 - m.base_local as f64) * m.rate;
        let v = if v < 0.0 { 0 } else { v.round() as u64 };
        let prev = self.inner.last_out.fetch_max(v, Ordering::AcqRel);
        Some(prev.max(v))
    }

    /// Latest filter statistics (all-zero for a master).
    pub fn stats(&self) -> ClockStats {
        self.inner.published.load().stats
    }
}

/// Encode and send one clock frame. `None` on transport/encode failure.
async fn send_frame(
    stream: &mut dyn TransportStream,
    frame_type: FrameType,
    seq: u32,
    payload: &[u8],
) -> Option<()> {
    let header = FrameHeader {
        channel: CLOCK_CHANNEL,
        frame_type,
        flags: FLAG_END_OF_MESSAGE,
        timestamp: SessionTimestamp(0),
        seq,
        frag_index: 0,
        frag_count: 1,
    };
    let mut out = Vec::with_capacity(HEADER_LEN + payload.len());
    encode_frame(&header, payload, &mut out).ok()?;
    stream.send(&out).await.ok()
}

/// Read one complete frame from the byte stream, buffering partial reads in
/// `acc`. `None` when the stream closes or errors.
async fn read_frame(
    stream: &mut dyn TransportStream,
    acc: &mut Vec<u8>,
) -> Option<(FrameHeader, Vec<u8>)> {
    loop {
        if acc.len() >= HEADER_LEN {
            let payload_len =
                u16::from_le_bytes([acc[PAYLOAD_LEN_OFFSET], acc[PAYLOAD_LEN_OFFSET + 1]]) as usize;
            let total = HEADER_LEN + payload_len;
            if acc.len() >= total {
                let frame: Vec<u8> = acc.drain(..total).collect();
                match decode_frame(&frame) {
                    Ok((header, payload)) => return Some((header, payload.to_vec())),
                    Err(_) => return None, // corrupt clock stream: give up
                }
            }
        }
        let mut buf = [0u8; 2048];
        let n = stream.recv(&mut buf).await.ok()?;
        if n == 0 {
            return None; // closed
        }
        acc.extend_from_slice(&buf[..n]);
    }
}

async fn master_loop(mut stream: Box<dyn TransportStream>, clock: LocalClock) {
    let mut acc: Vec<u8> = Vec::new();
    let mut seq = 0u32;
    loop {
        let Some((header, payload)) = read_frame(&mut *stream, &mut acc).await else {
            return;
        };
        if let FrameType::ClockProbe = header.frame_type {
            let t2 = clock();
            let Ok(probe) = postcard::from_bytes::<ClockProbe>(&payload) else {
                continue;
            };
            let t3 = clock();
            let reply = ClockReply {
                t1_ns: probe.t1_ns,
                t2_ns: t2,
                t3_ns: t3,
            };
            let Ok(body) = postcard::to_allocvec(&reply) else {
                return;
            };
            if send_frame(&mut *stream, FrameType::ClockReply, seq, &body)
                .await
                .is_none()
            {
                return;
            }
            seq = seq.wrapping_add(1);
        }
    }
}

/// Wait for the `ClockReply` matching probe time `t1`, skipping stale ones.
async fn wait_reply(
    stream: &mut dyn TransportStream,
    acc: &mut Vec<u8>,
    t1: u64,
) -> Option<ClockReply> {
    loop {
        let (header, payload) = read_frame(stream, acc).await?;
        if let FrameType::ClockReply = header.frame_type {
            if let Ok(reply) = postcard::from_bytes::<ClockReply>(&payload) {
                if reply.t1_ns == t1 {
                    return Some(reply);
                }
                // Stale reply from a timed-out probe: skip.
            }
        }
    }
}

async fn follower_loop(
    mut stream: Box<dyn TransportStream>,
    clock: LocalClock,
    published: Arc<ArcSwap<Published>>,
) {
    let mut filter = ClockFilter::new();
    let mut acc: Vec<u8> = Vec::new();
    let mut interval = tokio::time::interval(PROBE_INTERVAL);
    interval.set_missed_tick_behavior(tokio::time::MissedTickBehavior::Delay);
    let mut seq = 0u32;
    loop {
        interval.tick().await;
        let t1 = clock();
        let Ok(body) = postcard::to_allocvec(&ClockProbe { t1_ns: t1 }) else {
            return;
        };
        if send_frame(&mut *stream, FrameType::ClockProbe, seq, &body)
            .await
            .is_none()
        {
            return;
        }
        seq = seq.wrapping_add(1);
        match tokio::time::timeout(REPLY_TIMEOUT, wait_reply(&mut *stream, &mut acc, t1)).await {
            Ok(Some(reply)) => {
                let t4 = clock();
                filter.ingest(ClockSample {
                    t1: reply.t1_ns,
                    t2: reply.t2_ns,
                    t3: reply.t3_ns,
                    t4,
                });
                let mapping = match filter.mapping() {
                    Some((base_local, base_session, rate)) => Mapping {
                        locked: true,
                        base_local,
                        base_session,
                        rate,
                    },
                    None => Mapping {
                        locked: false,
                        base_local: 0,
                        base_session: 0.0,
                        rate: 1.0,
                    },
                };
                published.store(Arc::new(Published {
                    mapping,
                    stats: filter.stats(),
                }));
            }
            Ok(None) => return, // stream closed
            Err(_) => continue, // reply timed out; next probe
        }
    }
}
