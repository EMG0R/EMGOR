//! Reliable-ordered channel path (plan Task 5.2): one transport stream per
//! channel, fragmented to `MAX_FRAGMENT_PAYLOAD`, reassembled in order.

use bytes::Bytes;
use tokio::sync::{mpsc, watch};

use omniplex_metrics::MetricsHub;
use omniplex_transport::TransportStream;
use omniplex_wire::{
    encode_frame, ChannelId, FrameHeader, FrameType, SessionTimestamp, FLAG_END_OF_MESSAGE,
    MAX_FRAGMENT_PAYLOAD,
};

use crate::{chunk_of, frag_count_for, try_take_frame, MuxError};

/// Bound on the send-side and receive-side message queues (spec §9:
/// publisher-blocks backpressure).
pub const CHANNEL_QUEUE_CAP: usize = 64;

/// Sending half of a `ReliableOrdered` channel.
pub struct ReliableTx {
    pub(crate) tx: mpsc::Sender<(Bytes, SessionTimestamp)>,
}

impl ReliableTx {
    pub async fn send(&self, payload: Bytes, ts: SessionTimestamp) -> Result<(), MuxError> {
        self.tx
            .send((payload, ts))
            .await
            .map_err(|_| MuxError::Closed)
    }
}

/// Receiving half of a `ReliableOrdered` channel.
pub struct ReliableRx {
    pub(crate) rx: mpsc::Receiver<(Bytes, SessionTimestamp)>,
}

impl ReliableRx {
    pub async fn recv(&mut self) -> Option<(Bytes, SessionTimestamp)> {
        self.rx.recv().await
    }
}

/// Encode and write one message as one or more `Media` frames.
async fn send_message(
    stream: &mut dyn TransportStream,
    channel: ChannelId,
    seq: u32,
    ts: SessionTimestamp,
    payload: &Bytes,
) -> Result<(), ()> {
    let frag_count = frag_count_for(payload.len(), MAX_FRAGMENT_PAYLOAD);
    let mut out = Vec::new();
    for i in 0..frag_count {
        let chunk = chunk_of(payload, i, MAX_FRAGMENT_PAYLOAD);
        let flags = if i == frag_count - 1 {
            FLAG_END_OF_MESSAGE
        } else {
            0
        };
        let header = FrameHeader {
            channel,
            frame_type: FrameType::Media,
            flags,
            timestamp: ts,
            seq,
            frag_index: i,
            frag_count,
        };
        out.clear();
        encode_frame(&header, &chunk, &mut out).map_err(|_| ())?;
        stream.send(&out).await.map_err(|_| ())?;
    }
    Ok(())
}

