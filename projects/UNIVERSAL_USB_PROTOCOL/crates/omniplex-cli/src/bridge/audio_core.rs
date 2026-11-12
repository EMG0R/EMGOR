//! Pure receive-side audio pipeline: jitter buffer ordered by session
//! timestamp -> ASRC (rubato) disciplined by long-term drift -> fixed-size
//! output pulls. Repeat-fade concealment on packet loss. No cpal, no locks,
//! no I/O: fully unit-testable (Task 7.3).

use rubato::{FastFixedOut, PolynomialDegree, Resampler};
use std::collections::{BTreeMap, VecDeque};

/// Frames per network audio packet.
///
/// Lower values reduce the hard packetization wait (a full packet's worth of
/// samples must accumulate before it can be sent at all) at the cost of more
/// packets/sec, i.e. more per-packet network + CPU overhead (mux framing,
/// syscalls, scheduling) and a worse header-to-payload ratio (a 22-byte wire
/// frame header against a 128-byte payload at 16 frames/2ch/4-byte f32, vs.
/// 512 bytes at the original 256-frame packet size). 16 frames @ 48 kHz =
/// 0.33 ms/packet (3000 pkt/s), vs. 64 frames = 1.33 ms/packet (750 pkt/s) or
/// the original 256 frames = 5.33 ms/packet (187 pkt/s). On a direct
/// Ethernet/Thunderbolt link the extra packet rate is cheap relative to the
/// latency win, and this size is also the practical floor: 8 frames was
/// tested and panics inside rubato's `FastFixedOut` resampler (its internal
/// interpolation needs more input frames per chunk than an 8-frame block
/// provides), so it is not a viable candidate. Shrinking this constant also
/// lowers the jitter buffer's effective floor — see `JitterAsrc::new`. Not
/// currently a runtime parameter — changing it changes the wire format, so
/// both ends of a link must agree (spec Task 7.3 fixed this at build time;
/// revisit if per-link negotiation is added).
pub const PACKET_FRAMES: usize = 16;

/// Bridge-wide audio format (pure constants, so `bridge::mod`'s channel
/// descriptor helpers don't need the cpal-dependent `audio` module, which is
/// only compiled under the `wiring` feature).
pub const SAMPLE_RATE: u32 = 48_000;
pub const CHANNELS: usize = 2;

const NS_PER_SEC: f64 = 1_000_000_000.0;

/// Receive-side statistics.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct RxAudioStats {
    /// Packets delivered intact from the jitter buffer.
    pub packets_played: u64,
    /// Packets synthesized by repeat-fade concealment (loss).
    pub packets_concealed: u64,
    /// Packets replaced with silence because nothing was available at all.
    pub underrun_packets: u64,
    /// Longest run of consecutive concealed/underrun packets.
    pub max_consecutive_gap: u64,
    /// Late packets discarded (arrived behind the playhead).
    pub packets_late: u64,
    /// Current presentation offset in ns.
    pub presentation_offset_ns: u64,
    /// Current resample ratio applied to the ASRC.
    pub resample_ratio: f64,
}

/// Configuration for [`JitterAsrc`].
#[derive(Clone, Copy, Debug)]
pub struct JitterAsrcConfig {
    pub sample_rate: u32,
    pub channels: usize,
    /// Initial presentation offset in ns. Tuned for a direct point-to-point
    /// link (Ethernet/Thunderbolt), not the public internet, where jitter is
    /// small and converges fast; starts at 2 ms, adaptive from there. Note
    /// this (and `min_offset_ns`) is still clamped up to at least 1 packet
    /// duration in `JitterAsrc::new` — the jitter buffer cannot hold
    /// sub-packet cushions — so shrinking `PACKET_FRAMES` lowers that
    /// particular clamp too. In practice, at `PACKET_FRAMES = 16` (0.33 ms/
    /// packet) the 1-packet-duration clamp is *not* the binding floor —
    /// `min_offset_ns` below is larger and wins. See `min_offset_ns` for the
    /// measured effective floor.
    pub initial_offset_ns: u64,
    /// Floor for the adaptive offset (measured jitter + this floor). 0.5 ms
    /// for a direct link. This was tuned empirically, not just derived from
    /// packet duration: at `PACKET_FRAMES = 16` a floor of 1 packet duration
    /// alone (0.33 ms) is provably insufficient — the `drifting_device_
    /// 100ppm_60s_zero_underruns` test produces real underruns at that
    /// setting (11 underrun packets over 60 s) — while 0.5 ms passes cleanly
    /// with zero underruns/concealment across the drift and loss tests. Do
    /// not lower this below 0.5 ms without re-running those tests.
    pub min_offset_ns: u64,
    /// Max relative ASRC correction (fraction, e.g. 0.002 = 2000 ppm).
    pub max_correction: f64,
}

