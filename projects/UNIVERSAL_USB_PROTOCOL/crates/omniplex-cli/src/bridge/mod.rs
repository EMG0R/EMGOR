//! Device <-> channel bridges (Milestone 7). Pure, hardware-free logic lives
//! in `audio_core`, `video`, `osc`, and the generic pumps in `serial`; the
//! other modules wire real devices to session channels.

// Pure logic: no session/mux/device dependency beyond generic std::io traits
// (serial) — always compiled, always unit-testable.
pub mod audio_core;
pub mod osc;
pub mod screen_capture;
pub mod serial;
pub mod video;

// Device <-> channel wiring: needs `omniplex-mux`/`omniplex-clock` channel types, so it
// only compiles under the `wiring` feature (see crate Cargo.toml).
#[cfg(feature = "wiring")]
pub mod audio;
#[cfg(feature = "wiring")]
pub mod midi;

use omniplex_wire::{
    ChannelDescriptor, ChannelId, ChannelKind, DeliveryClass, FormatDescriptor, LatencyClass,
    PixelFormat, SampleFormat,
};

/// Channel id placeholder in outgoing descriptors: the mux allocates the
/// real id during ChannelOpen (dialer even / listener odd).
const UNASSIGNED: ChannelId = ChannelId(0);

pub fn midi_desc() -> ChannelDescriptor {
    ChannelDescriptor {
        id: UNASSIGNED,
        name: "midi/1".into(),
        kind: ChannelKind::Midi,
        delivery: DeliveryClass::ReliableOrdered,
        latency: LatencyClass::LatencyCritical,
        format: FormatDescriptor::Midi,
    }
}

pub fn osc_reliable_desc() -> ChannelDescriptor {
    ChannelDescriptor {
        id: UNASSIGNED,
        name: "osc/1".into(),
        kind: ChannelKind::Osc,
        delivery: DeliveryClass::ReliableOrdered,
        latency: LatencyClass::Interactive,
        format: FormatDescriptor::Osc,
    }
}

pub fn osc_lv_desc() -> ChannelDescriptor {
    ChannelDescriptor {
        id: UNASSIGNED,
        name: "osc/lv".into(),
        kind: ChannelKind::Osc,
        delivery: DeliveryClass::LatestValueWins,
        latency: LatencyClass::Interactive,
        format: FormatDescriptor::Osc,
    }
}

pub fn serial_desc(baud_hint: Option<u32>) -> ChannelDescriptor {
    ChannelDescriptor {
        id: UNASSIGNED,
        name: "serial/1".into(),
        kind: ChannelKind::Serial,
        delivery: DeliveryClass::ReliableOrdered,
        latency: LatencyClass::Interactive,
        format: FormatDescriptor::Serial { baud_hint },
    }
}

pub fn audio_desc() -> ChannelDescriptor {
    ChannelDescriptor {
        id: UNASSIGNED,
        name: "audio/main".into(),
        kind: ChannelKind::Audio,
        delivery: DeliveryClass::UnreliableTimestamped,
        latency: LatencyClass::LatencyCritical,
        format: FormatDescriptor::Audio {
            sample_rate: audio_core::SAMPLE_RATE,
            channels: audio_core::CHANNELS as u16,
            format: SampleFormat::F32,
        },
    }
}

/// Build a video channel descriptor. `pixel` and `name` are caller-supplied
/// so this serves both the synthetic test pattern (`"video/test"`,
/// `PixelFormat::Uyvy`) and real screen capture (`"video/screen"`,
/// `PixelFormat::Bgra`) — see `run.rs`'s `spawn_video_sender` and
/// `spawn_screen_capture_sender`.
pub fn video_desc(mode: video::VideoMode, pixel: PixelFormat, name: &str) -> ChannelDescriptor {
    ChannelDescriptor {
        id: UNASSIGNED,
        name: name.to_string(),
        kind: ChannelKind::Video,
        delivery: DeliveryClass::UnreliableTimestamped,
        latency: LatencyClass::Bulk,
        format: FormatDescriptor::Video {
            width: mode.width,
            height: mode.height,
            fps_num: mode.fps_num,
            fps_den: mode.fps_den,
            pixel,
        },
    }
}
