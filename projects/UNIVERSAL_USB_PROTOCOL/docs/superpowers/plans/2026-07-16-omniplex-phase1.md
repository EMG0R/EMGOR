# Omniplex — Phase 1 Implementation Plan

**Goal:** Working Rust core (design layers 1–3) streaming MIDI, OSC, serial,
audio, and video **simultaneously** between two Thunderbolt/USB4-connected
Macs over QUIC, with zero-config discovery, a disciplined session clock,
always-on metrics, and a minimal CLI companion app proving it end to end.

**Spec:** `docs/superpowers/specs/2026-07-16-omniplex-design.md`
(§ references below point there.)

**Architecture:** Cargo workspace of small crates mirroring the design
layers: wire format → transport abstraction (QUIC + in-process loopback) →
discovery → session clock → channel muxer → session facade → CLI app.
Everything except the QUIC socket and the CLI's device I/O is testable
against the loopback transport with injected loss/jitter/drift.

**Tech stack:** Rust stable, `tokio` (control plane only), `quinn` (QUIC),
`mdns-sd` (discovery), `cpal` (audio devices), `midir` (MIDI ports),
`rtrb` (lock-free SPSC ring buffer), `rubato` (async sample-rate
conversion), `serialport` (serial), `rcgen` (self-signed certs),
`tracing` (logs), `criterion` (benchmarks).

## Global Constraints (from spec — apply to every task)

- Max wire fragment size: **16 KB** (16384 bytes payload) — spec §6, hard bound.
- Latency classes: `latency-critical` / `interactive` / `bulk`; latency-critical
  is never delayed or dropped for bandwidth reasons (§9, §10).
- Delivery classes: `reliable-ordered` (QUIC stream), `unreliable-timestamped`
  (datagram), `latest-value-wins` (datagram + sender-side replacement) (§9).
- All media timestamps are in **session-clock time**, u64 nanoseconds (§8).
- No allocation, locks, or async on the audio hot path (§12).
- Protocol version negotiated via ALPN string `omniplex/1` + HELLO version range (§7, §10).
- mDNS service type: `_omniplex._udp.local`, scoped to the point-to-point interface (§7).
- QUIC certs: auto-generated self-signed, **not verified** in v1 (§3, §6).
- Every channel exposes metrics: one-way latency, jitter, drops by cause,
  bandwidth share (§11).
- Rust: `cargo fmt` + `cargo clippy -- -D warnings` clean at every commit.

## Workspace Layout

```
Cargo.toml                 # workspace root
crates/
  omniplex-wire/                # frame format, HELLO, channel descriptors, test vectors
  omniplex-transport/           # Transport trait + QuinnTransport + LoopbackTransport
  omniplex-discovery/           # interface watch, mDNS announce/browse, dialer election
  omniplex-clock/               # session clock: PTP-style exchange + discipline filter
  omniplex-mux/                 # channel manager, scheduler, fragmentation, backpressure
  omniplex-metrics/             # per-channel counters/histograms, snapshot API
  omniplex-session/             # facade: discovery → connect → HELLO → clock → mux
  omniplex-cli/                 # minimal companion app (device bridging, stats, load test)
spikes/
  tb-link-check/           # Milestone 0 spike binary
  quinn-behavior/          # Milestone 0 spike binary
docs/superpowers/specs/    # (existing) design doc; wire test vectors live in omniplex-wire/vectors/
```

Dependency direction is strictly downward: `omniplex-cli → omniplex-session →
{omniplex-mux, omniplex-clock, omniplex-discovery} → omniplex-transport → omniplex-wire`.
`omniplex-metrics` is a leaf used by mux/clock/session.

---

## Milestone 0 — De-risking spikes (do first; results can amend the spec)

### Task 0.1: Thunderbolt link ground truth (`spikes/tb-link-check`)

**Deliverable:** a short binary + a written result note. Requires the two
target Macs and a TB cable.

Steps:
1. `cargo new spikes/tb-link-check` — binary that enumerates network
   interfaces (via `if-addrs` crate), prints name/flags/IPv6 link-local
   addrs, and re-prints on change (poll 1 s).
2. Plug the two Macs together. Record: which interface appears
   (`bridge0`? `en_x`?), whether fe80:: addrs self-assign, how long
   until the interface is up.
3. Measure raw floor with system tools: `ping6 -c 1000 <peer%iface>`
   (latency/jitter), `iperf3` over the link (bandwidth).
