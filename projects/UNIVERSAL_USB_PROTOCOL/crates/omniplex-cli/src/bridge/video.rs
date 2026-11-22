//! Video test channel logic (Task 7.4), pure: UYVY test-pattern generator
//! with a burned-in frame counter, counter decode, receive continuity
//! checking, and the app-side bandwidth degradation ladder. Session wiring
//! lives in `run.rs`.

/// Width in pixels of one burned-in counter bit block.
const BIT_BLOCK_W: u32 = 4;
/// Height in pixels of the counter strip.
const BIT_STRIP_H: u32 = 4;
/// 32 counter bits + 32 inverted bits for validation.
const N_BITS: u32 = 64;
/// Minimum frame width able to carry the counter strip.
pub const MIN_WIDTH: u32 = BIT_BLOCK_W * N_BITS; // 256

/// (Y, U, V) for the eight moving color bars.
const BARS: [(u8, u8, u8); 8] = [
    (235, 128, 128), // white
    (210, 16, 146),  // yellow
    (170, 166, 16),  // cyan
    (145, 54, 34),   // green
    (106, 202, 222), // magenta
    (81, 90, 240),   // red
    (41, 240, 110),  // blue
    (16, 128, 128),  // black
];

/// Geometry + rate of the generated stream.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct VideoMode {
    pub width: u32,
    pub height: u32,
    pub fps_num: u32,
    pub fps_den: u32,
}

impl VideoMode {
    pub fn frame_bytes(&self) -> usize {
        (self.width * self.height * 2) as usize // UYVY = 2 bytes/px
    }
    pub fn frame_interval_ns(&self) -> u64 {
        1_000_000_000u64 * self.fps_den as u64 / self.fps_num as u64
    }
    pub fn fps(&self) -> f64 {
        self.fps_num as f64 / self.fps_den as f64
    }
}

impl Default for VideoMode {
    fn default() -> Self {
        Self {
            width: 1280,
            height: 720,
            fps_num: 30,
            fps_den: 1,
        }
    }
}

/// Parse `WxH@fps`, e.g. `1920x1080@60` (fps may be fractional, e.g. 29.97).
pub fn parse_mode(s: &str) -> Option<VideoMode> {
    let (dims, fps) = s.split_once('@')?;
    let (w, h) = dims.split_once('x')?;
    let width: u32 = w.parse().ok()?;
    let height: u32 = h.parse().ok()?;
    let fps: f64 = fps.parse().ok()?;
    if width < MIN_WIDTH || height < BIT_STRIP_H || fps <= 0.0 {
        return None;
    }
    let (fps_num, fps_den) = if (fps.fract()).abs() < 1e-9 {
        (fps as u32, 1)
    } else {
        ((fps * 1000.0).round() as u32, 1000)
    };
    Some(VideoMode {
        width,
        height,
        fps_num,
        fps_den,
    })
}

/// Generates UYVY frames: moving color bars + a burned-in 64-bit counter
/// strip (32-bit counter + 32-bit complement) across the top rows.
pub struct TestPatternGen {
    pub mode: VideoMode,
    counter: u32,
}

impl TestPatternGen {
    pub fn new(mode: VideoMode) -> Self {
        assert!(mode.width >= MIN_WIDTH && mode.width.is_multiple_of(2));
        Self { mode, counter: 0 }
    }

    /// Produce the next frame; returns (counter, uyvy bytes).
    pub fn next_frame(&mut self) -> (u32, Vec<u8>) {
        let counter = self.counter;
        self.counter = self.counter.wrapping_add(1);
        (counter, render_frame(self.mode, counter))
    }
}

/// Render one UYVY frame for `counter`.
pub fn render_frame(mode: VideoMode, counter: u32) -> Vec<u8> {
    let w = mode.width as usize;
    let h = mode.height as usize;
    let mut buf = vec![0u8; w * h * 2];
    let bar_w = (w / BARS.len()).max(2);
    let shift = (counter as usize) * 4;
    let bits: u64 = (counter as u64) | ((!counter as u64) << 32);
    for y in 0..h {
        let row = &mut buf[y * w * 2..(y + 1) * w * 2];
        for xpair in 0..w / 2 {
            let x0 = xpair * 2;
            let (y0, u0, v0, y1) = if (y as u32) < BIT_STRIP_H {
                let luma = |x: usize| -> u8 {
                    let block = (x as u32) / BIT_BLOCK_W;
                    if block < N_BITS && (bits >> block) & 1 == 1 {
                        235
                    } else {
                        16
                    }
                };
                (luma(x0), 128, 128, luma(x0 + 1))
            } else {
                let bar = |x: usize| BARS[((x + shift) / bar_w) % BARS.len()];
                let (ya, u, v) = bar(x0);
                let (yb, _, _) = bar(x0 + 1);
                (ya, u, v, yb)
            };
            row[xpair * 4] = u0;
            row[xpair * 4 + 1] = y0;
            row[xpair * 4 + 2] = v0;
            row[xpair * 4 + 3] = y1;
        }
    }
    buf
}

/// Decode the burned-in counter from a UYVY frame; None if the frame is too
/// small or the complement check fails (corrupt frame).
pub fn decode_counter(frame: &[u8], width: u32) -> Option<u32> {
    if width < MIN_WIDTH || frame.len() < (width * BIT_STRIP_H * 2) as usize {
        return None;
    }
    let w = width as usize;
    let sample_row = (BIT_STRIP_H / 2) as usize;
    let mut bits: u64 = 0;
    for block in 0..N_BITS as usize {
        let x = block * BIT_BLOCK_W as usize + BIT_BLOCK_W as usize / 2;
        let luma = frame[sample_row * w * 2 + x * 2 + 1];
        if luma > 128 {
            bits |= 1u64 << block;
        }
    }
    let counter = (bits & 0xFFFF_FFFF) as u32;
    let inv = (bits >> 32) as u32;
    if counter == !inv {
        Some(counter)
    } else {
        None
    }
}

