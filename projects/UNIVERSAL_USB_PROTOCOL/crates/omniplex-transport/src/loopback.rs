//! In-process loopback transport with injectable loss / latency / jitter /
//! bandwidth. Deterministic (seeded `SmallRng`) and driven entirely by tokio
//! time, so tests can use `tokio::time::pause()`.

use std::time::Duration;

use bytes::Bytes;
use rand::rngs::SmallRng;
use rand::{Rng, SeedableRng};
use tokio::sync::{mpsc, watch, Mutex};
use tokio::time::Instant;

use crate::{Transport, TransportError, TransportStream};

/// Loopback datagram MTU (mirrors a UDP-ish upper bound).
const LOOPBACK_MAX_DATAGRAM: usize = 65507;

/// Impairment configuration for a loopback pair. Applied per direction with
/// independent, deterministically seeded RNGs.
#[derive(Clone, Debug)]
pub struct LoopbackConfig {
    /// Probability in `[0.0, 1.0]` that a datagram is dropped.
    pub datagram_loss: f64,
    /// Fixed one-way latency.
    pub latency: Duration,
    /// Extra uniform random delay in `[0, jitter]` added per datagram.
    pub jitter: Duration,
    /// If set, datagrams are serialized through this bandwidth cap.
    pub bandwidth_bytes_per_sec: Option<u64>,
    /// Seed for the deterministic RNGs.
    pub seed: u64,
}

impl Default for LoopbackConfig {
    fn default() -> Self {
        Self {
            datagram_loss: 0.0,
            latency: Duration::ZERO,
            jitter: Duration::ZERO,
            bandwidth_bytes_per_sec: None,
            seed: 0,
        }
    }
}

struct RawDatagram {
    data: Bytes,
    sent_at: Instant,
}

/// One endpoint of an in-process transport pair. Create with
/// [`LoopbackTransport::pair`]. Requires a tokio runtime (spawns one shaper
/// task per direction).
pub struct LoopbackTransport {
    cfg: LoopbackConfig,
    // Datagrams out: raw queue feeding this direction's shaper task.
    dgram_tx: mpsc::UnboundedSender<RawDatagram>,
    // Datagrams in: output of the peer direction's shaper task.
    dgram_rx: Mutex<mpsc::UnboundedReceiver<Bytes>>,
    // Streams we open are handed to the peer's accept queue.
    stream_offer_tx: mpsc::UnboundedSender<LoopbackStream>,
    stream_accept_rx: Mutex<mpsc::UnboundedReceiver<LoopbackStream>>,
    // Closed when the peer transport is dropped (its `_peer_close_tx` drops).
    closed_rx: watch::Receiver<bool>,
    // Dropping this notifies the peer's `closed_rx`.
    _peer_close_tx: watch::Sender<bool>,
}

impl LoopbackTransport {
    /// Build a connected pair. Impairments in `cfg` apply independently to
    /// each direction, with RNG seeds `seed` and `seed + 1`.
    pub fn pair(cfg: LoopbackConfig) -> (LoopbackTransport, LoopbackTransport) {
        let (raw_ab_tx, raw_ab_rx) = mpsc::unbounded_channel();
        let (out_ab_tx, out_ab_rx) = mpsc::unbounded_channel();
        let (raw_ba_tx, raw_ba_rx) = mpsc::unbounded_channel();
        let (out_ba_tx, out_ba_rx) = mpsc::unbounded_channel();

        let (offer_ab_tx, accept_b_rx) = mpsc::unbounded_channel();
        let (offer_ba_tx, accept_a_rx) = mpsc::unbounded_channel();

        let (a_close_tx, a_closed_rx) = watch::channel(false);
        let (b_close_tx, b_closed_rx) = watch::channel(false);

        tokio::spawn(shaper(
            cfg.clone(),
            SmallRng::seed_from_u64(cfg.seed),
            raw_ab_rx,
            out_ab_tx,
        ));
        tokio::spawn(shaper(
            cfg.clone(),
            SmallRng::seed_from_u64(cfg.seed.wrapping_add(1)),
            raw_ba_rx,
            out_ba_tx,
        ));

        let a = LoopbackTransport {
            cfg: cfg.clone(),
            dgram_tx: raw_ab_tx,
            dgram_rx: Mutex::new(out_ba_rx),
            stream_offer_tx: offer_ab_tx,
            stream_accept_rx: Mutex::new(accept_a_rx),
            closed_rx: a_closed_rx,
            _peer_close_tx: b_close_tx,
        };
        let b = LoopbackTransport {
            cfg,
            dgram_tx: raw_ba_tx,
            dgram_rx: Mutex::new(out_ab_rx),
            stream_offer_tx: offer_ba_tx,
            stream_accept_rx: Mutex::new(accept_b_rx),
            closed_rx: b_closed_rx,
            _peer_close_tx: a_close_tx,
        };
        (a, b)
    }

