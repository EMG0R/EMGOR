// Wired audio aggregation — macOS-style.
//
// Enumerates every selectable ALSA sink (`alsa_output.*` — USB and platform
// I2S HATs like the HiFiBerry alike; see util::is_selectable_audio_node for
// the aloop/HDMI exclusions) and links its playback_FL/FR
// port pair into successive channel pairs of demiurge-sink-output (FL/FR,
// then RL/RR, etc.). Smaller channel counts come first — for a 2i2 + 4i4
// rig the 2i2 is the "front" interface, the 4i4 gets the "rear" pair.
//
// Sources go through the same process for demiurge-source-input.
//
// Bluetooth OUTPUT is the one exception to "wired only": a connected A2DP
// speaker (bluez_output.*) is treated as the HIGHEST-priority sink. BT sinks
// are placed FIRST in the sink list, ahead of every USB interface, so the BT
// speaker takes the front FL/FR pair (the main stereo mix). When the speaker
// disconnects its node vanishes, the next pw-mon event re-runs this pass, and
// the USB interface reclaims the front pair — fully dynamic, no restart. With
// no BT connected this is byte-for-byte the original USB-only behaviour.
//
// Non-preferred HDMI, built-in, and MIDI devices are still filtered out —
// the WirePlumber policy in config/wireplumber/50-demiurge.conf demotes HDMI
// nodes to zero priority (enabled but never defaults/drivers), and the
// is_selectable_audio_node guard here decides whether HDMI participates at
// all (only when explicitly preferred or hdmi_default=on).
//
// Grow-only: the Rust launcher re-runs this whenever pw-mon reports a
// node-add or node-remove event affecting a USB audio node. Previously
// assigned channel pairs stay put; newly plugged interfaces take the
// next free pair.

use std::collections::{BTreeMap, HashSet};
use std::process::{Command, Stdio};
use std::sync::{Mutex, OnceLock};

use crate::util;

// Sticky USB master tracking — mirrors PipeWire's own driver-election in
// software so we can log changes and expose the current master for status.
//
// PipeWire's select_driver() uses strict `>` (not `>=`) when comparing
// priority.driver values, so among equal-priority nodes the FIRST-connected
// device stays master when a second is plugged in — naturally sticky, no
// launcher code needed to enforce it. This static just records what PipeWire
// elected so the system log shows the transition clearly.
static STICKY_MASTER: OnceLock<Mutex<Option<String>>> = OnceLock::new();

fn sticky_master_lock() -> &'static Mutex<Option<String>> {
    STICKY_MASTER.get_or_init(|| Mutex::new(None))
}

/// Returns the name of the wired output currently acting as clock master,
/// or None if no wired interface is present (aloop or no-clock state).
pub fn current_master() -> Option<String> {
    sticky_master_lock().lock().unwrap().clone()
}

fn update_sticky_master(primary_wired: Option<&str>) {
    let mut guard = sticky_master_lock().lock().unwrap();
    match (guard.as_deref(), primary_wired) {
        (None, Some(p)) => {
            util::log(&format!("aggregate: clock master: {p}"));
            *guard = Some(p.to_string());
        }
        (Some(m), None) => {
            util::log(&format!("aggregate: clock master removed: {m}"));
            *guard = None;
        }
        (Some(m), Some(p)) if m != p => {
            util::log(&format!("aggregate: clock master changed: {m} → {p}"));
            *guard = Some(p.to_string());
        }
        _ => {}
    }
}

// Channel plan on the virtual layer. Add more pairs here if you grow the
// demiurge-sink / demiurge-source-input nodes to >4 channels in the
// pipewire config.
const SINK_PAIRS: &[(&str, &str)] = &[
    ("output_FL", "output_FR"),
    ("output_RL", "output_RR"),
];
const SOURCE_PAIRS: &[(&str, &str)] = &[
    ("input_FL", "input_FR"),
    ("input_RL", "input_RR"),
];

