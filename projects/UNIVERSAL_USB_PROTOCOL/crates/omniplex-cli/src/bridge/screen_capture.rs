//! Real screen-capture video source (Linux-only): full-screen, region, and
//! single-window capture from the Pi's own display, feeding the same
//! `VideoMode`/`DegradeLadder` plumbing that `bridge::video`'s synthetic
//! `TestPatternGen` uses (see `run.rs`'s `spawn_screen_capture_sender`).
//!
//! Everything in this file except the `platform` submodules is pure and
//! platform-independent: CLI-spec parsing, the fps/resolution math, and the
//! crop/resize helpers used to turn a natively-captured frame into whatever
//! the (possibly bandwidth-degraded) target `VideoMode` currently asks for.
//! All of that is exercised by `cargo test -p omniplex-cli` on any host.
//!
//! The actual capture backend (`platform::ScreenCapturer`) is real on Linux
//! (via the `scrap` crate for full-screen/region, and `x11rb` + the
//! XComposite extension for single-window capture) and an
//! `UnsupportedPlatform`-erroring stub everywhere else, so the crate keeps
//! building and testing on macOS. **None of the Linux capture path has been
//! exercised on real X11/hardware** — see the module doc on
//! `platform::linux` for exactly what could and could not be verified from
//! this (macOS) machine.

use super::video::VideoMode;

/// Default capture frame rate when a `--screen-capture` spec doesn't
/// override it. Kept separate from `video::VideoMode::default()`'s fps so
/// screen-capture and the test pattern can diverge independently.
pub const DEFAULT_FPS: f64 = 30.0;

/// What to capture.
#[derive(Clone, Debug, PartialEq)]
pub enum CaptureTarget {
    /// The whole primary display, optionally downscaled to `resize` before
    /// being sent (capture always happens at native resolution; `resize`
    /// only controls the *output* size, i.e. the bandwidth knob).
    FullScreen { resize: Option<(u32, u32)> },
    /// An arbitrary `x,y,width,height` rectangle of the primary display.
    Region {
        x: i32,
        y: i32,
        width: u32,
        height: u32,
    },
    /// The first viewable window whose title contains `name_substring`,
    /// captured via XComposite so it's correct regardless of
    /// occlusion/stacking/movement.
    Window { name_substring: String },
}

/// A fully parsed `--screen-capture` CLI value.
#[derive(Clone, Debug, PartialEq)]
pub struct ScreenCaptureSpec {
    pub target: CaptureTarget,
    pub fps: f64,
}

impl ScreenCaptureSpec {
    /// `fps` as a `(fps_num, fps_den)` pair using the same fractional-fps
    /// encoding as `video::parse_mode` (integral fps -> den 1; otherwise
    /// millihertz precision, e.g. 29.97 -> 29970/1000).
    pub fn fps_num_den(&self) -> (u32, u32) {
        if self.fps.fract().abs() < 1e-9 {
            (self.fps as u32, 1)
        } else {
            ((self.fps * 1000.0).round() as u32, 1000)
        }
    }
}

/// Parse `--screen-capture`'s value:
/// - `full[@WxH][@fps]` — e.g. `full`, `full@30`, `full@1920x1080`,
///   `full@1920x1080@30` (the two optional `@`-segments may appear in
///   either order; whichever contains an `x` is the resize, the other is
///   fps).
/// - `region:X,Y,W,H[@fps]` — e.g. `region:0,0,1280,720@15`.
/// - `window:NAME[@fps]` — e.g. `window:Firefox@20`; `NAME` is matched as a
///   substring against each viewable window's title.
pub fn parse_screen_capture(s: &str) -> Result<ScreenCaptureSpec, String> {
    if let Some(rest) = s.strip_prefix("full") {
        let (resize, fps) = parse_full_suffix(rest)?;
        return Ok(ScreenCaptureSpec {
            target: CaptureTarget::FullScreen { resize },
            fps,
        });
    }
    if let Some(rest) = s.strip_prefix("region:") {
        let (spec_part, fps) = split_trailing_fps(rest);
        let (x, y, width, height) = parse_region(spec_part)?;
        return Ok(ScreenCaptureSpec {
            target: CaptureTarget::Region {
                x,
                y,
                width,
                height,
            },
            fps: fps.unwrap_or(DEFAULT_FPS),
        });
    }
    if let Some(rest) = s.strip_prefix("window:") {
        let (name, fps) = split_trailing_fps(rest);
        if name.is_empty() {
            return Err(format!(
                "--screen-capture {s:?}: window: needs a name substring"
            ));
        }
        return Ok(ScreenCaptureSpec {
            target: CaptureTarget::Window {
                name_substring: name.to_string(),
            },
            fps: fps.unwrap_or(DEFAULT_FPS),
        });
    }
    Err(format!(
        "invalid --screen-capture value {s:?} (want full[@WxH][@fps], \
         region:X,Y,W,H[@fps], or window:NAME[@fps])"
    ))
}

