//! OSC bridge logic (Task 7.2), pure part: minimal OSC packet inspection
//! (address + type tags — payloads are forwarded verbatim) and the per-address
//! router implementing the spec §9 rule: messages whose type tags are only
//! floats/ints, arriving at more than 10 Hz on the same address, go to the
//! latest-value channel `osc/lv`; everything else goes reliable on `osc/1`.
//! UDP socket wiring lives in `run.rs`.

use std::collections::{HashMap, VecDeque};

/// Which channel an OSC packet should ride.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum OscRoute {
    /// `osc/1` — ReliableOrdered.
    Reliable,
    /// `osc/lv` — LatestValueWins.
    LatestValue,
}

/// Address + type tags of a plain OSC message.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct OscMeta {
    pub address: String,
    /// Type tag string without the leading ','; empty if the message has no
    /// type tag string.
    pub typetags: String,
}

/// Read a 4-byte-aligned padded OSC string starting at `off`.
fn read_padded_str(buf: &[u8], off: usize) -> Option<(&str, usize)> {
    let rest = buf.get(off..)?;
    let nul = rest.iter().position(|&b| b == 0)?;
    let s = std::str::from_utf8(&rest[..nul]).ok()?;
    let advance = (nul / 4 + 1) * 4; // include padding
    Some((s, off + advance))
}

/// Parse address + type tags from an OSC message. Returns None for bundles
/// (`#bundle`) and malformed packets — those always route reliable.
pub fn parse_meta(buf: &[u8]) -> Option<OscMeta> {
    let (address, off) = read_padded_str(buf, 0)?;
    if !address.starts_with('/') {
        return None; // bundle or garbage
    }
    let typetags = match read_padded_str(buf, off) {
        Some((tags, _)) if tags.starts_with(',') => tags[1..].to_string(),
        _ => String::new(),
    };
    Some(OscMeta {
        address: address.to_string(),
        typetags,
    })
}

/// True if every type tag is a numeric scalar (float32/64, int32/64).
fn numeric_only(typetags: &str) -> bool {
    !typetags.is_empty() && typetags.chars().all(|c| matches!(c, 'f' | 'i' | 'd' | 'h'))
}

/// Per-address rate tracker deciding the route for each outgoing packet.
pub struct OscRouter {
    window_ns: u64,
    threshold_hz: usize,
    seen: HashMap<String, VecDeque<u64>>,
}

impl Default for OscRouter {
    fn default() -> Self {
        Self::new()
    }
}

impl OscRouter {
    pub fn new() -> Self {
        Self {
            window_ns: 1_000_000_000,
            threshold_hz: 10,
            seen: HashMap::new(),
        }
    }

    /// Decide the route for a packet arriving at `now_ns` (any monotonic ns).
    pub fn route(&mut self, now_ns: u64, packet: &[u8]) -> OscRoute {
        let Some(meta) = parse_meta(packet) else {
            return OscRoute::Reliable; // bundles / malformed: reliable
        };
        if !numeric_only(&meta.typetags) {
            return OscRoute::Reliable;
        }
        let times = self.seen.entry(meta.address).or_default();
        times.push_back(now_ns);
        while let Some(&front) = times.front() {
            if now_ns.saturating_sub(front) > self.window_ns {
                times.pop_front();
            } else {
                break;
            }
        }
        if times.len() > self.threshold_hz {
            OscRoute::LatestValue
        } else {
            OscRoute::Reliable
        }
    }
}

/// Build a minimal OSC message (address + numeric/string args) — used by
/// tests and the bench tool.
#[cfg(test)]
pub fn make_msg(address: &str, typetags: &str) -> Vec<u8> {
    fn pad(v: &mut Vec<u8>, s: &str) {
        v.extend_from_slice(s.as_bytes());
        v.push(0);
        while !v.len().is_multiple_of(4) {
            v.push(0);
        }
    }
    let mut v = Vec::new();
    pad(&mut v, address);
    pad(&mut v, &format!(",{typetags}"));
    for c in typetags.chars() {
        match c {
            'f' | 'i' => v.extend_from_slice(&[0u8; 4]),
            'd' | 'h' => v.extend_from_slice(&[0u8; 8]),
            's' => pad(&mut v, "x"),
            _ => {}
        }
    }
    v
}

#[cfg(test)]
mod tests {
    use super::*;

    const MS: u64 = 1_000_000;

    #[test]
    fn parse_address_and_tags() {
        let m = make_msg("/fader/1", "ff");
        assert_eq!(
            parse_meta(&m),
            Some(OscMeta {
                address: "/fader/1".into(),
                typetags: "ff".into()
            })
        );
        assert_eq!(parse_meta(b"#bundle\0garbage"), None);
        assert_eq!(parse_meta(&[]), None);
        assert_eq!(parse_meta(&[0xFF, 0xFE, 0x00, 0x00]), None);
    }

    #[test]
    fn slow_numeric_messages_stay_reliable() {
        let mut r = OscRouter::new();
        let m = make_msg("/knob", "f");
        for i in 0..20u64 {
            assert_eq!(r.route(i * 200 * MS, &m), OscRoute::Reliable); // 5 Hz
        }
    }

    #[test]
    fn fast_numeric_messages_go_latest_value() {
        let mut r = OscRouter::new();
        let m = make_msg("/fader", "ff");
        let mut routes = Vec::new();
        for i in 0..100u64 {
            routes.push(r.route(i * 10 * MS, &m)); // 100 Hz
        }
        // Warmup allowed; steady state must be latest-value.
        assert!(routes[20..].iter().all(|&x| x == OscRoute::LatestValue));
        assert_eq!(routes[0], OscRoute::Reliable);
    }

    #[test]
    fn fast_non_numeric_messages_stay_reliable() {
        let mut r = OscRouter::new();
        let m = make_msg("/label", "s");
        for i in 0..50u64 {
            assert_eq!(r.route(i * 5 * MS, &m), OscRoute::Reliable); // 200 Hz
        }
        // Mixed tags too.
        let m = make_msg("/mixed", "fs");
        for i in 0..50u64 {
            assert_eq!(r.route(i * 5 * MS, &m), OscRoute::Reliable);
        }
    }

    #[test]
    fn bundles_and_malformed_stay_reliable() {
        let mut r = OscRouter::new();
        for i in 0..50u64 {
            assert_eq!(r.route(i * MS, b"#bundle\0\0\0\0\0"), OscRoute::Reliable);
            assert_eq!(r.route(i * MS, &[1, 2, 3]), OscRoute::Reliable);
        }
    }

    #[test]
    fn rate_window_expires() {
        let mut r = OscRouter::new();
        let m = make_msg("/burst", "i");
        for i in 0..30u64 {
            r.route(i * 10 * MS, &m); // burst at 100 Hz
        }
        // 2 s of silence, then a slow message: reliable again.
        assert_eq!(r.route(2_300 * MS + 300 * MS, &m), OscRoute::Reliable);
    }
}