pub fn apply() {
    // Build the desired edge set, compare against current, apply the diff.
    // Idempotent by construction — called on every pw-mon event and becomes
    // a no-op if nothing changed.

    let mut desired: HashSet<(String, String)> = HashSet::new();

    // OUTPUT. BT speakers first (highest priority), then USB interfaces, so a
    // connected A2DP speaker lands on SINK_PAIRS[0] = front FL/FR.
    //
    // Direct-mode exception: when there is EXACTLY ONE wired USB sink and no BT
    // (direct_output_device() is Some), the chain terminates straight on that
    // device (see graph::out_sink_ports) to skip the loopback's quantum of
    // latency. In that case the loopback must NOT also drive the device — the
    // chain owns it. We simply leave the sink links out of `desired`, so the
    // diff below removes any that survive from a previous multi-sink/BT setup.
    let sinks = output_sinks();
    // One pw-link enumeration for every device-port derivation below.
    let list_i = pw_link_list("-i");

    // Track the wired (USB or platform) clock master and log multi-interface
    // transitions. Predicate excludes bluez, aloop, and non-preferred HDMI.
    let wired_sinks: Vec<&String> = sinks.iter()
        .filter(|s| util::is_selectable_audio_node(s))
        .collect();
    update_sticky_master(wired_sinks.first().map(|s| s.as_str()));
    if wired_sinks.len() > 1 {
        let followers = wired_sinks[1..].iter().map(|s| s.as_str()).collect::<Vec<_>>().join(", ");
        util::log(&format!(
            "aggregate: multi-interface — {} wired outputs, master: {}, followers: {followers}",
            wired_sinks.len(), wired_sinks[0]
        ));
    }

    if direct_output_device().is_none() {
        if sinks.is_empty() {
            // NO real interface (no BT, no USB). Drive the sync layer from the
            // snd-aloop VIRTUAL FALLBACK CLOCK so the graph stays clocked
            // (always-on): link demiurge-sink-output → the aloop sink. Without
            // this the loopback has nothing driving it, the graph idles/SUSPENDS
            // (QUANT 0), and audio doesn't actually flow — passthrough/measurement
            // break. The aloop is lowest priority (config/aloop/90-demiurge-aloop.conf)
            // so a real interface always wins; the diff removes this link the
            // instant one appears.
            if let Some(aloop) = aloop_sink() {
                match util::device_port_pair(&list_i, &aloop, "playback_") {
                    Some((dl, dr)) => {
                        desired.insert(("demiurge-sink-output:output_FL".into(), dl));
                        desired.insert(("demiurge-sink-output:output_FR".into(), dr));
                        util::log(&format!("aggregate: no interface → graph clocked via snd-aloop fallback ({aloop})"));
                    }
                    None => util::log(&format!("aggregate: snd-aloop fallback {aloop} has no playback ports yet")),
                }
            } else {
                util::log("aggregate: no interface and no snd-aloop fallback sink found — graph will suspend");
            }
        } else {
            for (idx, sink) in sinks.iter().take(SINK_PAIRS.len()).enumerate() {
                let (lch, rch) = SINK_PAIRS[idx];
                // Device ports are DERIVED — a Pro Audio card exposes
                // playback_AUX0/AUX1 and no playback_FL whatsoever, and
                // pw-link fails silently on a nonexistent port, so naming
                // them here used to mean silent, error-free dead air.
                let Some((dl, dr)) = util::device_port_pair(&list_i, sink, "playback_") else {
                    util::log(&format!("aggregate: {sink} has no playback ports yet — skipped"));
                    continue;
                };
                desired.insert((format!("demiurge-sink-output:{lch}"), dl));
                desired.insert((format!("demiurge-sink-output:{rch}"), dr));
            }
            if sinks.len() > SINK_PAIRS.len() {
                util::log(&format!("aggregate: {} extra sink(s) ignored (grow channel count to use)", sinks.len() - SINK_PAIRS.len()));
            }
        }
    }

    // INPUT. Mirror of the direct-mode exception above: when there is exactly
    // one capture device (direct_input_device() is Some) the chain's first
    // stage takes it directly (graph::in_source_ports) and the loopback must
    // NOT also be pulling from it. Leaving these out of `desired` makes the
    // diff below tear down any demiurge-source links left over from a
    // multi-interface arrangement, so demiurge-source goes idle instead of
    // running a pointless second copy of the input at its own quantum.
    if direct_input_device().is_none() {
        let sources = enumerate_usb_sources();
        let list_o = pw_link_list("-o");
        for (idx, src) in sources.iter().take(SOURCE_PAIRS.len()).enumerate() {
            let (lch, rch) = SOURCE_PAIRS[idx];
            // Derived pair: capture_FL/FR (ACP), capture_AUX0/AUX1 (Pro
            // Audio), or a lone capture_MONO — which device_port_pair returns
            // as the SAME port twice, so a mono source still lands on one
            // link into the left channel after the set dedupes the pair.
            let Some((sl, sr)) = util::device_port_pair(&list_o, src, "capture_") else { continue };
            desired.insert((sl.clone(), format!("demiurge-source-input:{lch}")));
            if sr != sl {
                desired.insert((sr, format!("demiurge-source-input:{rch}")));
            }
        }
    }

    let current = current_demiurge_links();
    let to_add: Vec<_> = desired.difference(&current).cloned().collect();
    let to_remove: Vec<_> = current.difference(&desired).cloned().collect();

    if to_add.is_empty() && to_remove.is_empty() {
        return; // fully converged — stay silent
    }

    util::log(&format!("aggregate: diff +{} -{}", to_add.len(), to_remove.len()));
    for (src, dst) in &to_remove {
        let _ = Command::new("pw-link").args(["-d", src, dst]).stderr(Stdio::null()).status();
        util::log(&format!("aggregate: unlink {src} → {dst}"));
    }
    for (src, dst) in &to_add {
        let _ = Command::new("pw-link").args([src.as_str(), dst.as_str()]).stderr(Stdio::null()).status();
        util::log(&format!("aggregate: link {src} → {dst}"));
    }
}

