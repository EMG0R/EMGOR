//! Clock discipline filter — pure math, no I/O, no async, no cross-crate
//! dependencies (plan Task 4.1).
//!
//! Feed with PTP-style samples; ask it to convert local time → session time.
//!
//! Estimator: keep the min-delay samples (lowest `(t4-t1)-(t3-t2)`) in a
//! sliding window; linear-regress offset over local time across those to get
//! drift (ppm); slew, never step, after initial lock.

use std::collections::VecDeque;
use std::sync::atomic::{AtomicU64, Ordering};

/// One PTP-style exchange, all timestamps in nanoseconds:
/// `t1` follower send, `t2` master receive, `t3` master send,
/// `t4` follower receive. `t1`/`t4` are follower-local, `t2`/`t3` are
/// master-local (= session time).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct ClockSample {
    pub t1: u64,
    pub t2: u64,
    pub t3: u64,
    pub t4: u64,
}

/// Current discipline state as reported by [`ClockFilter::stats`].
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct ClockStats {
    /// Estimated `session - local` offset at the latest sample, ns.
    pub offset_ns: i64,
    /// Estimated drift of the master clock relative to local, ppm.
    pub drift_ppm: f64,
    /// Estimated error (RMS residual of the selected samples), ns.
    pub est_error_ns: u64,
}

#[derive(Clone, Copy, Debug)]
struct Point {
    /// Follower-local reference time for this sample (`t4`).
    local: u64,
    /// Measured offset `((t2-t1)+(t3-t4))/2`, ns.
    offset: i64,
    /// Measured round-trip path delay `(t4-t1)-(t3-t2)`, ns.
    delay: i64,
}

#[derive(Clone, Copy, Debug)]
struct Estimate {
    /// Regression slope: d(offset)/d(local), dimensionless (ns/ns).
    slope: f64,
    mean_local: f64,
    mean_offset: f64,
    rms_residual: f64,
}

impl Estimate {
    fn offset_at(&self, local_ns: f64) -> f64 {
        self.mean_offset + self.slope * (local_ns - self.mean_local)
    }
}

/// Sliding-window size (samples).
const WINDOW: usize = 64;
/// How many of the lowest-delay samples in the window feed the regression.
const BEST: usize = 8;
/// Samples required before the filter declares initial lock.
const MIN_LOCK_SAMPLES: usize = 4;
/// Servo time constant: a residual offset error is slewed out over ~this
/// many nanoseconds of local time.
const CORRECTION_TAU_NS: f64 = 2.0e8;
/// Maximum slew correction applied on top of the drift estimate (fraction).
const MAX_CORRECTION: f64 = 5.0e-3;
/// Drift estimates beyond this bound (fraction) are not trusted for the
/// servo rate (the raw value is still reported in stats).
const MAX_SLOPE_FOR_RATE: f64 = 1.0e-3;

/// PTP-style clock discipline filter.
pub struct ClockFilter {
    window: VecDeque<Point>,
    locked: bool,
    /// Servo mapping: `session(local) = base_session + (local - base_local) * rate`.
    base_local: u64,
    base_session: f64,
    rate: f64,
    stats: ClockStats,
    /// Highest session time ever returned; guarantees monotonic `to_session`.
    last_out: AtomicU64,
}

impl Default for ClockFilter {
    fn default() -> Self {
        Self::new()
    }
}

impl ClockFilter {
    pub fn new() -> Self {
        Self {
            window: VecDeque::with_capacity(WINDOW),
            locked: false,
            base_local: 0,
            base_session: 0.0,
            rate: 1.0,
            stats: ClockStats::default(),
            last_out: AtomicU64::new(0),
        }
    }

