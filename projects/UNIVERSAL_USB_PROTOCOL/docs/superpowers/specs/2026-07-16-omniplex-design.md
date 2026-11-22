# Omniplex — Design

**Status:** Draft v2, approved for planning
**Date:** 2026-07-16

> Naming note: the protocol is link-agnostic (v1 actually runs over
> Thunderbolt/USB4 peer networking, not raw USB). The protocol and its
> reference implementation are named **Omniplex**.

## 1. Problem & Vision

Creative and technical work today routes audio, video, MIDI, OSC, DMX, and
serial data between computers through a patchwork of separate protocols,
each requiring its own driver, its own network infrastructure, or its own
adapter box — Dante/AVB for synced audio, NDI for video, rtpMIDI for MIDI,
Art-Net/sACN for lighting. Connecting two machines to freely exchange *any
combination* of this "art data," in real time, with no switch, router, or
manual configuration, requires stitching several of these systems together
by hand.

This project defines an open protocol and reference implementation that
treats a direct USB-C connection (or the fastest link two machines actually
share) as a single adaptive bandwidth pool, and multiplexes any combination
of real-time channel types across it **simultaneously** — automatically
detecting the link, discovering the peer, negotiating roles, and adapting
to available bandwidth, with zero infrastructure and zero manual setup.

The design bar for every decision in this document: **lowest achievable
latency, glitch-free stability under sustained load, zero configuration.**
Where those conflict, latency-critical channels win over bandwidth-hungry
ones, and stability wins over peak throughput.

**Two deliverables, not one:**
- An **open protocol spec + SDK** (channel model, framing, negotiation,
  clock sync) that any developer or hardware maker can implement against —
  this is the part that needs to be trustworthy and neutral to get adopted,
  the way MIDI or USB itself became a standard other things are built on.
- A **flagship companion app** (the reference implementation) that is the
  most polished, zero-friction, "just works" experience of using the
  protocol — this is where the product competes, not the spec.

## 2. Goals

- Direct, real-time, low-latency streaming of audio, video, MIDI, OSC, DMX,
  timecode/sync, serial, and arbitrary raw data between two machines —
  **all channel types concurrently over one connection**, at whatever
  bandwidth the physical link supports.
- Zero manual configuration: plug two machines together, the software
  detects the link, discovers the peer, negotiates, and starts routing.
- Latency-critical traffic (MIDI/OSC/DMX) is **never delayed by bulk
  traffic (video)** — guaranteed by transport structure, not just by
  scheduling policy (§6).
- All media channels share one **session clock**, so audio, video, and
  lighting stay phase-aligned across the link (§8).
- No unnecessary DAC/ADC or lossy conversion — data stays digital
  end-to-end wherever the source and destination are both digital.
- Works across the best link available: Thunderbolt/USB4 peer networking
  when present, plain USB OTG host/device negotiation otherwise.
- Open, versioned protocol + SDK so third-party creative software (DAWs,
  VJ tools, lighting consoles, game engines) and hardware can adopt it.
- Cross-platform core (macOS, Windows, Linux), acknowledging macOS ships
  first since that's the primary development hardware.
- Built-in instrumentation: the protocol measures its own latency, jitter,
  and loss per channel, continuously (§11).

## 3. Non-Goals (v1)

- **Security/pairing beyond link-locality.** v1 carries visual/audio/
  control data between two directly-cabled machines; any peer on the
  point-to-point link interface is trusted. The transport (QUIC, §6)
  already encrypts with auto-generated self-signed certificates, so wire
  privacy exists by construction, but there is no user-facing pairing,
  authentication, or trust UI in v1. The spec reserves a pairing handshake
  extension point so it can be added without a breaking change.
- A dedicated physical bridge device for connecting two **host-only**
  machines with no shared Thunderbolt/USB4 link. This is a real,
  physically-necessary case (explained in §5), but it's a separate
  hardware project, scoped after the software core is proven.
- Many-node mesh routing / addressing beyond a single point-to-point link.
  The protocol's channel model is designed to not preclude this later, but
  v1 targets two directly-connected machines.
- Raspberry Pi gadget-mode bridging. Useful future transport target (Pi's
  USB-C OTG support is well-suited to it), but not required to prove the
  architecture, which is validated first on Mac-to-Mac over Thunderbolt.

## 4. Architecture

