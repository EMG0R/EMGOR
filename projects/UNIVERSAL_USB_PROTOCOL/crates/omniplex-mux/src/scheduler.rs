//! Send scheduler (plan Task 5.4): every datagram send funnels through one
//! task that enforces strict `LatencyClass` priority, paces `Bulk` traffic
//! with a token-bucket, and re-estimates bandwidth allocation every 100 ms.
//!
//! Adaptation note: the `Transport` trait (crates/omniplex-transport) has no
//! `acked-bytes/RTT` style stats hook, so "capacity" is either an injected
//! test cap (`bandwidth_cap_bps`, standing in for the plan's "loopback:
//! from `LoopbackConfig` cap") or, absent a cap, treated as unbounded (no
//! pacing/pressure) — real-transport bandwidth estimation from connection
//! stats is left to a future extension of `Transport`.

use std::collections::VecDeque;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Arc, Mutex};
use std::time::Duration;

use bytes::Bytes;
use dashmap::DashMap;
use tokio::sync::{broadcast, mpsc};
use tokio::time::Instant;

use omniplex_metrics::{DropCause, MetricsHub};
use omniplex_transport::Transport;
use omniplex_wire::{ChannelId, LatencyClass};

/// Event surfaced by the scheduler to the channel owner (spec §10
/// degradation ladder).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum MuxEvent {
    BandwidthPressure {
        channel: ChannelId,
        allocated_bps: u64,
    },
}

enum ChanQueue {
    Fifo(Mutex<VecDeque<(Instant, Bytes)>>),
    Latest(Mutex<Option<(Instant, Bytes)>>),
}

struct ChanEntry {
    latency: LatencyClass,
    queue: ChanQueue,
}

struct Inner {
    transport: Arc<dyn Transport>,
    metrics: MetricsHub,
    channels: DashMap<ChannelId, ChanEntry>,
    crit_tx: mpsc::UnboundedSender<ChannelId>,
    inter_tx: mpsc::UnboundedSender<ChannelId>,
    bulk_tx: mpsc::UnboundedSender<ChannelId>,
    events_tx: broadcast::Sender<MuxEvent>,
    bandwidth_cap_bps: Option<u64>,
    window_bytes: [AtomicU64; 3],
    bulk_alloc_bps: AtomicU64,
}

/// Cheaply clonable handle to the scheduler.
#[derive(Clone)]
pub struct Scheduler {
    inner: Arc<Inner>,
}

impl Scheduler {
    /// `bandwidth_cap_bps`: fixed capacity estimate to test against (stands
    /// in for a loopback bandwidth cap / real link stats). `None` means
    /// unbounded: no pacing, no pressure events.
    pub fn new(
        transport: Arc<dyn Transport>,
        metrics: MetricsHub,
        bandwidth_cap_bps: Option<u64>,
    ) -> Self {
        let (crit_tx, crit_rx) = mpsc::unbounded_channel();
        let (inter_tx, inter_rx) = mpsc::unbounded_channel();
        let (bulk_tx, bulk_rx) = mpsc::unbounded_channel();
        let (events_tx, _) = broadcast::channel(256);
        let inner = Arc::new(Inner {
            transport,
            metrics,
            channels: DashMap::new(),
            crit_tx,
            inter_tx,
            bulk_tx,
            events_tx,
            bandwidth_cap_bps,
            window_bytes: [AtomicU64::new(0), AtomicU64::new(0), AtomicU64::new(0)],
            bulk_alloc_bps: AtomicU64::new(bandwidth_cap_bps.unwrap_or(u64::MAX)),
        });
        tokio::spawn(send_loop(inner.clone(), crit_rx, inter_rx, bulk_rx));
        tokio::spawn(bandwidth_task(inner.clone()));
        Scheduler { inner }
    }

    /// Register a channel's outbound queue. `is_latest` selects
    /// latest-value-wins (single-slot, replace) vs. FIFO (unreliable)
    /// semantics.
    pub fn register(&self, id: ChannelId, latency: LatencyClass, is_latest: bool) {
        let queue = if is_latest {
            ChanQueue::Latest(Mutex::new(None))
        } else {
            ChanQueue::Fifo(Mutex::new(VecDeque::new()))
        };
        self.inner.channels.insert(id, ChanEntry { latency, queue });
    }

    pub fn unregister(&self, id: ChannelId) {
        self.inner.channels.remove(&id);
    }

    /// FIFO enqueue (unreliable-timestamped fragments). Never blocks.
    pub fn enqueue(&self, id: ChannelId, payload: Bytes) {
        let Some(entry) = self.inner.channels.get(&id) else {
            return;
        };
        if let ChanQueue::Fifo(q) = &entry.queue {
            q.lock().unwrap().push_back((Instant::now(), payload));
        }
        let latency = entry.latency;
        drop(entry);
        self.signal(id, latency);
    }

    /// Latest-value-wins replace: overwrites any value not yet sent,
    /// counting it as `DropCause::Replaced`.
    pub fn replace(&self, id: ChannelId, payload: Bytes) {
        let Some(entry) = self.inner.channels.get(&id) else {
            return;
        };
        if let ChanQueue::Latest(slot) = &entry.queue {
            let mut g = slot.lock().unwrap();
            if g.is_some() {
                self.inner.metrics.record_drop(id.0, DropCause::Replaced);
            }
            *g = Some((Instant::now(), payload));
        }
        let latency = entry.latency;
        drop(entry);
        self.signal(id, latency);
    }

    fn signal(&self, id: ChannelId, latency: LatencyClass) {
        let _ = match latency {
            LatencyClass::LatencyCritical => self.inner.crit_tx.send(id),
            LatencyClass::Interactive => self.inner.inter_tx.send(id),
            LatencyClass::Bulk => self.inner.bulk_tx.send(id),
        };
    }

