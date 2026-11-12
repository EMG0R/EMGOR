//! Always-on per-channel metrics for the Omniplex stack (design §11, §12).
//!
//! [`MetricsHub`] is a cheaply clonable (Arc-backed) hub of per-channel
//! state keyed by a plain `u16` channel id. Regular recording sites call
//! its methods directly; the audio callback uses the lock-free
//! [`RtRecorder`] SPSC path, drained into the hub by a background tokio
//! task (no locks or allocation in the callback).

use std::collections::BTreeMap;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Arc;
use std::time::Duration;

use dashmap::DashMap;
use hdrhistogram::Histogram;
use parking_lot::Mutex;

/// Why a frame/message on a channel was dropped.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum DropCause {
    /// Arrived too late to be useful (past its presentation deadline).
    Late,
    /// Never arrived (unreliable delivery loss).
    Lost,
    /// Dropped locally because the channel could not keep up.
    Backpressure,
    /// Superseded by a newer value (latest-value-wins channels).
    Replaced,
}

/// Per-cause drop counters for one channel.
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct DropCounters {
    pub late: u64,
    pub lost: u64,
    pub backpressure: u64,
    pub replaced: u64,
}

impl DropCounters {
    /// Sum of all drop causes.
    pub fn total(&self) -> u64 {
        self.late + self.lost + self.backpressure + self.replaced
    }
}

/// Point-in-time percentile summary of a latency histogram (nanoseconds).
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct HistogramSnapshot {
    /// Number of recorded samples.
    pub count: u64,
    pub p50: u64,
    pub p90: u64,
    pub p99: u64,
    pub max: u64,
}

/// Snapshot of one channel's metrics, as returned by [`MetricsHub::snapshot`].
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct ChannelMetrics {
    /// One-way latency distribution, nanoseconds.
    pub latency: HistogramSnapshot,
    /// RFC 3550-style jitter EWMA estimate, nanoseconds.
    pub jitter_ns: u64,
    pub drops: DropCounters,
    pub bytes_tx: u64,
    pub bytes_rx: u64,
    pub allocated_bps: u64,
}

/// Latency samples up to 60 s are recorded exactly; larger ones saturate.
const MAX_LATENCY_NS: u64 = 60_000_000_000;
/// Three significant figures keeps percentile error under 0.1 %.
const HIST_SIGFIGS: u8 = 3;

struct ChannelState {
    latency: Mutex<Histogram<u64>>,
    /// RFC 3550 §6.4.1: J += (|D| - J) / 16, kept in nanoseconds.
    jitter_ns: Mutex<f64>,
    drops_late: AtomicU64,
    drops_lost: AtomicU64,
    drops_backpressure: AtomicU64,
    drops_replaced: AtomicU64,
    bytes_tx: AtomicU64,
    bytes_rx: AtomicU64,
    allocated_bps: AtomicU64,
}

impl ChannelState {
    fn new() -> Self {
        Self {
            latency: Mutex::new(
                Histogram::new_with_bounds(1, MAX_LATENCY_NS, HIST_SIGFIGS)
                    .expect("valid histogram bounds"),
            ),
            jitter_ns: Mutex::new(0.0),
            drops_late: AtomicU64::new(0),
            drops_lost: AtomicU64::new(0),
            drops_backpressure: AtomicU64::new(0),
            drops_replaced: AtomicU64::new(0),
            bytes_tx: AtomicU64::new(0),
            bytes_rx: AtomicU64::new(0),
            allocated_bps: AtomicU64::new(0),
        }
    }

    fn snapshot(&self) -> ChannelMetrics {
        let latency = {
            let h = self.latency.lock();
            if h.is_empty() {
                HistogramSnapshot::default()
            } else {
                HistogramSnapshot {
                    count: h.len(),
                    p50: h.value_at_quantile(0.50),
                    p90: h.value_at_quantile(0.90),
                    p99: h.value_at_quantile(0.99),
                    max: h.max(),
                }
            }
        };
        ChannelMetrics {
            latency,
            jitter_ns: *self.jitter_ns.lock() as u64,
            drops: DropCounters {
                late: self.drops_late.load(Ordering::Relaxed),
                lost: self.drops_lost.load(Ordering::Relaxed),
                backpressure: self.drops_backpressure.load(Ordering::Relaxed),
                replaced: self.drops_replaced.load(Ordering::Relaxed),
            },
            bytes_tx: self.bytes_tx.load(Ordering::Relaxed),
            bytes_rx: self.bytes_rx.load(Ordering::Relaxed),
            allocated_bps: self.allocated_bps.load(Ordering::Relaxed),
        }
    }
}