Five layers, each independently testable and each with a clear
responsibility:

1. **Link Detection, Discovery & Negotiation** — on connect, determines
   what kind of link exists (Thunderbolt/USB4 peer-capable, USB OTG
   dual-role, or one side host-only), discovers the peer on that link
   (§7), and negotiates protocol version and session-clock roles — no
   user input.
2. **Transport** — moves frames over whatever link layer 1 found. v1:
   QUIC over the Thunderbolt/USB4 virtual network interface, giving
   independent per-channel streams and unreliable datagrams over a single
   connection (§6). Later phases: the same framing over raw
   vendor-specific USB bulk/interrupt endpoints in OTG gadget/host mode.
3. **Multiplexing / Channel Protocol** — the open, versioned spec. Maps
   named channels onto transport streams/datagrams with per-channel
   delivery semantics (§9), fragmentation rules that bound how long any
   latency-critical message can wait (§6), timestamping against the
   session clock (§8), and dynamic bandwidth adaptation by priority class.
4. **SDK / Channel Discovery & Pub-Sub API** — open library other apps and
   hardware use to publish/subscribe named channels (audio, MIDI, OSC,
   DMX, etc.) without touching transport or muxing internals directly.
5. **Companion App & Virtual Devices + Flagship UI** — the reference
   product. Terminates the protocol and exposes each channel as something
   native OS apps can consume directly: a virtual audio device, a virtual
   MIDI port, a virtual serial/COM port, a virtual camera. This is the only
   layer that is inherently OS-specific.

```
┌─────────────────────────────────────────────┐
│ 5. Companion App & Virtual Devices / UI      │  ← product, OS-specific
├─────────────────────────────────────────────┤
│ 4. SDK / Channel Discovery & Pub-Sub API     │  ← open, cross-platform
├─────────────────────────────────────────────┤
│ 3. Multiplexing / Channel Protocol (spec)    │  ← open, versioned spec
├─────────────────────────────────────────────┤
│ 2. Transport (QUIC / USB endpoints)          │  ← cross-platform
├─────────────────────────────────────────────┤
│ 1. Link Detection, Discovery & Negotiation   │  ← cross-platform
└─────────────────────────────────────────────┘
```

## 5. Link Strategy

The dividing line for "does this need hardware" is **Thunderbolt/USB4
presence, not OS**:

| Both sides have TB3/TB4/USB4 | Result |
|---|---|
| Yes | Symmetric PCIe-tunneled peer link (e.g. macOS/Windows/Linux Thunderbolt networking). No host/device asymmetry, no hardware bridge, pure software. High bandwidth (10–40 Gbps), sub-millisecond latency. |
| No, but both support USB OTG dual-role | Roles negotiated automatically via USB-C Dual Role Port mechanism, or a simple convention (host-only side always wins host role, e.g. a Mac connected to a Pi). No hardware bridge. |
| No — both sides are host-only with no shared TB/USB4 | Hard physical dead end. Neither side can act as a USB device, and there is no peer-networking mechanism available. **Requires a physical bridge device** with two independent USB device-controller endpoints, one per host, relaying between them internally. Out of scope for v1 (§3). |

v1 targets the first case exclusively: two Thunderbolt/USB4-equipped
Macs. This proves out all five architecture layers without requiring any
hardware to be designed or built. The OTG dual-role and physical-bridge
cases are designed for in the protocol (transport is abstracted behind
layer 2) but implemented in later phases.

### 5.1 Mixed-platform links (e.g. Mac ↔ Raspberry Pi)

Every link this protocol supports — Thunderbolt/USB4 peer networking, USB
OTG gadget networking, plain Ethernet — presents itself to layers 2+ as
nothing more than **an interface carrying a self-assigned IPv6 link-local
address**. Layer 1 does not need to know or care which physical media
produced that interface, and the two ends of a link do not need to be
running the same transport underneath, as long as each side's interface
carries IP. This is what makes mixed-platform topologies "just work" in
software with no new transport code:

- **Mac ↔ Mac:** Thunderbolt/USB4 on both ends — the primary v1 target,
  §5 case 1.
