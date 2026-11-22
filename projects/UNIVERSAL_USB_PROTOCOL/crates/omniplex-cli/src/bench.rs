//! `omniplex bench midi-under-load` (Task 7.5, spec §11 headline measurement).
//!
//! Compares 1 kHz MIDI-echo round-trip time on an idle link against the same
//! link saturated with the `video/test` bulk channel, and checks the spec
//! target: loaded p99 <= 2x idle p99. Percentile math lives in `crate::stats`
//! (pure, always compiled); this module only does the session-facing I/O, so
//! it needs the `wiring` feature (omniplex-mux/omniplex-session).

use crate::bridge::video::{TestPatternGen, VideoMode};
use crate::cli::{BenchRole, MidiUnderLoadArgs};
use crate::stats::{format_summary, passes_spec, summarize, RttSummary};
use bytes::Bytes;
use omniplex_discovery::Role;
use omniplex_mux::{ChannelRx, ChannelTx};
use omniplex_session::{Session, SessionConfig, SessionEvent};
use omniplex_transport::{LoopbackConfig, LoopbackTransport, Transport};
use omniplex_wire::{
    ChannelDescriptor, ChannelId, ChannelKind, DeliveryClass, FormatDescriptor, LatencyClass,
    PixelFormat, SessionTimestamp,
};
use std::error::Error;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::time::{Duration, Instant};

/// Duration of each RTT measurement pass.
const PASS_SECS: u64 = 60;
/// Target ping rate.
const PING_HZ: u64 = 1_000;

fn bench_desc() -> ChannelDescriptor {
    ChannelDescriptor {
        id: ChannelId(0),
        name: "midi/bench".into(),
        kind: ChannelKind::Midi,
        delivery: DeliveryClass::ReliableOrdered,
        latency: LatencyClass::LatencyCritical,
        format: FormatDescriptor::Midi,
    }
}

fn saturating_video_desc() -> ChannelDescriptor {
    ChannelDescriptor {
        id: ChannelId(0),
        name: "video/test".into(),
        kind: ChannelKind::Video,
        delivery: DeliveryClass::UnreliableTimestamped,
        latency: LatencyClass::Bulk,
        format: FormatDescriptor::Video {
            width: 1920,
            height: 1080,
            fps_num: 60,
            fps_den: 1,
            pixel: PixelFormat::Uyvy,
        },
    }
}

/// Entry point for `omniplex bench midi-under-load`. Returns `Ok(true)` if the
/// spec check passed (or, in two-machine `--role b` mode, once the process
/// finishes acting as the echo server). Non-zero process exit on `Ok(false)`
/// is handled by the caller in `main.rs`.
pub fn run_midi_under_load(args: MidiUnderLoadArgs) -> Result<bool, Box<dyn Error + Send + Sync>> {
    let rt = tokio::runtime::Runtime::new()?;
    if let Some(hours) = args.soak {
        return rt.block_on(soak(hours, args.loopback));
    }
    if args.loopback {
        return rt.block_on(loopback_bench());
    }
    rt.block_on(two_machine_bench(args.role))
}

// ---------------------------------------------------------------------------
// Loopback (in-process, CI-friendly) mode.

async fn loopback_bench() -> Result<bool, Box<dyn Error + Send + Sync>> {
    // Bandwidth-capped so the video flood can actually saturate the link.
    let cfg = LoopbackConfig {
        bandwidth_bytes_per_sec: Some(50_000_000 / 8), // 50 Mbps
        ..Default::default()
    };
    let (a_transport, b_transport) = LoopbackTransport::pair(cfg);
    let a: Arc<dyn Transport> = Arc::new(a_transport);
    let b: Arc<dyn Transport> = Arc::new(b_transport);

    let (session_a, mut events_a) =
        Session::from_transport(a, Role::Dialer, SessionConfig::default());
    let (session_b, events_b) =
        Session::from_transport(b, Role::Listener, SessionConfig::default());
    let session_a = Arc::new(session_a);
    let session_b = Arc::new(session_b);

    wait_connected(&mut events_a).await;
    let mut offered_b = spawn_offer_watcher(Arc::clone(&session_b), events_b);

    let (tx_a, rx_a) = session_a.open_channel(bench_desc()).await?;
    let (tx_b, rx_b) = wait_offered(&mut offered_b, "midi/bench").await;
    tokio::spawn(echo_loop(tx_b, rx_b));

    println!("omniplex bench midi-under-load (loopback): idle pass...");
    let idle = measure_rtt(&tx_a, rx_a).await?;
    println!("{}", format_summary("idle", &idle));

    // Saturate with a video flood A -> B while re-measuring.
    let (video_tx, _video_rx) = session_a.open_channel(saturating_video_desc()).await?;
    let stop = Arc::new(AtomicBool::new(false));
    let flood = tokio::spawn(video_flood(video_tx, session_a.clock(), Arc::clone(&stop)));

    let (tx_a2, rx_a2) = session_a.open_channel(bench_desc()).await?;
    let (tx_b2, rx_b2) = wait_offered(&mut offered_b, "midi/bench").await;
    tokio::spawn(echo_loop(tx_b2, rx_b2));
    println!("omniplex bench midi-under-load (loopback): loaded pass...");
    let loaded = measure_rtt(&tx_a2, rx_a2).await?;
    stop.store(true, Ordering::Relaxed);
    let _ = flood.await;
    println!("{}", format_summary("loaded", &loaded));

    let ok = passes_spec(&idle, &loaded);
    println!(
        "spec check (loaded p99 <= 2x idle p99): {}",
        if ok { "PASS" } else { "FAIL" }
    );
    Ok(ok)
}