/// Parse the `[@WxH][@fps]` suffix after `full`, in either order.
fn parse_full_suffix(rest: &str) -> Result<(Option<(u32, u32)>, f64), String> {
    let mut resize = None;
    let mut fps = DEFAULT_FPS;
    if rest.is_empty() {
        return Ok((resize, fps));
    }
    let rest = rest
        .strip_prefix('@')
        .ok_or_else(|| format!("expected @WxH or @fps after 'full', got {rest:?}"))?;
    for seg in rest.split('@') {
        if seg.contains('x') {
            let (w, h) = seg
                .split_once('x')
                .ok_or_else(|| format!("bad WxH segment {seg:?}"))?;
            let w: u32 = w.parse().map_err(|_| format!("bad width in {seg:?}"))?;
            let h: u32 = h.parse().map_err(|_| format!("bad height in {seg:?}"))?;
            if w == 0 || h == 0 {
                return Err("full: resize width/height must be > 0".into());
            }
            resize = Some((w, h));
        } else {
            fps = seg
                .parse::<f64>()
                .map_err(|_| format!("bad fps in {seg:?}"))?;
            if fps <= 0.0 {
                return Err("fps must be > 0".into());
            }
        }
    }
    Ok((resize, fps))
}

/// Split off a trailing `@fps` suffix if the text after the last `@` parses
/// as a positive number; otherwise the whole string is returned unchanged
/// (so a `window:` name that happens to contain `@` isn't mangled).
fn split_trailing_fps(s: &str) -> (&str, Option<f64>) {
    if let Some(idx) = s.rfind('@') {
        let (head, tail) = (&s[..idx], &s[idx + 1..]);
        if let Ok(fps) = tail.parse::<f64>() {
            if fps > 0.0 {
                return (head, Some(fps));
            }
        }
    }
    (s, None)
}

fn parse_region(s: &str) -> Result<(i32, i32, u32, u32), String> {
    let parts: Vec<&str> = s.split(',').collect();
    let [x, y, w, h] = parts.as_slice() else {
        return Err(format!("region needs X,Y,W,H, got {s:?}"));
    };
    let x: i32 = x.trim().parse().map_err(|_| format!("bad x in {s:?}"))?;
    let y: i32 = y.trim().parse().map_err(|_| format!("bad y in {s:?}"))?;
    let w: u32 = w
        .trim()
        .parse()
        .map_err(|_| format!("bad width in {s:?}"))?;
    let h: u32 = h
        .trim()
        .parse()
        .map_err(|_| format!("bad height in {s:?}"))?;
    if w == 0 || h == 0 {
        return Err("region width/height must be > 0".into());
    }
    Ok((x, y, w, h))
}

/// Bytes in one raw BGRA frame (4 bytes/pixel — what X11 `GetImage`/`scrap`
/// hand back, and what `PixelFormat::Bgra` on the wire expects).
pub fn bgra_frame_bytes(width: u32, height: u32) -> usize {
    width as usize * height as usize * 4
}

/// Compute the initial `VideoMode` for `target` given the actually-measured
/// native dimensions (display size for `FullScreen`, window size for
/// `Window`; ignored for `Region`, which is fully self-specified). Pure and
/// testable without an actual display connection — the real capturer calls
/// this with dimensions it measured; tests call it with synthetic ones.
pub fn resolve_mode(
    target: &CaptureTarget,
    native: (u32, u32),
    fps_num: u32,
    fps_den: u32,
) -> VideoMode {
    let (width, height) = match target {
        CaptureTarget::FullScreen { resize } => resize.unwrap_or(native),
        CaptureTarget::Region { width, height, .. } => (*width, *height),
        CaptureTarget::Window { .. } => native,
    };
    VideoMode {
        width,
        height,
        fps_num,
        fps_den,
    }
}