4. Write results into `spikes/tb-link-check/RESULTS.md`:
   interface naming pattern, link-local behavior, RTT p50/p99, Gbps.
5. **Gate:** if link-local IPv6 does not self-assign or the interface is
   not usable for UDP, stop and revise spec §7 before proceeding.
6. Commit.

### Task 0.2: quinn behavior verification (`spikes/quinn-behavior`)

**Deliverable:** ~150-line binary proving the three QUIC properties §6
depends on, run first on localhost, then over the TB link.

Steps:
1. Set up client+server `quinn::Endpoint` with `rcgen` self-signed cert
   and a client `ServerCertVerifier` that accepts anything (this exact
   config is reused in Task 2.2 — keep it copy-pasteable).
2. Test A — datagrams: confirm `max_datagram_size()` ≥ ~1200 B and
   round-trip datagrams work. Record the actual max (informs how audio
   packets and video fragments map to datagrams vs. small streams).
3. Test B — no head-of-line blocking: open two streams; flood stream 1
   with 100 MB while sending 1 KB pings on stream 2 every 5 ms; assert
   ping RTT p99 while flooded < 5× idle RTT.
4. Test C — priority: use `SendStream::set_priority()`; verify a
   high-priority stream's writes are serviced ahead of a saturating
   low-priority stream.
5. Write `spikes/quinn-behavior/RESULTS.md` with numbers.
   **Gate:** if datagram max is too small for a 16 KB fragment, the mux
   maps `unreliable-timestamped` fragments to ≤ datagram-size packets
   instead (note it; spec bound stays 16 KB as an upper limit). If
   priority is not honored, mux implements its own send scheduler on
   top of streams (Task 5.4 already structured to allow this).
6. Commit.

---

## Milestone 1 — Wire format (`omniplex-wire`)

Pure data: no I/O, no async. This crate IS the open spec's normative
encoding, so it also ships the reference test vectors (§12).

### Task 1.1: Core types

**Files:** `crates/omniplex-wire/src/{lib.rs, types.rs}`,
tests inline per Rust convention.

```rust
pub const MAX_FRAGMENT_PAYLOAD: usize = 16 * 1024; // spec §6, normative
pub const PROTOCOL_ALPN: &[u8] = b"omniplex/1";

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum DeliveryClass { ReliableOrdered, UnreliableTimestamped, LatestValueWins }

#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Debug)]
pub enum LatencyClass { LatencyCritical, Interactive, Bulk }

#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug)]
pub struct ChannelId(pub u16);          // 0 = control, 1 = clock, ≥2 dynamic

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub struct SessionTimestamp(pub u64);   // ns in session-clock time

#[derive(Clone, PartialEq, Debug)]
pub struct ChannelDescriptor {
    pub id: ChannelId,
    pub name: String,                   // e.g. "midi/1", "audio/main", "video/cam"
    pub kind: ChannelKind,              // Midi | Osc | Serial | Audio | Video | Dmx | Timecode | Raw
    pub delivery: DeliveryClass,
    pub latency: LatencyClass,
    pub format: FormatDescriptor,       // enum: per-kind params, see below
}

#[derive(Clone, PartialEq, Debug)]
pub enum FormatDescriptor {
    Midi,                                        // raw MIDI 1.0 bytes
    Osc,
    Serial { baud_hint: Option<u32> },
    Audio { sample_rate: u32, channels: u16, format: SampleFormat }, // F32 only in Phase 1
    Video { width: u32, height: u32, fps_num: u32, fps_den: u32, pixel: PixelFormat }, // UYVY, BGRA
    Dmx { universe: u16 },
    Timecode,
    Raw { delivery_hint: DeliveryClass },
}
```

Steps: write types + exhaustive `Debug/Clone/Eq` derives; unit tests for
invariants (e.g. `ChannelId(0)`/`ChannelId(1)` reserved constructors);
`cargo test -p omniplex-wire`; commit.

### Task 1.2: Frame encoding

**Files:** `crates/omniplex-wire/src/frame.rs`, `crates/omniplex-wire/vectors/*.bin`

Wire frame (little-endian, fixed header then payload):

```
u16 channel_id | u8 frame_type | u8 flags | u64 timestamp_ns
| u32 seq | u16 frag_index | u16 frag_count | u16 payload_len | payload…
```