// ---------------------------------------------------------------------------
// Two-machine mode (real discovery/QUIC session).

async fn two_machine_bench(role: BenchRole) -> Result<bool, Box<dyn Error + Send + Sync>> {
    let (session, mut events) = Session::start(SessionConfig::default());
    println!("omniplex bench: waiting for peer...");
    wait_connected(&mut events).await;

    match role {
        BenchRole::B => {
            // Echo server: forward every offered channel's traffic straight
            // back, forever (until the session drops).
            loop {
                match events.recv().await {
                    Some(SessionEvent::ChannelOffered(desc)) => {
                        if let Some((tx, rx)) = session.take_offered(desc.id) {
                            println!("omniplex bench (role b): echoing {}", desc.name);
                            tokio::spawn(echo_loop(tx, rx));
                        }
                    }
                    Some(_) => {}
                    None => break,
                }
            }
            Ok(true)
        }
        BenchRole::A => {
            let (tx, rx) = session.open_channel(bench_desc()).await?;
            println!("omniplex bench (role a): idle pass...");
            let idle = measure_rtt(&tx, rx).await?;
            println!("{}", format_summary("idle", &idle));

            let (video_tx, _video_rx) = session.open_channel(saturating_video_desc()).await?;
            let stop = Arc::new(AtomicBool::new(false));
            let flood = tokio::spawn(video_flood(video_tx, session.clock(), Arc::clone(&stop)));

            let (tx2, rx2) = session.open_channel(bench_desc()).await?;
            println!("omniplex bench (role a): loaded pass...");
            let loaded = measure_rtt(&tx2, rx2).await?;
            stop.store(true, Ordering::Relaxed);
            let _ = flood.await;
            println!("{}", format_summary("loaded", &loaded));

            let ok = passes_spec(&idle, &loaded);
            println!(
                "spec check (loaded p99 <= 2x idle p99): {}",
                if ok { "PASS" } else { "FAIL" }
            );
            Ok(ok)
        }
    }
}

// ---------------------------------------------------------------------------
// Soak mode.

async fn soak(hours: f64, _loopback: bool) -> Result<bool, Box<dyn Error + Send + Sync>> {
    let end = Instant::now() + Duration::from_secs_f64(hours * 3600.0);
    let stats = Arc::new(crate::bridge::audio::AudioBridgeStats::default());
    println!("omniplex bench --soak {hours}h: starting session, logging RSS every 60s");
    let (session, mut events) = Session::start(SessionConfig::default());
    wait_connected(&mut events).await;

    let (audio_tx, audio_rx) = session.open_channel(crate::bridge::audio_desc()).await?;
    let clock = session.clock();
    let audio_stats = Arc::clone(&stats);
    crate::bridge::audio::spawn_thread(
        audio_tx,
        audio_rx,
        clock,
        audio_stats,
        crate::bridge::audio::DEFAULT_AUDIO_BUFFER_FRAMES,
    );

    let mut ticker = tokio::time::interval(Duration::from_secs(60));
    let mut ok = true;
    while Instant::now() < end {
        ticker.tick().await;
        let rss = read_rss_kb().unwrap_or(0);
        let underruns = stats.output_ring_underruns.load(Ordering::Relaxed);
        println!(
            "soak: t={:?} rss={rss}kB underruns={underruns}",
            Instant::now()
        );
        if underruns > 0 {
            ok = false;
        }
        if let Ok(SessionEvent::Disconnected(reason)) = events.try_recv() {
            println!("soak: disconnected: {reason:?}");
        }
    }
    println!(
        "soak complete: {}",
        if ok {
            "PASS (zero underruns)"
        } else {
            "FAIL (underruns observed)"
        }
    );
    Ok(ok)
}

/// Best-effort resident set size in kB via `ps` (portable enough for macOS/Linux;
/// avoids adding a new dependency for a diagnostic-only soak log line).
fn read_rss_kb() -> Option<u64> {
    let pid = std::process::id().to_string();
    let out = std::process::Command::new("ps")
        .args(["-o", "rss=", "-p", &pid])
        .output()
        .ok()?;
    String::from_utf8_lossy(&out.stdout).trim().parse().ok()
}

// ---------------------------------------------------------------------------
// Shared helpers.