/// Receiver-side continuity checker over decoded counters.
#[derive(Debug, Default)]
pub struct ContinuityChecker {
    last: Option<u32>,
    pub frames_ok: u64,
    pub frames_corrupt: u64,
    pub gaps: u64,
    pub missing_frames: u64,
}

impl ContinuityChecker {
    pub fn feed(&mut self, decoded: Option<u32>) {
        let Some(c) = decoded else {
            self.frames_corrupt += 1;
            return;
        };
        if let Some(last) = self.last {
            let expected = last.wrapping_add(1);
            if c != expected {
                self.gaps += 1;
                self.missing_frames += c.wrapping_sub(expected) as u64;
            }
        }
        self.last = Some(c);
        self.frames_ok += 1;
    }
}

/// App-side degradation ladder (Task 5.4 contract): each BandwidthPressure
/// event halves fps first; once fps hit a quarter of the base rate, halve
/// resolution instead (down to the counter-strip minimum).
#[derive(Debug)]
pub struct DegradeLadder {
    base: VideoMode,
    pub current: VideoMode,
}

impl DegradeLadder {
    pub fn new(base: VideoMode) -> Self {
        Self {
            base,
            current: base,
        }
    }

    /// Apply one pressure event; returns the (possibly unchanged) new mode.
    pub fn on_pressure(&mut self) -> VideoMode {
        let cur = &mut self.current;
        if cur.fps() > self.base.fps() / 4.0 {
            cur.fps_den *= 2;
        } else if cur.width / 2 >= MIN_WIDTH && cur.height / 2 >= BIT_STRIP_H {
            cur.width = (cur.width / 2) & !1;
            cur.height = (cur.height / 2) & !1;
        }
        *cur
    }

    /// Pressure released: restore the base mode.
    pub fn reset(&mut self) -> VideoMode {
        self.current = self.base;
        self.current
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn counter_roundtrip_default_mode() {
        let mode = VideoMode::default();
        let mut gen = TestPatternGen::new(mode);
        for expect in 0..200u32 {
            let (c, frame) = gen.next_frame();
            assert_eq!(c, expect);
            assert_eq!(frame.len(), mode.frame_bytes());
            assert_eq!(decode_counter(&frame, mode.width), Some(expect));
        }
    }

    #[test]
    fn counter_roundtrip_min_width_and_big_counters() {
        let mode = VideoMode {
            width: MIN_WIDTH,
            height: 8,
            fps_num: 30,
            fps_den: 1,
        };
        for c in [0u32, 1, 255, 65_535, 0xDEAD_BEEF, u32::MAX] {
            let frame = render_frame(mode, c);
            assert_eq!(decode_counter(&frame, mode.width), Some(c));
        }
    }

    #[test]
    fn corrupt_frame_rejected() {
        let mode = VideoMode::default();
        let mut frame = render_frame(mode, 42);
        // Flip the exact luma byte `decode_counter` samples for bit block 0
        // (it reads row `BIT_STRIP_H / 2`, not row 0) so counter/complement
        // disagree.
        let w = mode.width as usize;
        let sample_row = (BIT_STRIP_H / 2) as usize;
        let idx = sample_row * w * 2 + 2 * 2 + 1;
        frame[idx] = if frame[idx] > 128 { 16 } else { 235 };
        assert_eq!(decode_counter(&frame, mode.width), None);
        assert_eq!(decode_counter(&[0u8; 16], mode.width), None);
    }

    #[test]
    fn continuity_checker_counts_gaps() {
        let mut cc = ContinuityChecker::default();
        for c in [0u32, 1, 2, /* 3, 4 dropped */ 5, 6 /* corrupt */] {
            cc.feed(Some(c));
        }
        cc.feed(None);
        assert_eq!(cc.frames_ok, 5);
        assert_eq!(cc.gaps, 1);
        assert_eq!(cc.missing_frames, 2);
        assert_eq!(cc.frames_corrupt, 1);
    }

    #[test]
    fn ladder_halves_fps_then_resolution() {
        let mut l = DegradeLadder::new(VideoMode {
            width: 1280,
            height: 720,
            fps_num: 30,
            fps_den: 1,
        });
        assert_eq!(l.on_pressure().fps(), 15.0);
        assert_eq!(l.on_pressure().fps(), 7.5); // base/4 reached
        let m = l.on_pressure();
        assert_eq!((m.width, m.height), (640, 360));
        assert_eq!(m.fps(), 7.5);
        let m = l.on_pressure();
        assert_eq!((m.width, m.height), (320, 180));
        let m = l.on_pressure();
        assert_eq!((m.width, m.height), (320, 180)); // floor: 160 < MIN_WIDTH
        assert_eq!(l.reset().fps(), 30.0);
    }

    #[test]
    fn parse_mode_variants() {
        assert_eq!(
            parse_mode("1920x1080@60"),
            Some(VideoMode {
                width: 1920,
                height: 1080,
                fps_num: 60,
                fps_den: 1
            })
        );
        let m = parse_mode("1280x720@29.97").unwrap();
        assert_eq!((m.fps_num, m.fps_den), (29_970, 1000));
        assert_eq!(parse_mode("bogus"), None);
        assert_eq!(parse_mode("100x100@30"), None); // below MIN_WIDTH
    }
}
