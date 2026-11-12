//! `omniplex run` wiring (Tasks 7.1–7.4): starts the session, spawns the enabled
//! bridges, prints session events, and renders the 1 Hz metrics table.

use crate::bridge::{self, osc::OscRoute, video::VideoMode};
use crate::ui::{render_table, MetricsRow};
use bytes::Bytes;
use omniplex_clock::SessionClock;
use omniplex_mux::{ChannelRx, ChannelTx};
use omniplex_session::{Session, SessionConfig, SessionEvent};
use omniplex_wire::{ChannelId, SessionTimestamp};
use std::collections::HashMap;
use std::sync::Arc;
use tokio::net::UdpSocket;
use tokio::sync::mpsc;

#[derive(Clone, Debug, Default)]
pub struct RunOptions {
    pub midi: bool,
    /// (recv_port, send_port) on localhost.
    pub osc: Option<(u16, u16)>,
    pub serial: Option<String>,
    pub audio: bool,
    /// cpal input/output stream buffer size in frames (see
    /// `cli::RunArgs::audio_buffer_frames`); 0 falls back to
    /// [`bridge::audio::DEFAULT_AUDIO_BUFFER_FRAMES`].
    pub audio_buffer_frames: u32,
    pub video: Option<VideoMode>,
    /// Real screen capture (full/region/window) instead of the synthetic
    /// test pattern; mutually exclusive with `video` (CLI enforces this via
    /// `conflicts_with`, but the two are also independent branches here).
    pub screen_capture: Option<bridge::screen_capture::ScreenCaptureSpec>,
    /// Write received raw video frames to stdout (for ffplay).
    pub video_stdout: bool,
}

/// Human class string for the metrics table.
fn class_str(d: omniplex_wire::DeliveryClass, l: omniplex_wire::LatencyClass) -> String {
    let d = match d {
        omniplex_wire::DeliveryClass::ReliableOrdered => "rel-ord",
        omniplex_wire::DeliveryClass::UnreliableTimestamped => "unrel-ts",
        omniplex_wire::DeliveryClass::LatestValueWins => "latest",
    };
    let l = match l {
        omniplex_wire::LatencyClass::LatencyCritical => "critical",
        omniplex_wire::LatencyClass::Interactive => "interactive",
        omniplex_wire::LatencyClass::Bulk => "bulk",
    };
    format!("{d}/{l}")
}

pub async fn run(opts: RunOptions) -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    let (session, mut events) = Session::start(SessionConfig::default());
    let session = Arc::new(session);
    println!("omniplex: session starting, waiting for peer…");

    // Channel-name registry feeding the metrics table.
    let registry: Arc<parking_lot_free::Registry> = Arc::new(parking_lot_free::Registry::default());

    // Pressure events fan out to the video bridge.
    let (pressure_tx, pressure_rx) = mpsc::channel::<(ChannelId, u64)>(16);

    // Bridges.
    if opts.midi {
        let (tx, rx) = open(&session, &registry, bridge::midi_desc()).await?;
        let clock = session.clock();
        tokio::spawn(async move {
            if let Err(e) = bridge::midi::run(tx, rx, clock).await {
                tracing::error!("midi bridge exited: {e}");
            }
        });
    }
    if let Some((recv_port, send_port)) = opts.osc {
        spawn_osc(&session, &registry, recv_port, send_port).await?;
    }
    if let Some(dev) = opts.serial.clone() {
        spawn_serial(&session, &registry, dev).await?;
    }
    if opts.audio {
        let (tx, rx) = open(&session, &registry, bridge::audio_desc()).await?;
        let clock = session.clock();
        let stats = Arc::new(bridge::audio::AudioBridgeStats::default());
        let buffer_frames = if opts.audio_buffer_frames == 0 {
            bridge::audio::DEFAULT_AUDIO_BUFFER_FRAMES
        } else {
            opts.audio_buffer_frames
        };
        bridge::audio::spawn_thread(tx, rx, clock, stats, buffer_frames);
    }
    if let Some(mode) = opts.video {
        spawn_video_sender(&session, &registry, mode, pressure_rx).await?;
    } else if let Some(spec) = opts.screen_capture {
        spawn_screen_capture_sender(&session, &registry, spec, pressure_rx).await?;
    }

    // Event loop + 1 Hz metrics table. Remote-initiated channels (the peer's
    // bridges) arrive as `SessionEvent::ChannelOffered`; the session stashes
    // their (ChannelTx, ChannelRx) until we retrieve them via
    // `take_offered` (there is no separate `accept_channel` on `Session` —
    // that's a `Mux`-level API the session facade already drives).
    let hub = session.metrics();
    let video_stdout = opts.video_stdout;
    let mut ticker = tokio::time::interval(std::time::Duration::from_secs(1));
    let mut prev_bytes: HashMap<u16, u64> = HashMap::new();
    loop {
        tokio::select! {
            ev = events.recv() => {
                let Some(ev) = ev else { break };
                println!("omniplex: event: {ev:?}");
                match ev {
                    SessionEvent::BandwidthPressure { channel, allocated_bps } => {
                        let _ = pressure_tx.try_send((channel, allocated_bps));
                    }
                    SessionEvent::ChannelOffered(desc) => {
                        if let Some((tx, rx)) = session.take_offered(desc.id) {
                            registry.insert(desc.id, &desc.name, class_str(desc.delivery, desc.latency));
                            println!("omniplex: peer opened channel {:?} ({})", desc.id, desc.name);
                            spawn_inbound(&desc, tx, rx, video_stdout, session.clock());
                        }
                    }
                    _ => {}
                }
            }
            _ = ticker.tick() => {
                let rows = rows_from_snapshot(&hub, &registry, &mut prev_bytes);
                print!("{}", render_table(&rows));
            }
        }
    }
    Ok(())
}

