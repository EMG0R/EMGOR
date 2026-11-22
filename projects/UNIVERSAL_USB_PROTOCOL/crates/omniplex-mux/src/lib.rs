//! Channel muxer for the Omniplex protocol (plan Milestone 5).
//!
//! Owns channel lifecycle (open/close handshake over the control stream),
//! the reliable-ordered fragmentation/reassembly path (one transport stream
//! per channel), the unreliable-timestamped and latest-value-wins datagram
//! paths (demultiplexed by channel id), and the send scheduler that
//! enforces strict latency-class priority plus bandwidth-aware pacing for
//! bulk traffic.

mod reliable;
mod scheduler;
mod unreliable;

pub mod registry;

pub use registry::{Mux, MuxError, Role};
pub use scheduler::MuxEvent;

pub use omniplex_wire::{
    ChannelDescriptor, ChannelId, ChannelKind, DeliveryClass, FormatDescriptor, LatencyClass,
    SessionTimestamp, MAX_FRAGMENT_PAYLOAD,
};

use bytes::Bytes;

/// Sending half of an open channel. The concrete behaviour (fragmentation,
/// backpressure, replace-on-send) depends on the channel's [`DeliveryClass`].
pub enum ChannelTx {
    Reliable(reliable::ReliableTx),
    Unreliable(unreliable::UnreliableTx),
    Latest(unreliable::LatestTx),
}

impl ChannelTx {
    /// Send one message, timestamped in session-clock time.
    ///
    /// - `ReliableOrdered`: fragments to `MAX_FRAGMENT_PAYLOAD`; awaits
    ///   (backpressures) when the 64-message send buffer is full.
    /// - `UnreliableTimestamped`: fragments to the transport's datagram
    ///   bound and enqueues on the scheduler; never blocks.
    /// - `LatestValueWins`: replaces any not-yet-sent pending value.
    pub async fn send(&self, payload: Bytes, ts: SessionTimestamp) -> Result<(), MuxError> {
        match self {
            ChannelTx::Reliable(t) => t.send(payload, ts).await,
            ChannelTx::Unreliable(t) => t.send(payload, ts).await,
            ChannelTx::Latest(t) => t.send(payload, ts).await,
        }
    }
}

/// Receiving half of an open channel.
pub enum ChannelRx {
    Reliable(reliable::ReliableRx),
    Unreliable(unreliable::UnreliableRx),
    Latest(unreliable::LatestRx),
}

impl ChannelRx {
    /// Receive the next reassembled message. `None` once the channel is
    /// closed and no further messages are buffered.
    pub async fn recv(&mut self) -> Option<(Bytes, SessionTimestamp)> {
        match self {
            ChannelRx::Reliable(r) => r.recv().await,
            ChannelRx::Unreliable(r) => r.recv().await,
            ChannelRx::Latest(r) => r.recv().await,
        }
    }
}

/// Parse one length-prefixed wire frame off the front of `acc`, if a full
/// frame is present. Shared by the control task, the reliable per-channel
/// task, and the stream-acceptor's bind-frame reader.
pub(crate) fn try_take_frame(
    acc: &mut Vec<u8>,
) -> Result<Option<(omniplex_wire::FrameHeader, Vec<u8>)>, omniplex_wire::WireError> {
    use omniplex_wire::{decode_frame, FRAME_HEADER_LEN};
    if acc.len() < FRAME_HEADER_LEN {
        return Ok(None);
    }
    let payload_len = u16::from_le_bytes([acc[20], acc[21]]) as usize;
    let total = FRAME_HEADER_LEN + payload_len;
    if acc.len() < total {
        return Ok(None);
    }
    let frame: Vec<u8> = acc.drain(..total).collect();
    match decode_frame(&frame) {
        Ok((header, payload)) => Ok(Some((header, payload.to_vec()))),
        Err(e) => Err(e),
    }
}

/// Number of `max_frag`-sized chunks needed to carry `len` bytes. Always at
/// least 1 (an empty message is one empty fragment).
pub(crate) fn frag_count_for(len: usize, max_frag: usize) -> u16 {
    let max_frag = max_frag.max(1);
    if len == 0 {
        1
    } else {
        len.div_ceil(max_frag) as u16
    }
}

/// The `i`-th chunk (zero-copy) of `payload`, `max_frag` bytes at a time.
pub(crate) fn chunk_of(payload: &Bytes, i: u16, max_frag: usize) -> Bytes {
    let start = i as usize * max_frag;
    let end = (start + max_frag).min(payload.len());
    if start >= payload.len() {
        Bytes::new()
    } else {
        payload.slice(start..end)
    }
}
