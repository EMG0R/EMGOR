//! Audio bridge wiring (Task 7.3): cpal devices <-> channel `audio/main`
//! (UnreliableTimestamped, LatencyCritical). All DSP/jitter/ASRC logic is in
//! [`super::audio_core`] (pure, unit-tested); this file only moves samples
//! between cpal callbacks, rtrb SPSC rings, and the channel.
//!
//! RT discipline (§12): the cpal callbacks touch only preallocated rtrb
//! rings — no allocation, no locks, no async.

use super::audio_core::{
    decode_payload, encode_payload, JitterAsrc, JitterAsrcConfig, PacketPacker, RxAudioStats,
    PACKET_FRAMES,
};
use bytes::Bytes;
use cpal::traits::{DeviceTrait, HostTrait, StreamTrait};
use omniplex_clock::SessionClock;
use omniplex_mux::{ChannelRx, ChannelTx};
use omniplex_wire::SessionTimestamp;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Arc;
use std::time::Instant;
use tokio::sync::mpsc;

pub use super::audio_core::{CHANNELS, SAMPLE_RATE};
/// Ring capacity: ~250 ms of audio.
const RING_SAMPLES: usize = SAMPLE_RATE as usize * CHANNELS / 4;

/// Default cpal input/output stream buffer size in frames. Low: 64 frames @
/// 48 kHz = 1.33 ms/side (down from a previous default of 128 frames = 2.67
/// ms/side). Lower values cut latency further but raise underrun risk on
/// weaker CPUs (e.g. a Raspberry Pi) — this default has not been validated
/// on such hardware, only in the (cpal-free) simulated jitter/drift unit
/// tests in `audio_core`; see `--audio-buffer-frames` to raise it back up if
/// you see glitches or `output_ring_underruns` climbing.
pub const DEFAULT_AUDIO_BUFFER_FRAMES: u32 = 64;

/// Shared counters the metrics/soak code can read.
#[derive(Default)]
pub struct AudioBridgeStats {
    pub rx_underrun_packets: AtomicU64,
    pub rx_concealed_packets: AtomicU64,
    pub output_ring_underruns: AtomicU64,
}

/// Spawns the audio bridge on a dedicated OS thread with its own
/// single-threaded Tokio runtime.
///
/// The cpal `Stream` handles created inside [`run`] are `!Send` (they wrap
/// platform audio-API objects) and live across `.await` points for the
/// duration of the bridge, so the bridge cannot be `tokio::spawn`ed onto the
/// shared multi-threaded runtime directly — it needs a runtime of its own.
pub fn spawn_thread(
    tx: ChannelTx,
    rx: ChannelRx,
    clock: SessionClock,
    stats: Arc<AudioBridgeStats>,
    buffer_frames: u32,
) -> std::thread::JoinHandle<Result<(), String>> {
    std::thread::Builder::new()
        .name("omniplex-audio-bridge".into())
        .spawn(move || {
            let rt = tokio::runtime::Builder::new_current_thread()
                .enable_all()
                .build()
                .map_err(|e| e.to_string())?;
            rt.block_on(run(tx, rx, clock, stats, buffer_frames))
                .map_err(|e| e.to_string())
        })
        .expect("failed to spawn omniplex-audio-bridge thread")
}