/// Crop a `w x h` rectangle at `(x, y)` out of a `src_w x src_h` BGRA
/// buffer. Out-of-bounds rows/columns (including fully negative `x`/`y`)
/// are zero-filled rather than panicking, since a region can be specified
/// past the edge of the display.
pub fn crop_bgra(src: &[u8], src_w: u32, src_h: u32, x: i32, y: i32, w: u32, h: u32) -> Vec<u8> {
    let mut out = vec![0u8; w as usize * h as usize * 4];
    let row_in_bounds = x >= 0 && (x as i64 + w as i64) <= src_w as i64;
    for row in 0..h {
        let sy = y + row as i32;
        if sy < 0 || sy as u32 >= src_h {
            continue;
        }
        let dst_row_start = row as usize * w as usize * 4;
        if row_in_bounds {
            let src_row_start = (sy as usize * src_w as usize + x as usize) * 4;
            let len = w as usize * 4;
            if src_row_start + len <= src.len() {
                out[dst_row_start..dst_row_start + len]
                    .copy_from_slice(&src[src_row_start..src_row_start + len]);
            }
            continue;
        }
        for col in 0..w {
            let sx = x + col as i32;
            if sx < 0 || sx as u32 >= src_w {
                continue;
            }
            let src_idx = (sy as usize * src_w as usize + sx as usize) * 4;
            let dst_idx = dst_row_start + col as usize * 4;
            if src_idx + 4 <= src.len() {
                out[dst_idx..dst_idx + 4].copy_from_slice(&src[src_idx..src_idx + 4]);
            }
        }
    }
    out
}

/// Nearest-neighbor resize of a BGRA buffer. Used to turn a
/// natively-captured frame into whatever (possibly `DegradeLadder`-shrunk)
/// output resolution is currently targeted. A no-op copy when the sizes
/// already match.
pub fn resize_nearest_bgra(src: &[u8], src_w: u32, src_h: u32, dst_w: u32, dst_h: u32) -> Vec<u8> {
    if src_w == dst_w && src_h == dst_h {
        return src.to_vec();
    }
    let mut out = vec![0u8; dst_w as usize * dst_h as usize * 4];
    if src_w == 0 || src_h == 0 || dst_w == 0 || dst_h == 0 {
        return out;
    }
    for dy in 0..dst_h {
        let sy = ((dy as u64 * src_h as u64) / dst_h as u64).min(src_h as u64 - 1) as u32;
        for dx in 0..dst_w {
            let sx = ((dx as u64 * src_w as u64) / dst_w as u64).min(src_w as u64 - 1) as u32;
            let src_idx = (sy as usize * src_w as usize + sx as usize) * 4;
            let dst_idx = (dy as usize * dst_w as usize + dx as usize) * 4;
            if src_idx + 4 <= src.len() {
                out[dst_idx..dst_idx + 4].copy_from_slice(&src[src_idx..src_idx + 4]);
            }
        }
    }
    out
}

/// Capture-path errors. Small and `Send` so they can cross the
/// `std::sync::mpsc` channel `run.rs` uses to report the capture thread's
/// startup result back to the async side.
#[derive(Debug)]
pub enum CaptureError {
    /// Not Linux — screen capture only makes sense on the Pi capturing its
    /// own display.
    UnsupportedPlatform,
    Io(String),
    X11(String),
    WindowNotFound(String),
}

impl std::fmt::Display for CaptureError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            CaptureError::UnsupportedPlatform => write!(
                f,
                "screen capture is only implemented on Linux (this bridges a Raspberry Pi's own display)"
            ),
            CaptureError::Io(s) => write!(f, "screen capture I/O error: {s}"),
            CaptureError::X11(s) => write!(f, "X11 error: {s}"),
            CaptureError::WindowNotFound(s) => write!(f, "no viewable window matching {s:?} found"),
        }
    }
}

impl std::error::Error for CaptureError {}

/// Drive one dedicated OS thread's capture loop: pull a frame, resize it to
/// whatever the current target mode asks for, hand it to `frame_tx`, sleep
/// until the next tick, and repeat — picking up new target modes
/// (`DegradeLadder`-driven fps/resolution changes) from `mode_rx`
/// non-blockingly each iteration. A failed `capture_frame` is logged and
/// retried next tick (so one transient X11 hiccup doesn't tear down the
/// whole bridge); `stop` is the only thing that can end the loop when
/// capture is failing on every call, since a send-failure check alone
/// would never fire if no frame is ever produced to send. `run.rs` sets
/// `stop` once the async side gives up on `frame_tx`'s receiver (its own
/// send also short-circuits the loop as a fast path when capture is
/// healthy).
///
/// Lives outside `platform` because it only calls `ScreenCapturer`'s public
/// methods — it's exercised by `capture_loop_stops_promptly_when_flagged`
/// below and, on Linux, is what `run.rs` actually spawns onto a dedicated
/// thread.
pub fn run_capture_loop(
    mut capturer: platform::ScreenCapturer,
    initial_mode: VideoMode,
    mode_rx: std::sync::mpsc::Receiver<VideoMode>,
    frame_tx: std::sync::mpsc::SyncSender<Vec<u8>>,
    stop: std::sync::Arc<std::sync::atomic::AtomicBool>,
) {
    let mut current = initial_mode;
    let mut next_due = std::time::Instant::now();
    while !stop.load(std::sync::atomic::Ordering::Relaxed) {
        while let Ok(m) = mode_rx.try_recv() {
            current = m;
        }
        match capturer.capture_frame(current) {
            Ok(frame) => {
                if frame_tx.send(frame).is_err() {
                    return;
                }
            }
            Err(e) => {
                tracing::error!("screen capture failed: {e}");
            }
        }
        next_due += std::time::Duration::from_nanos(current.frame_interval_ns().max(1));
        let now = std::time::Instant::now();
        if next_due > now {
            std::thread::sleep(next_due - now);
        } else {
            next_due = now;
        }
    }
}