`frame_type`: 0 Hello, 1 HelloAck, 2 ChannelOpen, 3 ChannelOpenAck,
4 ChannelClose, 5 Media, 6 ClockProbe, 7 ClockReply, 8 Bye.
`flags` bit 0: `end_of_message` (last fragment).

```rust
pub struct FrameHeader { pub channel: ChannelId, pub frame_type: FrameType,
    pub flags: u8, pub timestamp: SessionTimestamp, pub seq: u32,
    pub frag_index: u16, pub frag_count: u16 }

pub fn encode_frame(h: &FrameHeader, payload: &[u8], out: &mut Vec<u8>) -> Result<(), WireError>;
pub fn decode_frame(buf: &[u8]) -> Result<(FrameHeader, &[u8]), WireError>; // zero-copy payload
```

Steps (TDD):
1. Tests first: round-trip encode/decode for every frame type;
   `payload > MAX_FRAGMENT_PAYLOAD` → `WireError::FragmentTooLarge`;
   truncated buffer → `WireError::Truncated`; unknown frame_type →
   `WireError::UnknownFrameType` (channel-level error per §10, decode
   still returns header so mux can kill just the channel).
2. Implement; `cargo test -p omniplex-wire` green.
3. Generate reference test vectors: a small `#[test]`-gated generator
   writes `vectors/frame_*.bin` + a `vectors/README.md` describing each;
   a separate test decodes every vector file and asserts expected fields
   (this is the seed of the third-party conformance suite, §12).
4. Commit.

### Task 1.3: HELLO + control payloads

**Files:** `crates/omniplex-wire/src/control.rs`

Control payloads are serialized with `postcard` (compact, no_std-friendly,
schema defined by these structs — acceptable for v1; the spec doc will
freeze the byte layout from the vectors):

```rust
pub struct Hello { pub version_min: u16, pub version_max: u16,
    pub instance_id: [u8; 16], pub channels: Vec<ChannelDescriptor> }
pub struct HelloAck { pub version: u16, pub clock_master: bool,
    pub channels: Vec<ChannelDescriptor> }
pub struct ChannelOpen { pub desc: ChannelDescriptor }
pub struct ChannelOpenAck { pub id: ChannelId, pub accepted: bool, pub reason: Option<String> }
pub struct ClockProbe { pub t1_ns: u64 }                     // sender's local send time
pub struct ClockReply { pub t1_ns: u64, pub t2_ns: u64, pub t3_ns: u64 } // echo + recv + send
```

Version negotiation helper with tests:
`pub fn negotiate(a: (u16,u16), b: (u16,u16)) -> Option<u16>` — highest
overlap or None (clean failure per §10).
Steps: tests → impl → vectors for Hello/HelloAck → commit.

---

## Milestone 2 — Transport (`omniplex-transport`)

### Task 2.1: Transport trait + loopback

**Files:** `crates/omniplex-transport/src/{lib.rs, loopback.rs}`

The abstraction every later phase (raw USB OTG) must also satisfy (§6):

```rust
#[async_trait::async_trait]
pub trait Transport: Send + Sync + 'static {
    async fn open_stream(&self, priority: i32) -> Result<Box<dyn TransportStream>, TransportError>;
    async fn accept_stream(&self) -> Result<Box<dyn TransportStream>, TransportError>;
    fn send_datagram(&self, data: bytes::Bytes) -> Result<(), TransportError>; // non-blocking
    async fn recv_datagram(&self) -> Result<bytes::Bytes, TransportError>;
    fn max_datagram_size(&self) -> usize;
    async fn closed(&self) -> TransportError;    // resolves on link loss (§10)
}

#[async_trait::async_trait]
pub trait TransportStream: Send {
    async fn send(&mut self, data: &[u8]) -> Result<(), TransportError>;
    async fn recv(&mut self, buf: &mut [u8]) -> Result<usize, TransportError>; // 0 = closed
}
```

`LoopbackTransport::pair(config) -> (LoopbackTransport, LoopbackTransport)`
with `LoopbackConfig { datagram_loss: f64, latency: Duration,
jitter: Duration, bandwidth_bytes_per_sec: Option<u64>, seed: u64 }` —
deterministic (seeded `rand::SmallRng`), tokio-time based so tests can use
`tokio::time::pause()`. This is the test harness for everything above (§13).

Steps: trait + loopback with tests (datagram delivery, seeded loss rate
within tolerance, stream ordering, bandwidth cap shapes throughput,
`closed()` fires when one side drops); commit.

### Task 2.2: QuinnTransport