/// Runs the full duplex audio bridge until the channel closes.
///
/// `buffer_frames` sets the cpal input/output stream buffer size (frames per
/// hardware callback) explicitly via `cpal::BufferSize::Fixed`, rather than
/// letting the OS pick (which is often 512-1024 frames, i.e. 10.7-21.3 ms/side
/// at 48 kHz). Lower is lower-latency but raises underrun risk on weaker CPUs
/// (e.g. a Raspberry Pi).
pub async fn run(
    tx: ChannelTx,
    rx: ChannelRx,
    clock: SessionClock,
    stats: Arc<AudioBridgeStats>,
    buffer_frames: u32,
) -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    let host = cpal::default_host();
    let input = host
        .default_input_device()
        .ok_or("no default input device")?;
    let output = host
        .default_output_device()
        .ok_or("no default output device")?;
    let config = cpal::StreamConfig {
        channels: CHANNELS as u16,
        sample_rate: cpal::SampleRate(SAMPLE_RATE),
        buffer_size: cpal::BufferSize::Fixed(buffer_frames),
    };

    // ---- send side: input callback -> SPSC ring -> sender thread ----------
    let (mut in_prod, mut in_cons) = rtrb::RingBuffer::<f32>::new(RING_SAMPLES);
    // Parallel ring of (local_ns, n_samples) capture stamps, one per callback.
    let (mut ts_prod, mut ts_cons) = rtrb::RingBuffer::<(u64, u32)>::new(1024);
    let epoch = Instant::now();

    let in_stream = input.build_input_stream(
        &config,
        move |data: &[f32], _info: &cpal::InputCallbackInfo| {
            // No alloc/locks: copy what fits, drop the rest (overrun).
            let local_ns = epoch.elapsed().as_nanos() as u64;
            let n = data.len().min(in_prod.slots());
            for &s in &data[..n] {
                let _ = in_prod.push(s);
            }
            let _ = ts_prod.push((local_ns, n as u32));
        },
        |e| tracing::warn!("audio input stream error: {e}"),
        None,
    )?;

    // Sender thread: drain ring, stamp session time, pack PACKET_FRAMES-frame
    // packets (see audio_core::PACKET_FRAMES).
    let (pkt_tx, mut pkt_rx) = mpsc::channel::<(u64, Vec<f32>)>(64);
    let clock_send = clock.clone();
    std::thread::Builder::new()
        .name("omniplex-audio-send".into())
        .spawn(move || {
            let mut packer = PacketPacker::new(SAMPLE_RATE, CHANNELS);
            let mut scratch = vec![0.0f32; PACKET_FRAMES * CHANNELS * 4];
            loop {
                let Ok((local_ns, n)) = ts_cons.pop() else {
                    std::thread::sleep(std::time::Duration::from_millis(1));
                    continue;
                };
                let n = n as usize;
                for slot in scratch.iter_mut().take(n) {
                    *slot = in_cons.pop().unwrap_or(0.0);
                }
                // Convert the capture-time local stamp to session time using the
                // lock-free session clock read (Task 4.2): session_now - age.
                let Some(session_now) = clock_send.now_session_ns() else {
                    continue;
                };
                let age = epoch.elapsed().as_nanos() as u64 - local_ns;
                let ts = session_now.saturating_sub(age);
                for pkt in packer.push(ts, &scratch[..n]) {
                    if pkt_tx.blocking_send(pkt).is_err() {
                        return;
                    }
                }
            }
        })?;

    // Async forwarder: packets -> channel.
    let send_task = tokio::spawn(async move {
        while let Some((ts, samples)) = pkt_rx.recv().await {
            let payload = encode_payload(&samples);
            if tx
                .send(Bytes::from(payload), SessionTimestamp(ts))
                .await
                .is_err()
            {
                break;
            }
        }
    });

    // ---- receive side: channel -> jitter/ASRC core -> SPSC ring -> output --
    let (net_tx, net_rx) = std::sync::mpsc::sync_channel::<(u64, u64, Vec<f32>)>(256);
    let (mut out_prod, mut out_cons) = rtrb::RingBuffer::<f32>::new(RING_SAMPLES);

    // Receiver thread owns the pure core: pushes packets, pulls resampled
    // audio into the output ring whenever there is space.
    let clock_rx = clock.clone();
    let stats_rx = Arc::clone(&stats);
    std::thread::Builder::new()
        .name("omniplex-audio-recv".into())
        .spawn(move || {
            let mut core = JitterAsrc::new(JitterAsrcConfig {
                sample_rate: SAMPLE_RATE,
                channels: CHANNELS,
                ..Default::default()
            });
            let mut block = vec![0.0f32; PACKET_FRAMES * CHANNELS];
            loop {
                while let Ok((arrival, ts, samples)) = net_rx.try_recv() {
                    core.push_packet(arrival, ts, &samples);
                }
                let now = clock_rx.now_session_ns().unwrap_or(0);
                while out_prod.slots() >= block.len() {
                    core.pull(now, &mut block);
                    for &s in &block {
                        let _ = out_prod.push(s);
                    }
                }
                let s: RxAudioStats = core.stats();
                stats_rx
                    .rx_underrun_packets
                    .store(s.underrun_packets, Ordering::Relaxed);
                stats_rx
                    .rx_concealed_packets
                    .store(s.packets_concealed, Ordering::Relaxed);
                std::thread::sleep(std::time::Duration::from_millis(1));
            }
        })?;

    let stats_cb = Arc::clone(&stats);
    let out_stream = output.build_output_stream(
        &config,
        move |data: &mut [f32], _info: &cpal::OutputCallbackInfo| {
            let mut underrun = false;
            for s in data.iter_mut() {
                match out_cons.pop() {
                    Ok(v) => *s = v,
                    Err(_) => {
                        *s = 0.0;
                        underrun = true;
                    }
                }
            }
            if underrun {
                stats_cb
                    .output_ring_underruns
                    .fetch_add(1, Ordering::Relaxed);
            }
        },
        |e| tracing::warn!("audio output stream error: {e}"),
        None,
    )?;

    in_stream.play()?;
    out_stream.play()?;
    tracing::info!("audio bridge up: {SAMPLE_RATE} Hz, {CHANNELS} ch <-> audio/main");

    // Network receive loop (this task): channel -> receiver thread.
    let mut rx = rx;
    while let Some((bytes, ts)) = rx.recv().await {
        let arrival = clock.now_session_ns().unwrap_or(0);
        let samples = decode_payload(&bytes);
        if net_tx.try_send((arrival, ts.0, samples)).is_err() {
            tracing::warn!("audio receive queue full; packet dropped");
        }
    }
    send_task.abort();
    drop(in_stream);
    drop(out_stream);
    Ok(())
}