/// Convert `MetricsHub::snapshot()` into display rows.
///
/// NOTE (integrator): assumes `ChannelMetrics { latency: HistogramSnapshot
/// (with .p50()/.p99() -> u64 ns), jitter_ns, drops: DropCounters { late,
/// lost, backpressure, replaced }, bytes_tx, bytes_rx, allocated_bps }`
/// per the Milestone 6 plan. Adjust accessors here only.
fn rows_from_snapshot(
    hub: &omniplex_metrics::MetricsHub,
    registry: &parking_lot_free::Registry,
    prev_bytes: &mut HashMap<u16, u64>,
) -> Vec<MetricsRow> {
    let mut rows = Vec::new();
    for (id, m) in hub.snapshot() {
        let (name, class) = registry.get(ChannelId(id));
        let total = m.bytes_tx + m.bytes_rx;
        let prev = prev_bytes.insert(id, total).unwrap_or(total);
        let drops = m.drops.late + m.drops.lost + m.drops.backpressure + m.drops.replaced;
        rows.push(MetricsRow {
            channel: format!("{name} (#{})", id),
            class,
            p50_us: m.latency.p50 as f64 / 1000.0,
            p99_us: m.latency.p99 as f64 / 1000.0,
            jitter_us: m.jitter_ns as f64 / 1000.0,
            drops,
            bandwidth_bps: (total.saturating_sub(prev)) as f64 * 8.0,
        });
    }
    rows
}

async fn open(
    session: &Session,
    registry: &parking_lot_free::Registry,
    desc: omniplex_wire::ChannelDescriptor,
) -> Result<(ChannelTx, ChannelRx), Box<dyn std::error::Error + Send + Sync>> {
    let name = desc.name.clone();
    let class = class_str(desc.delivery, desc.latency);
    let (tx, rx) = session.open_channel(desc).await?;
    // We don't know the allocated id here without echoing it back from the
    // mux; register by name and patch ids from snapshots lazily.
    registry.insert_by_name(&name, class);
    println!("omniplex: opened channel {name}");
    Ok((tx, rx))
}

// ---------------------------------------------------------------------------
// OSC bridge wiring (pure router in bridge::osc).