**Files:** `crates/omniplex-transport/src/quinn_transport.rs`

- `QuinnTransport::listen(bind: SocketAddr) -> …` and
  `QuinnTransport::dial(peer: SocketAddr) -> …`, both using the exact
  cert config proven in Task 0.2 (rcgen self-signed, accept-any verifier,
  ALPN `omniplex/1`).
- Transport tuning per §6: `max_idle_timeout` 500 ms with 100 ms
  keep-alives (→ ~200 ms cable-pull detection, §7/§10), datagrams
  enabled, initial windows sized for the spike-measured link, congestion
  controller left default in Phase 1 **but** bulk pacing is done in the
  mux (Task 5.4), which is where the spec's pacing lives.
- `open_stream(priority)` maps to `SendStream::set_priority`.
- Integration test (localhost): run the whole Task 2.1 test suite
  generically over `QuinnTransport` via a shared
  `fn transport_conformance<T: Transport>(a: T, b: T)` helper — the same
  suite both transports must pass keeps the abstraction honest.
- Commit.

---

## Milestone 3 — Discovery (`omniplex-discovery`)

### Task 3.1: Interface watch + announce/browse + election

**Files:** `crates/omniplex-discovery/src/{lib.rs, ifwatch.rs, mdns.rs}`

```rust
pub struct PeerFound { pub addr: std::net::SocketAddrV6,  // link-local + scope id
    pub instance_id: [u8; 16], pub version_range: (u16, u16) }

pub enum Role { Dialer, Listener }
pub fn elect(ours: [u8;16], theirs: [u8;16]) -> Role;   // higher id dials (§7)

pub struct Discovery { /* … */ }
impl Discovery {
    pub fn start(cfg: DiscoveryConfig) -> Self;          // spawns tokio tasks
    pub async fn next_peer(&mut self) -> PeerFound;      // fires on each discovery
}
```

- `ifwatch`: polls interfaces (1 s, `if-addrs`) for point-to-point-capable
  interfaces gaining IPv6 link-local addrs. Filter by the naming pattern
  recorded in Task 0.1 RESULTS.md, plus an env override
  (`OMNIPLEX_IFACE=name`) for testing over any interface.
- `mdns`: `mdns-sd` daemon bound to that interface only; registers
  `_omniplex._udp.local` with TXT `v=1-1`, `id=<hex instance id>`, and the QUIC
  UDP port; browses for the same and yields `PeerFound`.
- Unit tests: `elect` determinism/antisymmetry (`elect(a,b)` never equals
  `elect(b,a)`); TXT record parse round-trip; malformed TXT ignored.
- Integration test (self-discovery over loopback interface, ignored-by-
  default `#[ignore]` tag since it needs real mDNS): announce + browse
  finds itself, filters own instance id out.
- Commit.

---

## Milestone 4 — Session clock (`omniplex-clock`)

### Task 4.1: Discipline filter (pure math, fully unit-testable)

**Files:** `crates/omniplex-clock/src/{lib.rs, filter.rs}`

```rust
/// Feed with PTP-style samples; ask it to convert local time → session time.
pub struct ClockFilter { /* offset + drift estimate */ }
pub struct ClockSample { pub t1: u64, pub t2: u64, pub t3: u64, pub t4: u64 } // ns

impl ClockFilter {
    pub fn new() -> Self;
    pub fn ingest(&mut self, s: ClockSample);
    /// offset = ((t2 - t1) + (t3 - t4)) / 2, assuming symmetric path (§8 —
    /// true on point-to-point TB). Estimator: keep the min-delay samples
    /// (lowest (t4-t1)-(t3-t2)) in a sliding window; linear-regress offset
    /// over local time across the window to get drift (ppm); slew, never step,
    /// after initial lock.
    pub fn to_session(&self, local_ns: u64) -> Option<u64>;  // None until locked
    pub fn stats(&self) -> ClockStats;  // current offset, drift ppm, est. error
}
```

Tests (all deterministic, no I/O): constant offset recovered exactly;
constant drift (e.g. +50 ppm) recovered within 1 ppm after N samples;
jittery samples (seeded noise ±200 µs) → offset error < 50 µs using
min-delay filtering; a single 10 ms outlier sample does not disturb the
estimate; `to_session` is monotonic (slewing never steps backwards).
Commit.

### Task 4.2: Clock protocol loop

**Files:** `crates/omniplex-clock/src/proto.rs`