// ---------------------------------------------------------------------------
// Platform backend: real on Linux, an erroring stub everywhere else so the
// crate keeps building/testing on macOS.

#[cfg(target_os = "linux")]
pub mod platform {
    //! Real Linux/X11 capture backend.
    //!
    //! Full-screen and region capture go through the `scrap` crate (XCB +
    //! MIT-SHM `GetImage`, native BGRA/BGRX 32bpp, no row padding assumed —
    //! `scrap`'s X11 backend always requests a tightly-packed
    //! `pixel_width * width * height` shm segment). Region capture is a
    //! post-capture crop of the full-display frame (`scrap` has no partial
    //! grab), which is fine since it's still a real, live capture.
    //!
    //! Window capture uses the real XComposite extension via `x11rb`:
    //! `RedirectWindow` + `NameWindowPixmap` redirect the window's
    //! rendering into an off-screen pixmap that's read back independent of
    //! occlusion/stacking/movement — not a naive crop of a full-screen grab
    //! against a stale bounding box. The one corner cut for effort: pixel
    //! readback uses plain X11 `GetImage` over the wire rather than
    //! MIT-SHM, so it is *correct* at any window size/position but not as
    //! fast as the SHM path used for full/region capture; wiring window
    //! capture through SHM too is a reasonable fast-follow if a Pi's link
    //! to its own X server turns out to be the bottleneck.
    //!
    //! **Verification status**: this module type-checks under
    //! `cargo check --target x86_64-unknown-linux-gnu` (confirmed during
    //! development) but has **not** run against a real X server — this
    //! development machine is macOS. Treat the window-enumeration,
    //! XComposite redirection, and SHM-frame-format assumptions as
    //! reviewed-but-unexercised until validated on an actual Pi.

    use super::{
        crop_bgra, resize_nearest_bgra, resolve_mode, CaptureError, CaptureTarget,
        ScreenCaptureSpec,
    };
    use crate::bridge::video::VideoMode;
    use x11rb::connection::Connection;
    use x11rb::protocol::composite::{self, ConnectionExt as _, Redirect};
    use x11rb::protocol::xproto::{self, AtomEnum, ConnectionExt as _, ImageFormat, Window};
    use x11rb::rust_connection::RustConnection;

    impl From<x11rb::errors::ConnectError> for CaptureError {
        fn from(e: x11rb::errors::ConnectError) -> Self {
            CaptureError::X11(e.to_string())
        }
    }
    impl From<x11rb::errors::ConnectionError> for CaptureError {
        fn from(e: x11rb::errors::ConnectionError) -> Self {
            CaptureError::X11(e.to_string())
        }
    }
    impl From<x11rb::errors::ReplyError> for CaptureError {
        fn from(e: x11rb::errors::ReplyError) -> Self {
            CaptureError::X11(e.to_string())
        }
    }
    impl From<x11rb::errors::ReplyOrIdError> for CaptureError {
        fn from(e: x11rb::errors::ReplyOrIdError) -> Self {
            CaptureError::X11(e.to_string())
        }
    }

    /// Real screen capturer: either the `scrap`-backed full/region path, or
    /// the `x11rb`+XComposite window path. Deliberately *not* `Send` (it
    /// owns raw XCB/shm state) — `run.rs` constructs and drives it entirely
    /// within one dedicated OS thread rather than moving it across an
    /// `.await` point.
    pub struct ScreenCapturer {
        native: VideoMode,
        backend: Backend,
    }

    enum Backend {
        Display {
            capturer: scrap::Capturer,
            native_w: u32,
            native_h: u32,
            /// `Some((x, y, w, h))` for `Region`; `None` for `FullScreen`.
            region: Option<(i32, i32, u32, u32)>,
        },
        Window {
            conn: RustConnection,
            window: Window,
        },
    }

