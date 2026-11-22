//! QUIC transport backed by `quinn` (spec §6).
//!
//! rcgen self-signed cert on the server, accept-any verifier on the client
//! (certs are not verified in v1, §3/§6), ALPN `omniplex/1`, 500 ms idle timeout
//! with 100 ms keep-alives (~cable-pull detection, §7/§10), datagrams enabled.

use std::net::{IpAddr, Ipv4Addr, Ipv6Addr, SocketAddr};
use std::sync::Arc;
use std::time::Duration;

use bytes::Bytes;
use quinn::crypto::rustls::{QuicClientConfig, QuicServerConfig};
use rustls::client::danger::{HandshakeSignatureValid, ServerCertVerified, ServerCertVerifier};
use rustls::pki_types::{CertificateDer, PrivatePkcs8KeyDer, ServerName, UnixTime};

use crate::{Transport, TransportError, TransportStream};

const ALPN: &[u8] = b"omniplex/1";
const MAX_IDLE: Duration = Duration::from_millis(500);
const KEEP_ALIVE: Duration = Duration::from_millis(100);

fn install_crypto_provider() {
    // Idempotent; fails harmlessly if another provider is already installed.
    let _ = rustls::crypto::ring::default_provider().install_default();
}

fn setup(e: impl std::fmt::Display) -> TransportError {
    TransportError::Setup(e.to_string())
}

fn transport_config() -> Arc<quinn::TransportConfig> {
    let mut tc = quinn::TransportConfig::default();
    tc.max_idle_timeout(Some(
        quinn::IdleTimeout::try_from(MAX_IDLE).expect("valid idle timeout"),
    ));
    tc.keep_alive_interval(Some(KEEP_ALIVE));
    // Datagrams are enabled by default in quinn; keep the default buffers.
    Arc::new(tc)
}

fn server_config() -> Result<quinn::ServerConfig, TransportError> {
    install_crypto_provider();
    let cert = rcgen::generate_simple_self_signed(vec!["omniplex".into()]).map_err(setup)?;
    let cert_der = CertificateDer::from(cert.cert);
    let key_der = PrivatePkcs8KeyDer::from(cert.key_pair.serialize_der());
    let mut crypto = rustls::ServerConfig::builder()
        .with_no_client_auth()
        .with_single_cert(vec![cert_der], key_der.into())
        .map_err(setup)?;
    crypto.alpn_protocols = vec![ALPN.to_vec()];
    let mut sc = quinn::ServerConfig::with_crypto(Arc::new(
        QuicServerConfig::try_from(crypto).map_err(setup)?,
    ));
    sc.transport_config(transport_config());
    Ok(sc)
}

fn client_config() -> Result<quinn::ClientConfig, TransportError> {
    install_crypto_provider();
    let provider = rustls::crypto::CryptoProvider::get_default()
        .expect("crypto provider installed")
        .clone();
    let mut crypto = rustls::ClientConfig::builder()
        .dangerous()
        .with_custom_certificate_verifier(Arc::new(AcceptAnyServerCert { provider }))
        .with_no_client_auth();
    crypto.alpn_protocols = vec![ALPN.to_vec()];
    let mut cc =
        quinn::ClientConfig::new(Arc::new(QuicClientConfig::try_from(crypto).map_err(setup)?));
    cc.transport_config(transport_config());
    Ok(cc)
}

/// v1 does not verify certs (§3/§6): accept any server certificate but keep
/// real signature verification so the TLS handshake stays well-formed.
#[derive(Debug)]
struct AcceptAnyServerCert {
    provider: Arc<rustls::crypto::CryptoProvider>,
}

impl ServerCertVerifier for AcceptAnyServerCert {
    fn verify_server_cert(
        &self,
        _end_entity: &CertificateDer<'_>,
        _intermediates: &[CertificateDer<'_>],
        _server_name: &ServerName<'_>,
        _ocsp_response: &[u8],
        _now: UnixTime,
    ) -> Result<ServerCertVerified, rustls::Error> {
        Ok(ServerCertVerified::assertion())
    }

    fn verify_tls12_signature(
        &self,
        message: &[u8],
        cert: &CertificateDer<'_>,
        dss: &rustls::DigitallySignedStruct,
    ) -> Result<HandshakeSignatureValid, rustls::Error> {
        rustls::crypto::verify_tls12_signature(
            message,
            cert,
            dss,
            &self.provider.signature_verification_algorithms,
        )
    }

    fn verify_tls13_signature(
        &self,
        message: &[u8],
        cert: &CertificateDer<'_>,
        dss: &rustls::DigitallySignedStruct,
    ) -> Result<HandshakeSignatureValid, rustls::Error> {
        rustls::crypto::verify_tls13_signature(
            message,
            cert,
            dss,
            &self.provider.signature_verification_algorithms,
        )
    }

    fn supported_verify_schemes(&self) -> Vec<rustls::SignatureScheme> {
        self.provider
            .signature_verification_algorithms
            .supported_schemes()
    }
}

/// A bound server endpoint that can accept incoming Omniplex connections.
/// Useful when you need `local_addr()` before the peer dials (tests, mDNS
/// port registration).
pub struct QuinnListener {
    endpoint: quinn::Endpoint,
}

impl QuinnListener {
    pub fn bind(addr: SocketAddr) -> Result<Self, TransportError> {
        let endpoint = quinn::Endpoint::server(server_config()?, addr).map_err(setup)?;
        Ok(Self { endpoint })
    }