    /// Feed one completed exchange into the filter.
    pub fn ingest(&mut self, s: ClockSample) {
        // offset = ((t2 - t1) + (t3 - t4)) / 2, assuming a symmetric path
        // (true on point-to-point TB).
        let offset = ((s.t2 as i64 - s.t1 as i64) + (s.t3 as i64 - s.t4 as i64)) / 2;
        let delay = (s.t4 as i64 - s.t1 as i64) - (s.t3 as i64 - s.t2 as i64);
        if self.window.len() == WINDOW {
            self.window.pop_front();
        }
        self.window.push_back(Point {
            local: s.t4,
            offset,
            delay,
        });

        let Some(est) = self.estimate() else { return };

        let now = s.t4;
        let target = now as f64 + est.offset_at(now as f64);
        if !self.locked {
            // Initial lock is allowed to step.
            self.base_local = now;
            self.base_session = target;
            self.rate = 1.0 + est.slope.clamp(-MAX_SLOPE_FOR_RATE, MAX_SLOPE_FOR_RATE);
            self.locked = true;
        } else {
            // Slew, never step: rebase continuously at `now` and fold the
            // residual error into the rate, bounded so the mapping stays
            // monotonic and never jumps.
            let cur = self.base_session + (now as f64 - self.base_local as f64) * self.rate;
            let err = target - cur;
            let correction = (err / CORRECTION_TAU_NS).clamp(-MAX_CORRECTION, MAX_CORRECTION);
            let slope = est.slope.clamp(-MAX_SLOPE_FOR_RATE, MAX_SLOPE_FOR_RATE);
            self.base_local = now;
            self.base_session = cur;
            self.rate = (1.0 + slope + correction).clamp(0.5, 1.5);
        }

        self.stats = ClockStats {
            offset_ns: est.offset_at(now as f64).round() as i64,
            drift_ppm: est.slope * 1.0e6,
            est_error_ns: est.rms_residual.round() as u64,
        };
    }

    /// Convert local time to session time. `None` until locked.
    /// Monotonic: slewing never steps backwards, and consecutive calls never
    /// return a smaller value than a previous one.
    pub fn to_session(&self, local_ns: u64) -> Option<u64> {
        if !self.locked {
            return None;
        }
        let v = self.base_session + (local_ns as f64 - self.base_local as f64) * self.rate;
        let v = if v < 0.0 { 0 } else { v.round() as u64 };
        let prev = self.last_out.fetch_max(v, Ordering::AcqRel);
        Some(prev.max(v))
    }

    /// Current offset, drift ppm, and estimated error.
    pub fn stats(&self) -> ClockStats {
        self.stats
    }

    /// Current servo mapping `(base_local_ns, base_session_ns, rate)`, for
    /// lock-free publication by the protocol loop. `None` until locked.
    pub fn mapping(&self) -> Option<(u64, f64, f64)> {
        if self.locked {
            Some((self.base_local, self.base_session, self.rate))
        } else {
            None
        }
    }