// Output sinks in priority order: Bluetooth A2DP first (highest priority),
// then wired USB interfaces ordered by channel count. THE single source of
// truth for "what can play audio right now" — shared with graph (via
// direct_output_device) so routing and aggregation can never disagree.
pub fn output_sinks() -> Vec<String> {
    let mut sinks = enumerate_bt_sinks();
    let bt_len = sinks.len();
    sinks.extend(enumerate_usb_sinks());
    // The user's preferred interface (by product name — live.conf `interface`
    // or the global pick, cached by main) moves to the front of the USB
    // segment: it gets the front pair and clock-master role. HDMI preferences
    // match by HDMI-ness (util::sink_matches_preference) since HDMI product
    // names never appear in PipeWire node names — without that, an explicitly
    // chosen HDMI sink could never win the front pair from an attached USB
    // interface even though is_selectable_audio_node had already admitted it.
    if let Some(want) = util::preferred_interface() {
        if let Some(pos) = sinks[bt_len..].iter().position(|s| util::sink_matches_preference(s, &want)) {
            let s = sinks.remove(bt_len + pos);
            sinks.insert(bt_len, s);
        }
    }
    sinks
}

// Direct-output fast path. When EXACTLY ONE wired USB interface is present —
// no Bluetooth speaker, no second interface — there is nothing to mix, so the
// chain can terminate straight on that device's hardware playback ports and
// skip the demiurge-sink loopback (which otherwise costs a full quantum of
// output latency). Returns the device node name in that case.
//
// The moment a second sink or a BT speaker appears this returns None and
// routing falls back to the loopback so the aggregate keeps its mixing point.
// Both graph::apply and aggregate::apply consult this every tick, so the
// switch is automatic and self-healing on hot-plug.
pub fn direct_output_device() -> Option<String> {
    match output_sinks().as_slice() {
        [only] if util::is_selectable_audio_node(only) => Some(only.clone()),
        _ => None,
    }
}