/// Drive one reliable channel's stream: writes outgoing messages fragmented
/// to the wire, reassembles incoming ones in order. Runs until the stream
/// closes, a malformed frame is seen (kills only this channel), or `stop_rx`
/// signals a local close. Spawned once per channel by the registry.
pub(crate) async fn run_channel_task(
    mut stream: Box<dyn TransportStream>,
    channel: ChannelId,
    mut send_rx: mpsc::Receiver<(Bytes, SessionTimestamp)>,
    recv_tx: mpsc::Sender<(Bytes, SessionTimestamp)>,
    metrics: MetricsHub,
    mut stop_rx: watch::Receiver<bool>,
) {
    let mut acc: Vec<u8> = Vec::new();
    let mut read_buf = vec![0u8; 32 * 1024];
    let mut send_seq: u32 = 0;
    let mut msg_buf: Vec<u8> = Vec::new();
    let mut msg_ts: Option<SessionTimestamp> = None;

    loop {
        tokio::select! {
            biased;
            changed = stop_rx.changed() => {
                if changed.is_err() || *stop_rx.borrow() {
                    return;
                }
            }
            msg = send_rx.recv() => {
                match msg {
                    None => return,
                    Some((payload, ts)) => {
                        if send_message(&mut *stream, channel, send_seq, ts, &payload)
                            .await
                            .is_err()
                        {
                            return;
                        }
                        send_seq = send_seq.wrapping_add(1);
                    }
                }
            }
            n = stream.recv(&mut read_buf) => {
                match n {
                    Ok(0) | Err(_) => return,
                    Ok(n) => {
                        acc.extend_from_slice(&read_buf[..n]);
                        loop {
                            match try_take_frame(&mut acc) {
                                Ok(None) => break,
                                Err(_) => return, // malformed frame: kill only this channel
                                Ok(Some((header, payload))) => {
                                    if header.channel != channel {
                                        return; // mismatched routing: treat as malformed
                                    }
                                    if msg_ts.is_none() {
                                        msg_ts = Some(header.timestamp);
                                    }
                                    msg_buf.extend_from_slice(&payload);
                                    if header.flags & FLAG_END_OF_MESSAGE != 0 {
                                        let complete = std::mem::take(&mut msg_buf);
                                        let ts = msg_ts.take().unwrap_or(header.timestamp);
                                        metrics.add_bytes_rx(channel.0, complete.len() as u64);
                                        if recv_tx.send((Bytes::from(complete), ts)).await.is_err() {
                                            return;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use omniplex_metrics::MetricsHub;
    use omniplex_transport::{LoopbackConfig, LoopbackTransport, Transport};
    use std::time::Duration;

    /// Wraps a `TransportStream` and asserts every frame it carries respects
    /// the 16 KB fragment bound (plan Task 5.2 test requirement).
    struct AssertingStream {
        inner: Box<dyn TransportStream>,
    }

    #[async_trait::async_trait]
    impl TransportStream for AssertingStream {
        async fn send(&mut self, data: &[u8]) -> Result<(), omniplex_transport::TransportError> {
            let (_header, payload) =
                omniplex_wire::decode_frame(data).expect("every write must be one valid frame");
            assert!(
                payload.len() <= MAX_FRAGMENT_PAYLOAD,
                "fragment bound exceeded: {} > {}",
                payload.len(),
                MAX_FRAGMENT_PAYLOAD
            );
            self.inner.send(data).await
        }
        async fn recv(
            &mut self,
            buf: &mut [u8],
        ) -> Result<usize, omniplex_transport::TransportError> {
            self.inner.recv(buf).await
        }
    }

    async fn spawn_pair(
        a: Box<dyn TransportStream>,
        b: Box<dyn TransportStream>,
        channel: ChannelId,
    ) -> (
        mpsc::Sender<(Bytes, SessionTimestamp)>,
        mpsc::Receiver<(Bytes, SessionTimestamp)>,
        mpsc::Sender<(Bytes, SessionTimestamp)>,
        mpsc::Receiver<(Bytes, SessionTimestamp)>,
        watch::Sender<bool>,
        watch::Sender<bool>,
    ) {
        let (a_send_tx, a_send_rx) = mpsc::channel(CHANNEL_QUEUE_CAP);
        let (a_recv_tx, a_recv_rx) = mpsc::channel(CHANNEL_QUEUE_CAP);
        let (b_send_tx, b_send_rx) = mpsc::channel(CHANNEL_QUEUE_CAP);
        let (b_recv_tx, b_recv_rx) = mpsc::channel(CHANNEL_QUEUE_CAP);
        let (a_stop_tx, a_stop_rx) = watch::channel(false);
        let (b_stop_tx, b_stop_rx) = watch::channel(false);
        let metrics = MetricsHub::new();
        let wrapped_a: Box<dyn TransportStream> = Box::new(AssertingStream { inner: a });
        let wrapped_b: Box<dyn TransportStream> = Box::new(AssertingStream { inner: b });
        // Task on stream "a" sends whatever comes in via `a_send_rx` and
        // delivers whatever it *reads* off the wire (i.e. what "b" sent)
        // into `a_recv_tx`; symmetrically for "b".
        tokio::spawn(run_channel_task(
            wrapped_a,
            channel,
            a_send_rx,
            a_recv_tx.clone(),
            metrics.clone(),
            a_stop_rx,
        ));
        tokio::spawn(run_channel_task(
            wrapped_b,
            channel,
            b_send_rx,
            b_recv_tx.clone(),
            metrics,
            b_stop_rx,
        ));
        (
            a_send_tx, a_recv_rx, b_send_tx, b_recv_rx, a_stop_tx, b_stop_tx,
        )
    }

    #[tokio::test(start_paused = true)]
    async fn one_byte_and_one_mb_round_trip() {
        let (t_a, t_b) = LoopbackTransport::pair(LoopbackConfig::default());
        let sa = t_a.open_stream(0).await.unwrap();
        let sb = t_b.accept_stream().await.unwrap();
        let (a_tx, a_rx, _b_tx, mut b_rx, _sa_stop, _sb_stop) =
            spawn_pair(sa, sb, ChannelId(2)).await;

        let one_byte = Bytes::from_static(b"\x42");
        a_tx.send((one_byte.clone(), SessionTimestamp(1)))
            .await
            .unwrap();
        let (got, ts) = tokio::time::timeout(Duration::from_secs(5), b_rx.recv())
            .await
            .unwrap()
            .unwrap();
        assert_eq!(got, one_byte);
        assert_eq!(ts, SessionTimestamp(1));

        let one_mb = Bytes::from(vec![0xABu8; 1024 * 1024]);
        a_tx.send((one_mb.clone(), SessionTimestamp(2)))
            .await
            .unwrap();
        let (got, ts) = tokio::time::timeout(Duration::from_secs(5), b_rx.recv())
            .await
            .unwrap()
            .unwrap();
        assert_eq!(got, one_mb);
        assert_eq!(ts, SessionTimestamp(2));

        drop(a_rx);
    }

    #[tokio::test(start_paused = true)]
    async fn full_buffer_blocks_sender() {
        // A non-zero per-write stream latency means the channel task can
        // only drain `send_rx` at a fixed (slow) rate, so a producer
        // flooding sends without ever awaiting the receive side will
        // genuinely saturate the 64-slot outgoing buffer and block.
        let cfg = LoopbackConfig {
            latency: Duration::from_millis(50),
            ..Default::default()
        };
        let (t_a, t_b) = LoopbackTransport::pair(cfg);
        let sa = t_a.open_stream(0).await.unwrap();
        let sb = t_b.accept_stream().await.unwrap();
        let (a_tx, _a_rx, _b_tx, mut b_rx, _sa_stop, _sb_stop) =
            spawn_pair(sa, sb, ChannelId(2)).await;
        // Keep b's receive side draining continuously so backpressure can
        // only come from a's own outgoing buffer, not b's incoming one.
        tokio::spawn(async move { while b_rx.recv().await.is_some() {} });

        // Push well beyond the 64-slot buffer. Pure channel pushes take no
        // simulated time, but once the buffer (and the one message the
        // consumer has already dequeued and is busy 50ms-latency-writing)
        // is exhausted, `send` must genuinely await the consumer's pace.
        const EXTRA: u32 = 20;
        let start = tokio::time::Instant::now();
        for i in 0..(CHANNEL_QUEUE_CAP as u32 + EXTRA) {
            tokio::time::timeout(
                Duration::from_secs(60),
                a_tx.send((Bytes::from_static(b"x"), SessionTimestamp(i as u64))),
            )
            .await
            .expect("sender must eventually unblock as the consumer drains")
            .unwrap();
        }
        let elapsed = start.elapsed();
        // With no backpressure this loop would finish in ~0 simulated time;
        // pacing at one drain per 50ms latency period forces at least
        // roughly EXTRA * 50ms once the buffer saturates.
        assert!(
            elapsed >= Duration::from_millis(EXTRA as u64 * 50 / 2),
            "expected the buffer to have blocked the sender, elapsed = {elapsed:?}"
        );
    }

    #[tokio::test(start_paused = true)]
    async fn malformed_frame_kills_only_that_channel() {
        // Two independent channel pairs over two independent loopback
        // stream pairs. Corrupt one peer's outgoing bytes; the other pair
        // must keep working.
        let (t_a, t_b) = LoopbackTransport::pair(LoopbackConfig::default());

        let sa1 = t_a.open_stream(0).await.unwrap();
        let sb1 = t_b.accept_stream().await.unwrap();
        let sa2 = t_a.open_stream(0).await.unwrap();
        let sb2 = t_b.accept_stream().await.unwrap();

        // Channel 1: b1 is a raw stream we write garbage into directly,
        // simulating a malformed peer; a1 runs the real channel task.
        let (a1_send_tx, a1_send_rx) = mpsc::channel(CHANNEL_QUEUE_CAP);
        let (a1_recv_tx, mut a1_recv_rx) = mpsc::channel(CHANNEL_QUEUE_CAP);
        let (_stop1_tx, stop1_rx) = watch::channel(false);
        tokio::spawn(run_channel_task(
            sa1,
            ChannelId(2),
            a1_send_rx,
            a1_recv_tx,
            MetricsHub::new(),
            stop1_rx,
        ));
        let mut sb1 = sb1;
        // Not a valid frame header at all (too short + garbage bytes).
        sb1.send(&[0xFF; 4]).await.unwrap();
        drop(a1_send_tx);

        // Channel 2: fully healthy pair.
        let (a2_tx, a2_recv_rx, _b2_tx, mut b2_recv_rx, _s1, _s2) =
            spawn_pair(sa2, sb2, ChannelId(4)).await;
        a2_tx
            .send((Bytes::from_static(b"still alive"), SessionTimestamp(7)))
            .await
            .unwrap();
        let (got, _) = tokio::time::timeout(Duration::from_secs(5), b2_recv_rx.recv())
            .await
            .unwrap()
            .unwrap();
        assert_eq!(got, Bytes::from_static(b"still alive"));

        // Channel 1's task should have died quietly (no message ever
        // delivered) without panicking or affecting channel 2.
        assert!(a1_recv_rx.recv().await.is_none());
        drop(a2_recv_rx);
    }
}