/// One record pushed through the real-time SPSC path.
type RtRecord = (u16, u64);

/// Central metrics hub. `Clone` is cheap (shared `Arc`).
#[derive(Clone)]
pub struct MetricsHub {
    channels: Arc<DashMap<u16, ChannelState>>,
}

impl Default for MetricsHub {
    fn default() -> Self {
        Self::new()
    }
}

impl MetricsHub {
    pub fn new() -> Self {
        Self {
            channels: Arc::new(DashMap::new()),
        }
    }

    fn with_channel<R>(&self, channel: u16, f: impl FnOnce(&ChannelState) -> R) -> R {
        if let Some(state) = self.channels.get(&channel) {
            return f(&state);
        }
        let state = self
            .channels
            .entry(channel)
            .or_insert_with(ChannelState::new);
        f(&state)
    }

    /// Record one one-way latency sample (nanoseconds) for `channel`.
    /// Samples above 60 s are clamped.
    pub fn record_latency_ns(&self, channel: u16, latency_ns: u64) {
        self.with_channel(channel, |s| {
            s.latency.lock().saturating_record(latency_ns);
        });
    }

    /// Feed one inter-arrival transit-time difference `|D|` (nanoseconds)
    /// into the RFC 3550 jitter EWMA: `J += (|D| - J) / 16`.
    pub fn record_jitter(&self, channel: u16, transit_delta_ns: u64) {
        self.with_channel(channel, |s| {
            let mut j = s.jitter_ns.lock();
            *j += (transit_delta_ns as f64 - *j) / 16.0;
        });
    }

    /// Count one dropped frame/message on `channel`.
    pub fn record_drop(&self, channel: u16, cause: DropCause) {
        self.with_channel(channel, |s| {
            let counter = match cause {
                DropCause::Late => &s.drops_late,
                DropCause::Lost => &s.drops_lost,
                DropCause::Backpressure => &s.drops_backpressure,
                DropCause::Replaced => &s.drops_replaced,
            };
            counter.fetch_add(1, Ordering::Relaxed);
        });
    }

    /// Add transmitted payload bytes for `channel`.
    pub fn add_bytes_tx(&self, channel: u16, bytes: u64) {
        self.with_channel(channel, |s| {
            s.bytes_tx.fetch_add(bytes, Ordering::Relaxed);
        });
    }

    /// Add received payload bytes for `channel`.
    pub fn add_bytes_rx(&self, channel: u16, bytes: u64) {
        self.with_channel(channel, |s| {
            s.bytes_rx.fetch_add(bytes, Ordering::Relaxed);
        });
    }

    /// Record the bandwidth currently allocated to `channel` (bits/sec).
    pub fn set_allocated_bps(&self, channel: u16, bps: u64) {
        self.with_channel(channel, |s| {
            s.allocated_bps.store(bps, Ordering::Relaxed);
        });
    }

    /// Snapshot every channel seen so far. Channels never recorded to are
    /// absent from the map.
    pub fn snapshot(&self) -> BTreeMap<u16, ChannelMetrics> {
        self.channels
            .iter()
            .map(|entry| (*entry.key(), entry.value().snapshot()))
            .collect()
    }

    /// Create a lock-free real-time latency recorder backed by a
    /// preallocated SPSC ring of `capacity` records, and spawn a
    /// background tokio task (on the current runtime) that drains it into
    /// this hub. The drain task exits once the [`RtRecorder`] is dropped
    /// and the ring is empty.
    ///
    /// Must be called from within a tokio runtime.
    pub fn rt_recorder(&self, capacity: usize) -> RtRecorder {
        let (producer, mut consumer) = rtrb::RingBuffer::<RtRecord>::new(capacity);
        let hub = self.clone();
        tokio::spawn(async move {
            loop {
                let mut drained = false;
                while let Ok((channel, latency_ns)) = consumer.pop() {
                    hub.record_latency_ns(channel, latency_ns);
                    drained = true;
                }
                if consumer.is_abandoned() && consumer.is_empty() {
                    break;
                }
                if !drained {
                    tokio::time::sleep(Duration::from_millis(1)).await;
                } else {
                    // Yield so a fast producer cannot starve the runtime.
                    tokio::task::yield_now().await;
                }
            }
        });
        RtRecorder { producer }
    }
}

/// Real-time-safe latency recorder for audio-callback use: `record` is a
/// single lock-free, allocation-free SPSC push. Single producer — move it
/// into the one real-time thread that uses it.
pub struct RtRecorder {
    producer: rtrb::Producer<RtRecord>,
}

impl RtRecorder {
    /// Push one `(channel, latency_ns)` record. Returns `false` (dropping
    /// the sample) if the ring is full — never blocks.
    pub fn record(&mut self, channel: u16, latency_ns: u64) -> bool {
        self.producer.push((channel, latency_ns)).is_ok()
    }