async fn spawn_osc(
    session: &Arc<Session>,
    registry: &Arc<parking_lot_free::Registry>,
    recv_port: u16,
    send_port: u16,
) -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    let (rel_tx, mut rel_rx) = open(session, registry, bridge::osc_reliable_desc()).await?;
    let (lv_tx, mut lv_rx) = open(session, registry, bridge::osc_lv_desc()).await?;
    let clock = session.clock();

    let sock = Arc::new(UdpSocket::bind(("127.0.0.1", recv_port)).await?);
    let send_sock = Arc::clone(&sock);

    // Inbound (both channels) -> local UDP send_port.
    tokio::spawn(async move {
        loop {
            tokio::select! {
                r = rel_rx.recv() => {
                    let Some((bytes, _)) = r else { break };
                    let _ = send_sock.send_to(&bytes, ("127.0.0.1", send_port)).await;
                }
                r = lv_rx.recv() => {
                    let Some((bytes, _)) = r else { break };
                    let _ = send_sock.send_to(&bytes, ("127.0.0.1", send_port)).await;
                }
            }
        }
    });

    // UDP recv -> route -> channel.
    tokio::spawn(async move {
        let mut router = bridge::osc::OscRouter::new();
        let mut buf = vec![0u8; 65536];
        let epoch = std::time::Instant::now();
        loop {
            let Ok((n, _from)) = sock.recv_from(&mut buf).await else {
                break;
            };
            let now = epoch.elapsed().as_nanos() as u64;
            let ts = clock.now_session_ns().unwrap_or(0);
            let payload = Bytes::copy_from_slice(&buf[..n]);
            let res = match router.route(now, &buf[..n]) {
                OscRoute::Reliable => rel_tx.send(payload, SessionTimestamp(ts)).await,
                OscRoute::LatestValue => lv_tx.send(payload, SessionTimestamp(ts)).await,
            };
            if res.is_err() {
                break;
            }
        }
    });
    println!("omniplex: osc bridge up: udp :{recv_port} -> channels, channels -> udp :{send_port}");
    Ok(())
}

// ---------------------------------------------------------------------------
// Serial bridge wiring (pure pumps in bridge::serial).

async fn spawn_serial(
    session: &Arc<Session>,
    registry: &Arc<parking_lot_free::Registry>,
    dev: String,
) -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    const BAUD: u32 = 115_200;
    let (tx, mut rx) = open(session, registry, bridge::serial_desc(Some(BAUD))).await?;
    let clock = session.clock();
    let (reader, writer) = bridge::serial::open_port(&dev, BAUD)?;

    // Port -> channel: blocking reader thread -> bounded queue -> async send.
    let (dev_tx, dev_rx) = std::sync::mpsc::sync_channel::<Vec<u8>>(bridge::serial::SERIAL_QUEUE);
    std::thread::spawn(move || bridge::serial::pump_reader(reader, dev_tx));
    tokio::task::spawn_blocking(move || {
        // recv() blocks; tx.send is awaited on a scoped runtime handle so the
        // bounded channel gives publisher-blocks backpressure both ways.
        let rt = tokio::runtime::Handle::current();
        while let Ok(chunk) = dev_rx.recv() {
            let ts = clock.now_session_ns().unwrap_or(0);
            if rt
                .block_on(tx.send(Bytes::from(chunk), SessionTimestamp(ts)))
                .is_err()
            {
                break;
            }
        }
    });

    // Channel -> port: async recv -> bounded queue -> blocking writer thread.
    let (wr_tx, wr_rx) = std::sync::mpsc::sync_channel::<Vec<u8>>(bridge::serial::SERIAL_QUEUE);
    std::thread::spawn(move || bridge::serial::pump_writer(writer, wr_rx));
    tokio::spawn(async move {
        while let Some((bytes, _ts)) = rx.recv().await {
            // Blocking send: if the port is slower than the peer, this task
            // stalls and the channel's bounded buffer backpressures the peer.
            let wr_tx = wr_tx.clone();
            let bytes = bytes.to_vec();
            if tokio::task::spawn_blocking(move || wr_tx.send(bytes))
                .await
                .is_err()
            {
                break;
            }
        }
    });
    println!("omniplex: serial bridge up: {dev} <-> serial/1 @ {BAUD}");
    Ok(())
}

// ---------------------------------------------------------------------------
// Video test channel wiring (pure generator/ladder in bridge::video).