async fn wait_connected(events: &mut tokio::sync::mpsc::Receiver<SessionEvent>) {
    while let Some(ev) = events.recv().await {
        if matches!(ev, SessionEvent::Connected { .. }) {
            return;
        }
    }
}

/// Watches a session's events for `ChannelOffered` and stashes `(name ->
/// take_offered result)` so the bench harness (which knows the channel by
/// name, not id) can pick it up.
fn spawn_offer_watcher(
    session: Arc<Session>,
    mut events: tokio::sync::mpsc::Receiver<SessionEvent>,
) -> tokio::sync::mpsc::UnboundedReceiver<(String, ChannelTx, ChannelRx)> {
    let (tx, rx) = tokio::sync::mpsc::unbounded_channel();
    tokio::spawn(async move {
        while let Some(ev) = events.recv().await {
            if let SessionEvent::ChannelOffered(desc) = ev {
                if let Some((ctx, crx)) = session.take_offered(desc.id) {
                    let _ = tx.send((desc.name, ctx, crx));
                }
            }
        }
    });
    rx
}

async fn wait_offered(
    rx: &mut tokio::sync::mpsc::UnboundedReceiver<(String, ChannelTx, ChannelRx)>,
    name: &str,
) -> (ChannelTx, ChannelRx) {
    loop {
        let (offered_name, tx, crx) = rx.recv().await.expect("peer offered the bench channel");
        if offered_name == name {
            return (tx, crx);
        }
        // Not the channel we're waiting for right now (e.g. video/test
        // arriving between two bench-channel opens) — drain it quietly and
        // keep looking; our own tx side is unused for channels we don't own.
        drop(tx);
        tokio::spawn(async move {
            let mut crx = crx;
            while crx.recv().await.is_some() {}
        });
    }
}

/// B side: forward every received payload straight back unmodified.
async fn echo_loop(tx: ChannelTx, mut rx: ChannelRx) {
    loop {
        match rx.recv().await {
            Some((bytes, ts)) => {
                if tx.send(bytes, ts).await.is_err() {
                    return;
                }
            }
            None => return,
        }
    }
}

/// A side: send `PING_HZ` timestamped pings for `PASS_SECS`, matching each
/// echoed reply back to its send time to build the RTT distribution.
async fn measure_rtt(
    tx: &ChannelTx,
    mut rx: ChannelRx,
) -> Result<RttSummary, Box<dyn Error + Send + Sync>> {
    let n = PING_HZ * PASS_SECS;
    let period = Duration::from_nanos(1_000_000_000 / PING_HZ);
    let epoch = Instant::now();
    let mut samples = Vec::with_capacity(n as usize);

    let recv_task = tokio::spawn(async move {
        let mut got = Vec::with_capacity(n as usize);
        while got.len() < n as usize {
            match rx.recv().await {
                Some((bytes, _ts)) => {
                    if bytes.len() >= 8 {
                        let send_ns = u64::from_le_bytes(bytes[0..8].try_into().unwrap());
                        got.push(send_ns);
                    }
                }
                None => break,
            }
        }
        got
    });

    let mut ticker = tokio::time::interval(period);
    for _ in 0..n {
        ticker.tick().await;
        let send_ns = epoch.elapsed().as_nanos() as u64;
        let mut payload = send_ns.to_le_bytes().to_vec();
        payload.resize(16, 0);
        if tx
            .send(Bytes::from(payload), SessionTimestamp(send_ns))
            .await
            .is_err()
        {
            break;
        }
    }

    // Give in-flight echoes a moment to land, then stop waiting.
    let recv_deadline = tokio::time::sleep(Duration::from_millis(500));
    tokio::pin!(recv_deadline);
    let received = tokio::select! {
        r = recv_task => r.unwrap_or_default(),
        _ = &mut recv_deadline => Vec::new(),
    };

    let now = epoch.elapsed().as_nanos() as u64;
    for send_ns in received {
        samples.push(now.saturating_sub(send_ns));
    }
    Ok(summarize(samples).unwrap_or(RttSummary {
        count: 0,
        p50: 0,
        p90: 0,
        p99: 0,
        max: 0,
    }))
}

/// Bulk video flood used to saturate the link for the loaded pass.
async fn video_flood(tx: ChannelTx, clock: omniplex_clock::SessionClock, stop: Arc<AtomicBool>) {
    let mode = VideoMode {
        width: 1920,
        height: 1080,
        fps_num: 60,
        fps_den: 1,
    };
    let mut gen = TestPatternGen::new(mode);
    let mut interval = tokio::time::interval(Duration::from_nanos(mode.frame_interval_ns()));
    while !stop.load(Ordering::Relaxed) {
        interval.tick().await;
        let (_c, frame) = gen.next_frame();
        let ts = clock.now_session_ns().unwrap_or(0);
        if tx
            .send(Bytes::from(frame), SessionTimestamp(ts))
            .await
            .is_err()
        {
            return;
        }
    }
}