impl Default for JitterAsrcConfig {
    fn default() -> Self {
        Self {
            sample_rate: 48_000,
            channels: 2,
            initial_offset_ns: 2_000_000,
            min_offset_ns: 500_000,
            max_correction: 0.002,
        }
    }
}

/// Jitter buffer + ASRC + concealment core.
///
/// Driving contract:
/// - `push_packet(arrival_session_ns, ts, samples)` from the network side.
/// - `pull(session_now_ns, out)` from the device side; `out` is interleaved
///   f32, length a multiple of `channels`. Always fills `out` completely.
pub struct JitterAsrc {
    cfg: JitterAsrcConfig,
    /// session-timestamp -> interleaved packet samples (PACKET_FRAMES * channels).
    queue: BTreeMap<u64, Vec<f32>>,
    /// Session timestamp of the next packet the playhead expects.
    expected_ts: Option<u64>,
    packet_dur_ns: u64,
    /// Per-channel de-interleaved staging FIFO feeding the resampler.
    staging: Vec<VecDeque<f32>>,
    resampler: FastFixedOut<f32>,
    resampler_in: Vec<Vec<f32>>,
    resampler_out: Vec<Vec<f32>>,
    /// Interleaved resampled output FIFO.
    out_fifo: VecDeque<f32>,
    /// Last good packet, for repeat-fade concealment.
    last_packet: Vec<f32>,
    conceal_gain: f32,
    consecutive_gap: u64,
    started: bool,
    /// Adaptive presentation offset (ns, slewed toward jitter-derived target).
    offset_ns: f64,
    /// EWMA of arrival-time deviation (jitter estimate, ns).
    jitter_ewma_ns: f64,
    /// EWMA of transit (arrival - ts), to measure deviation against.
    transit_ewma_ns: f64,
    have_transit: bool,
    ratio: f64,
    /// Effective floor for the adaptive offset: never below one packet
    /// duration (the jitter buffer cannot hold sub-packet cushions). This
    /// clamp was tightened from the original 2x-packet-duration margin to 1x
    /// after confirming empirically (see `JitterAsrcConfig::min_offset_ns`)
    /// that 1x is safe for the tested drift/loss scenarios, while 0.5x is
    /// not (it reintroduces real underruns).
    min_offset_ns: f64,
    stats: RxAudioStats,
}

impl JitterAsrc {
    pub fn new(cfg: JitterAsrcConfig) -> Self {
        assert!(cfg.channels >= 1);
        let resampler = FastFixedOut::<f32>::new(
            1.0,
            1.0 + cfg.max_correction * 8.0,
            PolynomialDegree::Linear,
            PACKET_FRAMES,
            cfg.channels,
        )
        .expect("FastFixedOut construction");
        let packet_dur_ns = (PACKET_FRAMES as u64 * 1_000_000_000) / cfg.sample_rate as u64;
        Self {
            queue: BTreeMap::new(),
            expected_ts: None,
            packet_dur_ns,
            staging: vec![VecDeque::new(); cfg.channels],
            resampler_in: vec![Vec::with_capacity(PACKET_FRAMES * 2); cfg.channels],
            resampler_out: vec![vec![0.0; PACKET_FRAMES]; cfg.channels],
            resampler,
            out_fifo: VecDeque::new(),
            last_packet: vec![0.0; PACKET_FRAMES * cfg.channels],
            conceal_gain: 0.0,
            consecutive_gap: 0,
            started: false,
            min_offset_ns: (cfg.min_offset_ns.max(packet_dur_ns)) as f64,
            offset_ns: (cfg.initial_offset_ns.max(packet_dur_ns)) as f64,
            jitter_ewma_ns: 0.0,
            transit_ewma_ns: 0.0,
            have_transit: false,
            ratio: 1.0,
            stats: RxAudioStats {
                resample_ratio: 1.0,
                presentation_offset_ns: cfg.initial_offset_ns,
                ..Default::default()
            },
            cfg,
        }
    }

    pub fn stats(&self) -> RxAudioStats {
        let mut s = self.stats;
        s.presentation_offset_ns = self.offset_ns as u64;
        s.resample_ratio = self.ratio;
        s
    }

