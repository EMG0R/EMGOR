//! The same conformance suite run over both transports (plan Task 2.2).

use std::net::{Ipv4Addr, SocketAddr};

use omniplex_transport::{
    transport_conformance, LoopbackConfig, LoopbackTransport, QuinnListener, QuinnTransport,
};

#[tokio::test]
async fn loopback_conformance() {
    let (a, b) = LoopbackTransport::pair(LoopbackConfig::default());
    transport_conformance(a, b).await;
}

#[tokio::test]
async fn loopback_conformance_with_latency() {
    let cfg = LoopbackConfig {
        latency: std::time::Duration::from_micros(200),
        jitter: std::time::Duration::from_micros(100),
        seed: 1,
        ..Default::default()
    };
    let (a, b) = LoopbackTransport::pair(cfg);
    transport_conformance(a, b).await;
}

#[tokio::test]
async fn quinn_conformance_localhost() {
    let listener =
        QuinnListener::bind(SocketAddr::from((Ipv4Addr::LOCALHOST, 0))).expect("bind listener");
    let addr = listener.local_addr().expect("local addr");
    let accept = tokio::spawn(async move { listener.accept().await.expect("accept") });
    let a = QuinnTransport::dial(addr).await.expect("dial");
    let b = accept.await.unwrap();
    transport_conformance(a, b).await;
}
