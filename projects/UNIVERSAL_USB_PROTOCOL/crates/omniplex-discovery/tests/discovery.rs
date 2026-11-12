use omniplex_discovery::mdns::{encode_txt, parse_txt, TxtRecord};
use omniplex_discovery::{elect, Discovery, DiscoveryConfig, Role};
use rand::{Rng, SeedableRng};

#[test]
fn elect_is_deterministic() {
    let mut rng = rand::rngs::SmallRng::seed_from_u64(1);
    for _ in 0..1000 {
        let a: [u8; 16] = rng.gen();
        let b: [u8; 16] = rng.gen();
        assert_eq!(elect(a, b), elect(a, b));
        assert_eq!(elect(b, a), elect(b, a));
    }
}

#[test]
fn elect_is_antisymmetric_for_distinct_ids() {
    let mut rng = rand::rngs::SmallRng::seed_from_u64(2);
    for _ in 0..1000 {
        let a: [u8; 16] = rng.gen();
        let b: [u8; 16] = rng.gen();
        if a == b {
            continue;
        }
        assert_ne!(elect(a, b), elect(b, a), "a={a:?} b={b:?}");
    }
    // Edge pairs.
    let lo = [0u8; 16];
    let hi = [0xffu8; 16];
    assert_eq!(elect(hi, lo), Role::Dialer);
    assert_eq!(elect(lo, hi), Role::Listener);
    assert_ne!(elect(hi, lo), elect(lo, hi));
}

#[test]
fn txt_round_trip() {
    let mut rng = rand::rngs::SmallRng::seed_from_u64(3);
    for _ in 0..100 {
        let id: [u8; 16] = rng.gen();
        let min: u16 = rng.gen_range(0..100);
        let max: u16 = rng.gen_range(min..u16::MAX);
        let port: u16 = rng.gen();
        let txt = encode_txt(id, (min, max), port);
        assert_eq!(txt[0].0, "v");
        assert_eq!(txt[1].0, "id");
        assert_eq!(txt[1].1.len(), 32);
        assert_eq!(txt[2].0, "port");
        let parsed = parse_txt(Some(&txt[0].1), Some(&txt[1].1), Some(&txt[2].1))
            .expect("round trip parses");
        assert_eq!(
            parsed,
            TxtRecord {
                instance_id: id,
                version_range: (min, max),
                port,
            }
        );
    }
}

#[test]
fn malformed_txt_ignored() {
    let good_id = "000102030405060708090a0b0c0d0e0f";
    // Baseline sanity: this combination is valid.
    assert!(parse_txt(Some("1-1"), Some(good_id), Some("4433")).is_some());

    // Missing fields.
    assert!(parse_txt(None, Some(good_id), Some("4433")).is_none());
    assert!(parse_txt(Some("1-1"), None, Some("4433")).is_none());
    assert!(parse_txt(Some("1-1"), Some(good_id), None).is_none());
    // Bad version strings.
    assert!(parse_txt(Some(""), Some(good_id), Some("4433")).is_none());
    assert!(parse_txt(Some("1"), Some(good_id), Some("4433")).is_none());
    assert!(parse_txt(Some("a-b"), Some(good_id), Some("4433")).is_none());
    assert!(parse_txt(Some("2-1"), Some(good_id), Some("4433")).is_none());
    assert!(parse_txt(Some("1-99999999"), Some(good_id), Some("4433")).is_none());
    // Bad ids.
    assert!(parse_txt(Some("1-1"), Some("short"), Some("4433")).is_none());
    assert!(parse_txt(Some("1-1"), Some(&"0".repeat(33)), Some("4433")).is_none());
    assert!(parse_txt(
        Some("1-1"),
        Some("zz0102030405060708090a0b0c0d0e0f"),
        Some("4433")
    )
    .is_none());
    // Bad ports.
    assert!(parse_txt(Some("1-1"), Some(good_id), Some("")).is_none());
    assert!(parse_txt(Some("1-1"), Some(good_id), Some("65536")).is_none());
    assert!(parse_txt(Some("1-1"), Some(good_id), Some("-1")).is_none());
    assert!(parse_txt(Some("1-1"), Some(good_id), Some("port")).is_none());
}

/// Two Discovery instances on the same real interface find each other and
/// filter their own announcements out. Needs real mDNS; run with:
/// `OMNIPLEX_IFACE=en0 cargo test -p omniplex-discovery -- --ignored`
#[tokio::test]
#[ignore = "requires a real network interface and mDNS"]
async fn self_discovery_over_real_interface() {
    let mk = |seed: u8, port: u16| DiscoveryConfig {
        instance_id: [seed; 16],
        version_range: (1, 1),
        udp_port: port,
    };
    let cfg_a = mk(0xaa, 40001);
    let cfg_b = mk(0xbb, 40002);
    let mut a = Discovery::start(cfg_a);
    let mut b = Discovery::start(cfg_b);

    let (pa, pb) = tokio::time::timeout(std::time::Duration::from_secs(15), async {
        tokio::join!(a.next_peer(), b.next_peer())
    })
    .await
    .expect("peers discovered within 15s");

    // Each side sees the other, never itself.
    assert_eq!(pa.instance_id, cfg_b.instance_id);
    assert_eq!(pb.instance_id, cfg_a.instance_id);
    assert_eq!(pa.addr.port(), cfg_b.udp_port);
    assert_eq!(pb.addr.port(), cfg_a.udp_port);
    assert_eq!(pa.version_range, (1, 1));
    // Election is consistent across both views.
    assert_ne!(
        elect(cfg_a.instance_id, pa.instance_id),
        elect(cfg_b.instance_id, pb.instance_id)
    );
}