    impl ScreenCapturer {
        pub fn new(spec: &ScreenCaptureSpec) -> Result<Self, CaptureError> {
            let (fps_num, fps_den) = spec.fps_num_den();
            match &spec.target {
                CaptureTarget::FullScreen { .. } => {
                    let display =
                        scrap::Display::primary().map_err(|e| CaptureError::Io(e.to_string()))?;
                    let native_w = display.width() as u32;
                    let native_h = display.height() as u32;
                    let capturer = scrap::Capturer::new(display)
                        .map_err(|e| CaptureError::Io(e.to_string()))?;
                    let native = resolve_mode(&spec.target, (native_w, native_h), fps_num, fps_den);
                    Ok(Self {
                        native,
                        backend: Backend::Display {
                            capturer,
                            native_w,
                            native_h,
                            region: None,
                        },
                    })
                }
                CaptureTarget::Region {
                    x,
                    y,
                    width,
                    height,
                } => {
                    let display =
                        scrap::Display::primary().map_err(|e| CaptureError::Io(e.to_string()))?;
                    let native_w = display.width() as u32;
                    let native_h = display.height() as u32;
                    let capturer = scrap::Capturer::new(display)
                        .map_err(|e| CaptureError::Io(e.to_string()))?;
                    let native = resolve_mode(&spec.target, (native_w, native_h), fps_num, fps_den);
                    Ok(Self {
                        native,
                        backend: Backend::Display {
                            capturer,
                            native_w,
                            native_h,
                            region: Some((*x, *y, *width, *height)),
                        },
                    })
                }
                CaptureTarget::Window { name_substring } => {
                    let (conn, screen_num) =
                        x11rb::connect(None).map_err(|e| CaptureError::X11(e.to_string()))?;
                    let root = conn.setup().roots[screen_num].root;
                    let window = find_window_by_substring(&conn, root, name_substring)?;
                    let (w, h) = redirect_window(&conn, window)?;
                    let native = resolve_mode(&spec.target, (w as u32, h as u32), fps_num, fps_den);
                    Ok(Self {
                        native,
                        backend: Backend::Window { conn, window },
                    })
                }
            }
        }

        pub fn native_mode(&self) -> VideoMode {
            self.native
        }

        /// Capture one frame and resize it to `target`'s resolution if the
        /// `DegradeLadder` has shrunk it below the native capture size.
        pub fn capture_frame(&mut self, target: VideoMode) -> Result<Vec<u8>, CaptureError> {
            let (raw, w, h) = match &mut self.backend {
                Backend::Display {
                    capturer,
                    native_w,
                    native_h,
                    region,
                } => {
                    let frame = capturer
                        .frame()
                        .map_err(|e| CaptureError::Io(e.to_string()))?;
                    let bytes: &[u8] = &frame;
                    match region {
                        Some((x, y, rw, rh)) => (
                            crop_bgra(bytes, *native_w, *native_h, *x, *y, *rw, *rh),
                            *rw,
                            *rh,
                        ),
                        None => (bytes.to_vec(), *native_w, *native_h),
                    }
                }
                Backend::Window { conn, window } => {
                    let (data, w, h) = capture_window_frame(conn, *window)?;
                    (data, w as u32, h as u32)
                }
            };
            if target.width == w && target.height == h {
                Ok(raw)
            } else {
                Ok(resize_nearest_bgra(&raw, w, h, target.width, target.height))
            }
        }
    }

    /// Fetch a window's title: `_NET_WM_NAME` (UTF-8) first, falling back
    /// to the ICCCM `WM_NAME` (Latin-1).
    fn window_title(conn: &RustConnection, win: Window) -> Option<String> {
        let net_wm_name = conn
            .intern_atom(false, b"_NET_WM_NAME")
            .ok()?
            .reply()
            .ok()?
            .atom;
        let utf8_string = conn
            .intern_atom(false, b"UTF8_STRING")
            .ok()?
            .reply()
            .ok()?
            .atom;
        if let Ok(cookie) = conn.get_property(false, win, net_wm_name, utf8_string, 0, u32::MAX) {
            if let Ok(reply) = cookie.reply() {
                if !reply.value.is_empty() {
                    if let Ok(s) = String::from_utf8(reply.value) {
                        return Some(s);
                    }
                }
            }
        }
        let reply = conn
            .get_property(false, win, AtomEnum::WM_NAME, AtomEnum::STRING, 0, u32::MAX)
            .ok()?
            .reply()
            .ok()?;
        if reply.value.is_empty() {
            None
        } else {
            Some(reply.value.iter().map(|&b| b as char).collect())
        }
    }

