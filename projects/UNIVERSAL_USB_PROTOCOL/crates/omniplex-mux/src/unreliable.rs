//! Unreliable-timestamped and latest-value-wins datagram paths (plan
//! Task 5.3). Sends funnel through the [`crate::scheduler::Scheduler`];
//! receives are demultiplexed by channel id (see `registry.rs`) into a
//! per-channel reassembly task defined here.

use std::collections::HashMap;
use std::sync::atomic::{AtomicBool, AtomicU32, Ordering};
use std::sync::{Arc, Mutex};
use std::time::Duration;

use bytes::Bytes;
use tokio::sync::{mpsc, watch, Notify};
use tokio::time::Instant;

use omniplex_metrics::{DropCause, MetricsHub};
use omniplex_wire::{
    encode_frame, ChannelId, FrameHeader, FrameType, SessionTimestamp, FLAG_END_OF_MESSAGE,
};

use crate::scheduler::Scheduler;
use crate::{chunk_of, frag_count_for, MuxError};

/// Default reassembly deadline for non-video unreliable channels (spec §9).
pub const DEFAULT_DEADLINE: Duration = Duration::from_millis(5);

/// Receiver-buffer capacity for both unreliable and latest-value channels.
pub const RX_RING_CAP: usize = 64;

/// One fragment routed from the global datagram demux to a channel's
/// reassembly task.
#[derive(Clone)]
pub(crate) struct DgramFrag {
    pub ts: SessionTimestamp,
    pub seq: u32,
    pub frag_index: u16,
    pub frag_count: u16,
    pub payload: Bytes,
}

/// Small bounded MPSC-ish ring with drop-oldest-on-full semantics (used for
/// the app-facing receive side, where the mpsc "block on full" behaviour of
/// the reliable path is *not* wanted — spec §9 unreliable backpressure is
/// drop-oldest).
pub(crate) struct Ring<T> {
    queue: Mutex<std::collections::VecDeque<T>>,
    notify: Notify,
    cap: usize,
    closed: AtomicBool,
}

impl<T> Ring<T> {
    pub fn new(cap: usize) -> Self {
        Self {
            queue: Mutex::new(std::collections::VecDeque::new()),
            notify: Notify::new(),
            cap,
            closed: AtomicBool::new(false),
        }
    }

    /// Push, evicting the oldest item if at capacity. Returns `true` if an
    /// item was evicted.
    pub fn push_drop_oldest(&self, item: T) -> bool {
        let mut q = self.queue.lock().unwrap();
        let dropped = if q.len() >= self.cap {
            q.pop_front();
            true
        } else {
            false
        };
        q.push_back(item);
        drop(q);
        self.notify.notify_one();
        dropped
    }

    pub async fn pop(&self) -> Option<T> {
        loop {
            {
                let mut q = self.queue.lock().unwrap();
                if let Some(v) = q.pop_front() {
                    return Some(v);
                }
                if self.closed.load(Ordering::Acquire) {
                    return None;
                }
            }
            self.notify.notified().await;
        }
    }

    pub fn close(&self) {
        self.closed.store(true, Ordering::Release);
        self.notify.notify_waiters();
    }
}

// ---------------------------------------------------------------- sending

/// Sending half of an `UnreliableTimestamped` channel.
pub struct UnreliableTx {
    pub(crate) channel: ChannelId,
    pub(crate) scheduler: Scheduler,
    pub(crate) seq: Arc<AtomicU32>,
    pub(crate) max_frag: usize,
}

impl UnreliableTx {
    pub async fn send(&self, payload: Bytes, ts: SessionTimestamp) -> Result<(), MuxError> {
        let seq = self.seq.fetch_add(1, Ordering::Relaxed);
        let frag_count = frag_count_for(payload.len(), self.max_frag);
        for i in 0..frag_count {
            let chunk = chunk_of(&payload, i, self.max_frag);
            let flags = if i == frag_count - 1 {
                FLAG_END_OF_MESSAGE
            } else {
                0
            };
            let header = FrameHeader {
                channel: self.channel,
                frame_type: FrameType::Media,
                flags,
                timestamp: ts,
                seq,
                frag_index: i,
                frag_count,
            };
            let mut out = Vec::new();
            encode_frame(&header, &chunk, &mut out).map_err(|e| MuxError::Encode(e.to_string()))?;
            self.scheduler.enqueue(self.channel, Bytes::from(out));
        }
        Ok(())
    }
}

/// Sending half of a `LatestValueWins` channel: single pending slot,
/// replaced (not queued) on each send until it hits the wire.
pub struct LatestTx {
    pub(crate) channel: ChannelId,
    pub(crate) scheduler: Scheduler,
    pub(crate) seq: Arc<AtomicU32>,
}

impl LatestTx {
    pub async fn send(&self, payload: Bytes, ts: SessionTimestamp) -> Result<(), MuxError> {
        if payload.len() > omniplex_wire::MAX_FRAGMENT_PAYLOAD {
            return Err(MuxError::Encode(
                "latest-value-wins payload exceeds MAX_FRAGMENT_PAYLOAD".into(),
            ));
        }
        let seq = self.seq.fetch_add(1, Ordering::Relaxed);
        let header = FrameHeader {
            channel: self.channel,
            frame_type: FrameType::Media,
            flags: FLAG_END_OF_MESSAGE,
            timestamp: ts,
            seq,
            frag_index: 0,
            frag_count: 1,
        };
        let mut out = Vec::new();
        encode_frame(&header, &payload, &mut out).map_err(|e| MuxError::Encode(e.to_string()))?;
        self.scheduler.replace(self.channel, Bytes::from(out));
        Ok(())
    }
}