    pub fn local_addr(&self) -> Result<SocketAddr, TransportError> {
        self.endpoint.local_addr().map_err(setup)
    }

    pub async fn accept(&self) -> Result<QuinnTransport, TransportError> {
        let incoming = self.endpoint.accept().await.ok_or(TransportError::Closed)?;
        let conn = incoming
            .await
            .map_err(|e| TransportError::Connection(e.to_string()))?;
        Ok(QuinnTransport {
            conn,
            _endpoint: self.endpoint.clone(),
        })
    }
}

/// QUIC-backed [`Transport`]: one connection between exactly two peers.
pub struct QuinnTransport {
    conn: quinn::Connection,
    // Keep the endpoint (and its UDP socket / driver) alive.
    _endpoint: quinn::Endpoint,
}

impl QuinnTransport {
    /// Bind `bind` and accept the first incoming connection.
    pub async fn listen(bind: SocketAddr) -> Result<Self, TransportError> {
        QuinnListener::bind(bind)?.accept().await
    }

    /// Dial `peer` from an ephemeral local port of the same address family.
    pub async fn dial(peer: SocketAddr) -> Result<Self, TransportError> {
        let local: SocketAddr = match peer.ip() {
            IpAddr::V4(_) => (Ipv4Addr::UNSPECIFIED, 0).into(),
            IpAddr::V6(_) => (Ipv6Addr::UNSPECIFIED, 0).into(),
        };
        let mut endpoint = quinn::Endpoint::client(local).map_err(setup)?;
        endpoint.set_default_client_config(client_config()?);
        let conn = endpoint
            .connect(peer, "omniplex")
            .map_err(setup)?
            .await
            .map_err(|e| TransportError::Connection(e.to_string()))?;
        Ok(Self {
            conn,
            _endpoint: endpoint,
        })
    }

    pub fn remote_address(&self) -> SocketAddr {
        self.conn.remote_address()
    }
}

fn conn_err(e: quinn::ConnectionError) -> TransportError {
    match e {
        quinn::ConnectionError::ApplicationClosed(_)
        | quinn::ConnectionError::ConnectionClosed(_)
        | quinn::ConnectionError::LocallyClosed => TransportError::Closed,
        other => TransportError::Connection(other.to_string()),
    }
}

#[async_trait::async_trait]
impl Transport for QuinnTransport {
    async fn open_stream(&self, priority: i32) -> Result<Box<dyn TransportStream>, TransportError> {
        let (send, recv) = self.conn.open_bi().await.map_err(conn_err)?;
        send.set_priority(priority)
            .map_err(|e| TransportError::Stream(e.to_string()))?;
        Ok(Box::new(QuinnStream { send, recv }))
    }

    async fn accept_stream(&self) -> Result<Box<dyn TransportStream>, TransportError> {
        let (send, recv) = self.conn.accept_bi().await.map_err(conn_err)?;
        Ok(Box::new(QuinnStream { send, recv }))
    }

    fn send_datagram(&self, data: Bytes) -> Result<(), TransportError> {
        let size = data.len();
        self.conn.send_datagram(data).map_err(|e| match e {
            quinn::SendDatagramError::TooLarge => TransportError::DatagramTooLarge {
                size,
                max: self.conn.max_datagram_size().unwrap_or(0),
            },
            quinn::SendDatagramError::ConnectionLost(e) => conn_err(e),
            other => TransportError::Connection(other.to_string()),
        })
    }

    async fn recv_datagram(&self) -> Result<Bytes, TransportError> {
        self.conn.read_datagram().await.map_err(conn_err)
    }

    fn max_datagram_size(&self) -> usize {
        self.conn.max_datagram_size().unwrap_or(0)
    }

    async fn closed(&self) -> TransportError {
        conn_err(self.conn.closed().await)
    }
}

struct QuinnStream {
    send: quinn::SendStream,
    recv: quinn::RecvStream,
}

#[async_trait::async_trait]
impl TransportStream for QuinnStream {
    async fn send(&mut self, data: &[u8]) -> Result<(), TransportError> {
        self.send
            .write_all(data)
            .await
            .map_err(|e| TransportError::Stream(e.to_string()))
    }

    async fn recv(&mut self, buf: &mut [u8]) -> Result<usize, TransportError> {
        match self.recv.read(buf).await {
            Ok(Some(n)) => Ok(n),
            Ok(None) => Ok(0), // FIN: peer finished the stream
            Err(quinn::ReadError::ConnectionLost(e)) => Err(conn_err(e)),
            Err(e) => Err(TransportError::Stream(e.to_string())),
        }
    }
}
