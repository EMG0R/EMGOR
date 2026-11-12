//! Wire frame encoding/decoding (spec §6).
//!
//! Layout, little-endian, fixed 22-byte header then payload:
//!
//! ```text
//! u16 channel_id | u8 frame_type | u8 flags | u64 timestamp_ns
//! | u32 seq | u16 frag_index | u16 frag_count | u16 payload_len | payload…
//! ```

use crate::types::{ChannelId, SessionTimestamp, MAX_FRAGMENT_PAYLOAD};

/// Size of the fixed frame header in bytes.
pub const FRAME_HEADER_LEN: usize = 22;

/// `flags` bit 0: this frame carries the last fragment of a message.
pub const FLAG_END_OF_MESSAGE: u8 = 0b0000_0001;

/// Frame type discriminant carried in the header.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
#[repr(u8)]
pub enum FrameType {
    Hello = 0,
    HelloAck = 1,
    ChannelOpen = 2,
    ChannelOpenAck = 3,
    ChannelClose = 4,
    Media = 5,
    ClockProbe = 6,
    ClockReply = 7,
    Bye = 8,
}

impl FrameType {
    /// All frame types, in wire-discriminant order.
    pub const ALL: [FrameType; 9] = [
        FrameType::Hello,
        FrameType::HelloAck,
        FrameType::ChannelOpen,
        FrameType::ChannelOpenAck,
        FrameType::ChannelClose,
        FrameType::Media,
        FrameType::ClockProbe,
        FrameType::ClockReply,
        FrameType::Bye,
    ];

    pub fn from_u8(v: u8) -> Option<FrameType> {
        Self::ALL.get(v as usize).copied()
    }
}

/// Errors produced while encoding or decoding frames.
#[derive(Clone, PartialEq, Eq, Debug, thiserror::Error)]
pub enum WireError {
    /// Payload exceeds [`MAX_FRAGMENT_PAYLOAD`].
    #[error(
        "fragment payload of {len} bytes exceeds MAX_FRAGMENT_PAYLOAD ({MAX_FRAGMENT_PAYLOAD})"
    )]
    FragmentTooLarge { len: usize },
    /// Buffer ended before the header (or declared payload) was complete.
    #[error("buffer truncated: need {needed} bytes, have {have}")]
    Truncated { needed: usize, have: usize },
    /// Unknown `frame_type` discriminant. Carries the channel so the mux can
    /// kill just that channel and keep the session alive (spec §10).
    #[error("unknown frame type {value} on channel {}", channel.0)]
    UnknownFrameType { channel: ChannelId, value: u8 },
}

/// Decoded fixed header of a wire frame.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub struct FrameHeader {
    pub channel: ChannelId,
    pub frame_type: FrameType,
    pub flags: u8,
    pub timestamp: SessionTimestamp,
    pub seq: u32,
    pub frag_index: u16,
    pub frag_count: u16,
}

/// Encode a frame (header + payload) by appending to `out`.
pub fn encode_frame(h: &FrameHeader, payload: &[u8], out: &mut Vec<u8>) -> Result<(), WireError> {
    if payload.len() > MAX_FRAGMENT_PAYLOAD {
        return Err(WireError::FragmentTooLarge { len: payload.len() });
    }
    out.reserve(FRAME_HEADER_LEN + payload.len());
    out.extend_from_slice(&h.channel.0.to_le_bytes());
    out.push(h.frame_type as u8);
    out.push(h.flags);
    out.extend_from_slice(&h.timestamp.0.to_le_bytes());
    out.extend_from_slice(&h.seq.to_le_bytes());
    out.extend_from_slice(&h.frag_index.to_le_bytes());
    out.extend_from_slice(&h.frag_count.to_le_bytes());
    out.extend_from_slice(&(payload.len() as u16).to_le_bytes());
    out.extend_from_slice(payload);
    Ok(())
}