- **Mac ↔ Raspberry Pi (no shared Thunderbolt):** the Pi is forced into
  USB gadget/Ethernet mode (`g_ether`) over its OTG-capable port, which
  creates a `usb0`-style interface on both ends — §5 case 2, still no
  hardware bridge required, since every current Pi model supports gadget
  mode. Bandwidth is USB 2.0-bounded (~200–300 Mbps realistic) since the
  OTG controllers on Pi 4/5's USB-C port and the Zero/Zero 2 W's
  micro-USB port are USB 2.0, not USB 3.
- **Mac ↔ Raspberry Pi over Ethernet:** if the Mac has no shared
  Thunderbolt with the peer, a standard USB-C-to-Gigabit-Ethernet adapter
  on the Mac plus a plain Ethernet cable to the Pi's Ethernet port is
  **higher bandwidth and lower latency than the USB gadget path** on any
  Pi model with a real Ethernet controller (Pi 4B/5, CM4/5 on the right
  carrier) — those boards' Ethernet MAC is not behind the USB controller
  (unlike Pi 3B+), so it reaches close to the real ~940 Mbps Gigabit
  ceiling. The Mac's USB-C-Ethernet adapter enumerates under the same
  `en*` naming as its Thunderbolt interface, so **no code distinguishes
  this case from case 1** — it is literally the same interface-matching
  logic. Both ends self-assign fe80:: link-local addresses with no
  DHCP/router present, identically to the Thunderbolt case.
- **Pi ↔ Pi over Ethernet:** the simplest and highest-bandwidth Pi-side
  topology when both boards have Ethernet jacks — a direct cable between
  two Ethernet ports (auto-MDIX handles straight-through vs. crossover on
  any modern port, no special cable needed), same link-local
  self-addressing, same discovery/mux/clock code unmodified.

The only per-platform work this requires is teaching layer 1's interface
matcher the naming conventions each OS uses for these interfaces (macOS:
`bridge*`/`en*`; Linux: `eth*`/`enp*`/`eno*`/`ens*`) — implemented in
`omniplex-discovery`'s `is_point_to_point_capable`. Everything from the
transport layer up (QUIC, HELLO negotiation, session clock, channel mux,
delivery-class semantics) is identical across every row of this list; none
of it is aware of what physical link produced the interface it's running
over.

## 6. Transport: QUIC, and Why

The v1 transport is **QUIC** (via the `quinn` Rust crate) over the
Thunderbolt/USB4 virtual network interface — not a plain TCP or UDP
socket. This is the load-bearing decision for "multiple data types
simultaneously with low latency," for one structural reason:

**A single TCP stream cannot keep the simultaneity promise.** Once a 2 MB
video frame is queued in a TCP send buffer, a MIDI message serializes
behind it — head-of-line blocking that no userspace scheduler can undo.
QUIC gives, over one connection:

- **Independent streams per channel** — loss or queuing on the video
  stream never stalls the MIDI stream. This is the mechanism behind the
  §2 guarantee, not just a policy.
