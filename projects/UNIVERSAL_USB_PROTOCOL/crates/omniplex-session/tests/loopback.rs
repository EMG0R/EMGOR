//! Integration tests for the session facade over an in-process
//! `LoopbackTransport` pair (plan Task 5.5).

use std::sync::Arc;
use std::time::Duration;

use bytes::Bytes;
use tokio::sync::mpsc;

use omniplex_session::{DisconnectReason, Role, Session, SessionConfig, SessionEvent};
use omniplex_transport::{LoopbackConfig, LoopbackTransport};
use omniplex_wire::{
    ChannelDescriptor, ChannelId, ChannelKind, DeliveryClass, FormatDescriptor, LatencyClass,
    SessionTimestamp,
};

fn cfg(version_range: (u16, u16), id_byte: u8) -> SessionConfig {
    SessionConfig {
        instance_id: [id_byte; 16],
        version_range,
        ..SessionConfig::default()
    }
}

async fn next_event(rx: &mut mpsc::Receiver<SessionEvent>) -> SessionEvent {
    tokio::time::timeout(Duration::from_secs(30), rx.recv())
        .await
        .expect("timed out waiting for session event")
        .expect("event channel closed")
}

fn midi_descriptor() -> ChannelDescriptor {
    ChannelDescriptor {
        id: ChannelId::FIRST_DYNAMIC,
        name: "midi/1".into(),
        kind: ChannelKind::Midi,
        delivery: DeliveryClass::ReliableOrdered,
        latency: LatencyClass::LatencyCritical,
        format: FormatDescriptor::Midi,
    }
}

/// HELLO completes on both sides, version 1 is negotiated, the follower's
/// clock locks (paused tokio time), and a MIDI channel opened by A is
/// offered to B and round-trips a message each way.
#[tokio::test(start_paused = true)]
async fn hello_clock_and_midi_round_trip() {
    let (ta, tb) = LoopbackTransport::pair(LoopbackConfig {
        latency: Duration::from_micros(200),
        jitter: Duration::from_micros(100),
        ..LoopbackConfig::default()
    });

    // A has the higher instance id → A dials → A is clock master.
    let (session_a, mut events_a) =
        Session::from_transport(Arc::new(ta), Role::Dialer, cfg((1, 1), 0xAA));
    let (session_b, mut events_b) =
        Session::from_transport(Arc::new(tb), Role::Listener, cfg((1, 1), 0x11));

    assert_eq!(
        next_event(&mut events_a).await,
        SessionEvent::Connected { version: 1 }
    );
    assert_eq!(
        next_event(&mut events_b).await,
        SessionEvent::Connected { version: 1 }
    );

    // The follower (listener) clock must lock within a few seconds of
    // (paused, auto-advancing) time.
    let mut locked = false;
    for _ in 0..100 {
        if session_b.clock().now_session_ns().is_some() {
            locked = true;
            break;
        }
        tokio::time::sleep(Duration::from_millis(100)).await;
    }
    assert!(locked, "follower session clock never locked");
    // The master's clock is its own monotonic time: always available.
    assert!(session_a.clock().now_session_ns().is_some());

    // A opens a MIDI channel; B sees it offered.
    let (tx_a, mut rx_a) = session_a
        .open_channel(midi_descriptor())
        .await
        .expect("open midi channel");

    let offered = match next_event(&mut events_b).await {
        SessionEvent::ChannelOffered(desc) => desc,
        other => panic!("expected ChannelOffered, got {other:?}"),
    };
    assert_eq!(offered.kind, ChannelKind::Midi);
    assert_eq!(offered.name, "midi/1");
    let (tx_b, mut rx_b) = session_b
        .take_offered(offered.id)
        .expect("offered channel endpoints stashed");

    // Round-trip: A → B, then B → A.
    let note_on = [0x90u8, 60, 100];
    tx_a.send(Bytes::copy_from_slice(&note_on), SessionTimestamp(1_000))
        .await
        .expect("send A→B");
    let (payload, _ts) = rx_b.recv().await.expect("recv on B");
    assert_eq!(&payload[..], &note_on[..]);

    let note_off = [0x80u8, 60, 0];
    tx_b.send(Bytes::copy_from_slice(&note_off), SessionTimestamp(2_000))
        .await
        .expect("send B→A");
    let (payload, _ts) = rx_a.recv().await.expect("recv on A");
    assert_eq!(&payload[..], &note_off[..]);
}

/// Non-overlapping version ranges (2,3) vs (1,1) end in a clean
/// `Disconnected(VersionMismatch)` on both sides.
#[tokio::test(start_paused = true)]
async fn version_mismatch_disconnects_both_sides() {
    let (ta, tb) = LoopbackTransport::pair(LoopbackConfig::default());

    let (_session_a, mut events_a) =
        Session::from_transport(Arc::new(ta), Role::Dialer, cfg((2, 3), 0xAA));
    let (_session_b, mut events_b) =
        Session::from_transport(Arc::new(tb), Role::Listener, cfg((1, 1), 0x11));

    assert_eq!(
        next_event(&mut events_a).await,
        SessionEvent::Disconnected(DisconnectReason::VersionMismatch)
    );
    assert_eq!(
        next_event(&mut events_b).await,
        SessionEvent::Disconnected(DisconnectReason::VersionMismatch)
    );
}
