//! Loopback integration test for the clock protocol loop (plan Task 4.2).
//!
//! NOTE: this file compiles only once `omniplex-wire` and `omniplex-transport` land
//! (they are being built in parallel against the same plan). The pure-math
//! filter tests remain green standalone via
//! `cargo test -p omniplex-clock --no-default-features`.
#![cfg(feature = "proto")]

use std::sync::Arc;
use std::time::Duration;

use omniplex_clock::reexports::omniplex_transport::{LoopbackConfig, LoopbackTransport, Transport};
use omniplex_clock::{LocalClock, SessionClock};

fn rt() -> tokio::runtime::Runtime {
    tokio::runtime::Builder::new_current_thread()
        .enable_all()
        .build()
        .unwrap()
}

fn loopback_cfg(seed: u64) -> LoopbackConfig {
    LoopbackConfig {
        datagram_loss: 0.0,
        latency: Duration::from_micros(200),
        jitter: Duration::from_micros(100),
        bandwidth_bytes_per_sec: None,
        seed,
    }
}

async fn spawn_pair(
    seed: u64,
    master_clock: LocalClock,
    follower_clock: LocalClock,
) -> (SessionClock, SessionClock) {
    let (ta, tb) = LoopbackTransport::pair(loopback_cfg(seed));
    let (master_stream, follower_stream) = tokio::join!(ta.open_stream(100), tb.accept_stream());
    let master = SessionClock::spawn_master_with_clock(master_stream.unwrap(), master_clock);
    let follower =
        SessionClock::spawn_follower_with_clock(follower_stream.unwrap(), follower_clock);
    (master, follower)
}

/// Follower locks within 2 s of (paused) time over a 200 us / 100 us-jitter
/// link and agrees with the master's session time.
#[test]
fn follower_locks_within_2s_over_jittery_loopback() {
    rt().block_on(async {
        tokio::time::pause();
        let epoch = tokio::time::Instant::now();
        let master_clock: LocalClock =
            Arc::new(move || epoch.elapsed().as_nanos() as u64 + 7_000_000_000);
        let follower_clock: LocalClock = Arc::new(move || epoch.elapsed().as_nanos() as u64);
        let (master, follower) = spawn_pair(7, master_clock, follower_clock).await;

        tokio::time::sleep(Duration::from_secs(2)).await;

        let f = follower
            .now_session_ns()
            .expect("follower must lock within 2 s");
        let m = master.now_session_ns().unwrap();
        let err = f as i64 - m as i64;
        assert!(err.abs() < 150_000, "session offset error {err} ns");
        assert!(follower.stats().est_error_ns < 150_000);
    });
}

/// Follower whose local clock runs 50 ppm fast tracks the simulated drift:
/// drift is reported, session time stays aligned, and reads are monotonic.
#[test]
fn simulated_drift_is_tracked() {
    rt().block_on(async {
        tokio::time::pause();
        let epoch = tokio::time::Instant::now();
        let master_clock: LocalClock =
            Arc::new(move || epoch.elapsed().as_nanos() as u64 + 3_000_000_000);
        // Follower local clock runs fast by 50 ppm.
        let follower_clock: LocalClock =
            Arc::new(move || (epoch.elapsed().as_nanos() as f64 * 1.000_05) as u64);
        let (master, follower) = spawn_pair(21, master_clock, follower_clock).await;

        let mut last = 0u64;
        for _ in 0..60 {
            tokio::time::sleep(Duration::from_millis(100)).await;
            if let Some(v) = follower.now_session_ns() {
                assert!(v >= last, "now_session_ns stepped back: {v} < {last}");
                last = v;
            }
        }

        // Master runs ~50 ppm slow relative to the follower's local clock.
        let drift = follower.stats().drift_ppm;
        assert!(
            (drift + 50.0).abs() < 5.0,
            "drift {drift} ppm, expected about -50"
        );
        let f = follower.now_session_ns().unwrap();
        let m = master.now_session_ns().unwrap();
        let err = f as i64 - m as i64;
        assert!(err.abs() < 150_000, "session offset error {err} ns");
    });
}
