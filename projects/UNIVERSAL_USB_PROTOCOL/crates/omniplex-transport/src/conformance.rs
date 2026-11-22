//! Generic conformance suite every [`Transport`] implementation must pass
//! (keeps the abstraction honest across Loopback, Quinn, and future USB OTG).
//!
//! Assumes `a` and `b` are the two ends of one *unimpaired* link (no loss).
//! Panics on any violation, so it plugs directly into `#[tokio::test]`s.

use std::time::Duration;

use bytes::Bytes;

use crate::{Transport, TransportStream};

const STEP_TIMEOUT: Duration = Duration::from_secs(10);

async fn step<F, T>(name: &str, fut: F) -> T
where
    F: std::future::Future<Output = T>,
{
    tokio::time::timeout(STEP_TIMEOUT, fut)
        .await
        .unwrap_or_else(|_| panic!("conformance step timed out: {name}"))
}

async fn read_exact(s: &mut dyn TransportStream, buf: &mut [u8]) {
    let mut filled = 0;
    while filled < buf.len() {
        let n = s
            .recv(&mut buf[filled..])
            .await
            .expect("stream recv failed");
        assert!(n > 0, "stream closed early ({filled}/{} bytes)", buf.len());
        filled += n;
    }
}

/// Run the shared conformance suite over one connected transport pair.
/// Consumes both ends (the final check drops `a` and asserts `b.closed()`).
pub async fn transport_conformance<T: Transport>(a: T, b: T) {
    // --- datagram plumbing ------------------------------------------------
    let max = a.max_datagram_size();
    assert!(max >= 1024, "max_datagram_size too small: {max}");

    const N_DGRAMS: u64 = 32;
    for i in 0..N_DGRAMS {
        a.send_datagram(Bytes::from(i.to_le_bytes().to_vec()))
            .expect("send_datagram a->b");
    }
    let mut got: Vec<u64> = Vec::new();
    for _ in 0..N_DGRAMS {
        let d = step("recv datagram a->b", b.recv_datagram())
            .await
            .expect("recv_datagram");
        got.push(u64::from_le_bytes(d.as_ref().try_into().unwrap()));
    }
    got.sort_unstable();
    assert_eq!(got, (0..N_DGRAMS).collect::<Vec<_>>());

    b.send_datagram(Bytes::from_static(b"reverse"))
        .expect("send_datagram b->a");
    let d = step("recv datagram b->a", a.recv_datagram())
        .await
        .expect("recv_datagram");
    assert_eq!(d.as_ref(), b"reverse");

    // A datagram at exactly max_datagram_size must be accepted.
    a.send_datagram(Bytes::from(vec![0xAB; max]))
        .expect("max-size datagram");
    let d = step("recv max-size datagram", b.recv_datagram())
        .await
        .expect("recv_datagram");
    assert_eq!(d.len(), max);

    // --- stream open/accept + echo ---------------------------------------
    let mut sa = step("open_stream", a.open_stream(50))
        .await
        .expect("open_stream");
    sa.send(b"hello over stream").await.expect("stream send");
    let mut sb = step("accept_stream", b.accept_stream())
        .await
        .expect("accept_stream");
    let mut buf = [0u8; 17];
    step("stream recv", read_exact(sb.as_mut(), &mut buf)).await;
    assert_eq!(&buf, b"hello over stream");
    sb.send(b"echo").await.expect("stream echo send");
    let mut buf = [0u8; 4];
    step("stream echo recv", read_exact(sa.as_mut(), &mut buf)).await;
    assert_eq!(&buf, b"echo");
    // Live streams keep some transports (quinn) alive; release them so the
    // final closed() check observes a genuinely dead link.
    drop(sa);
    drop(sb);

    // --- bulk transfer: ordering + integrity across a large payload ------
    const BULK: usize = 1 << 20; // 1 MiB
    let payload: Vec<u8> = (0..BULK).map(|i| (i % 251) as u8).collect();
    let mut sa = step("open_stream bulk", a.open_stream(0))
        .await
        .expect("open_stream");
    let expect = payload.clone();
    let sender = tokio::spawn(async move {
        for chunk in payload.chunks(16 * 1024) {
            sa.send(chunk).await.expect("bulk send");
        }
        sa // keep the send side alive until the reader is done
    });
    let mut sb = step("accept_stream bulk", b.accept_stream())
        .await
        .expect("accept_stream");
    let mut got = vec![0u8; BULK];
    step("bulk recv", read_exact(sb.as_mut(), &mut got)).await;
    assert_eq!(got, expect, "bulk payload corrupted or reordered");
    drop(step("bulk sender join", sender).await.unwrap());
    drop(sb);

    // --- concurrent streams stay independent ------------------------------
    let mut opened = Vec::new();
    for tag in [b"s0", b"s1", b"s2"] {
        let mut s = step("open concurrent", a.open_stream(10))
            .await
            .expect("open_stream");
        s.send(tag).await.expect("send tag");
        opened.push((*tag, s));
    }
    let mut seen = Vec::new();
    for _ in 0..3 {
        let mut s = step("accept concurrent", b.accept_stream())
            .await
            .expect("accept_stream");
        let mut tag = [0u8; 2];
        step("recv tag", read_exact(s.as_mut(), &mut tag)).await;
        s.send(&tag).await.expect("echo tag"); // echo back on the same stream
        seen.push(tag);
    }
    seen.sort_unstable();
    assert_eq!(seen, [*b"s0", *b"s1", *b"s2"]);
    for (tag, mut s) in opened {
        let mut echoed = [0u8; 2];
        step("recv tag echo", read_exact(s.as_mut(), &mut echoed)).await;
        assert_eq!(echoed, tag, "stream cross-talk");
    }

    // --- closed() resolves on link loss -----------------------------------
    drop(a);
    step("closed() after peer drop", b.closed()).await;
}