// -------------------------------------------------------------- receiving

pub struct UnreliableRx {
    pub(crate) ring: Arc<Ring<(Bytes, SessionTimestamp)>>,
}

impl UnreliableRx {
    pub async fn recv(&mut self) -> Option<(Bytes, SessionTimestamp)> {
        self.ring.pop().await
    }
}

pub struct LatestRx {
    pub(crate) ring: Arc<Ring<(Bytes, SessionTimestamp)>>,
}

impl LatestRx {
    pub async fn recv(&mut self) -> Option<(Bytes, SessionTimestamp)> {
        self.ring.pop().await
    }
}

struct Partial {
    ts: SessionTimestamp,
    parts: Vec<Option<Bytes>>,
    received: usize,
    deadline_at: Instant,
}

/// Per-channel reassembly task for `UnreliableTimestamped`: reassembles
/// fragments keyed by `seq`, drops whole messages that miss their deadline
/// (`DropCause::Late`), delivers complete ones into a drop-oldest ring
/// (`DropCause::Backpressure` on overflow).
pub(crate) async fn run_unreliable_rx(
    mut route_rx: mpsc::Receiver<DgramFrag>,
    out: Arc<Ring<(Bytes, SessionTimestamp)>>,
    metrics: MetricsHub,
    channel: ChannelId,
    deadline: Duration,
    mut stop_rx: watch::Receiver<bool>,
) {
    let mut pending: HashMap<u32, Partial> = HashMap::new();
    loop {
        let next_deadline = pending.values().map(|p| p.deadline_at).min();
        tokio::select! {
            biased;
            changed = stop_rx.changed() => {
                if changed.is_err() || *stop_rx.borrow() {
                    break;
                }
            }
            frag = route_rx.recv() => {
                match frag {
                    None => break,
                    Some(f) => {
                        let frag_count = f.frag_count.max(1) as usize;
                        let entry = pending.entry(f.seq).or_insert_with(|| Partial {
                            ts: f.ts,
                            parts: vec![None; frag_count],
                            received: 0,
                            deadline_at: Instant::now() + deadline,
                        });
                        let idx = f.frag_index as usize;
                        if idx < entry.parts.len() && entry.parts[idx].is_none() {
                            entry.parts[idx] = Some(f.payload);
                            entry.received += 1;
                        }
                        if entry.received == entry.parts.len() {
                            let p = pending.remove(&f.seq).unwrap();
                            let mut buf = Vec::new();
                            for part in p.parts.into_iter().flatten() {
                                buf.extend_from_slice(&part);
                            }
                            let msg = Bytes::from(buf);
                            metrics.add_bytes_rx(channel.0, msg.len() as u64);
                            if out.push_drop_oldest((msg, p.ts)) {
                                metrics.record_drop(channel.0, DropCause::Backpressure);
                            }
                        }
                    }
                }
            }
            _ = sleep_until_opt(next_deadline) => {
                let now = Instant::now();
                let expired: Vec<u32> = pending
                    .iter()
                    .filter(|(_, p)| p.deadline_at <= now)
                    .map(|(k, _)| *k)
                    .collect();
                for k in expired {
                    pending.remove(&k);
                    metrics.record_drop(channel.0, DropCause::Late);
                }
            }
        }
    }
    out.close();
}

/// Per-channel receive task for `LatestValueWins`: single-frame messages,
/// only delivers strictly increasing `seq` (stale/reordered datagrams never
/// override a newer delivered value).
pub(crate) async fn run_latest_rx(
    mut route_rx: mpsc::Receiver<DgramFrag>,
    out: Arc<Ring<(Bytes, SessionTimestamp)>>,
    metrics: MetricsHub,
    channel: ChannelId,
    mut stop_rx: watch::Receiver<bool>,
) {
    let mut last_delivered: Option<u32> = None;
    loop {
        tokio::select! {
            biased;
            changed = stop_rx.changed() => {
                if changed.is_err() || *stop_rx.borrow() {
                    break;
                }
            }
            frag = route_rx.recv() => {
                match frag {
                    None => break,
                    Some(f) => {
                        let newer = match last_delivered {
                            None => true,
                            Some(last) => (f.seq.wrapping_sub(last) as i32) > 0,
                        };
                        if !newer {
                            continue;
                        }
                        last_delivered = Some(f.seq);
                        metrics.add_bytes_rx(channel.0, f.payload.len() as u64);
                        if out.push_drop_oldest((f.payload, f.ts)) {
                            metrics.record_drop(channel.0, DropCause::Backpressure);
                        }
                    }
                }
            }
        }
    }
    out.close();
}

/// Sleeps until `deadline`, or forever (never resolves) if `None` — lets
/// the reassembly loop `select!` on "no pending deadline" without pulling
/// in an extra dependency for a `Pending` future.
async fn sleep_until_opt(deadline: Option<Instant>) {
    match deadline {
        Some(d) => tokio::time::sleep_until(d).await,
        None => std::future::pending().await,
    }
}