/// Decode one frame from the start of `buf`, returning the header and a
/// zero-copy slice of the payload.
pub fn decode_frame(buf: &[u8]) -> Result<(FrameHeader, &[u8]), WireError> {
    if buf.len() < FRAME_HEADER_LEN {
        return Err(WireError::Truncated {
            needed: FRAME_HEADER_LEN,
            have: buf.len(),
        });
    }
    let channel = ChannelId(u16::from_le_bytes([buf[0], buf[1]]));
    let raw_type = buf[2];
    let flags = buf[3];
    let timestamp = SessionTimestamp(u64::from_le_bytes(buf[4..12].try_into().unwrap()));
    let seq = u32::from_le_bytes(buf[12..16].try_into().unwrap());
    let frag_index = u16::from_le_bytes(buf[16..18].try_into().unwrap());
    let frag_count = u16::from_le_bytes(buf[18..20].try_into().unwrap());
    let payload_len = u16::from_le_bytes(buf[20..22].try_into().unwrap()) as usize;

    let frame_type = FrameType::from_u8(raw_type).ok_or(WireError::UnknownFrameType {
        channel,
        value: raw_type,
    })?;

    let needed = FRAME_HEADER_LEN + payload_len;
    if buf.len() < needed {
        return Err(WireError::Truncated {
            needed,
            have: buf.len(),
        });
    }

    Ok((
        FrameHeader {
            channel,
            frame_type,
            flags,
            timestamp,
            seq,
            frag_index,
            frag_count,
        },
        &buf[FRAME_HEADER_LEN..needed],
    ))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn header(ft: FrameType) -> FrameHeader {
        FrameHeader {
            channel: ChannelId(42),
            frame_type: ft,
            flags: FLAG_END_OF_MESSAGE,
            timestamp: SessionTimestamp(0x0102_0304_0506_0708),
            seq: 0xDEAD_BEEF,
            frag_index: 3,
            frag_count: 7,
        }
    }

    #[test]
    fn round_trip_every_frame_type() {
        for ft in FrameType::ALL {
            let h = header(ft);
            let payload = [1u8, 2, 3, 4, 5];
            let mut buf = Vec::new();
            encode_frame(&h, &payload, &mut buf).unwrap();
            assert_eq!(buf.len(), FRAME_HEADER_LEN + payload.len());
            let (h2, p2) = decode_frame(&buf).unwrap();
            assert_eq!(h2, h);
            assert_eq!(p2, payload);
        }
    }

    #[test]
    fn round_trip_empty_and_max_payload() {
        let h = header(FrameType::Media);
        for payload in [vec![], vec![0xAB; MAX_FRAGMENT_PAYLOAD]] {
            let mut buf = Vec::new();
            encode_frame(&h, &payload, &mut buf).unwrap();
            let (h2, p2) = decode_frame(&buf).unwrap();
            assert_eq!(h2, h);
            assert_eq!(p2, payload.as_slice());
        }
    }

    #[test]
    fn oversize_payload_rejected() {
        let h = header(FrameType::Media);
        let payload = vec![0u8; MAX_FRAGMENT_PAYLOAD + 1];
        let mut buf = Vec::new();
        assert_eq!(
            encode_frame(&h, &payload, &mut buf),
            Err(WireError::FragmentTooLarge {
                len: MAX_FRAGMENT_PAYLOAD + 1
            })
        );
        assert!(buf.is_empty());
    }

    #[test]
    fn truncated_header_rejected() {
        let h = header(FrameType::Media);
        let mut buf = Vec::new();
        encode_frame(&h, b"hello", &mut buf).unwrap();
        for cut in 0..FRAME_HEADER_LEN {
            assert_eq!(
                decode_frame(&buf[..cut]),
                Err(WireError::Truncated {
                    needed: FRAME_HEADER_LEN,
                    have: cut
                })
            );
        }
    }

    #[test]
    fn truncated_payload_rejected() {
        let h = header(FrameType::Media);
        let mut buf = Vec::new();
        encode_frame(&h, b"hello", &mut buf).unwrap();
        let cut = buf.len() - 2;
        assert_eq!(
            decode_frame(&buf[..cut]),
            Err(WireError::Truncated {
                needed: FRAME_HEADER_LEN + 5,
                have: cut
            })
        );
    }

    #[test]
    fn unknown_frame_type_reports_channel() {
        let h = header(FrameType::Media);
        let mut buf = Vec::new();
        encode_frame(&h, b"x", &mut buf).unwrap();
        buf[2] = 200; // corrupt frame_type
        assert_eq!(
            decode_frame(&buf),
            Err(WireError::UnknownFrameType {
                channel: ChannelId(42),
                value: 200
            })
        );
    }

    #[test]
    fn trailing_bytes_ignored() {
        let h = header(FrameType::ClockProbe);
        let mut buf = Vec::new();
        encode_frame(&h, b"abc", &mut buf).unwrap();
        buf.extend_from_slice(b"trailing-next-frame");
        let (h2, p2) = decode_frame(&buf).unwrap();
        assert_eq!(h2, h);
        assert_eq!(p2, b"abc");
    }

    #[test]
    fn header_is_little_endian_at_documented_offsets() {
        let h = FrameHeader {
            channel: ChannelId(0x0201),
            frame_type: FrameType::Media,
            flags: 0x01,
            timestamp: SessionTimestamp(0x1122_3344_5566_7788),
            seq: 0xA1B2_C3D4,
            frag_index: 0x0E0F,
            frag_count: 0x1011,
        };
        let mut buf = Vec::new();
        encode_frame(&h, b"\xFF", &mut buf).unwrap();
        assert_eq!(&buf[0..2], &[0x01, 0x02]); // channel LE
        assert_eq!(buf[2], 5); // Media
        assert_eq!(buf[3], 0x01); // flags
        assert_eq!(
            &buf[4..12],
            &[0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11]
        );
        assert_eq!(&buf[12..16], &[0xD4, 0xC3, 0xB2, 0xA1]);
        assert_eq!(&buf[16..18], &[0x0F, 0x0E]);
        assert_eq!(&buf[18..20], &[0x11, 0x10]);
        assert_eq!(&buf[20..22], &[0x01, 0x00]); // payload_len = 1
        assert_eq!(buf[22], 0xFF);
    }
}