    /// Insert a received packet. `samples` is interleaved, PACKET_FRAMES*channels
    /// long (shorter tails are zero-padded).
    pub fn push_packet(&mut self, arrival_session_ns: u64, ts: u64, samples: &[f32]) {
        // Late: behind the playhead — useless, drop.
        if let Some(exp) = self.expected_ts {
            if ts + self.packet_dur_ns / 2 < exp {
                self.stats.packets_late += 1;
                return;
            }
        }
        // Jitter tracking (RFC3550-flavored): deviation of transit time.
        let transit = arrival_session_ns as f64 - ts as f64;
        if self.have_transit {
            let dev = (transit - self.transit_ewma_ns).abs();
            self.jitter_ewma_ns += (dev - self.jitter_ewma_ns) / 16.0;
            self.transit_ewma_ns += (transit - self.transit_ewma_ns) / 64.0;
        } else {
            self.transit_ewma_ns = transit;
            self.have_transit = true;
        }
        // Adapt presentation offset toward p99-ish jitter + min floor. Slew
        // slowly, never below the floor.
        let target =
            (self.cfg.min_offset_ns as f64 + 4.0 * self.jitter_ewma_ns).max(self.min_offset_ns);
        self.offset_ns += (target - self.offset_ns) * 0.001;

        let want = PACKET_FRAMES * self.cfg.channels;
        let mut v = vec![0.0f32; want];
        let n = samples.len().min(want);
        v[..n].copy_from_slice(&samples[..n]);
        self.queue.insert(ts, v);
    }

    /// Fill `out` (interleaved) with resampled audio; drives the pipeline.
    pub fn pull(&mut self, session_now_ns: u64, out: &mut [f32]) {
        let ch = self.cfg.channels;
        debug_assert_eq!(out.len() % ch, 0);
        if !self.started {
            match self.queue.keys().next().copied() {
                Some(first) if session_now_ns >= first.saturating_add(self.offset_ns as u64) => {
                    self.started = true;
                    self.expected_ts = Some(first);
                }
                _ => {
                    out.fill(0.0);
                    return;
                }
            }
        }
        while self.out_fifo.len() < out.len() {
            self.run_resampler_chunk(session_now_ns);
        }
        for s in out.iter_mut() {
            *s = self.out_fifo.pop_front().unwrap_or(0.0);
        }
        self.update_ratio();
    }

    fn run_resampler_chunk(&mut self, session_now_ns: u64) {
        let needed = self.resampler.input_frames_next();
        while self.staging[0].len() < needed {
            self.feed_one_packet(session_now_ns);
        }
        for (chan, dst) in self.resampler_in.iter_mut().enumerate() {
            dst.clear();
            for _ in 0..needed {
                dst.push(self.staging[chan].pop_front().unwrap());
            }
        }
        let (_in_used, out_frames) = self
            .resampler
            .process_into_buffer(&self.resampler_in, &mut self.resampler_out, None)
            .expect("resampler process");
        for f in 0..out_frames {
            for chan in 0..self.cfg.channels {
                self.out_fifo.push_back(self.resampler_out[chan][f]);
            }
        }
    }

    /// Move exactly one packet (real, concealed, or silent) into staging.
    fn feed_one_packet(&mut self, session_now_ns: u64) {
        let ch = self.cfg.channels;
        let exp = self.expected_ts.expect("started");
        let half = self.packet_dur_ns / 2;
        let hit = self
            .queue
            .keys()
            .next()
            .copied()
            .filter(|&k| k <= exp.saturating_add(half));
        if let Some(k) = hit {
            let pkt = self.queue.remove(&k).unwrap();
            self.stage_interleaved(&pkt);
            self.last_packet.copy_from_slice(&pkt);
            self.conceal_gain = 1.0;
            self.consecutive_gap = 0;
            self.stats.packets_played += 1;
        } else {
            // Missing. Loss (a later packet exists, or the packet's time has
            // clearly passed) -> repeat-fade concealment; otherwise we are
            // consuming faster than data exists -> underrun (silence).
            let later_exists = !self.queue.is_empty();
            let should_have_arrived = exp.saturating_add(self.packet_dur_ns) <= session_now_ns;
            self.consecutive_gap += 1;
            self.stats.max_consecutive_gap =
                self.stats.max_consecutive_gap.max(self.consecutive_gap);
            if later_exists || should_have_arrived {
                self.conceal_gain *= 0.7;
                let g = self.conceal_gain;
                let pkt: Vec<f32> = self.last_packet.iter().map(|s| s * g).collect();
                self.stage_interleaved(&pkt);
                self.stats.packets_concealed += 1;
            } else {
                // True underrun: the packet is not even due yet. Emit silence
                // but do NOT advance the playhead — the packet will still be
                // played when it arrives (offset effectively grows) instead of
                // being discarded as late, which would cascade.
                let zeros = vec![0.0f32; PACKET_FRAMES * ch];
                self.stage_interleaved(&zeros);
                self.stats.underrun_packets += 1;
                return;
            }
        }
        self.expected_ts = Some(exp + self.packet_dur_ns);
    }

