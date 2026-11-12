//! mDNS announce + browse for `_omniplex._udp.local.`, scoped to a single
//! interface, using `mdns-sd`.
//!
//! TXT records:
//! - `v=<min>-<max>`   negotiated version range
//! - `id=<32 hex>`     128-bit instance id
//! - `port=<udp port>` QUIC UDP port

use std::net::{Ipv6Addr, SocketAddrV6};

use mdns_sd::{IfKind, ServiceDaemon, ServiceEvent, ServiceInfo};
use tokio::sync::mpsc;

use crate::ifwatch::IfaceInfo;
use crate::{DiscoveryConfig, PeerFound};

/// mDNS service type for Omniplex (spec §7).
pub const SERVICE_TYPE: &str = "_omniplex._udp.local.";

/// Parsed contents of a peer's TXT records.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct TxtRecord {
    pub instance_id: [u8; 16],
    pub version_range: (u16, u16),
    pub port: u16,
}

/// Encode TXT key/value pairs for our announcement.
pub fn encode_txt(
    instance_id: [u8; 16],
    version_range: (u16, u16),
    port: u16,
) -> [(String, String); 3] {
    [
        (
            "v".into(),
            format!("{}-{}", version_range.0, version_range.1),
        ),
        ("id".into(), hex_encode(&instance_id)),
        ("port".into(), port.to_string()),
    ]
}

/// Parse TXT values back. Returns `None` for anything malformed.
pub fn parse_txt(v: Option<&str>, id: Option<&str>, port: Option<&str>) -> Option<TxtRecord> {
    let v = v?;
    let (min, max) = v.split_once('-')?;
    let min: u16 = min.parse().ok()?;
    let max: u16 = max.parse().ok()?;
    if min > max {
        return None;
    }
    let instance_id = hex_decode_16(id?)?;
    let port: u16 = port?.parse().ok()?;
    Some(TxtRecord {
        instance_id,
        version_range: (min, max),
        port,
    })
}

pub fn hex_encode(bytes: &[u8]) -> String {
    let mut s = String::with_capacity(bytes.len() * 2);
    for b in bytes {
        s.push_str(&format!("{b:02x}"));
    }
    s
}

pub fn hex_decode_16(s: &str) -> Option<[u8; 16]> {
    if s.len() != 32 || !s.is_ascii() {
        return None;
    }
    let mut out = [0u8; 16];
    for (i, chunk) in s.as_bytes().chunks(2).enumerate() {
        let hi = (chunk[0] as char).to_digit(16)?;
        let lo = (chunk[1] as char).to_digit(16)?;
        out[i] = ((hi << 4) | lo) as u8;
    }
    Some(out)
}

/// Announce ourselves and browse for peers on `iface`, sending each
/// discovered peer (own instance id filtered out) down `tx`. Runs until the
/// channel closes or the daemon errors.
pub async fn run(
    cfg: DiscoveryConfig,
    iface: IfaceInfo,
    tx: mpsc::Sender<PeerFound>,
) -> Result<(), mdns_sd::Error> {
    let daemon = ServiceDaemon::new()?;
    // Scope the daemon to the chosen interface only.
    daemon.disable_interface(IfKind::All)?;
    daemon.enable_interface(IfKind::Name(iface.name.clone()))?;

    let id_hex = hex_encode(&cfg.instance_id);
    let host = format!("{id_hex}.local.");
    let txt = encode_txt(cfg.instance_id, cfg.version_range, cfg.udp_port);
    let txt_ref: Vec<(&str, &str)> = txt.iter().map(|(k, v)| (k.as_str(), v.as_str())).collect();
    let info = ServiceInfo::new(
        SERVICE_TYPE,
        &id_hex,
        &host,
        std::net::IpAddr::V6(iface.addr),
        cfg.udp_port,
        &txt_ref[..],
    )?;
    daemon.register(info)?;

    let receiver = daemon.browse(SERVICE_TYPE)?;
    loop {
        let event = match receiver.recv_async().await {
            Ok(ev) => ev,
            Err(_) => break,
        };
        if let ServiceEvent::ServiceResolved(resolved) = event {
            let parsed = parse_txt(
                resolved.get_property_val_str("v"),
                resolved.get_property_val_str("id"),
                resolved.get_property_val_str("port"),
            );
            let Some(rec) = parsed else {
                tracing::debug!(name = %resolved.get_fullname(), "ignoring peer with malformed TXT");
                continue;
            };
            if rec.instance_id == cfg.instance_id {
                continue; // our own announcement
            }
            let Some(v6) = resolved.get_addresses().iter().find_map(|a| match a {
                std::net::IpAddr::V6(v6) => Some(*v6),
                std::net::IpAddr::V4(_) => None,
            }) else {
                continue;
            };
            let addr = SocketAddrV6::new(v6, rec.port, 0, scope_for(&v6, iface.scope_id));
            let peer = PeerFound {
                addr,
                instance_id: rec.instance_id,
                version_range: rec.version_range,
            };
            if tx.send(peer).await.is_err() {
                break;
            }
        }
    }
    let _ = daemon.shutdown();
    Ok(())
}

fn scope_for(addr: &Ipv6Addr, iface_scope: u32) -> u32 {
    // Link-local peers need the scope id of the interface we heard them on.
    if (addr.segments()[0] & 0xffc0) == 0xfe80 {
        iface_scope
    } else {
        0
    }
}