    pub fn subscribe_events(&self) -> broadcast::Receiver<MuxEvent> {
        self.inner.events_tx.subscribe()
    }
}

/// Strict-priority send loop: critical is always drained ahead of
/// interactive, ahead of bulk. Bulk sends are paced by spawning their
/// (possibly delayed) transmission as a separate task so a long pacing
/// sleep never blocks the critical/interactive path.
async fn send_loop(
    inner: Arc<Inner>,
    mut crit_rx: mpsc::UnboundedReceiver<ChannelId>,
    mut inter_rx: mpsc::UnboundedReceiver<ChannelId>,
    mut bulk_rx: mpsc::UnboundedReceiver<ChannelId>,
) {
    let mut bulk_next_free = Instant::now();
    loop {
        tokio::select! {
            biased;
            Some(id) = crit_rx.recv() => {
                send_one(&inner, id, LatencyClass::LatencyCritical, &mut bulk_next_free).await;
            }
            Some(id) = inter_rx.recv() => {
                send_one(&inner, id, LatencyClass::Interactive, &mut bulk_next_free).await;
            }
            Some(id) = bulk_rx.recv() => {
                send_one(&inner, id, LatencyClass::Bulk, &mut bulk_next_free).await;
            }
            else => return,
        }
    }
}

fn take_ready(inner: &Inner, id: ChannelId) -> Option<(Instant, Bytes)> {
    let entry = inner.channels.get(&id)?;
    match &entry.queue {
        ChanQueue::Fifo(q) => q.lock().unwrap().pop_front(),
        ChanQueue::Latest(q) => q.lock().unwrap().take(),
    }
}

async fn send_one(
    inner: &Arc<Inner>,
    id: ChannelId,
    latency: LatencyClass,
    bulk_next_free: &mut Instant,
) {
    let Some((enqueued_at, payload)) = take_ready(inner, id) else {
        return;
    };
    let len = payload.len();
    let idx = match latency {
        LatencyClass::LatencyCritical => 0,
        LatencyClass::Interactive => 1,
        LatencyClass::Bulk => 2,
    };
    inner.window_bytes[idx].fetch_add(len as u64, Ordering::Relaxed);

    if latency == LatencyClass::Bulk {
        let rate = (inner.bulk_alloc_bps.load(Ordering::Relaxed) as f64 / 8.0).max(1.0);
        let now = Instant::now();
        let start = (*bulk_next_free).max(now);
        let dur = Duration::from_secs_f64(len as f64 / rate);
        *bulk_next_free = start + dur;
        let inner = inner.clone();
        tokio::spawn(async move {
            tokio::time::sleep_until(start).await;
            let _ = inner.transport.send_datagram(payload);
            inner.metrics.add_bytes_tx(id.0, len as u64);
            inner
                .metrics
                .record_latency_ns(id.0, enqueued_at.elapsed().as_nanos() as u64);
        });
    } else {
        let _ = inner.transport.send_datagram(payload);
        inner.metrics.add_bytes_tx(id.0, len as u64);
        inner
            .metrics
            .record_latency_ns(id.0, enqueued_at.elapsed().as_nanos() as u64);
    }
}

const TICK: Duration = Duration::from_millis(100);
const EWMA_ALPHA: f64 = 0.3;

/// Recomputes the EWMA bandwidth estimate and bulk allocation every 100 ms,
/// independent of the send loop (so a busy send loop can never starve this
/// recompute).
async fn bandwidth_task(inner: Arc<Inner>) {
    let mut tick = tokio::time::interval(TICK);
    tick.set_missed_tick_behavior(tokio::time::MissedTickBehavior::Delay);
    let mut crit_ewma = 0.0f64;
    let mut inter_ewma = 0.0f64;
    let mut bulk_ewma = 0.0f64;
    loop {
        tick.tick().await;
        let crit_bytes = inner.window_bytes[0].swap(0, Ordering::Relaxed) as f64;
        let inter_bytes = inner.window_bytes[1].swap(0, Ordering::Relaxed) as f64;
        let bulk_bytes = inner.window_bytes[2].swap(0, Ordering::Relaxed) as f64;
        let dt = TICK.as_secs_f64();
        crit_ewma += EWMA_ALPHA * ((crit_bytes * 8.0 / dt) - crit_ewma);
        inter_ewma += EWMA_ALPHA * ((inter_bytes * 8.0 / dt) - inter_ewma);
        bulk_ewma += EWMA_ALPHA * ((bulk_bytes * 8.0 / dt) - bulk_ewma);

        let Some(cap) = inner.bandwidth_cap_bps else {
            inner.bulk_alloc_bps.store(u64::MAX, Ordering::Relaxed);
            continue;
        };
        let cap = cap as f64;
        let total_demand = crit_ewma + inter_ewma + bulk_ewma;
        let pressure = total_demand > 0.9 * cap;
        let ceiling = if pressure { 0.9 * cap } else { cap };
        let bulk_alloc = (ceiling - crit_ewma - inter_ewma).max(0.0);
        inner
            .bulk_alloc_bps
            .store(bulk_alloc as u64, Ordering::Relaxed);

        let bulk_ids: Vec<ChannelId> = inner
            .channels
            .iter()
            .filter(|e| e.latency == LatencyClass::Bulk)
            .map(|e| *e.key())
            .collect();
        if !bulk_ids.is_empty() {
            let share = bulk_alloc as u64 / bulk_ids.len() as u64;
            for id in &bulk_ids {
                inner.metrics.set_allocated_bps(id.0, share);
                if pressure {
                    let _ = inner.events_tx.send(MuxEvent::BandwidthPressure {
                        channel: *id,
                        allocated_bps: share,
                    });
                }
            }
        }
    }
}