async fn spawn_video_sender(
    session: &Arc<Session>,
    registry: &Arc<parking_lot_free::Registry>,
    mode: VideoMode,
    mut pressure: mpsc::Receiver<(ChannelId, u64)>,
) -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    let (tx, _rx) = open(
        session,
        registry,
        bridge::video_desc(mode, omniplex_wire::PixelFormat::Uyvy, "video/test"),
    )
    .await?;
    let clock = session.clock();
    tokio::spawn(async move {
        let mut ladder = bridge::video::DegradeLadder::new(mode);
        let mut gen = bridge::video::TestPatternGen::new(mode);
        let mut interval =
            tokio::time::interval(std::time::Duration::from_nanos(mode.frame_interval_ns()));
        loop {
            tokio::select! {
                _ = interval.tick() => {
                    let (_counter, frame) = gen.next_frame();
                    let ts = clock.now_session_ns().unwrap_or(0);
                    if tx.send(Bytes::from(frame), SessionTimestamp(ts)).await.is_err() {
                        break;
                    }
                }
                Some((chan, bps)) = pressure.recv() => {
                    let new_mode = ladder.on_pressure();
                    tracing::info!(
                        "bandwidth pressure on {chan:?} ({bps} bps): degrading to {}x{}@{:.2}",
                        new_mode.width, new_mode.height, new_mode.fps()
                    );
                    if new_mode.width != gen.mode.width || new_mode.height != gen.mode.height {
                        gen = bridge::video::TestPatternGen::new(new_mode);
                    }
                    interval = tokio::time::interval(std::time::Duration::from_nanos(
                        new_mode.frame_interval_ns(),
                    ));
                }
            }
        }
    });
    println!(
        "omniplex: video test pattern up: {}x{}@{:.2} -> video/test",
        mode.width,
        mode.height,
        mode.fps()
    );
    Ok(())
}

// ---------------------------------------------------------------------------
// Real screen-capture wiring (Linux-only backend in
// bridge::screen_capture::platform; parsing/math is pure and cross-platform).

async fn spawn_screen_capture_sender(
    session: &Arc<Session>,
    registry: &Arc<parking_lot_free::Registry>,
    spec: bridge::screen_capture::ScreenCaptureSpec,
    mut pressure: mpsc::Receiver<(ChannelId, u64)>,
) -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    // `scrap`/`x11rb` capture state isn't `Send`, so the capturer is built
    // and driven entirely on one dedicated OS thread (never crossing an
    // `.await`) — the same shape `bridge::serial`'s reader/writer pumps
    // use. The thread reports its (possibly failing) startup result back
    // over a std::sync::mpsc rendezvous before we open the channel, since
    // we need the real capture dimensions (native display/window size) for
    // the channel descriptor.
    let (ready_tx, ready_rx) = std::sync::mpsc::sync_channel::<
        Result<bridge::video::VideoMode, bridge::screen_capture::CaptureError>,
    >(1);
    let (frame_tx, frame_rx) = std::sync::mpsc::sync_channel::<Vec<u8>>(4);
    let (mode_tx, mode_rx) = std::sync::mpsc::channel::<bridge::video::VideoMode>();
    let stop = Arc::new(std::sync::atomic::AtomicBool::new(false));
    let thread_spec = spec.clone();
    let thread_stop = Arc::clone(&stop);
    std::thread::spawn(move || {
        match bridge::screen_capture::platform::ScreenCapturer::new(&thread_spec) {
            Ok(capturer) => {
                let native = capturer.native_mode();
                if ready_tx.send(Ok(native)).is_err() {
                    return; // Async side gave up already.
                }
                bridge::screen_capture::run_capture_loop(
                    capturer,
                    native,
                    mode_rx,
                    frame_tx,
                    thread_stop,
                );
            }
            Err(e) => {
                let _ = ready_tx.send(Err(e));
            }
        }
    });

    let native_mode = tokio::task::spawn_blocking(move || ready_rx.recv())
        .await
        .map_err(|e| format!("screen capture startup task panicked: {e}"))?
        .map_err(|_| "screen capture thread exited before reporting startup".to_string())?
        .map_err(|e| format!("screen capture init failed: {e}"))?;

    let (tx, _rx) = open(
        session,
        registry,
        bridge::video_desc(
            native_mode,
            omniplex_wire::PixelFormat::Bgra,
            "video/screen",
        ),
    )
    .await?;
    let clock = session.clock();

    // Frame pump: blocking recv from the capture thread -> async channel
    // send, mirroring bridge::serial's device-thread -> channel pump.
    let pump_stop = Arc::clone(&stop);
    tokio::task::spawn_blocking(move || {
        let rt = tokio::runtime::Handle::current();
        while let Ok(frame) = frame_rx.recv() {
            let ts = clock.now_session_ns().unwrap_or(0);
            if rt
                .block_on(tx.send(Bytes::from(frame), SessionTimestamp(ts)))
                .is_err()
            {
                break;
            }
        }
        pump_stop.store(true, std::sync::atomic::Ordering::Relaxed);
    });

    // Pressure -> degrade ladder -> capture-thread target mode updates.
    tokio::spawn(async move {
        let mut ladder = bridge::video::DegradeLadder::new(native_mode);
        while let Some((chan, bps)) = pressure.recv().await {
            let new_mode = ladder.on_pressure();
            tracing::info!(
                "bandwidth pressure on {chan:?} ({bps} bps): degrading screen capture to {}x{}@{:.2}",
                new_mode.width, new_mode.height, new_mode.fps()
            );
            if mode_tx.send(new_mode).is_err() {
                break;
            }
        }
    });

    println!(
        "omniplex: screen capture up: {:?} @ {:.2} fps ({}x{}) -> video/screen",
        spec.target,
        native_mode.fps(),
        native_mode.width,
        native_mode.height
    );
    Ok(())
}