```rust
pub struct SessionClock { /* Arc<inner> */ }
impl SessionClock {
    /// Master: answers ClockProbe with ClockReply. Follower: sends a probe
    /// every 100 ms on the dedicated clock stream (ChannelId(1), latency-critical),
    /// feeds the filter. Master's session time IS its local monotonic ns.
    pub fn spawn_master(stream: Box<dyn TransportStream>) -> Self;
    pub fn spawn_follower(stream: Box<dyn TransportStream>) -> Self;
    pub fn now_session_ns(&self) -> Option<u64>;   // lock-free read (ArcSwap of (offset, drift, ref))
    pub fn stats(&self) -> ClockStats;
}
```

Local monotonic source: `mach_absolute_time`-backed `Instant`-to-ns helper
(one small `time.rs`, later cfg'd per OS). The `(offset, drift, reference)`
triple is published via `arc_swap::ArcSwap` so the audio thread can call
`now_session_ns` without locks (§12 constraint).

Integration test over `LoopbackTransport` with `latency: 200µs,
jitter: 100µs`: follower locks within 2 s of tokio-paused time; injected
drift (scale follower's local clock by 1.00005 in test) is tracked.
Commit.

---

## Milestone 5 — Muxer (`omniplex-mux`) — the heart

### Task 5.1: Channel registry + open/close handshake

**Files:** `crates/omniplex-mux/src/{lib.rs, registry.rs}`

```rust
pub struct Mux { /* … */ }
pub struct ChannelTx { /* per-delivery-class sender, see 5.2/5.3 */ }
pub struct ChannelRx { /* … */ }

impl Mux {
    pub fn new(t: Arc<dyn Transport>, clock: SessionClock, metrics: MetricsHub) -> Self;
    pub async fn open_channel(&self, desc: ChannelDescriptor) -> Result<(ChannelTx, ChannelRx), MuxError>;
    pub async fn accept_channel(&self) -> (ChannelDescriptor, ChannelTx, ChannelRx); // remote-initiated
    pub async fn close_channel(&self, id: ChannelId);
}
```

Open = send `ChannelOpen` on control stream (ChannelId(0), reliable,
priority max), await `ChannelOpenAck`; ids allocated even/odd by role to
avoid collisions (dialer even, listener odd, starting at 2). Rejection
(`accepted: false`) surfaces as `MuxError::Rejected(reason)`.
Tests over loopback: open/ack round trip, id collision-freedom when both
sides open simultaneously, close tears down both ends, unknown-channel
frame → that channel errored, session survives (§10 malformed-peer rule).
Commit.

### Task 5.2: Reliable-ordered path + fragmentation

**Files:** `crates/omniplex-mux/src/reliable.rs`

Each reliable channel gets its own transport stream at a priority derived
from `LatencyClass` (LatencyCritical=100, Interactive=50, Bulk=0).
`ChannelTx::send(payload, ts)` splits payload into ≤ `MAX_FRAGMENT_PAYLOAD`
frames (`frag_index/frag_count`, `end_of_message` on last); `ChannelRx::recv()
-> (Bytes, SessionTimestamp)` reassembles. Backpressure: bounded (64-msg)
channel; `send` awaits when full (publisher-blocks semantics, §9).
Tests: 1-byte and 1 MB messages round-trip; fragment bound never exceeded
(assert on every encoded frame in a test transport wrapper); interleaved
messages on two channels don't corrupt; full buffer blocks sender until
drained. Commit.

### Task 5.3: Unreliable + latest-value paths

**Files:** `crates/omniplex-mux/src/unreliable.rs`

- `UnreliableTimestamped`: payload → datagrams (fragment to
  `min(MAX_FRAGMENT_PAYLOAD, transport.max_datagram_size() - header)`),
  reassembly buffer keyed by `(channel, seq)` with a per-channel deadline
  (default 2 frame-intervals for video, 5 ms otherwise): message with
  missing fragments past deadline is discarded whole and counted
  (`drop_cause: Late`) (§9 video rule). Receiver buffer bounded
  (drop-oldest, `drop_cause: Backpressure`).
- `LatestValueWins`: sender side holds **one** pending slot per channel;
  a new `send` before the previous hit the wire replaces it (counted
  `drop_cause: Replaced`). Receiver: only delivers seq > last-delivered.
- Tests over lossy loopback (10 % datagram loss, seeded): unreliable
  channel delivers ~90 % complete small messages, zero corrupt ones;
  multi-fragment messages with a lost fragment are dropped whole;
  latest-value channel under a send flood delivers a strictly increasing
  subsequence ending at the final value; stale (reordered) datagram never
  delivered after a newer one. Commit.

### Task 5.4: Send scheduler + pacing + bandwidth adaptation

**Files:** `crates/omniplex-mux/src/scheduler.rs`

All datagram sends funnel through one scheduler task:

- **Strict priority tiers** by `LatencyClass`: a latency-critical datagram
  in queue is always sent before any interactive/bulk one (§2 guarantee —
  this also covers the case where transport-level stream priority proved
  weak in Task 0.2).
- **Pacing for bulk only:** token bucket per bulk channel, refilled from
  its current allocation; latency-critical is exempt (§6 "they always go
  now").
- **Bandwidth estimation:** EWMA of acked-bytes-per-RTT from
  `quinn::Connection::stats()` (loopback: from `LoopbackConfig` cap),
  re-estimated every 100 ms.
- **Degradation ladder (§10):** when estimated demand > 90 % of estimated
  capacity: shrink bulk allocations proportionally and emit
  `MuxEvent::BandwidthPressure { channel, allocated }` to the channel
  owner (the app layer decides how video degrades — resolution/fps —
  because only it can re-encode; the mux enforces the allocation).
  Latency-critical never throttled.
- Tests (tokio-paused time over bandwidth-capped loopback): with a
  50 Mbps cap, a bulk flood + 1 kHz latency-critical pings → ping p99
  queueing delay < 1 ms of simulated time; bulk throughput ≈ cap; pressure
  events fire; removing the flood restores allocations. Commit.

### Task 5.5: Session facade (`omniplex-session`)

**Files:** `crates/omniplex-session/src/lib.rs`

Ties layers together — the API the CLI (and later the SDK, Phase 2) uses:

```rust
pub struct Session { /* … */ }
pub enum SessionEvent { PeerDiscovered, Connected { version: u16 },
    ChannelOffered(ChannelDescriptor), Disconnected(DisconnectReason),
    BandwidthPressure { channel: ChannelId, allocated_bps: u64 } }

impl Session {
    /// Full §7 flow: watch link → mDNS → elect → dial/listen (QuinnTransport)
    /// → HELLO/HelloAck (version negotiate, clock-master election = dialer)
    /// → spawn SessionClock → Mux ready. Auto-reconnect loop on disconnect.
    pub fn start(cfg: SessionConfig) -> (Session, mpsc::Receiver<SessionEvent>);
    pub async fn open_channel(&self, desc: ChannelDescriptor) -> Result<(ChannelTx, ChannelRx), MuxError>;
    pub fn clock(&self) -> SessionClock;
    pub fn metrics(&self) -> MetricsHub;
}
```

Also a test-only constructor `Session::from_transport(loopback, role)`
skipping discovery. Integration test: two `from_transport` sessions
complete HELLO, negotiate version 1, follower's clock locks, a MIDI-kind
channel opened by A is offered to B and round-trips a message; version
ranges (2,3) vs (1,1) → clean `Disconnected(VersionMismatch)` on both
sides. Commit.

---

## Milestone 6 — Metrics (`omniplex-metrics`)

### Task 6.1: MetricsHub

**Files:** `crates/omniplex-metrics/src/lib.rs`

```rust
pub struct MetricsHub { /* Arc<DashMap<ChannelId, ChannelMetrics>> */ }
pub struct ChannelMetrics {
    pub latency: HistogramSnapshot,   // one-way ns, from frame ts vs clock.now (§11)
    pub jitter_ns: u64,               // RFC3550-style EWMA
    pub drops: DropCounters,          // late / lost / backpressure / replaced
    pub bytes_tx: u64, pub bytes_rx: u64, pub allocated_bps: u64,
}
pub fn snapshot(&self) -> BTreeMap<ChannelId, ChannelMetrics>;
```

Hot-path recording is a lock-free `hdrhistogram` behind a per-channel
`parking_lot::Mutex` **except** on the audio path, which records into a
preallocated SPSC ring drained by the metrics task (no locks in the
callback, §12). Wire it into 5.2/5.3/5.4 recording sites (they were
written against `MetricsHub` from the start — this task implements the
real hub replacing the initial no-op). Tests: concurrent recording totals
add up; one-way latency computed from a fake clock matches injected
loopback latency within jitter. Commit.

---

## Milestone 7 — Minimal companion app (`omniplex-cli`)

Bridges real devices ↔ channels. No DriverKit/virtual drivers in Phase 1
(§ layer 5 flagship is Phase 2): MIDI uses `midir` **virtual ports**
(macOS supports these natively — DAWs see "Omniplex Bridge"), audio uses `cpal`
real devices, OSC uses local UDP sockets, serial uses `serialport`, video
uses a synthetic test-pattern source + receiver that writes raw frames
(validated with ffplay) — enough to prove the pipeline and run the
measurements; camera capture joins in Phase 2.

### Task 7.1: CLI skeleton + status view

**Files:** `crates/omniplex-cli/src/{main.rs, ui.rs}`

`clap` commands: `omniplex run [--midi] [--osc <recv_port>:<send_port>]
[--serial <dev>] [--audio] [--video-test]`, `omniplex bench midi-under-load`,
`omniplex info`. `run` starts `Session::start`, prints session events, and
renders a 1 Hz metrics table (channel, class, one-way p50/p99, jitter,
drops, bandwidth) from `MetricsHub::snapshot` — this table is the §11
"shown in the app" requirement. Test: `omniplex info` unit-smoke; the rest is
exercised in 7.x integration. Commit.

### Task 7.2: MIDI + OSC + serial bridges

**Files:** `crates/omniplex-cli/src/bridge/{midi.rs, osc.rs, serial.rs}`

- MIDI: `midir` virtual input "Omniplex In" → channel `midi/1`
  (ReliableOrdered, LatencyCritical); channel → virtual output "Omniplex Out".
- OSC: UDP listen on `recv_port` → channel `osc/1`; per-address routing:
  message address seen with type tags of only floats/ints at >10 Hz →
  latest-value channel `osc/lv`, otherwise reliable `osc/1` (§9 rule,
  implemented as two channels); inbound → UDP `send_port` on localhost.
- Serial: `serialport` read loop → `serial/1` (ReliableOrdered,
  Interactive); inbound → port write, honoring backpressure (blocks the
  channel, §9).
- Loopback-session integration test for each bridge's encode/route logic
  (device I/O behind small traits so tests inject bytes). Commit.

### Task 7.3: Audio bridge (the RT-discipline task)

**Files:** `crates/omniplex-cli/src/bridge/audio.rs`

- Send side: `cpal` input callback writes f32 frames + local-ns capture
  time into an `rtrb` SPSC ring (preallocated, no alloc/locks in callback);
  a sender thread drains the ring, stamps session time via
  `clock.now_session_ns()` (lock-free read, Task 4.2), packs fixed
  256-sample-frame packets into `audio/main`
  (UnreliableTimestamped, LatencyCritical).
- Receive side: network task pushes packets into a jitter buffer ordered
  by session timestamp; `cpal` output callback pulls via a second SPSC
  ring; **ASRC** (`rubato`) between jitter buffer and output ring,
  ratio disciplined by long-term drift between session clock and the
  output device clock (§8). Presentation offset starts at 10 ms, shrinks
  toward measured p99 jitter + 2 ms (adaptive, §8). Missing packet →
  repeat-fade concealment (§9).
- Tests: jitter-buffer + ASRC logic extracted pure (no cpal) and tested:
  drifting fake device clock (+100 ppm) over 60 s of simulated audio →
  zero underruns, output continuous (no discontinuity > 1 sample-delta);
  5 % packet loss → concealment invoked, never silence gaps > 1 packet.
  Manual: two-Mac listening test. Commit.

### Task 7.4: Video test channel

**Files:** `crates/omniplex-cli/src/bridge/video.rs`

Test-pattern generator (moving bars + frame counter burned in, UYVY
1280×720@30 default, `--video-test 1920x1080@60` to push load) →
`video/test` (UnreliableTimestamped, Bulk); receiver reassembles frames,
checks continuity by decoded counter, optionally pipes raw frames to
stdout for `ffplay -f rawvideo …` viewing. Handles
`BandwidthPressure` by halving fps then resolution (the app-side ladder,
Task 5.4 contract). Loopback test: 200 frames through lossy loopback →
every delivered frame intact (whole-frame drop rule), counter gaps only
at drops. Commit.

### Task 7.5: `omniplex bench midi-under-load` (the headline measurement, §11)

**Files:** `crates/omniplex-cli/src/bench.rs`

Runs on two Macs (`--role a|b`): B echoes `midi/bench` messages back;
A sends 1 kHz timestamped MIDI for 60 s, twice — idle link, then with
`video/test` at a rate that saturates (ramped until pressure events) —
and prints both RTT distributions (p50/p90/p99/max) plus the spec target
check: **loaded p99 within 2× idle p99** (pass/fail exit code).
Also `--soak <hours>` mode: all channels running, asserts zero audio
underruns and bounded memory (RSS sampled/logged) — the §11 soak.
Loopback-mode variant (`--loopback`) runs the same bench in-process in CI
with a bandwidth-capped loopback (asserts the §5.4 numbers hold end-to-end
through the real bridges). Commit.

---

## Milestone 8 — End-to-end validation (§13)

### Task 8.1: Two-Mac acceptance run

Checklist executed on the real hardware, results recorded in
`docs/superpowers/specs/phase1-acceptance.md`:

- [ ] Cable in → both `omniplex run` instances connected, media flowing < 2 s (§7 target), zero dialogs.
- [ ] MIDI: play from a DAW on Mac A through "Omniplex In", arrives at DAW on Mac B; feels immediate.
- [ ] OSC: TouchOSC/monitor round-trip both directions.
- [ ] Serial: `screen`-to-`screen` byte-stream both directions.
- [ ] Audio: program material A→B, listen for 10 min, zero glitches; metrics table shows one-way latency and stable clock error (report the numbers).
- [ ] Video: test pattern at 1080p60 alongside everything else, smooth, counter-continuous except reported drops.
- [ ] All of the above **simultaneously** for 15 min — the §2 headline.
- [ ] `omniplex bench midi-under-load` two-Mac run: record distributions; loaded p99 ≤ 2× idle p99.
- [ ] Cable pull mid-session: both sides report disconnect < 1 s, no hang; re-plug → auto-resume, channels return (§7 step 5, §10).
- [ ] Clock: follower `ClockStats` estimated error < 100 µs sustained (§8 target).
- [ ] 24 h soak (`--soak 24`): zero underruns, RSS flat (§11).

Any failure loops back to the owning task. When green: tag `v0.1.0-phase1`,
and Phase 2 (SDK extraction + flagship UI) plans against this baseline.

---

## Execution order & parallelism

Strictly ordered: 0 → 1 → 2 → 3/4 (parallelizable) → 5 → 6 → 7 → 8.
Milestones 3 and 4 both depend only on 1–2 and can be built in either
order or in parallel. Within Milestone 7, tasks 7.2/7.3/7.4 are
independent after 7.1.

## Post-Phase-1 addendum: mixed-platform links (Mac ↔ Pi, Pi ↔ Pi)

Spec §5.1 covers this in detail; noted here since it changes almost
nothing in this plan. `omniplex-discovery`'s `is_point_to_point_capable`
(`crates/omniplex-discovery/src/ifwatch.rs`) now also matches Linux Ethernet
naming (`eth*`/`enp*`/`eno*`/`ens*`) in addition to the macOS `bridge*`/
`en*` cases — a Mac's USB-C-to-Ethernet adapter already matched `en*`
before this change, so Mac-side behavior is unaffected; only the Pi side
needed the addition. No other crate changed: transport, clock, mux, and
session are all interface-agnostic already. This means a Mac ↔ Pi (over a
USB-C-Ethernet adapter + Ethernet cable) or Pi ↔ Pi (direct Ethernet
cable) run of `omniplex-cli` should work today with no further code changes,
ahead of the USB OTG dual-role work still scoped for Phase 3.

## Risks & contingencies

- **TB interface behavior differs from assumption** (Task 0.1 gate):
  worst case, fall back to any shared Ethernet/Wi-Fi interface for
  development (the code is interface-agnostic via `OMNIPLEX_IFACE`) while the
  TB path is investigated — everything above layer 1 is unaffected.
- **quinn priority/datagram limits** (Task 0.2 gate): scheduler in 5.4 is
  already designed to enforce priority above the transport; datagram-size
  limits only change fragment sizing, not the 16 KB spec bound.
- **cpal input+output callback timing on macOS** proves flaky: switch the
  audio bridge to direct CoreAudio AUHAL via `coreaudio-rs` — isolated to
  `bridge/audio.rs` by design.
- **mdns-sd interface scoping insufficient on macOS**: fall back to a
  hand-rolled announce (single UDP multicast on the scoped socket) —
  isolated to `omniplex-discovery/src/mdns.rs`; the `Discovery` API is unchanged.
