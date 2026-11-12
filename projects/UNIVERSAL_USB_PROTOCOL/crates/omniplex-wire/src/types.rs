//! Core protocol types (spec §6, §8, §9).

use serde::{Deserialize, Serialize};

/// Maximum payload bytes carried by a single wire fragment (spec §6, normative).
pub const MAX_FRAGMENT_PAYLOAD: usize = 16 * 1024;

/// ALPN string identifying protocol major version 1 (spec §7, §10).
pub const PROTOCOL_ALPN: &[u8] = b"omniplex/1";

/// How payloads on a channel are delivered (spec §9).
#[derive(Clone, Copy, PartialEq, Eq, Debug, Serialize, Deserialize)]
pub enum DeliveryClass {
    /// QUIC stream: reliable, ordered, backpressured.
    ReliableOrdered,
    /// Datagram: may be lost or reordered, timestamped.
    UnreliableTimestamped,
    /// Datagram + sender-side replacement of stale values.
    LatestValueWins,
}

/// Scheduling priority tier (spec §9, §10). Ordered: LatencyCritical < Interactive < Bulk,
/// i.e. lower `Ord` value = more urgent.
#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Debug, Serialize, Deserialize)]
pub enum LatencyClass {
    LatencyCritical,
    Interactive,
    Bulk,
}

/// Channel identifier. 0 = control, 1 = clock, ≥ 2 dynamically allocated.
#[derive(Clone, Copy, PartialEq, Eq, Hash, Debug, Serialize, Deserialize)]
pub struct ChannelId(pub u16);

impl ChannelId {
    /// Reserved control channel (HELLO, channel open/close, bye).
    pub const CONTROL: ChannelId = ChannelId(0);
    /// Reserved session-clock channel (probe/reply).
    pub const CLOCK: ChannelId = ChannelId(1);
    /// First id available for dynamically opened channels.
    pub const FIRST_DYNAMIC: ChannelId = ChannelId(2);

    /// True for the reserved control/clock ids.
    pub fn is_reserved(self) -> bool {
        self.0 < Self::FIRST_DYNAMIC.0
    }
}

/// A point in session-clock time, u64 nanoseconds (spec §8).
#[derive(Clone, Copy, PartialEq, Eq, Debug, Serialize, Deserialize)]
pub struct SessionTimestamp(pub u64);

/// What kind of data flows on a channel.
#[derive(Clone, Copy, PartialEq, Eq, Debug, Serialize, Deserialize)]
pub enum ChannelKind {
    Midi,
    Osc,
    Serial,
    Audio,
    Video,
    Dmx,
    Timecode,
    Raw,
}

/// Audio sample format. F32 only in Phase 1.
#[derive(Clone, Copy, PartialEq, Eq, Debug, Serialize, Deserialize)]
pub enum SampleFormat {
    F32,
}

/// Video pixel format supported in Phase 1.
#[derive(Clone, Copy, PartialEq, Eq, Debug, Serialize, Deserialize)]
pub enum PixelFormat {
    Uyvy,
    Bgra,
}

/// Per-kind format parameters carried in a channel descriptor.
#[derive(Clone, PartialEq, Debug, Serialize, Deserialize)]
pub enum FormatDescriptor {
    /// Raw MIDI 1.0 bytes.
    Midi,
    Osc,
    Serial {
        baud_hint: Option<u32>,
    },
    Audio {
        sample_rate: u32,
        channels: u16,
        format: SampleFormat,
    },
    Video {
        width: u32,
        height: u32,
        fps_num: u32,
        fps_den: u32,
        pixel: PixelFormat,
    },
    Dmx {
        universe: u16,
    },
    Timecode,
    Raw {
        delivery_hint: DeliveryClass,
    },
}

/// Everything a peer needs to know to accept and route a channel.
#[derive(Clone, PartialEq, Debug, Serialize, Deserialize)]
pub struct ChannelDescriptor {
    pub id: ChannelId,
    /// e.g. "midi/1", "audio/main", "video/cam"
    pub name: String,
    pub kind: ChannelKind,
    pub delivery: DeliveryClass,
    pub latency: LatencyClass,
    pub format: FormatDescriptor,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn reserved_channel_ids() {
        assert_eq!(ChannelId::CONTROL, ChannelId(0));
        assert_eq!(ChannelId::CLOCK, ChannelId(1));
        assert_eq!(ChannelId::FIRST_DYNAMIC, ChannelId(2));
        assert!(ChannelId::CONTROL.is_reserved());
        assert!(ChannelId::CLOCK.is_reserved());
        assert!(!ChannelId::FIRST_DYNAMIC.is_reserved());
        assert!(!ChannelId(100).is_reserved());
    }

    #[test]
    fn latency_class_ordering_most_urgent_first() {
        assert!(LatencyClass::LatencyCritical < LatencyClass::Interactive);
        assert!(LatencyClass::Interactive < LatencyClass::Bulk);
    }

    #[test]
    fn constants_match_spec() {
        assert_eq!(MAX_FRAGMENT_PAYLOAD, 16384);
        assert_eq!(PROTOCOL_ALPN, b"omniplex/1");
    }

    #[test]
    fn channel_descriptor_postcard_round_trip() {
        let desc = ChannelDescriptor {
            id: ChannelId(7),
            name: "audio/main".into(),
            kind: ChannelKind::Audio,
            delivery: DeliveryClass::UnreliableTimestamped,
            latency: LatencyClass::LatencyCritical,
            format: FormatDescriptor::Audio {
                sample_rate: 48_000,
                channels: 2,
                format: SampleFormat::F32,
            },
        };
        let bytes = postcard::to_allocvec(&desc).unwrap();
        let back: ChannelDescriptor = postcard::from_bytes(&bytes).unwrap();
        assert_eq!(back, desc);
    }
}