    /// Free slots currently available in the ring.
    pub fn slots(&self) -> usize {
        self.producer.slots()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::thread;

    #[test]
    fn concurrent_recording_totals_add_up_exactly() {
        let hub = MetricsHub::new();
        const THREADS: u64 = 8;
        const ITERS: u64 = 10_000;

        thread::scope(|s| {
            for t in 0..THREADS {
                let hub = hub.clone();
                s.spawn(move || {
                    let channel = (t % 2) as u16; // two contended channels
                    for _ in 0..ITERS {
                        hub.add_bytes_tx(channel, 3);
                        hub.add_bytes_rx(channel, 5);
                        hub.record_drop(channel, DropCause::Late);
                        hub.record_drop(channel, DropCause::Backpressure);
                        hub.record_latency_ns(channel, 1_000);
                    }
                });
            }
        });

        let snap = hub.snapshot();
        assert_eq!(snap.len(), 2);
        let per_channel = THREADS / 2 * ITERS;
        for ch in [0u16, 1u16] {
            let m = &snap[&ch];
            assert_eq!(m.bytes_tx, per_channel * 3);
            assert_eq!(m.bytes_rx, per_channel * 5);
            assert_eq!(m.drops.late, per_channel);
            assert_eq!(m.drops.backpressure, per_channel);
            assert_eq!(m.drops.lost, 0);
            assert_eq!(m.drops.replaced, 0);
            assert_eq!(m.drops.total(), 2 * per_channel);
            assert_eq!(m.latency.count, per_channel);
        }
    }

    #[test]
    fn histogram_percentiles_sane_for_known_distribution() {
        let hub = MetricsHub::new();
        // Uniform 1..=10_000 ns.
        for v in 1..=10_000u64 {
            hub.record_latency_ns(7, v);
        }
        let snap = hub.snapshot();
        let lat = snap[&7].latency;
        assert_eq!(lat.count, 10_000);
        // 3 significant figures -> allow 1% slack.
        let close =
            |got: u64, want: u64| (got as f64 - want as f64).abs() <= want as f64 * 0.01 + 1.0;
        assert!(close(lat.p50, 5_000), "p50 = {}", lat.p50);
        assert!(close(lat.p90, 9_000), "p90 = {}", lat.p90);
        assert!(close(lat.p99, 9_900), "p99 = {}", lat.p99);
        assert!(close(lat.max, 10_000), "max = {}", lat.max);
        // Monotone.
        assert!(lat.p50 <= lat.p90 && lat.p90 <= lat.p99 && lat.p99 <= lat.max);
    }

    #[test]
    fn jitter_ewma_converges_rfc3550_style() {
        let hub = MetricsHub::new();
        for _ in 0..500 {
            hub.record_jitter(3, 1_600);
        }
        let j = hub.snapshot()[&3].jitter_ns;
        // J converges to the constant sample value.
        assert!((1_590..=1_600).contains(&j), "jitter = {j}");
    }

    #[tokio::test]
    async fn rt_ring_path_delivers_all_records() {
        let hub = MetricsHub::new();
        const N: u64 = 5_000;
        let mut rec = hub.rt_recorder(256);

        // Simulated audio callback thread: lock-free pushes only.
        let producer = thread::spawn(move || {
            let mut sent = 0u64;
            while sent < N {
                if rec.record(42, 100 + (sent % 100)) {
                    sent += 1;
                } else {
                    std::hint::spin_loop();
                }
            }
            drop(rec); // signals the drain task to finish
        });

        // Wait until everything has been drained into the hub.
        let deadline = std::time::Instant::now() + Duration::from_secs(10);
        loop {
            let count = hub
                .snapshot()
                .get(&42)
                .map(|m| m.latency.count)
                .unwrap_or(0);
            if count == N {
                break;
            }
            assert!(std::time::Instant::now() < deadline, "drained {count}/{N}");
            tokio::time::sleep(Duration::from_millis(5)).await;
        }
        producer.join().unwrap();

        let m = hub.snapshot()[&42];
        assert_eq!(m.latency.count, N);
        assert!(m.latency.p50 >= 100 && m.latency.max <= 200);
    }

    #[test]
    fn snapshot_of_unknown_channel_absent() {
        let hub = MetricsHub::new();
        hub.record_latency_ns(1, 10);
        hub.set_allocated_bps(2, 1_000_000);
        let snap = hub.snapshot();
        assert!(snap.contains_key(&1));
        assert!(snap.contains_key(&2));
        assert!(!snap.contains_key(&999));
    }
}
