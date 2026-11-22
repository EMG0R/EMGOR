//! Transport abstraction for the Omniplex protocol (spec §6).
//!
//! Provides the [`Transport`] / [`TransportStream`] traits plus two
//! implementations: an in-process [`LoopbackTransport`] with injectable
//! loss / latency / jitter / bandwidth (the test harness for every layer
//! above), and a QUIC-backed [`QuinnTransport`].

pub mod conformance;
pub mod loopback;
pub mod quinn_transport;

pub use conformance::transport_conformance;
pub use loopback::{LoopbackConfig, LoopbackTransport};
pub use quinn_transport::{QuinnListener, QuinnTransport};

/// Errors surfaced by any transport implementation.
#[derive(Debug, thiserror::Error)]
pub enum TransportError {
    /// The link / connection is gone (peer dropped, idle timeout, cable pull).
    #[error("transport closed")]
    Closed,
    /// Datagram exceeds `max_datagram_size()`.
    #[error("datagram too large ({size} > {max})")]
    DatagramTooLarge { size: usize, max: usize },
    /// Connection-level failure.
    #[error("connection error: {0}")]
    Connection(String),
    /// Stream-level failure.
    #[error("stream error: {0}")]
    Stream(String),
    /// Failure while setting up an endpoint (bind, TLS config, …).
    #[error("setup error: {0}")]
    Setup(String),
}

/// The abstraction every later phase (raw USB OTG) must also satisfy (§6).
#[async_trait::async_trait]
pub trait Transport: Send + Sync + 'static {
    async fn open_stream(&self, priority: i32) -> Result<Box<dyn TransportStream>, TransportError>;
    async fn accept_stream(&self) -> Result<Box<dyn TransportStream>, TransportError>;
    fn send_datagram(&self, data: bytes::Bytes) -> Result<(), TransportError>; // non-blocking
    async fn recv_datagram(&self) -> Result<bytes::Bytes, TransportError>;
    fn max_datagram_size(&self) -> usize;
    async fn closed(&self) -> TransportError; // resolves on link loss (§10)
}

#[async_trait::async_trait]
pub trait TransportStream: Send {
    async fn send(&mut self, data: &[u8]) -> Result<(), TransportError>;
    async fn recv(&mut self, buf: &mut [u8]) -> Result<usize, TransportError>; // 0 = closed
}