// Capture sources in priority order — the input-side mirror of output_sinks().
// There is no Bluetooth equivalent here on purpose: DEMIURGE never takes audio
// IN over Bluetooth (HSP/HFP would drag the whole graph down to 8/16 kHz and
// force the A2DP output speaker into the same low-quality duplex profile), so
// this list is wired sources only, preferred interface first.
pub fn input_sources() -> Vec<String> {
    let mut sources = enumerate_usb_sources();
    if let Some(want) = util::preferred_interface() {
        if let Some(pos) = sources.iter().position(|s| util::sink_matches_preference(s, &want)) {
            let s = sources.remove(pos);
            sources.insert(0, s);
        }
    }
    sources
}

// Direct-INPUT fast path — the exact mirror of direct_output_device().
//
// When EXACTLY ONE wired capture source is present there is nothing to merge,
// so the chain's first stage can take that device's capture ports directly and
// skip the demiurge-source loopback. This matters more on the input side than
// it did on the output side: demiurge-source runs at its own quantum (measured
// at 256 while the hardware ran at 64), so the loopback was not costing one
// quantum of the graph's rate — it was costing one quantum of ITS rate.
//
// Returns None — i.e. keep the loopback — when:
//   - two or more capture devices are attached (the loopback is the merge
//     point, exactly as demiurge-sink is on the output side), or
//   - no selectable capture device is attached at all. The aloop is excluded
//     by is_selectable_audio_node, which is what preserves the always-on
//     virtual-clock design: with nothing plugged in, the sync layer still
//     clocks off snd-aloop through the loopback and testing/latency's
//     `internal` mode still has an entrance to inject at.
//
// Consulted fresh on every tick by both graph::apply and aggregate::apply, so
// hot-plugging a second interface switches the whole rig back to the loopback
// with no restart — same self-healing property the output path has.
pub fn direct_input_device() -> Option<String> {
    match input_sources().as_slice() {
        [only] if util::is_selectable_audio_node(only) => Some(only.clone()),
        _ => None,
    }
}

// Snapshot every current link where either side touches the DEMIURGE
// virtual aggregation layer (demiurge-sink-output / demiurge-source-input).
// Only these links are under aggregate's control — we never touch
// user/session patch-graph links here.
fn current_demiurge_links() -> HashSet<(String, String)> {
    let listing = pw_link_list("-l");
    let mut out: HashSet<(String, String)> = HashSet::new();
    let mut current_header: Option<String> = None;
    for line in listing.lines() {
        if !line.starts_with(' ') && !line.is_empty() {
            current_header = Some(line.to_string());
            continue;
        }
        let Some(header) = current_header.as_ref() else { continue; };
        let trimmed = line.trim_start();
        if let Some(peer) = trimmed.strip_prefix("|-> ") {
            // header is a source port, peer is a destination
            if header.starts_with("demiurge-sink-output:") || peer.starts_with("demiurge-source-input:") {
                out.insert((header.clone(), peer.to_string()));
            }
        } else if let Some(peer) = trimmed.strip_prefix("|<- ") {
            // header is a destination port, peer is a source
            if peer.starts_with("demiurge-sink-output:") || header.starts_with("demiurge-source-input:") {
                out.insert((peer.to_string(), header.clone()));
            }
        }
    }
    out
}

fn enumerate_usb_sinks() -> Vec<String> {
    enumerate_usb("-i", "playback_")
}

// The snd-aloop VIRTUAL FALLBACK-CLOCK sink (ACP profile node, e.g.
// alsa_output.platform-snd_aloop.0.analog-stereo). When no real interface is
// present, demiurge-sink-output is linked here so the aloop drives the graph and
// the sync layer stays clocked / always-on. Lowest priority (see
// config/aloop/90-demiurge-aloop.conf) so a real interface always outranks it.
fn aloop_sink() -> Option<String> {
    pw_link_list("-i")
        .lines()
        .filter_map(|l| l.split(':').next())
        .find(|c| c.starts_with("alsa_output.") && c.contains("snd_aloop"))
        .map(|s| s.to_string())
}