    fn is_closed(&self) -> bool {
        *self.closed_rx.borrow() || self.closed_rx.has_changed().is_err()
    }
}

/// Per-direction datagram shaper: applies loss, bandwidth serialization,
/// fixed latency and uniform jitter, all on tokio time.
async fn shaper(
    cfg: LoopbackConfig,
    mut rng: SmallRng,
    mut raw_rx: mpsc::UnboundedReceiver<RawDatagram>,
    out_tx: mpsc::UnboundedSender<Bytes>,
) {
    // Time at which the simulated link is next free to transmit.
    let mut link_free_at = Instant::now();
    while let Some(RawDatagram { data, sent_at }) = raw_rx.recv().await {
        if cfg.datagram_loss > 0.0 && rng.gen::<f64>() < cfg.datagram_loss {
            continue;
        }
        let mut deliver_at = sent_at + cfg.latency;
        if let Some(bps) = cfg.bandwidth_bytes_per_sec {
            let start = link_free_at.max(sent_at);
            let tx_dur = Duration::from_secs_f64(data.len() as f64 / bps as f64);
            link_free_at = start + tx_dur;
            deliver_at = link_free_at + cfg.latency;
        }
        if !cfg.jitter.is_zero() {
            let j = rng.gen_range(0..=cfg.jitter.as_nanos() as u64);
            deliver_at += Duration::from_nanos(j);
        }
        tokio::time::sleep_until(deliver_at).await;
        if out_tx.send(data).is_err() {
            return; // receiver side dropped
        }
    }
}

#[async_trait::async_trait]
impl Transport for LoopbackTransport {
    async fn open_stream(
        &self,
        _priority: i32,
    ) -> Result<Box<dyn TransportStream>, TransportError> {
        if self.is_closed() {
            return Err(TransportError::Closed);
        }
        let (local, remote) = LoopbackStream::pipe(self.cfg.latency);
        self.stream_offer_tx
            .send(remote)
            .map_err(|_| TransportError::Closed)?;
        Ok(Box::new(local))
    }

    async fn accept_stream(&self) -> Result<Box<dyn TransportStream>, TransportError> {
        let mut rx = self.stream_accept_rx.lock().await;
        match rx.recv().await {
            Some(s) => Ok(Box::new(s)),
            None => Err(TransportError::Closed),
        }
    }

    fn send_datagram(&self, data: Bytes) -> Result<(), TransportError> {
        if data.len() > LOOPBACK_MAX_DATAGRAM {
            return Err(TransportError::DatagramTooLarge {
                size: data.len(),
                max: LOOPBACK_MAX_DATAGRAM,
            });
        }
        if self.is_closed() {
            return Err(TransportError::Closed);
        }
        self.dgram_tx
            .send(RawDatagram {
                data,
                sent_at: Instant::now(),
            })
            .map_err(|_| TransportError::Closed)
    }

    async fn recv_datagram(&self) -> Result<Bytes, TransportError> {
        let mut rx = self.dgram_rx.lock().await;
        rx.recv().await.ok_or(TransportError::Closed)
    }

    fn max_datagram_size(&self) -> usize {
        LOOPBACK_MAX_DATAGRAM
    }