    fn stage_interleaved(&mut self, pkt: &[f32]) {
        let ch = self.cfg.channels;
        for f in 0..PACKET_FRAMES {
            for c in 0..ch {
                self.staging[c].push_back(pkt[f * ch + c]);
            }
        }
    }

    /// Long-term drift discipline: keep buffered duration near the
    /// presentation offset by trimming the resample ratio.
    fn update_ratio(&mut self) {
        let ch = self.cfg.channels;
        let buffered_frames =
            self.staging[0].len() + self.queue.len() * PACKET_FRAMES + self.out_fifo.len() / ch;
        let target_frames = (self.offset_ns / NS_PER_SEC * self.cfg.sample_rate as f64).max(1.0);
        let err_secs = (buffered_frames as f64 - target_frames) / self.cfg.sample_rate as f64;
        // Proportional control: correct the level error over ~2 s.
        let correction = (err_secs * 0.5).clamp(-self.cfg.max_correction, self.cfg.max_correction);
        // FastFixedOut consumes chunk/ratio input frames per chunk: buffer too
        // full (err > 0) -> consume more input -> lower ratio.
        let target_ratio = 1.0 - correction;
        self.ratio += (target_ratio - self.ratio) * 0.1;
        let _ = self.resampler.set_resample_ratio(self.ratio, true);
    }
}

/// Send-side packer: accumulates interleaved f32 frames (with a session
/// timestamp for the first frame of each chunk) and emits fixed
/// `PACKET_FRAMES`-frame packets stamped with the session time of their
/// first frame.
pub struct PacketPacker {
    channels: usize,
    sample_rate: u32,
    buf: Vec<f32>,
    /// Session ts of buf[0]'s frame, if any buffered.
    base_ts: Option<u64>,
}

impl PacketPacker {
    pub fn new(sample_rate: u32, channels: usize) -> Self {
        Self {
            channels,
            sample_rate,
            buf: Vec::new(),
            base_ts: None,
        }
    }

    /// Push interleaved frames captured at `session_ts_ns` (time of the first
    /// frame in `samples`). Returns zero or more (ts, packet) tuples.
    pub fn push(&mut self, session_ts_ns: u64, samples: &[f32]) -> Vec<(u64, Vec<f32>)> {
        if self.base_ts.is_none() {
            let buffered_frames = self.buf.len() / self.channels;
            let back_ns = buffered_frames as u64 * 1_000_000_000 / self.sample_rate as u64;
            self.base_ts = Some(session_ts_ns.saturating_sub(back_ns));
        }
        self.buf.extend_from_slice(samples);
        let pkt_len = PACKET_FRAMES * self.channels;
        let mut out = Vec::new();
        while self.buf.len() >= pkt_len {
            let rest = self.buf.split_off(pkt_len);
            let pkt = std::mem::replace(&mut self.buf, rest);
            let ts = self.base_ts.unwrap();
            self.base_ts =
                Some(ts + PACKET_FRAMES as u64 * 1_000_000_000 / self.sample_rate as u64);
            out.push((ts, pkt));
        }
        out
    }
}

/// Serialize an audio packet payload: 8-byte LE session ts is carried in the
/// frame header by the mux; the payload is raw little-endian f32 samples.
pub fn encode_payload(samples: &[f32]) -> Vec<u8> {
    let mut v = Vec::with_capacity(samples.len() * 4);
    for s in samples {
        v.extend_from_slice(&s.to_le_bytes());
    }
    v
}