// Bluetooth A2DP output sinks: `bluez_output.<MAC>.<n>` nodes that expose a
// stereo playback_FL/FR pair. These are the ONE non-wired sink DEMIURGE links.
// Returned ahead of USB sinks by the caller so a connected speaker is always
// the front (highest-priority) output. Sorted by node name for a stable order
// if (rarely) two speakers are connected at once.
fn enumerate_bt_sinks() -> Vec<String> {
    let listing = pw_link_list("-i");
    let mut sinks: Vec<String> = Vec::new();
    for line in listing.lines() {
        let Some((client, port)) = line.split_once(':') else { continue; };
        if !client.starts_with("bluez_output.") { continue; }
        if port == "playback_FL" && !sinks.iter().any(|s| s == client) {
            sinks.push(client.to_string());
        }
    }
    sinks.sort();
    sinks
}

fn enumerate_usb_sources() -> Vec<String> {
    // One prefix pass now covers what used to be two name-specific passes
    // (capture_FL then capture_MONO) — and additionally covers capture_AUX0
    // on Pro Audio, which neither of the old passes matched.
    let mut sources = enumerate_usb("-o", "capture_");
    // The Pocket OpGorator (Daisy Seed) USB capture device is bridged ONLY
    // through the explicit `opgorator` chain token (live.rs / supervisor.rs
    // / graph.rs), never through this generic USB auto-bridge into
    // demiurge-source. Auto-bridging here too would double-path its audio
    // whenever an `opgorator` chain exists (both the explicit chain AND
    // this demiurge-source injection), and would silently inject it even
    // when no chain declares it at all. Exclude unconditionally, whether or
    // not an `opgorator` chain is present, so behavior is predictable:
    // silent unless explicitly declared.
    sources.retain(|s| !s.contains("OpGorator") && !s.contains("Daisy"));
    sources
}

// List unique selectable ALSA client names (util::is_selectable_audio_node —
// USB + platform, minus aloop and non-preferred HDMI) that expose ports in
// the given direction (`prefix` = "playback_" | "capture_").
//
// MATCHES ON DIRECTION, NOT ON A PORT NAME. This used to take an exact port
// suffix (":playback_FL"), which quietly made "is this a sink?" mean "does it
// have a port called playback_FL?" — false for every card on the Pro Audio
// profile, whose ports are playback_AUX0..7. The device then vanished from
// enumeration entirely: not misrouted, simply absent, so the rig fell through
// to the aloop fallback and played nothing out of the real card.
//
// Ordering is BTreeMap-sorted (smallest channel count first, tie-broken
// by device name) — good enough until the Rust launcher starts actually
// asking each device for its channel count; for now the channel count
// proxy is the number of playback_*/capture_* ports on the device.
fn enumerate_usb(flag: &str, prefix: &str) -> Vec<String> {
    let listing = pw_link_list(flag);
    // client → channel port count, used for ordering
    let mut channel_counts: BTreeMap<String, usize> = BTreeMap::new();
    // First pass: every selectable client carrying this direction
    let mut candidates: Vec<String> = Vec::new();
    for line in listing.lines() {
        let Some((client, port)) = line.split_once(':') else { continue; };
        if !util::is_selectable_audio_node(client) { continue; }
        if port.starts_with(prefix) && !candidates.contains(&client.to_string()) {
            candidates.push(client.to_string());
        }
    }
    // Second pass: count channel-shaped ports per candidate
    for line in listing.lines() {
        let Some((client, port)) = line.split_once(':') else { continue; };
        if !candidates.contains(&client.to_string()) { continue; }
        if port.starts_with("playback_") || port.starts_with("capture_") {
            *channel_counts.entry(client.to_string()).or_insert(0) += 1;
        }
    }
    // Sort by (channel count, name)
    let mut ordered: Vec<(usize, String)> = channel_counts.into_iter()
        .map(|(name, count)| (count, name))
        .collect();
    ordered.sort();
    ordered.into_iter().map(|(_, name)| name).collect()
}

fn pw_link_list(flag: &str) -> String {
    Command::new("pw-link")
        .arg(flag)
        .output()
        .map(|o| String::from_utf8_lossy(&o.stdout).into_owned())
        .unwrap_or_default()
}