    /// Min-delay selection + linear regression over the window.
    fn estimate(&self) -> Option<Estimate> {
        if self.window.len() < MIN_LOCK_SAMPLES {
            return None;
        }
        let mut pts: Vec<Point> = self.window.iter().copied().collect();
        pts.sort_by_key(|p| p.delay);
        pts.truncate(BEST.min(pts.len()));

        let n = pts.len() as f64;
        let mean_local = pts.iter().map(|p| p.local as f64).sum::<f64>() / n;
        let mean_offset = pts.iter().map(|p| p.offset as f64).sum::<f64>() / n;
        let mut sxx = 0.0;
        let mut sxy = 0.0;
        for p in &pts {
            let dx = p.local as f64 - mean_local;
            let dy = p.offset as f64 - mean_offset;
            sxx += dx * dx;
            sxy += dx * dy;
        }
        let slope = if sxx > 0.0 { sxy / sxx } else { 0.0 };
        let mut sq = 0.0;
        for p in &pts {
            let r = p.offset as f64 - (mean_offset + slope * (p.local as f64 - mean_local));
            sq += r * r;
        }
        Some(Estimate {
            slope,
            mean_local,
            mean_offset,
            rms_residual: (sq / n).sqrt(),
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Deterministic xorshift64 so tests need no external RNG crate.
    fn xorshift(s: &mut u64) -> u64 {
        *s ^= *s << 13;
        *s ^= *s >> 7;
        *s ^= *s << 17;
        *s
    }

    fn sample(t1: u64, d1: i64, proc: i64, d2: i64, off: i64) -> ClockSample {
        let t2 = (t1 as i64 + d1 + off) as u64;
        let t3 = (t2 as i64 + proc) as u64;
        let t4 = (t3 as i64 + d2 - off) as u64;
        ClockSample { t1, t2, t3, t4 }
    }

    #[test]
    fn unlocked_returns_none() {
        let f = ClockFilter::new();
        assert_eq!(f.to_session(123), None);
        assert_eq!(f.mapping(), None);
    }

    #[test]
    fn constant_offset_recovered_exactly() {
        const OFF: i64 = 3_000_000; // 3 ms
        let mut f = ClockFilter::new();
        for i in 0..20u64 {
            let t1 = 1_000_000_000 + i * 100_000_000;
            f.ingest(sample(t1, 50_000, 10_000, 50_000, OFF));
        }
        let s = f.stats();
        assert_eq!(s.offset_ns, OFF);
        assert!(s.drift_ppm.abs() < 1e-6, "drift {} ppm", s.drift_ppm);
        assert_eq!(s.est_error_ns, 0);
        let local = 5_000_000_000u64;
        assert_eq!(f.to_session(local), Some((local as i64 + OFF) as u64));
    }

    #[test]
    fn constant_drift_recovered_within_1ppm() {
        // Master clock runs 50 ppm fast relative to local.
        let m = |r: f64| (r * 1.000_05 + 1_000_000_000.0).round();
        let mut f = ClockFilter::new();
        let (d, p) = (50_000.0, 10_000.0);
        let mut last_t4 = 0u64;
        for i in 0..200u64 {
            let r1 = (10_000_000 + i * 100_000_000) as f64;
            let s = ClockSample {
                t1: r1 as u64,
                t2: m(r1 + d) as u64,
                t3: m(r1 + d + p) as u64,
                t4: (r1 + d + p + d) as u64,
            };
            last_t4 = s.t4;
            f.ingest(s);
        }
        let s = f.stats();
        assert!(
            (s.drift_ppm - 50.0).abs() < 1.0,
            "drift {} ppm, want 50 +/- 1",
            s.drift_ppm
        );
        // Conversion tracks the drifting master within a few microseconds.
        let expected = m(last_t4 as f64) as i64;
        let got = f.to_session(last_t4).unwrap() as i64;
        assert!(
            (got - expected).abs() < 5_000,
            "session error {} ns",
            got - expected
        );
    }

    #[test]
    fn jittery_samples_offset_error_under_50us() {
        const OFF: i64 = 2_500_000;
        let mut seed = 0x1234_5678_9abc_def0u64;
        let mut f = ClockFilter::new();
        let mut last_t4 = 0u64;
        for i in 0..600u64 {
            let t1 = 1_000_000 + i * 100_000_000;
            // Per-leg delay 150 us base + up to 200 us seeded jitter.
            let d1 = 150_000 + (xorshift(&mut seed) % 200_000) as i64;
            let d2 = 150_000 + (xorshift(&mut seed) % 200_000) as i64;
            let s = sample(t1, d1, 10_000, d2, OFF);
            last_t4 = s.t4;
            f.ingest(s);
        }
        let s = f.stats();
        assert!(
            (s.offset_ns - OFF).abs() < 50_000,
            "offset error {} ns",
            s.offset_ns - OFF
        );
        let got = f.to_session(last_t4).unwrap() as i64;
        let err = got - (last_t4 as i64 + OFF);
        assert!(err.abs() < 50_000, "to_session error {} ns", err);
    }

    #[test]
    fn single_10ms_outlier_ignored() {
        const OFF: i64 = 1_000_000;
        let mut f = ClockFilter::new();
        for i in 0..30u64 {
            f.ingest(sample(i * 100_000_000, 50_000, 10_000, 50_000, OFF));
        }
        // One sample delayed 10 ms on the forward leg: its measured offset
        // is off by +5 ms, but its path delay is huge -> min-delay filtered.
        f.ingest(sample(30 * 100_000_000, 10_050_000, 10_000, 50_000, OFF));
        for i in 31..41u64 {
            f.ingest(sample(i * 100_000_000, 50_000, 10_000, 50_000, OFF));
        }
        let s = f.stats();
        assert_eq!(s.offset_ns, OFF, "outlier disturbed the estimate");
        assert_eq!(s.est_error_ns, 0);
        let local = 41 * 100_000_000u64;
        assert_eq!(f.to_session(local), Some((local as i64 + OFF) as u64));
    }

    #[test]
    fn to_session_is_monotonic_across_backward_master_step() {
        let mut f = ClockFilter::new();
        let mut last = 0u64;
        for i in 0..300u64 {
            let t1 = 1_000_000_000 + i * 100_000_000;
            // Master apparently steps back 5 ms halfway through.
            let off = if i < 150 { 1_000_000 } else { -4_000_000 };
            f.ingest(sample(t1, 50_000, 10_000, 50_000, off));
            for k in 0..10u64 {
                let l = t1 + 200_000 + k * 10_000_000;
                if let Some(v) = f.to_session(l) {
                    assert!(v >= last, "stepped back: {v} < {last} at sample {i}");
                    last = v;
                }
            }
        }
        // And it converges (by slewing) to the new offset.
        let s = f.stats();
        assert!(
            (s.offset_ns + 4_000_000).abs() < 50_000,
            "did not converge, offset {} ns",
            s.offset_ns
        );
        let l = 1_000_000_000 + 300 * 100_000_000u64;
        let got = f.to_session(l).unwrap() as i64;
        assert!((got - (l as i64 - 4_000_000)).abs() < 50_000);
    }
}
