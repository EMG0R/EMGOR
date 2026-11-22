//! HELLO and control payloads (spec §7, §10), serialized with `postcard`.

use serde::{Deserialize, Serialize};

use crate::types::{ChannelDescriptor, ChannelId};

/// First message from each side after the transport connects (spec §7).
#[derive(Clone, PartialEq, Debug, Serialize, Deserialize)]
pub struct Hello {
    pub version_min: u16,
    pub version_max: u16,
    pub instance_id: [u8; 16],
    pub channels: Vec<ChannelDescriptor>,
}

/// Response to [`Hello`], carrying the negotiated version.
#[derive(Clone, PartialEq, Debug, Serialize, Deserialize)]
pub struct HelloAck {
    pub version: u16,
    pub clock_master: bool,
    pub channels: Vec<ChannelDescriptor>,
}

/// Request to open a channel.
#[derive(Clone, PartialEq, Debug, Serialize, Deserialize)]
pub struct ChannelOpen {
    pub desc: ChannelDescriptor,
}

/// Accept/reject response to [`ChannelOpen`].
#[derive(Clone, PartialEq, Debug, Serialize, Deserialize)]
pub struct ChannelOpenAck {
    pub id: ChannelId,
    pub accepted: bool,
    pub reason: Option<String>,
}

/// Follower → master clock probe. `t1_ns` is the sender's local send time.
#[derive(Clone, Copy, PartialEq, Eq, Debug, Serialize, Deserialize)]
pub struct ClockProbe {
    pub t1_ns: u64,
}

/// Master → follower reply: echoed t1, master receive time, master send time.
#[derive(Clone, Copy, PartialEq, Eq, Debug, Serialize, Deserialize)]
pub struct ClockReply {
    pub t1_ns: u64,
    pub t2_ns: u64,
    pub t3_ns: u64,
}

/// Version negotiation (spec §10): highest version in both `(min, max)`
/// ranges, or `None` for a clean failure.
pub fn negotiate(a: (u16, u16), b: (u16, u16)) -> Option<u16> {
    let low = a.0.max(b.0);
    let high = a.1.min(b.1);
    if low <= high {
        Some(high)
    } else {
        None
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::types::{
        ChannelKind, DeliveryClass, FormatDescriptor, LatencyClass, PixelFormat, SampleFormat,
    };

    pub(crate) fn sample_channels() -> Vec<ChannelDescriptor> {
        vec![
            ChannelDescriptor {
                id: ChannelId(2),
                name: "midi/1".into(),
                kind: ChannelKind::Midi,
                delivery: DeliveryClass::ReliableOrdered,
                latency: LatencyClass::LatencyCritical,
                format: FormatDescriptor::Midi,
            },
            ChannelDescriptor {
                id: ChannelId(4),
                name: "audio/main".into(),
                kind: ChannelKind::Audio,
                delivery: DeliveryClass::UnreliableTimestamped,
                latency: LatencyClass::LatencyCritical,
                format: FormatDescriptor::Audio {
                    sample_rate: 48_000,
                    channels: 2,
                    format: SampleFormat::F32,
                },
            },
            ChannelDescriptor {
                id: ChannelId(6),
                name: "video/test".into(),
                kind: ChannelKind::Video,
                delivery: DeliveryClass::UnreliableTimestamped,
                latency: LatencyClass::Bulk,
                format: FormatDescriptor::Video {
                    width: 1280,
                    height: 720,
                    fps_num: 30,
                    fps_den: 1,
                    pixel: PixelFormat::Uyvy,
                },
            },
        ]
    }

    #[test]
    fn hello_round_trip() {
        let hello = Hello {
            version_min: 1,
            version_max: 1,
            instance_id: [0xAB; 16],
            channels: sample_channels(),
        };
        let bytes = postcard::to_allocvec(&hello).unwrap();
        let back: Hello = postcard::from_bytes(&bytes).unwrap();
        assert_eq!(back, hello);
    }

    #[test]
    fn hello_ack_round_trip() {
        let ack = HelloAck {
            version: 1,
            clock_master: true,
            channels: sample_channels(),
        };
        let bytes = postcard::to_allocvec(&ack).unwrap();
        let back: HelloAck = postcard::from_bytes(&bytes).unwrap();
        assert_eq!(back, ack);
    }

    #[test]
    fn channel_open_and_ack_round_trip() {
        let open = ChannelOpen {
            desc: sample_channels().remove(0),
        };
        let bytes = postcard::to_allocvec(&open).unwrap();
        assert_eq!(postcard::from_bytes::<ChannelOpen>(&bytes).unwrap(), open);

        for ack in [
            ChannelOpenAck {
                id: ChannelId(2),
                accepted: true,
                reason: None,
            },
            ChannelOpenAck {
                id: ChannelId(9),
                accepted: false,
                reason: Some("unsupported format".into()),
            },
        ] {
            let bytes = postcard::to_allocvec(&ack).unwrap();
            assert_eq!(postcard::from_bytes::<ChannelOpenAck>(&bytes).unwrap(), ack);
        }
    }

    #[test]
    fn clock_probe_reply_round_trip() {
        let probe = ClockProbe { t1_ns: 123_456_789 };
        let bytes = postcard::to_allocvec(&probe).unwrap();
        assert_eq!(postcard::from_bytes::<ClockProbe>(&bytes).unwrap(), probe);

        let reply = ClockReply {
            t1_ns: 123_456_789,
            t2_ns: 123_500_000,
            t3_ns: 123_600_000,
        };
        let bytes = postcard::to_allocvec(&reply).unwrap();
        assert_eq!(postcard::from_bytes::<ClockReply>(&bytes).unwrap(), reply);
    }

    #[test]
    fn corrupt_control_payload_fails_cleanly() {
        let hello = Hello {
            version_min: 1,
            version_max: 1,
            instance_id: [0; 16],
            channels: sample_channels(),
        };
        let mut bytes = postcard::to_allocvec(&hello).unwrap();
        bytes.truncate(bytes.len() / 2);
        assert!(postcard::from_bytes::<Hello>(&bytes).is_err());
    }

    #[test]
    fn negotiate_overlap_picks_highest() {
        assert_eq!(negotiate((1, 1), (1, 1)), Some(1));
        assert_eq!(negotiate((1, 3), (2, 5)), Some(3));
        assert_eq!(negotiate((2, 5), (1, 3)), Some(3)); // symmetric
        assert_eq!(negotiate((1, 4), (1, 4)), Some(4));
        assert_eq!(negotiate((2, 2), (1, 3)), Some(2));
    }

    #[test]
    fn negotiate_no_overlap_is_none() {
        assert_eq!(negotiate((2, 3), (1, 1)), None);
        assert_eq!(negotiate((1, 1), (2, 3)), None);
    }
}