/// Route a peer-opened channel to the matching local bridge behavior.
fn spawn_inbound(
    desc: &omniplex_wire::ChannelDescriptor,
    _tx: ChannelTx,
    mut rx: ChannelRx,
    video_stdout: bool,
    _clock: SessionClock,
) {
    let name = desc.name.clone();
    if name.starts_with("video/") {
        let (width, pixel) = match desc.format {
            omniplex_wire::FormatDescriptor::Video { width, pixel, .. } => (width, pixel),
            _ => (0, omniplex_wire::PixelFormat::Uyvy),
        };
        // Only `video/test` carries the synthetic burned-in counter;
        // anything else (e.g. `video/screen`'s real captured BGRA frames)
        // has nothing for `decode_counter` to find, so don't bother trying
        // — just track frame count/size instead of a bogus corrupt-rate.
        let has_counter = name == "video/test" && matches!(pixel, omniplex_wire::PixelFormat::Uyvy);
        tokio::spawn(async move {
            let mut cc = bridge::video::ContinuityChecker::default();
            let mut frames = 0u64;
            use std::io::Write;
            while let Some((frame, _ts)) = rx.recv().await {
                if video_stdout {
                    let _ = std::io::stdout().write_all(&frame);
                } else if has_counter {
                    cc.feed(bridge::video::decode_counter(&frame, width));
                    if (cc.frames_ok + cc.frames_corrupt) % 300 == 0 {
                        eprintln!(
                            "{name} rx: ok={} corrupt={} gaps={} missing={}",
                            cc.frames_ok, cc.frames_corrupt, cc.gaps, cc.missing_frames
                        );
                    }
                } else {
                    frames += 1;
                    if frames.is_multiple_of(300) {
                        eprintln!(
                            "{name} rx: frames={frames} last_frame_bytes={}",
                            frame.len()
                        );
                    }
                }
            }
        });
    } else {
        // Other inbound kinds are handled by the locally spawned bridges
        // (which opened their own directional channels); drain politely.
        tokio::spawn(async move { while rx.recv().await.is_some() {} });
    }
}

/// Tiny name/class registry (std-mutex; not on any hot path).
mod parking_lot_free {
    use omniplex_wire::ChannelId;
    use std::collections::HashMap;
    use std::sync::Mutex;

    #[derive(Default)]
    pub struct Registry {
        by_id: Mutex<HashMap<u16, (String, String)>>,
        by_name: Mutex<HashMap<String, String>>,
    }

    impl Registry {
        pub fn insert(&self, id: ChannelId, name: &str, class: String) {
            self.by_id
                .lock()
                .unwrap()
                .insert(id.0, (name.to_string(), class));
        }
        pub fn insert_by_name(&self, name: &str, class: String) {
            self.by_name.lock().unwrap().insert(name.to_string(), class);
        }
        pub fn get(&self, id: ChannelId) -> (String, String) {
            if let Some((n, c)) = self.by_id.lock().unwrap().get(&id.0) {
                return (n.clone(), c.clone());
            }
            (format!("ch{}", id.0), "?".into())
        }
    }
}
