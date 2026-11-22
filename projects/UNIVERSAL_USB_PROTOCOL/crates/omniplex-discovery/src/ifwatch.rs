//! Interface watching: find a point-to-point-capable interface carrying an
//! IPv6 link-local address, polling every second via `if-addrs`.
//!
//! The interface can be forced with the `OMNIPLEX_IFACE` environment variable
//! (useful for testing over loopback or any other interface).

use std::net::Ipv6Addr;
use std::time::Duration;

/// Environment variable that forces a specific interface name.
pub const IFACE_ENV: &str = "OMNIPLEX_IFACE";

/// Poll interval for interface changes.
pub const POLL_INTERVAL: Duration = Duration::from_secs(1);

/// A usable interface: name, its IPv6 link-local address, and the scope id
/// (interface index) needed to build a `SocketAddrV6`.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct IfaceInfo {
    pub name: String,
    pub addr: Ipv6Addr,
    pub scope_id: u32,
}

/// Is this interface name one we consider point-to-point capable?
///
/// macOS: Thunderbolt/USB4 links show up as `bridge*` (Thunderbolt Bridge)
/// or `en*` members — the same `en*` prefix also covers a USB-C-to-Ethernet
/// adapter, so a Mac's Thunderbolt port and a Mac's Ethernet-adapter port
/// are indistinguishable to this matcher, which is exactly what we want.
/// Linux (Raspberry Pi OS and others): plain Ethernet is `eth*`, or `enp*`/
/// `eno*`/`ens*` under systemd predictable network interface naming. All of
/// these self-assign an fe80:: link-local address with no DHCP/router
/// present, same as the macOS cases.
pub fn is_point_to_point_capable(name: &str) -> bool {
    name.starts_with("bridge")
        || name.starts_with("en")
        || name.starts_with("eth")
        || name.starts_with("eno")
        || name.starts_with("ens")
}

fn is_link_local(addr: &Ipv6Addr) -> bool {
    // fe80::/10
    (addr.segments()[0] & 0xffc0) == 0xfe80
}

/// One poll: return the first matching interface with an IPv6 link-local
/// address, honoring the `OMNIPLEX_IFACE` override.
pub fn candidate_interface() -> Option<IfaceInfo> {
    let forced = std::env::var(IFACE_ENV).ok();
    let ifaces = if_addrs::get_if_addrs().ok()?;
    for iface in &ifaces {
        match &forced {
            Some(want) => {
                if &iface.name != want {
                    continue;
                }
            }
            None => {
                if iface.is_loopback() || !is_point_to_point_capable(&iface.name) {
                    continue;
                }
            }
        }
        if let if_addrs::IfAddr::V6(v6) = &iface.addr {
            if is_link_local(&v6.ip) {
                return Some(IfaceInfo {
                    name: iface.name.clone(),
                    addr: v6.ip,
                    scope_id: iface.index.unwrap_or(0),
                });
            }
        }
    }
    None
}

/// Poll every second until a suitable interface appears.
pub async fn wait_for_interface() -> IfaceInfo {
    loop {
        if let Some(info) = candidate_interface() {
            return info;
        }
        tokio::time::sleep(POLL_INTERVAL).await;
    }
}

#[cfg(test)]
mod matcher_tests {
    use super::is_point_to_point_capable;

    #[test]
    fn matches_macos_thunderbolt_and_ethernet_adapter_interfaces() {
        assert!(is_point_to_point_capable("bridge0"));
        assert!(is_point_to_point_capable("en0"));
        assert!(is_point_to_point_capable("en5")); // USB-C-to-Ethernet adapter
    }

    #[test]
    fn matches_linux_ethernet_interfaces() {
        assert!(is_point_to_point_capable("eth0"));
        assert!(is_point_to_point_capable("enp0s31f6"));
        assert!(is_point_to_point_capable("eno1"));
        assert!(is_point_to_point_capable("ens33"));
    }

    #[test]
    fn rejects_wifi_and_loopback_names() {
        assert!(!is_point_to_point_capable("wlan0"));
        assert!(!is_point_to_point_capable("lo"));
        assert!(!is_point_to_point_capable("lo0"));
    }
}
