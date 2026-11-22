# NEPTR video I/O profiles

The synth render graph has one internal resolution. Video transport is a
boundary concern: it may downscale, change frame rate, or change codec without
making Channel A, Channel B, jit.op, particles, or the master chain render at
different sizes.

## Output options

| transport | best use | quality/latency | network requirement |
|---|---|---|---|
| NDI High Bandwidth / SpeedHQ | local show, receiver wants NDI | high quality, low-ish latency, intra-frame | wired gigabit recommended; Wi-Fi is adaptive-only |
| NDI HX / HX3 | bandwidth-limited NDI receiver | efficient, more buffering and codec latency | Wi-Fi possible; depends on SDK/codec path |
| RTP/UDP H.264 or HEVC | controlled LAN, custom receiver | tunable latency and bitrate | Wi-Fi/LAN; needs receiver config |
| SRT or RIST | unreliable Wi-Fi, routed links | resilient, higher latency | Wi-Fi/WAN friendly; receiver must support it |
| WebRTC | browser/phone viewing | adaptive bitrate and congestion control | easiest browser path; highest implementation complexity |
| raw UYVY/RGB over UDP | lab or very low-latency wired path | no codec loss, extreme bandwidth | only reduced resolution or a network faster than gigabit |

Recommended output order:

1. Full NDI on wired Ethernet, with 1080p30 and 720p60 profiles.
2. SRT/RIST or RTP/HEVC for wireless and higher-latency links.
3. WebRTC if browser viewing becomes a first-class performance need.
4. Raw transport only as a measured diagnostic mode.

The encoder/readback path should be shared. A single boundary frame can feed
NDI, SRT/RIST, recording, and a preview without each consumer reading the GPU.

## Phone camera input

The input side is different from output: the phone is already good at encoding,
so compressed transport is preferable.

- RTSP/HEVC is the primary controlled path when an app can publish it.
- NDI HX is useful when discovery and an NDI workflow matter more than latency.
- SRT is useful for a phone app that can publish reliably over unstable Wi-Fi.
- WebRTC is attractive for browser-originated camera input, but should not be
  the first Pi integration because it adds a large dependency surface.
- USB/UVC is not assumed for iPhone; it is a separate Android/webcam fallback.

All inputs use the same `StreamSource` contract: a non-blocking latest-frame
read, timestamp, dimensions, connected state, and a dropout policy that holds
the last good frame instead of injecting black.

## Adaptive policy

The stream profile has independent width, height, fps, codec, bitrate, and
latency targets. Automatic adaptation changes the boundary profile in this
order:

1. drop output/input frame rate;
2. reduce boundary resolution;
3. reduce codec quality or bitrate;
4. only then ask the render governor to protect the unified internal graph.

HDMI output sizing is observed from the active `Scene` role. The internal graph
does not become a different resolution per HDMI connector; PRESENT scales the
unified result to each physical output. Manual stream settings override the
auto profile, and manual internal render settings override only the internal
governor—not the network boundary.

The boundary profile parser now accepts this explicit environment/API shape;
the transport adapters still consume the profile in a later phase:

```text
NV_IO_AUTO=0|1
NV_IO_WIDTH / NV_IO_HEIGHT / NV_IO_FPS
NV_IO_BITRATE / NV_IO_CODEC / NV_IO_LATENCY
NV_IO_TRANSPORT=ndi|rtp|srt|rist|webrtc|raw
```

These are profiles, not promises: every new transport must be measured on the
actual Pi and report dropped frames, encode/readback time, queue depth, and
effective bitrate.