    async fn closed(&self) -> TransportError {
        let mut rx = self.closed_rx.clone();
        loop {
            if *rx.borrow_and_update() {
                return TransportError::Closed;
            }
            if rx.changed().await.is_err() {
                return TransportError::Closed;
            }
        }
    }
}

/// One end of an in-process reliable-ordered byte stream.
pub struct LoopbackStream {
    tx: mpsc::UnboundedSender<Vec<u8>>,
    rx: mpsc::UnboundedReceiver<Vec<u8>>,
    latency: Duration,
    pending: Vec<u8>,
    pending_pos: usize,
}

impl LoopbackStream {
    fn pipe(latency: Duration) -> (LoopbackStream, LoopbackStream) {
        let (tx_ab, rx_ab) = mpsc::unbounded_channel();
        let (tx_ba, rx_ba) = mpsc::unbounded_channel();
        (
            LoopbackStream {
                tx: tx_ab,
                rx: rx_ba,
                latency,
                pending: Vec::new(),
                pending_pos: 0,
            },
            LoopbackStream {
                tx: tx_ba,
                rx: rx_ab,
                latency,
                pending: Vec::new(),
                pending_pos: 0,
            },
        )
    }
}

#[async_trait::async_trait]
impl TransportStream for LoopbackStream {
    async fn send(&mut self, data: &[u8]) -> Result<(), TransportError> {
        if !self.latency.is_zero() {
            tokio::time::sleep(self.latency).await;
        }
        self.tx
            .send(data.to_vec())
            .map_err(|_| TransportError::Closed)
    }