    /// Depth-first walk of the window tree collecting every viewable
    /// window's `(id, title)`.
    fn enumerate_windows(
        conn: &RustConnection,
        root: Window,
    ) -> Result<Vec<(Window, String)>, CaptureError> {
        let mut out = Vec::new();
        let mut stack = vec![root];
        while let Some(win) = stack.pop() {
            let tree = conn.query_tree(win)?.reply()?;
            for child in tree.children {
                if let Ok(cookie) = conn.get_window_attributes(child) {
                    if let Ok(attrs) = cookie.reply() {
                        if attrs.map_state == xproto::MapState::VIEWABLE {
                            if let Some(title) = window_title(conn, child) {
                                out.push((child, title));
                            }
                        }
                    }
                }
                stack.push(child);
            }
        }
        Ok(out)
    }

    fn find_window_by_substring(
        conn: &RustConnection,
        root: Window,
        needle: &str,
    ) -> Result<Window, CaptureError> {
        enumerate_windows(conn, root)?
            .into_iter()
            .find(|(_, title)| title.contains(needle))
            .map(|(w, _)| w)
            .ok_or_else(|| CaptureError::WindowNotFound(needle.to_string()))
    }

    /// Redirect `window`'s rendering to off-screen storage via XComposite
    /// and return its current geometry.
    fn redirect_window(conn: &RustConnection, window: Window) -> Result<(u16, u16), CaptureError> {
        composite::query_version(conn, 0, 4)?.reply()?;
        conn.composite_redirect_window(window, Redirect::AUTOMATIC)?
            .check()?;
        let geom = conn.get_geometry(window)?.reply()?;
        Ok((geom.width, geom.height))
    }

    /// Grab the current contents of a composite-redirected window through
    /// its "named window pixmap" — correct regardless of what's stacked
    /// above it or whether it has moved, since the pixmap is off-screen
    /// storage XComposite keeps in sync with the window's own rendering.
    fn capture_window_frame(
        conn: &RustConnection,
        window: Window,
    ) -> Result<(Vec<u8>, u16, u16), CaptureError> {
        let pixmap = conn.generate_id()?;
        conn.composite_name_window_pixmap(window, pixmap)?.check()?;
        let geom = conn.get_geometry(pixmap)?.reply()?;
        let image = conn
            .get_image(
                ImageFormat::Z_PIXMAP,
                pixmap,
                0,
                0,
                geom.width,
                geom.height,
                !0,
            )?
            .reply()?;
        conn.free_pixmap(pixmap)?;
        Ok((image.data, geom.width, geom.height))
    }
}

#[cfg(not(target_os = "linux"))]
pub mod platform {
    //! Non-Linux stub: screen capture bridges a Pi's own display, so it
    //! only makes sense on Linux. Every method fails fast with
    //! `CaptureError::UnsupportedPlatform`.

    use super::{CaptureError, ScreenCaptureSpec, VideoMode};

    #[derive(Debug)]
    pub struct ScreenCapturer;

    impl ScreenCapturer {
        pub fn new(_spec: &ScreenCaptureSpec) -> Result<Self, CaptureError> {
            Err(CaptureError::UnsupportedPlatform)
        }

        pub fn native_mode(&self) -> VideoMode {
            VideoMode::default()
        }

