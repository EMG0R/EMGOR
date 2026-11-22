//! Omniplex peer discovery: interface watching + mDNS announce/browse + role
//! election (design §7).
//!
//! Zero-config: watch for a point-to-point-capable interface gaining an
//! IPv6 link-local address, announce `_omniplex._udp.local.` on it, browse for
//! peers, and elect who dials (higher instance id dials).

pub mod ifwatch;
pub mod mdns;

use std::cmp::Ordering;
use std::net::SocketAddrV6;

use tokio::sync::mpsc;
use tokio::task::JoinHandle;

/// A peer discovered via mDNS.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct PeerFound {
    /// Link-local IPv6 address + QUIC UDP port, with scope id set.
    pub addr: SocketAddrV6,
    pub instance_id: [u8; 16],
    pub version_range: (u16, u16),
}

/// Which side initiates the QUIC connection.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Role {
    Dialer,
    Listener,
}

/// Deterministic role election: the higher instance id dials (§7).
///
/// For distinct ids, `elect(a, b)` and `elect(b, a)` always disagree, so
/// exactly one side dials.
pub fn elect(ours: [u8; 16], theirs: [u8; 16]) -> Role {
    match ours.cmp(&theirs) {
        Ordering::Greater => Role::Dialer,
        _ => Role::Listener,
    }
}

/// Configuration for [`Discovery::start`].
#[derive(Debug, Clone, Copy)]
pub struct DiscoveryConfig {
    /// Our random 128-bit instance id.
    pub instance_id: [u8; 16],
    /// Supported protocol version range (min, max), announced as `v=min-max`.
    pub version_range: (u16, u16),
    /// UDP port our QUIC endpoint listens on, announced as `port=<n>`.
    pub udp_port: u16,
}

/// Running discovery: interface watcher + mDNS announce/browse tasks.
pub struct Discovery {
    rx: mpsc::Receiver<PeerFound>,
    task: JoinHandle<()>,
}

impl Discovery {
    /// Spawn the discovery tasks. Must be called from within a tokio runtime.
    pub fn start(cfg: DiscoveryConfig) -> Self {
        let (tx, rx) = mpsc::channel(16);
        let task = tokio::spawn(async move {
            loop {
                let iface = ifwatch::wait_for_interface().await;
                tracing::info!(iface = %iface.name, addr = %iface.addr, "announcing on interface");
                match mdns::run(cfg, iface, tx.clone()).await {
                    Ok(()) => break, // channel closed; Discovery dropped
                    Err(err) => {
                        tracing::warn!(%err, "mdns task failed; retrying");
                        tokio::time::sleep(ifwatch::POLL_INTERVAL).await;
                    }
                }
            }
        });
        Self { rx, task }
    }

    /// Wait for the next discovered peer. Pends forever if discovery has
    /// stopped producing (e.g. no interface ever appears).
    pub async fn next_peer(&mut self) -> PeerFound {
        loop {
            if let Some(peer) = self.rx.recv().await {
                return peer;
            }
            // Sender dropped (task exiting): never resolves.
            std::future::pending::<()>().await;
        }
    }
}

impl Drop for Discovery {
    fn drop(&mut self) {
        self.task.abort();
    }
}