    async fn recv(&mut self, buf: &mut [u8]) -> Result<usize, TransportError> {
        if self.pending_pos >= self.pending.len() {
            match self.rx.recv().await {
                Some(chunk) => {
                    self.pending = chunk;
                    self.pending_pos = 0;
                }
                None => return Ok(0), // peer stream dropped = closed
            }
        }
        let n = (self.pending.len() - self.pending_pos).min(buf.len());
        buf[..n].copy_from_slice(&self.pending[self.pending_pos..self.pending_pos + n]);
        self.pending_pos += n;
        Ok(n)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::Arc;

    #[tokio::test(start_paused = true)]
    async fn datagram_delivery_round_trip() {
        let (a, b) = LoopbackTransport::pair(LoopbackConfig::default());
        for i in 0..10u8 {
            a.send_datagram(Bytes::from(vec![i; 8])).unwrap();
        }
        for i in 0..10u8 {
            let d = tokio::time::timeout(Duration::from_secs(1), b.recv_datagram())
                .await
                .unwrap()
                .unwrap();
            assert_eq!(d.as_ref(), &[i; 8]);
        }
        // And the reverse direction.
        b.send_datagram(Bytes::from_static(b"pong")).unwrap();
        let d = a.recv_datagram().await.unwrap();
        assert_eq!(d.as_ref(), b"pong");
    }

    #[tokio::test(start_paused = true)]
    async fn latency_is_applied() {
        let cfg = LoopbackConfig {
            latency: Duration::from_millis(10),
            ..Default::default()
        };
        let (a, b) = LoopbackTransport::pair(cfg);
        let start = Instant::now();
        a.send_datagram(Bytes::from_static(b"x")).unwrap();
        let _ = b.recv_datagram().await.unwrap();
        let elapsed = start.elapsed();
        assert!(elapsed >= Duration::from_millis(10), "elapsed {elapsed:?}");
        assert!(elapsed < Duration::from_millis(20), "elapsed {elapsed:?}");
    }

    #[tokio::test(start_paused = true)]
    async fn seeded_loss_rate_within_tolerance() {
        const N: usize = 5000;
        const LOSS: f64 = 0.25;
        let cfg = LoopbackConfig {
            datagram_loss: LOSS,
            seed: 42,
            ..Default::default()
        };
        let (a, b) = LoopbackTransport::pair(cfg);
        for i in 0..N {
            a.send_datagram(Bytes::from(i.to_le_bytes().to_vec()))
                .unwrap();
        }
        let mut received = 0usize;
        while tokio::time::timeout(Duration::from_millis(100), b.recv_datagram())
            .await
            .is_ok()
        {
            received += 1;
        }
        let expected = (N as f64 * (1.0 - LOSS)) as usize;
        let tol = (N as f64 * 0.03) as usize; // ~5 sigma for a binomial at N=5000
        assert!(
            received.abs_diff(expected) <= tol,
            "received {received}, expected {expected} +/- {tol}"
        );
    }

    #[tokio::test(start_paused = true)]
    async fn loss_is_deterministic_for_a_seed() {
        let count = |seed| async move {
            let cfg = LoopbackConfig {
                datagram_loss: 0.5,
                seed,
                ..Default::default()
            };
            let (a, b) = LoopbackTransport::pair(cfg);
            for _ in 0..500 {
                a.send_datagram(Bytes::from_static(b"d")).unwrap();
            }
            let mut n = 0usize;
            while tokio::time::timeout(Duration::from_millis(50), b.recv_datagram())
                .await
                .is_ok()
            {
                n += 1;
            }
            n
        };
        assert_eq!(count(7).await, count(7).await);
    }

    #[tokio::test(start_paused = true)]
    async fn stream_ordering_preserved() {
        let (a, b) = LoopbackTransport::pair(LoopbackConfig::default());
        let mut sa = a.open_stream(0).await.unwrap();
        let mut sb = b.accept_stream().await.unwrap();

        let mut sent = Vec::new();
        for i in 0..100u32 {
            let chunk = i.to_le_bytes();
            sent.extend_from_slice(&chunk);
            sa.send(&chunk).await.unwrap();
        }
        drop(sa);

        let mut got = Vec::new();
        let mut buf = [0u8; 7]; // odd size to force partial reads
        loop {
            let n = sb.recv(&mut buf).await.unwrap();
            if n == 0 {
                break;
            }
            got.extend_from_slice(&buf[..n]);
        }
        assert_eq!(got, sent);
    }

    #[tokio::test(start_paused = true)]
    async fn bandwidth_cap_shapes_throughput() {
        const BPS: u64 = 1_000_000; // 1 MB/s
        const PKT: usize = 1000;
        const N: usize = 200; // 200 KB total -> ~200 ms on the wire
        let cfg = LoopbackConfig {
            bandwidth_bytes_per_sec: Some(BPS),
            ..Default::default()
        };
        let (a, b) = LoopbackTransport::pair(cfg);
        let start = Instant::now();
        for _ in 0..N {
            a.send_datagram(Bytes::from(vec![0u8; PKT])).unwrap();
        }
        for _ in 0..N {
            b.recv_datagram().await.unwrap();
        }
        let elapsed = start.elapsed().as_secs_f64();
        let ideal = (N * PKT) as f64 / BPS as f64;
        assert!(
            elapsed >= ideal * 0.95 && elapsed <= ideal * 1.15,
            "elapsed {elapsed}s vs ideal {ideal}s"
        );
    }

    #[tokio::test(start_paused = true)]
    async fn closed_fires_when_peer_drops() {
        let (a, b) = LoopbackTransport::pair(LoopbackConfig::default());
        drop(b);
        let err = tokio::time::timeout(Duration::from_secs(1), a.closed())
            .await
            .expect("closed() must resolve after peer drop");
        assert!(matches!(err, TransportError::Closed));
        assert!(matches!(
            a.send_datagram(Bytes::from_static(b"x")),
            Err(TransportError::Closed)
        ));
        assert!(matches!(
            a.recv_datagram().await,
            Err(TransportError::Closed)
        ));
    }

    #[tokio::test(start_paused = true)]
    async fn closed_pending_before_drop_resolves() {
        let (a, b) = LoopbackTransport::pair(LoopbackConfig::default());
        let a = Arc::new(a);
        let watcher = {
            let a = Arc::clone(&a);
            tokio::spawn(async move { a.closed().await })
        };
        tokio::time::sleep(Duration::from_millis(5)).await;
        assert!(!watcher.is_finished());
        drop(b);
        let err = tokio::time::timeout(Duration::from_secs(1), watcher)
            .await
            .unwrap()
            .unwrap();
        assert!(matches!(err, TransportError::Closed));
    }
}