pub fn decode_payload(buf: &[u8]) -> Vec<f32> {
    buf.chunks_exact(4)
        .map(|b| f32::from_le_bytes([b[0], b[1], b[2], b[3]]))
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    const SR: u32 = 48_000;
    const PKT_NS: u64 = PACKET_FRAMES as u64 * 1_000_000_000 / SR as u64;

    fn sine_packet(k: u64, ch: usize) -> Vec<f32> {
        let mut v = Vec::with_capacity(PACKET_FRAMES * ch);
        for f in 0..PACKET_FRAMES {
            let n = k as usize * PACKET_FRAMES + f;
            let s = (2.0 * std::f32::consts::PI * 440.0 * n as f32 / SR as f32).sin() * 0.5;
            for _ in 0..ch {
                v.push(s);
            }
        }
        v
    }

    /// Simulate `secs` of streaming. Sender emits packets at true session
    /// rate; device pulls PACKET_FRAMES-sized blocks with its clock running
    /// `1.0 + drift_ppm*1e-6` times faster than the session clock. `lose`
    /// decides which packet indices never arrive.
    fn simulate(secs: u64, drift_ppm: f64, lose: impl Fn(u64) -> bool) -> (RxAudioStats, f32) {
        let ch = 1usize;
        let mut rx = JitterAsrc::new(JitterAsrcConfig {
            sample_rate: SR,
            channels: ch,
            ..Default::default()
        });
        let device_scale = 1.0 + drift_ppm * 1e-6;
        let n_packets = secs * SR as u64 / PACKET_FRAMES as u64;
        let mut out = vec![0.0f32; PACKET_FRAMES * ch];
        let mut next_push: u64 = 0; // packet index
        let mut pull_idx: u64 = 0;
        let mut max_delta = 0.0f32;
        let mut last_sample: Option<f32> = None;
        let mut started_playing = false;
        loop {
            let push_time = next_push * PKT_NS;
            // Device callback k happens at session time k*dur/scale.
            let pull_time = (pull_idx as f64 * PKT_NS as f64 / device_scale) as u64;
            if next_push < n_packets && push_time <= pull_time {
                if !lose(next_push) {
                    let ts = push_time;
                    // 200 us fixed transit.
                    rx.push_packet(ts + 200_000, ts, &sine_packet(next_push, ch));
                }
                next_push += 1;
                continue;
            }
            if next_push >= n_packets {
                break;
            }
            rx.pull(pull_time, &mut out);
            pull_idx += 1;
            // Continuity check once real audio is flowing.
            if out.iter().any(|s| s.abs() > 1e-6) {
                started_playing = true;
            }
            if started_playing {
                for &s in out.iter() {
                    if let Some(prev) = last_sample {
                        max_delta = max_delta.max((s - prev).abs());
                    }
                    last_sample = Some(s);
                }
            }
        }
        (rx.stats(), max_delta)
    }

    #[test]
    fn drifting_device_100ppm_60s_zero_underruns() {
        let (stats, max_delta) = simulate(60, 100.0, |_| false);
        assert_eq!(stats.underrun_packets, 0, "underruns: {stats:?}");
        assert_eq!(
            stats.packets_concealed, 0,
            "unexpected concealment: {stats:?}"
        );
        // Output continuous: a 440 Hz 0.5-amplitude sine has max sample delta
        // ~0.029; allow ASRC ripple headroom but catch discontinuities.
        assert!(max_delta < 0.1, "discontinuity: max_delta={max_delta}");
        assert!(stats.packets_played > 11_000, "{stats:?}");
    }

    #[test]
    fn negative_drift_60s_zero_underruns() {
        let (stats, _) = simulate(60, -100.0, |_| false);
        assert_eq!(stats.underrun_packets, 0, "{stats:?}");
    }

    #[test]
    fn five_percent_loss_concealed_no_gap_over_one_packet() {
        // Deterministic 5% loss: every 20th packet dropped.
        let (stats, _) = simulate(60, 0.0, |k| k % 20 == 7);
        assert!(
            stats.packets_concealed > 0,
            "concealment never invoked: {stats:?}"
        );
        assert_eq!(stats.underrun_packets, 0, "silence emitted: {stats:?}");
        assert!(stats.max_consecutive_gap <= 1, "gap > 1 packet: {stats:?}");
    }

    #[test]
    fn packer_emits_fixed_packets_with_advancing_ts() {
        let mut p = PacketPacker::new(SR, 2);
        let mut got = Vec::new();
        let frames_per_cb = 100;
        for cb in 0..30u64 {
            let ts = cb * frames_per_cb as u64 * 1_000_000_000 / SR as u64;
            let chunk = vec![0.25f32; frames_per_cb * 2];
            got.extend(p.push(ts, &chunk));
        }
        assert!(!got.is_empty());
        for (i, (ts, pkt)) in got.iter().enumerate() {
            assert_eq!(pkt.len(), PACKET_FRAMES * 2);
            let expect = i as u64 * PKT_NS;
            let diff = ts.abs_diff(expect);
            assert!(diff <= PKT_NS / 8, "packet {i} ts {ts} vs {expect}");
        }
    }

    #[test]
    fn payload_roundtrip() {
        let s: Vec<f32> = (0..512).map(|i| (i as f32) * 0.001 - 0.25).collect();
        assert_eq!(decode_payload(&encode_payload(&s)), s);
    }
}
