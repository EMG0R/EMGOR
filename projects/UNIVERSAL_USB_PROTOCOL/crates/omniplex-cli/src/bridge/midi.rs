//! MIDI bridge (Task 7.2): midir **virtual ports** "Omniplex In"/"Omniplex Out"
//! <-> channel `midi/1` (ReliableOrdered, LatencyCritical). DAWs on this Mac
//! see the virtual ports; bytes are forwarded verbatim.

use bytes::Bytes;
use midir::os::unix::{VirtualInput, VirtualOutput};
use midir::{MidiInput, MidiOutput};
use omniplex_clock::SessionClock;
use omniplex_mux::{ChannelRx, ChannelTx};
use omniplex_wire::SessionTimestamp;
use tokio::sync::mpsc;

/// Runs until the channel closes. Creates the virtual ports, pumps
/// device -> channel and channel -> device.
pub async fn run(
    tx: ChannelTx,
    mut rx: ChannelRx,
    clock: SessionClock,
) -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
    // Device -> channel: the midir callback runs on a CoreMIDI thread; it
    // only copies bytes into an unbounded queue (never blocks MIDI timing).
    let (dev_tx, mut dev_rx) = mpsc::unbounded_channel::<Vec<u8>>();
    let midi_in = MidiInput::new("Omniplex Bridge")?;
    let _in_conn = midi_in.create_virtual(
        "Omniplex In",
        move |_ts_us, msg, _| {
            let _ = dev_tx.send(msg.to_vec());
        },
        (),
    )?;

    let midi_out = MidiOutput::new("Omniplex Bridge")?;
    let mut out_conn = midi_out.create_virtual("Omniplex Out")?;

    tracing::info!("midi bridge up: virtual ports \"Omniplex In\" / \"Omniplex Out\" <-> midi/1");

    loop {
        tokio::select! {
            msg = dev_rx.recv() => {
                let Some(msg) = msg else { break };
                let ts = clock.now_session_ns().unwrap_or(0);
                if tx.send(Bytes::from(msg), SessionTimestamp(ts)).await.is_err() {
                    break;
                }
            }
            inbound = rx.recv() => {
                let Some((bytes, _ts)) = inbound else { break };
                if let Err(e) = out_conn.send(&bytes) {
                    tracing::warn!("midi out send failed: {e}");
                }
            }
        }
    }
    Ok(())
}