- **Unreliable datagrams** — for audio/video payloads where a late packet
  is worse than a lost one (retransmitting a stale video slice wastes the
  exact bandwidth and time we're trying to protect).
- **Stream prioritization** — the muxer assigns each channel's stream a
  priority from its declared latency class (§9), and latency-critical
  streams are serviced first at every send opportunity.
- For free: connection establishment with version negotiation (ALPN),
  keep-alives and loss detection (feeds link-loss handling, §10), path
  MTU discovery, and encryption (satisfying §3's "privacy by construction"
  with zero configuration — self-signed certs are auto-generated and not
  verified in v1).

**Fragmentation rule (spec-level, mandatory):** no frame on the wire may
exceed **16 KB**; bulk payloads (video frames, large blobs) are sliced
into fragments at or below this bound and interleaved. Combined with
strict priority servicing, this bounds the worst-case delay a MIDI/OSC/DMX
message can experience behind bulk traffic to *one fragment's
serialization time* — microseconds at Thunderbolt speeds. This bound is a
protocol guarantee that any conforming implementation must uphold.

**Congestion/pacing:** on a dedicated point-to-point link, the default
internet congestion controller is too timid and too bursty. v1 tunes for
the measured link: bandwidth is probed at session start and re-estimated
continuously (§11 metrics feed this), bulk channels are paced smoothly to
their allocation instead of bursting, and latency-critical channels are
exempt from pacing (they're tiny; they always go now).

Later transports (raw USB OTG endpoints, Phase 3) implement the same
abstract interface layer 3 depends on: ordered streams, unreliable
datagrams, priority servicing, and the 16 KB fragment bound.

## 7. Discovery & Session Establishment

Zero-config, fully automatic, no user input:

1. **Link watch:** layer 1 watches for the Thunderbolt/USB4 virtual
   network interface appearing (cable plugged in). Both ends
   self-assign IPv6 link-local addresses — this happens at the OS level
   with no DHCP and no router, which is what makes the no-infrastructure
   promise physically true.
2. **Announce & find:** each end announces itself via **mDNS/DNS-SD on
   that interface only** (service type `_omniplex._udp.local`, TXT records
   carrying protocol version range and instance ID) and simultaneously
   browses for the same service. Scoping mDNS to the point-to-point
   interface means discovery cannot leak onto or be confused by Wi-Fi/LAN.
3. **Deterministic dialer election:** both ends see each other at
   effectively the same moment; the peer with the lexically higher
   instance ID dials, the other listens. No user choice, no race, no
   double connection.
4. **Hello & negotiation:** the first stream on the QUIC connection
   carries a `HELLO` exchange: highest mutually-supported protocol
   version (or clean, explicit failure — never silent misinterpretation),
   channel catalogs offered by each side, and session-clock role election
   (§8).
5. **Steady state:** channels open/close dynamically on their own streams
   for the life of the session. Cable pulled → QUIC loss detection fires
   within ~200 ms → session torn down cleanly and layer 5 shows the state
   change; re-plug → discovery re-runs from step 1 and channels resume
   automatically with the same channel names.

Target: **cable-in to first media flowing in under 2 seconds**, with no
dialog boxes on either machine.

## 8. Session Clock & Media Synchronization

This is a first-class protocol component, not a channel type. Two machines'
sample clocks *will* drift (tens of ppm — an audio glitch every few
minutes if ignored), and "multiple media types simultaneously" is only
awesome if they stay aligned. The design:

- **Clock election:** during `HELLO`, one side is elected session-clock
  master (deterministic, same election as §7 step 3 unless overridden).
- **Clock discipline:** the follower runs a continuous PTP-style two-way
  timestamp exchange over a dedicated latency-critical stream. On a
  point-to-point TB link (symmetric, sub-ms, no switches) this converges
  to **tens-of-microseconds alignment** — a dedicated cable is a *better*
  environment for this than Dante's switched networks.
- **Timestamps everywhere:** every media frame (audio, video, DMX,
  timecode) carries a timestamp in session-clock time. Receivers render
  at `timestamp + presentation offset`, where the offset per channel is
  the minimum that measured jitter allows (adaptive, starts conservative,
  shrinks as the link proves stable).
- **Audio drift handling:** the receiving side's virtual audio device
  cannot literally run on the sender's crystal, so the audio channel
  specifies a small jitter buffer plus **asynchronous sample-rate
  conversion** disciplined by the session clock — glitch-free playback
  indefinitely, not just until the buffers drift apart. (Where the OS
  allows slaving the virtual device's clock directly, that's preferred
  and the ASRC becomes a no-op.)
- **Cross-media alignment:** because audio, video, and lighting all
  timestamp against the same session clock, lip-sync and light-sync are
  a rendering-offset choice, not a hope.

## 9. Channel Catalog & Delivery Semantics

The channel protocol is extensible — new channel types are defined by
declaring their **delivery class + latency class + format descriptor**,
not by changing the core protocol. Three delivery classes, mapped directly
onto transport primitives:

| Delivery class | Transport mapping | Behavior |
|---|---|---|
| **Reliable-ordered** | QUIC stream | Every byte arrives, in order. For control/data where loss is corruption. |
| **Unreliable-timestamped** | QUIC datagrams | Late = dropped; receiver renders by timestamp. For continuous media where a stale packet is worthless. |
| **Latest-value-wins** | QUIC datagrams + sender-side replacement | Only the newest state matters; a queued stale value is *replaced*, never delivered late. For state-shaped data. |

v1 channel types:

- **MIDI** — reliable-ordered, latency-critical. Tiny messages; a lost
  note-off is a stuck note, so reliability is required.
- **OSC** — sender chooses per-address: latest-value-wins for continuous
  controls (faders, XY pads), reliable-ordered for bangs/triggers.
  Latency-critical either way.
- **Serial** — reliable-ordered byte-stream, moderate latency tolerance.
- **Audio** — unreliable-timestamped PCM, latency-critical. Fixed-size
  packets of N sample-frames; format (sample rate, bit depth, channel
  count) negotiated at channel open. Clock discipline per §8. Packet loss
  concealment (repeat/fade) on the rare drop rather than retransmission.
- **Video** — unreliable-timestamped, highest bandwidth, most
  latency-tolerant of the real-time channels. Frames sliced per §6's
  fragment bound. Format negotiated at channel open: raw pixel formats
  when bandwidth allows (TB usually does — this is the "no lossy
  conversion" goal honored), with an optional intra-only codec profile
  for when it doesn't. A frame missing fragments past its deadline is
  discarded whole; the previous frame persists.
- **DMX / lighting** — latest-value-wins per universe, latency-critical.
  This is the correct semantic for lighting: a stale universe snapshot
  must be replaced by a newer one, never queued behind it.
- **Timecode** — unreliable-timestamped carriage of MTC/LTC-style
  positional time, riding on the session clock (§8) for phase accuracy.
- **Raw / passthrough** — escape hatch for arbitrary binary data; the
  publisher declares which delivery class it needs. New use cases don't
  require a protocol version bump to experiment.

Each channel also declares a **latency class** (`latency-critical` /
`interactive` / `bulk`) which the muxer maps to transport priority and
pacing exemption (§6), and the bandwidth-degradation order (§10).

**Backpressure (per delivery class):** if a subscriber can't drain fast
enough — a slow virtual-device consumer, a stalled app — reliable-ordered
channels apply backpressure to their publisher (SDK write blocks or errors,
publisher's choice); unreliable and latest-value channels drop-oldest at
the receiver, bounded buffer, and the drop is counted in metrics (§11).
A slow video consumer can never stall MIDI, and can never grow memory
without bound.

## 10. Error Handling & Resilience

- **Link loss:** QUIC loss detection surfaces cable-pull within ~200 ms;
  layer 1 tears the session down and reports a clear state change to
  layer 5 — never a silent hang. Reconnect is automatic (§7 step 5) and
  channels resume by name.
- **Bandwidth exceeded:** if declared channel demand exceeds measured link
  capacity, layer 3 degrades strictly by latency class — video reduces
  resolution/framerate/switches to its codec profile first; bulk raw
  channels are throttled next; latency-critical channels (MIDI/OSC/DMX/
  clock) are **never** delayed or dropped for bandwidth reasons. The whole
  link never fails because one channel over-asked.
- **Version mismatch:** negotiated in `HELLO` via ALPN + version range;
  two implementations settle on the highest mutually-supported version or
  fail with an explicit, user-visible error — never silent
  misinterpretation of frames.
- **Malformed peer:** a conforming implementation treats any frame that
  violates the spec (oversized fragment, unknown critical field) as a
  channel-level error, not a session-level one, where possible: kill the
  offending channel, keep the session.

## 11. Instrumentation & Evaluation

Built into the protocol from day one, not bolted on — both because the
adaptive muxer needs live measurements to allocate bandwidth (§6), and
because the project's claims are only credible if they're measured.

**Always-on metrics (per channel, exposed by SDK and shown in the app):**
one-way latency (session clock makes this directly measurable — a rare
luxury), jitter distribution, loss/drop counts by cause (late vs. lost
vs. backpressure), bandwidth share, and clock-discipline error.

**Evaluation plan (the headline numbers):**
- **MIDI-under-load:** MIDI round-trip latency distribution while video
  saturates the link. Target: the distribution is indistinguishable from
  MIDI on an idle link. This is the graph that proves the architecture.
- **Sync accuracy:** measured audio/video/DMX alignment across the link
  vs. session-clock error, over hours.
- **Stability soak:** 24 h all-channels-running session with zero audio
  glitches and bounded memory.
- **Baselines:** same hardware, compared against rtpMIDI + NDI running
  side-by-side over the same TB interface (the "naive stack of existing
  protocols" alternative), and against Dante on audio latency/sync. The
  unified protocol must measurably beat the naive stack on
  latency-under-load, sync alignment, and setup time — that delta is the
  publishable result.

**Hardware-loopback verification:** headline latency numbers are measured
externally (audio loopback cable, photodiode-on-screen for video, DMX
capture), not self-reported by the software under test.

## 12. Technology Choices

- **Core (layers 1–4):** Rust. Matches C-level performance and low-level
  I/O control (QUIC via `quinn`, raw USB later via `nusb`, audio via
  `cpal`, MIDI via `midir`) with memory safety that matters for code
  juggling many concurrent real-time buffers — a use-after-free or
  overrun in muxing code is a correctness and security bug, not just a
  crash. One codebase across macOS/Windows/Linux.
- **Threading model — control plane vs. media plane:** `tokio` async runs
  the control plane (discovery, negotiation, channel management, metrics)
  where its scheduling model fits. **Real-time media does not flow
  through async tasks:** audio moves between the transport and the
  OS audio callback via lock-free SPSC ring buffers, with the callback
  side allocation-free and lock-free (no mutexes, no `async`, no
  allocation on the hot path). An async executor makes no scheduling-
  latency guarantees, and a dropout caused by a busy executor is
  indistinguishable to the user from a broken protocol. Video and other
  bulk channels use dedicated paced sender threads fed by the same
  ring-buffer discipline.
- **Zero-copy hot path:** media payloads are written once into
  transmit buffers and referenced, not copied, through the mux layer;
  received fragments assemble in place. At 10–40 Gbps, per-copy CPU cost
  is the difference between "works" and "works with headroom."
- **OS-specific glue:** kept thin, wrapped behind an FFI boundary the Rust
  core calls into — macOS CoreAudio Audio Server Plugin / DriverKit for
  virtual audio/camera devices, Windows WDF for virtual drivers. This is
  unavoidable (no Rust bindings exist for these driver frameworks) but
  scoped narrowly to "expose this channel as an OS-native virtual device,"
  not general logic.
- **Flagship UI (layer 5):** Tauri (Rust backend + web-based frontend) —
  keeps the real-time engine native Rust while allowing a modern, polished
  UI without an Electron-weight runtime wrapping the whole system, and
  without re-implementing the UI once per platform.
- **Protocol spec:** written and published independently of any single
  implementation (plain document + reference test vectors + a conformance
  test suite that any implementation can run against), so third parties
  can implement it in other languages without depending on the Rust SDK.

## 13. Testing Strategy

- Layers 1–4 (Rust core) are testable without any OS-specific virtual
  device work: discovery, negotiation, clock discipline, and muxing logic
  are exercised against a loopback/simulated transport in unit and
  integration tests — including simulated adverse conditions (injected
  loss, jitter, bandwidth caps, clock drift) so the resilience behaviors
  in §10 are tested continuously, not just observed anecdotally.
- The 16 KB fragment bound, priority servicing, and delivery-class
  semantics (§6, §9) are validated by conformance tests derived from the
  spec's reference test vectors.
- Layer 5 (virtual devices) needs manual/OS-level verification per
  platform (does the virtual MIDI port actually show up in a DAW, does
  the virtual camera show up in a video app) since OS driver frameworks
  are not meaningfully unit-testable.
- End-to-end validation for v1: two Macs connected via Thunderbolt/USB4,
  running the companion app, exchanging live MIDI/OSC/serial/audio/video
  **simultaneously**, observed in real creative tools (a DAW for
  audio/MIDI, an OSC monitor, a video app), with the §11 metrics
  recorded during the session.

## 14. Phased Scope

1. **Phase 1 (this spec's implementation target):** Rust core for layers
   1–3 over QUIC on Thunderbolt/USB4 peer networking between two Macs:
   discovery (§7), session clock (§8), channel catalog and delivery
   semantics (§9), always-on metrics (§11). Minimal companion app (not
   yet the full flagship UI) to prove the pipeline end-to-end and run the
   MIDI-under-load and sync-accuracy measurements.
2. **Phase 2:** SDK (layer 4) opens the protocol to third-party
   publish/subscribe use; flagship UI (layer 5) polish; full evaluation
   suite and baseline comparisons (§11).
3. **Phase 3:** USB OTG dual-role transport implementing the layer-2
   abstract interface over raw USB endpoints (including Raspberry Pi as a
   device-side target).
4. **Phase 4:** Physical bridge hardware for host-only-to-host-only links,
   if still needed once phases 1–3 are proven; optional pairing/auth
   extension (§3) if usage moves beyond trusted point-to-point setups.
5. **Phase 5:** Windows/Linux companion app parity.

Phase 1 is the scope for the implementation plan that follows this
design.
