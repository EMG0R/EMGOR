//! `omniplex-wire` — the normative wire encoding of the Omniplex protocol
//! (design spec layers 1–3, §6–§10).
//!
//! Pure data: no I/O, no async. Reference test vectors live in `vectors/`
//! and are verified by `tests/vectors.rs`.

pub mod control;
pub mod frame;
pub mod types;

pub use control::{
    negotiate, ChannelOpen, ChannelOpenAck, ClockProbe, ClockReply, Hello, HelloAck,
};
pub use frame::{
    decode_frame, encode_frame, FrameHeader, FrameType, WireError, FLAG_END_OF_MESSAGE,
    FRAME_HEADER_LEN,
};
pub use types::{
    ChannelDescriptor, ChannelId, ChannelKind, DeliveryClass, FormatDescriptor, LatencyClass,
    PixelFormat, SampleFormat, SessionTimestamp, MAX_FRAGMENT_PAYLOAD, PROTOCOL_ALPN,
};