        pub fn capture_frame(&mut self, _target: VideoMode) -> Result<Vec<u8>, CaptureError> {
            Err(CaptureError::UnsupportedPlatform)
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parse_full_bare() {
        let spec = parse_screen_capture("full").unwrap();
        assert_eq!(
            spec,
            ScreenCaptureSpec {
                target: CaptureTarget::FullScreen { resize: None },
                fps: DEFAULT_FPS,
            }
        );
    }

    #[test]
    fn parse_full_with_fps_only() {
        let spec = parse_screen_capture("full@15").unwrap();
        assert_eq!(spec.target, CaptureTarget::FullScreen { resize: None });
        assert_eq!(spec.fps, 15.0);
    }

    #[test]
    fn parse_full_with_resize_only() {
        let spec = parse_screen_capture("full@1920x1080").unwrap();
        assert_eq!(
            spec.target,
            CaptureTarget::FullScreen {
                resize: Some((1920, 1080))
            }
        );
        assert_eq!(spec.fps, DEFAULT_FPS);
    }

    #[test]
    fn parse_full_with_resize_and_fps_either_order() {
        let a = parse_screen_capture("full@1920x1080@30").unwrap();
        let b = parse_screen_capture("full@30@1920x1080").unwrap();
        assert_eq!(a, b);
        assert_eq!(
            a.target,
            CaptureTarget::FullScreen {
                resize: Some((1920, 1080))
            }
        );
        assert_eq!(a.fps, 30.0);
    }

    #[test]
    fn parse_region_basic() {
        let spec = parse_screen_capture("region:10,20,640,480").unwrap();
        assert_eq!(
            spec.target,
            CaptureTarget::Region {
                x: 10,
                y: 20,
                width: 640,
                height: 480
            }
        );
        assert_eq!(spec.fps, DEFAULT_FPS);
    }

    #[test]
    fn parse_region_with_fps() {
        let spec = parse_screen_capture("region:0,0,320,240@10").unwrap();
        assert_eq!(spec.fps, 10.0);
        assert_eq!(
            spec.target,
            CaptureTarget::Region {
                x: 0,
                y: 0,
                width: 320,
                height: 240
            }
        );
    }

    #[test]
    fn parse_region_negative_origin() {
        // A multi-monitor setup can put a display's origin left/above (0,0).
        let spec = parse_screen_capture("region:-100,-50,640,480").unwrap();
        assert_eq!(
            spec.target,
            CaptureTarget::Region {
                x: -100,
                y: -50,
                width: 640,
                height: 480
            }
        );
    }

    #[test]
    fn parse_region_rejects_bad_shape() {
        assert!(parse_screen_capture("region:10,20,640").is_err());
        assert!(parse_screen_capture("region:10,20,0,480").is_err());
        assert!(parse_screen_capture("region:a,b,c,d").is_err());
    }

    #[test]
    fn parse_window_basic() {
        let spec = parse_screen_capture("window:Firefox").unwrap();
        assert_eq!(
            spec.target,
            CaptureTarget::Window {
                name_substring: "Firefox".into()
            }
        );
        assert_eq!(spec.fps, DEFAULT_FPS);
    }

    #[test]
    fn parse_window_with_fps() {
        let spec = parse_screen_capture("window:Firefox@20").unwrap();
        assert_eq!(
            spec.target,
            CaptureTarget::Window {
                name_substring: "Firefox".into()
            }
        );
        assert_eq!(spec.fps, 20.0);
    }

    #[test]
    fn parse_window_name_with_literal_at_is_preserved() {
        // "user@host - term" is a plausible window title; the trailing
        // text after the last '@' isn't a valid fps, so the whole string
        // is the name substring, not silently truncated.
        let spec = parse_screen_capture("window:user@host - term").unwrap();
        assert_eq!(
            spec.target,
            CaptureTarget::Window {
                name_substring: "user@host - term".into()
            }
        );
        assert_eq!(spec.fps, DEFAULT_FPS);
    }

    #[test]
    fn parse_window_rejects_empty_name() {
        assert!(parse_screen_capture("window:").is_err());
    }

    #[test]
    fn parse_rejects_unknown_prefix() {
        assert!(parse_screen_capture("bogus").is_err());
        assert!(parse_screen_capture("fullscreen").is_err());
    }

    #[test]
    fn fps_num_den_matches_video_parse_mode_convention() {
        assert_eq!(
            ScreenCaptureSpec {
                target: CaptureTarget::FullScreen { resize: None },
                fps: 30.0
            }
            .fps_num_den(),
            (30, 1)
        );
        let (num, den) = ScreenCaptureSpec {
            target: CaptureTarget::FullScreen { resize: None },
            fps: 29.97,
        }
        .fps_num_den();
        assert_eq!((num, den), (29_970, 1000));
    }

    #[test]
    fn resolve_mode_full_screen_uses_native_unless_resized() {
        let mode = resolve_mode(
            &CaptureTarget::FullScreen { resize: None },
            (1920, 1080),
            30,
            1,
        );
        assert_eq!((mode.width, mode.height), (1920, 1080));

        let mode = resolve_mode(
            &CaptureTarget::FullScreen {
                resize: Some((1280, 720)),
            },
            (1920, 1080),
            30,
            1,
        );
        assert_eq!((mode.width, mode.height), (1280, 720));
    }

    #[test]
    fn resolve_mode_region_ignores_native() {
        let mode = resolve_mode(
            &CaptureTarget::Region {
                x: 0,
                y: 0,
                width: 640,
                height: 480,
            },
            (1920, 1080),
            30,
            1,
        );
        assert_eq!((mode.width, mode.height), (640, 480));
    }

    #[test]
    fn resolve_mode_window_uses_measured_geometry() {
        let mode = resolve_mode(
            &CaptureTarget::Window {
                name_substring: "x".into(),
            },
            (800, 600),
            30,
            1,
        );
        assert_eq!((mode.width, mode.height), (800, 600));
    }

    fn bgra_pixel(x: u32, y: u32) -> [u8; 4] {
        [
            (x & 0xFF) as u8,
            (y & 0xFF) as u8,
            ((x + y) & 0xFF) as u8,
            0xFF,
        ]
    }

    fn make_bgra(w: u32, h: u32) -> Vec<u8> {
        let mut buf = vec![0u8; bgra_frame_bytes(w, h)];
        for y in 0..h {
            for x in 0..w {
                let idx = (y as usize * w as usize + x as usize) * 4;
                buf[idx..idx + 4].copy_from_slice(&bgra_pixel(x, y));
            }
        }
        buf
    }

    #[test]
    fn crop_bgra_in_bounds_matches_source_pixels() {
        let src = make_bgra(100, 80);
        let cropped = crop_bgra(&src, 100, 80, 10, 20, 30, 15);
        assert_eq!(cropped.len(), bgra_frame_bytes(30, 15));
        for y in 0..15u32 {
            for x in 0..30u32 {
                let idx = (y as usize * 30 + x as usize) * 4;
                assert_eq!(&cropped[idx..idx + 4], &bgra_pixel(10 + x, 20 + y));
            }
        }
    }

    #[test]
    fn crop_bgra_clamps_partial_out_of_bounds() {
        let src = make_bgra(50, 50);
        // Rectangle straddling the right/bottom edge: in-bounds portion
        // must still match; out-of-bounds portion is zero, not garbage.
        let cropped = crop_bgra(&src, 50, 50, 40, 40, 20, 20);
        for y in 0..20u32 {
            for x in 0..20u32 {
                let idx = (y as usize * 20 + x as usize) * 4;
                let (sx, sy) = (40 + x, 40 + y);
                if sx < 50 && sy < 50 {
                    assert_eq!(&cropped[idx..idx + 4], &bgra_pixel(sx, sy));
                } else {
                    assert_eq!(&cropped[idx..idx + 4], &[0, 0, 0, 0]);
                }
            }
        }
    }

    #[test]
    fn crop_bgra_fully_negative_origin_is_all_zero() {
        let src = make_bgra(50, 50);
        let cropped = crop_bgra(&src, 50, 50, -100, -100, 10, 10);
        assert!(cropped.iter().all(|&b| b == 0));
    }

    #[test]
    fn resize_nearest_identity_is_a_copy() {
        let src = make_bgra(16, 12);
        let out = resize_nearest_bgra(&src, 16, 12, 16, 12);
        assert_eq!(out, src);
    }

    #[test]
    fn resize_nearest_downscale_halves_dimensions() {
        let src = make_bgra(64, 48);
        let out = resize_nearest_bgra(&src, 64, 48, 32, 24);
        assert_eq!(out.len(), bgra_frame_bytes(32, 24));
        // Corner pixels should map back to the source's corners.
        assert_eq!(&out[0..4], &bgra_pixel(0, 0));
    }

    #[test]
    fn resize_nearest_upscale_produces_expected_size() {
        let src = make_bgra(8, 8);
        let out = resize_nearest_bgra(&src, 8, 8, 32, 16);
        assert_eq!(out.len(), bgra_frame_bytes(32, 16));
    }

    #[test]
    fn bgra_frame_bytes_is_four_bytes_per_pixel() {
        assert_eq!(bgra_frame_bytes(1920, 1080), 1920 * 1080 * 4);
    }

    #[test]
    fn non_linux_capturer_reports_unsupported() {
        // On this (macOS) machine `platform` is always the stub; on Linux
        // CI this test simply doesn't compile-in (see cfg above) since
        // `platform::ScreenCapturer` there is the real backend. The
        // parsing/math tests above are the ones that must hold on every
        // platform.
        #[cfg(not(target_os = "linux"))]
        {
            let spec = ScreenCaptureSpec {
                target: CaptureTarget::FullScreen { resize: None },
                fps: DEFAULT_FPS,
            };
            let err = platform::ScreenCapturer::new(&spec).unwrap_err();
            assert!(matches!(err, CaptureError::UnsupportedPlatform));
            assert!(err.to_string().contains("Linux"));
        }
    }

    #[test]
    fn capture_loop_stops_promptly_when_flagged() {
        // Exercises run_capture_loop's control flow for real (using the
        // stub `ScreenCapturer` unit struct directly, bypassing `::new()`
        // which always errors off-Linux): with `stop` already set, the
        // loop must return without ever blocking, even though the stub's
        // `capture_frame` always errors and so never exercises the
        // send-failure fast path.
        #[cfg(not(target_os = "linux"))]
        {
            use std::sync::atomic::AtomicBool;
            use std::sync::Arc;

            let stop = Arc::new(AtomicBool::new(true));
            let (_mode_tx, mode_rx) = std::sync::mpsc::channel();
            let (frame_tx, _frame_rx) = std::sync::mpsc::sync_channel(1);
            let mode = VideoMode {
                width: 64,
                height: 64,
                fps_num: 1000,
                fps_den: 1,
            };
            run_capture_loop(platform::ScreenCapturer, mode, mode_rx, frame_tx, stop);
            // Reaching here (rather than hanging) is the assertion.
        }
    }
}
