// Small shared helpers: logging, SIGTERM handling, path utilities.

use std::fs::{self, OpenOptions};
use std::io::Write;
use std::path::PathBuf;
use std::process::{Command, Stdio};
use std::os::unix::process::CommandExt;
use std::sync::OnceLock;
use std::sync::atomic::{AtomicBool, Ordering};
use std::thread;
use std::time::Duration;

use crate::live::LiveConfig;

// Graph rate and quantum are both sourced from live.conf (`rate =`,
// `quantum =`) and plumbed through `pin_clock_metadata` and into every
// child wrapper via DEMIURGE_RATE / DEMIURGE_QUANTUM env vars. The
// defaults are 48000 / 128 = ~2.67 ms per buffer hop, and are FALLBACKS
// ONLY — used when live.conf is missing or omits the key. The authoritative
// quantum lives in ~/demiurge/live.conf (`quantum =`); keep this default
// equal to it so the failure path can never disagree with the running graph
// (a fallback below the graph quantum = total silence, gotchas Q7a).
// Changing rate at
// runtime forces a full restart of every stage because rate is baked
// into JACK clients at spawn time (chuck --srate, csound -r). The
// monitor.alsa.rules in WirePlumber config also pin device.bus = "usb"
// to this rate — keep it in sync if you ever override away from 48k.
pub const DEFAULT_RATE:    u32 = 48000;
pub const DEFAULT_QUANTUM: u32 = 128;

// Make sure this user's PipeWire/WirePlumber are running (sync-layer mode). A
// previous `sync_layer = off` run stops them; restart them so the graph exists.
pub fn ensure_pipewire_running() {
    let _ = Command::new("systemctl")
        .args(["--user", "start", "pipewire", "wireplumber", "pipewire-pulse"])
        .stderr(Stdio::null()).status();
}

// sync_layer = off path. The launcher itself bypasses the PipeWire graph and
// runs the chain's first program directly on the audio interface (raw ALSA,
// lowest latency), then `exec`s into it so systemd supervises the program. This
// keeps the mode decision INSIDE the launcher, driven only by live.conf — there
// is no separate mode-select service. Engine-agnostic: the program is run via
// its normal wrapper with DEMIURGE_DIRECT=1; each wrapper honours that flag
// (Csound is the first). Returns only on failure (then the caller exits).
pub fn run_bypass(lc: &LiveConfig, live_path: &str) {
    let home = std::env::var("HOME").unwrap_or_else(|_| "/home/gorpi7".into());

    let prog_raw = lc.chains.get(0).and_then(|c| c.stages.get(0)).cloned().unwrap_or_default();
    if prog_raw.is_empty() {
        log("sync_layer=off but the chain is empty — nothing to run");
        return;
    }
    let program = match prog_raw.strip_prefix('~') {
        Some(rest) => format!("{home}{rest}"),
        None => prog_raw,
    };
    // Legacy default string, only used when nothing resolves AND the user
    // named nothing — see resolve_bypass_iface (it now probes instead).
    let iface = lc.audio_interface.clone().unwrap_or_else(|| "hw:CARD=USB,DEV=0".into());
    let rate = lc.rate.unwrap_or(DEFAULT_RATE);
    // Bypass owns the card directly so WirePlumber's period-size is out of the
    // picture, but the ksmps constraint is not: csound rounds -b up to a ksmps
    // multiple and dies when -b is below ksmps, exactly as in sync mode. Vet
    // here too — a bypass rig with a bad quantum is a systemd restart loop with
    // no audio and no explanation.
    let quantum = vet_quantum(live_path, lc.quantum.unwrap_or(DEFAULT_QUANTUM));

    // Resolve the program's wrapper by file type (same lang map as the launcher).
    let lang = match program.rsplit('.').next().unwrap_or("") {
        "csd" => "csound", "ck" => "chuck", "dsp" => "faust",
        "scd" => "sc", "pd" => "pd", "cpp" => "cpp", "strudel" => "strudel",
        _ => "",
    };
    if lang.is_empty() {
        log(&format!("sync_layer=off: no direct-mode wrapper for '{program}'"));
        return;
    }
    let wrapper = format!("/opt/demiurge/bin/demiurge-run-{lang}");

    log(&format!("sync_layer=off — '{program}' direct on {iface} (PipeWire graph bypassed)"));

    // Own the hardware: stop this user's PipeWire graph.
    let _ = Command::new("systemctl")
        .args(["--user", "stop", "pipewire", "wireplumber", "pipewire.socket", "pipewire-pulse.socket"])
        .stderr(Stdio::null()).status();
    thread::sleep(Duration::from_millis(800));

    // Resolve which interface to run on (set-explicit → global → legacy →
    // best attached; see resolve_interface). The result is FORCED into the
    // engine via DEMIURGE_AUDIO_IF — never the wrapper's hardcoded default.
    let Some(iface) = resolve_bypass_iface(lc, &iface) else {
        // Honest failure: resolve_bypass_iface already logged the missing
        // device and the cards that exist. Return -> caller exits nonzero.
        return;
    };

    // MIDI routing is NOT the launcher's job — DEMIURGE stays device-agnostic.
    // The program reads the shared "Midi Through" pool (its wrapper's -M flag);
    // whatever FEEDS that pool — a project's own controller/serial-bridge service,
    // an aconnect rule, etc. — is configured outside DEMIURGE in the user's own
    // setup (e.g. NEPTR's neptr-bridge.service). Nothing device-specific here.

    // Playback-only cards (HDMI, output-only USB DACs) have no capture PCM;
    // telling the wrapper up front (DEMIURGE_AUDIO_CAPTURE=none → it omits
    // -iadc) is what keeps Csound from restart-looping on an unopenable
    // capture device. Generic by construction: "resolved card has no capture
    // → run output-only", nothing HDMI-specific.
    let capture = iface.split("CARD=").nth(1)
        .and_then(|s| s.split(',').next())
        .map(card_has_capture)
        .unwrap_or(true);
    if !capture {
        log(&format!("interface: {iface} has no capture PCM — output-only (DEMIURGE_AUDIO_CAPTURE=none)"));
    }

    // Hand off to the wrapper in direct mode — replaces this process so systemd
    // supervises the program directly.
    let err = Command::new(&wrapper)
        .arg(&program)
        .env("DEMIURGE_DIRECT", "1")
        .env("DEMIURGE_AUDIO_CAPTURE", if capture { "auto" } else { "none" })
        .env("DEMIURGE_AUDIO_IF", &iface)
        .env("DEMIURGE_RATE", rate.to_string())
        .env("DEMIURGE_QUANTUM", quantum.to_string())
        .exec();   // only returns on failure
    log(&format!("sync_layer=off: exec {wrapper} failed: {err}"));
}

// ---------- Audio interface resolution ----------
//
// Interfaces are identified by PRODUCT NAME ("Volt 2", "Scarlett Solo USB"),
// never by hw:N or card id — those change with plug order. Resolution order:
//
//   1. live.conf `interface = <name>`   (per-set explicit)     — if attached
//   2. ~/demiurge/interface.conf        (global, last UI pick) — if attached
//   3. live.conf `audio_interface`      (legacy CARD= id)      — if attached
//   4. first attached real interface    (never loopback / MIDI-only; HDMI
//      skipped unless live.conf `hdmi_default = on`)
//
// Rules 1-3 CAN land on an HDMI card — explicit user intent wins. Only the
// rule-4 fallback refuses HDMI by default.
//
// See docs/superpowers/specs/2026-07-26-interface-selection-design.md and
// docs/superpowers/specs/2026-08-05-hifiberry-hdmi-interface-design.md.

#[derive(Clone, Debug)]
pub struct AudioCard {
    pub id:   String,   // ALSA card id, e.g. "V2" — valid only for this boot/plug
    pub name: String,   // product name, e.g. "Volt 2" — the stable identity
    pub is_hdmi: bool,  // vc4hdmi* — selectable only explicitly (or hdmi_default=on)
}

// Case-insensitive, underscore/space-insensitive containment either way, so
// "Volt 2" matches card name "Volt 2" and PipeWire-ish "Volt_2" spellings.
fn iface_norm(s: &str) -> String {
    s.to_ascii_lowercase().replace('_', " ").trim().to_string()
}

pub fn iface_name_matches(candidate: &str, want: &str) -> bool {
    let c = iface_norm(candidate);
    let w = iface_norm(want);
    !w.is_empty() && (c.contains(&w) || w.contains(&c))
}

// Whether an interface-preference string names an HDMI device ("vc4-hdmi-0",
// "HDMI 1", ...). HDMI ALSA product names never appear verbatim inside
// PipeWire node names (alsa_output.platform-...hdmi...), so HDMI preferences
// are matched by their HDMI-ness rather than by substring.
pub fn preference_is_hdmi(want: &str) -> bool {
    iface_norm(want).contains("hdmi")
}

// Sink-node ↔ preference matcher for sync-layer sink ordering
// (aggregate::output_sinks): normal product names match by substring; an
// HDMI preference matches any HDMI node. Known limitation: with cables in
// BOTH HDMI ports the first-enumerated HDMI node wins regardless of which
// vc4-hdmi-N was named — acceptable, one HDMI audio cable in practice.
pub fn sink_matches_preference(node: &str, want: &str) -> bool {
    iface_name_matches(node, want)
        || (preference_is_hdmi(want) && node.to_ascii_lowercase().contains("hdmi"))
}

// THE hardware port-pair derivation, shared by graph (direct in/out paths)
// and aggregate (sink/source linking) so they can never disagree about which
// two ports of a device carry the stereo mix.
//
// WHY THIS IS NOT A CONSTANT. A hardware node's port NAMES are not stable
// across ALSA card profiles — they are a property of the profile, not of the
// card. The same duplex I2S codec presents as:
//
//   ACP "stereo-fallback" profile   playback_FL / playback_FR
//   Pro Audio profile               playback_AUX0 .. playback_AUX7
//   single-input USB capture        capture_MONO (one port, both sides)
//
// Hardcoding FL/FR meant that switching a card to Pro Audio produced an
// entirely unlinked graph: pw-link was asked for ports that do not exist, it
// failed silently per-edge, and the rig came up in DEAD SILENCE with every
// service reporting active. That failure mode is why this is derived rather
// than listed — a device-general rule beats an ever-growing name table, and a
// card with some third naming scheme now works with no code change at all.
//
// Order of preference:
//   1. the canonical FRONT pair (<prefix>FL / <prefix>FR) when both exist.
//      This preserves the previous behaviour byte-for-byte on every ACP
//      device, and on a card that declares a front pair it is also the
//      correct channel choice — not merely the compatible one.
//   2. otherwise the FIRST TWO ports carrying this direction, in pw-link
//      enumeration order, which is the device's own channel order (so Pro
//      Audio's AUX0/AUX1 = the codec's first output pair = the same physical
//      jacks the front pair mapped to).
//   3. otherwise, with exactly one port, that port for BOTH sides — the
//      existing capture_MONO behaviour, now reached generically.
//
// `listing` is the output of `pw-link -i` (for playback_) or `pw-link -o`
// (for capture_); passing it in keeps this a pure function and lets callers
// that link many devices at once pay for only one pw-link invocation.
pub fn device_port_pair(listing: &str, node: &str, prefix: &str) -> Option<(String, String)> {
    let np = format!("{node}:");
    let ports: Vec<&str> = listing
        .lines()
        .filter(|l| l.starts_with(&np) && l[np.len()..].starts_with(prefix))
        .collect();

    let fl = format!("{np}{prefix}FL");
    let fr = format!("{np}{prefix}FR");
    if ports.contains(&fl.as_str()) && ports.contains(&fr.as_str()) {
        return Some((fl, fr));
    }
    match ports.as_slice() {
        [] => None,
        [only] => Some((only.to_string(), only.to_string())),
        [l, r, ..] => Some((l.to_string(), r.to_string())),
    }
}

// Does this ALSA card have a capture PCM? /proc/asound/<id>/ contains pcmNc
// entries for capture and pcmNp for playback; HDMI (vc4hdmi*) and output-only
// USB DACs expose pcm0p only. Unreadable → assume capture exists (status quo:
// the wrapper opens -iadc as before).
fn card_has_capture(card_id: &str) -> bool {
    fs::read_dir(format!("/proc/asound/{card_id}"))
        .map(|rd| rd.flatten().any(|e| {
            let n = e.file_name().to_string_lossy().into_owned();
            n.starts_with("pcm") && n.ends_with('c')
        }))
        .unwrap_or(true)
}

// Playback-capable ALSA cards (`aplay -l`), excluding the loopback cards
// (snd-aloop). HDMI cards (vc4hdmi*) ARE listed — tagged is_hdmi so the
// resolver's rule-4 fallback can skip them while explicit selection still
// works. Order = ALSA card order (bus-agnostic: USB and platform/I2S HATs
// like the HiFiBerry appear alike). MIDI-only devices never appear in
// `aplay -l` so they are excluded for free.
pub fn list_playback_cards() -> Vec<AudioCard> {
    let mut cards: Vec<AudioCard> = Vec::new();
    if let Ok(o) = Command::new("aplay").arg("-l").output() {
        for l in String::from_utf8_lossy(&o.stdout).lines() {
            // "card 2: V2 [Volt 2], device 0: USB Audio [USB Audio]"
            if !l.starts_with("card ") { continue; }
            let Some(rest) = l.split_once(':').map(|(_, r)| r) else { continue };
            let Some(id) = rest.split_whitespace().next() else { continue };
            if id == "DemiurgeLoop" || id == "Loopback" { continue; }
            let name = rest.split_once('[')
                .and_then(|(_, r)| r.split_once(']'))
                .map(|(n, _)| n.trim().to_string())
                .unwrap_or_else(|| id.to_string());
            if !cards.iter().any(|c| c.id == id) {
                cards.push(AudioCard {
                    id: id.to_string(),
                    name,
                    is_hdmi: id.starts_with("vc4hdmi"),
                });
            }
        }
    }
    cards
}

// Global remembered choice — the last interface the user explicitly picked in
// any UI. Lives OUTSIDE live.conf because demiurge-set overwrites live.conf
// wholesale on every patch switch. Written only by demiurge-interface.
pub fn read_global_interface() -> Option<String> {
    let home = std::env::var("HOME").unwrap_or_else(|_| "/home/gorpi7".into());
    let text = fs::read_to_string(format!("{home}/demiurge/interface.conf")).ok()?;
    for raw in text.lines() {
        let line = raw.split('#').next().unwrap_or("").trim();
        if line.is_empty() { continue; }
        let name = match line.split_once('=') {
            Some((k, v)) if k.trim() == "interface" => v.trim(),
            Some(_) => continue,
            None => line,
        };
        if !name.is_empty() { return Some(name.to_string()); }
    }
    None
}

// One resolution pass over the currently attached cards. Returns the card and
// which rule chose it ("set" | "global" | "legacy" | "attached").
pub fn resolve_interface(lc: &LiveConfig) -> Option<(AudioCard, &'static str)> {
    let cards = list_playback_cards();
    if let Some(want) = &lc.interface {
        if let Some(c) = cards.iter().find(|c| iface_name_matches(&c.name, want)) {
            return Some((c.clone(), "set"));
        }
    }
    if let Some(want) = read_global_interface() {
        if let Some(c) = cards.iter().find(|c| iface_name_matches(&c.name, &want)) {
            return Some((c.clone(), "global"));
        }
    }
    if let Some(dev) = &lc.audio_interface {
        if let Some(id) = dev.split("CARD=").nth(1).and_then(|s| s.split(',').next()) {
            if let Some(c) = cards.iter().find(|c| c.id == id) {
                return Some((c.clone(), "legacy"));
            }
        }
    }
    // Rule 4: first attached — HDMI cards are skipped unless live.conf
    // `hdmi_default = on`, in which case they join AFTER every non-HDMI card.
    let (non_hdmi, hdmi): (Vec<AudioCard>, Vec<AudioCard>) =
        cards.into_iter().partition(|c| !c.is_hdmi);
    non_hdmi.into_iter().next()
        .or_else(|| if lc.hdmi_default { hdmi.into_iter().next() } else { None })
        .map(|c| (c, "attached"))
}

// True when live.conf or the global file names a specific interface — used to
// decide whether "first attached" deserves a grace wait (give the preferred
// device time to enumerate) or should be adopted immediately.
fn has_interface_preference(lc: &LiveConfig) -> bool {
    lc.interface.is_some()
        || read_global_interface().is_some()
        || lc.audio_interface.as_deref().is_some_and(|d| d.contains("CARD="))
}

// The preferred interface NAME for sink ordering in sync-layer mode (rules
// 1-2 only — the legacy CARD id is bypass-specific). aggregate::output_sinks
// puts the matching USB sink first so it becomes the front pair/clock master.
pub fn preferred_interface_name(lc: &LiveConfig) -> Option<String> {
    lc.interface.clone().or_else(read_global_interface)
}

// Cached copy for aggregate::output_sinks, which runs on every pw-mon event
// and has no LiveConfig in scope. main refreshes it at startup and on every
// live.conf reload.
static PREFERRED_IFACE: OnceLock<std::sync::Mutex<Option<String>>> = OnceLock::new();

pub fn set_preferred_interface(name: Option<String>) {
    let cell = PREFERRED_IFACE.get_or_init(|| std::sync::Mutex::new(None));
    *cell.lock().unwrap() = name;
}

pub fn preferred_interface() -> Option<String> {
    PREFERRED_IFACE.get().and_then(|c| c.lock().unwrap().clone())
}

// Cached live.conf `hdmi_default` (default off). Like PREFERRED_IFACE, main
// refreshes it at startup and on every live.conf reload so the node predicate
// below (which runs on pw-mon events with no LiveConfig in scope) stays true.
static HDMI_DEFAULT: AtomicBool = AtomicBool::new(false);

pub fn set_hdmi_default(v: bool) {
    HDMI_DEFAULT.store(v, Ordering::Relaxed);
}

pub fn hdmi_default() -> bool {
    HDMI_DEFAULT.load(Ordering::Relaxed)
}

// Cached live.conf `input` (default ON — the selected interface supplies both
// directions). Same pattern and the same reason as HDMI_DEFAULT: graph::apply
// runs on pw-mon events with no LiveConfig in scope. Stored as the positive
// sense here even though live.rs stores the negative, because every reader
// wants to ask "is the default input on?".
static DEFAULT_INPUT: AtomicBool = AtomicBool::new(true);

pub fn set_default_input(v: bool) {
    DEFAULT_INPUT.store(v, Ordering::Relaxed);
}

pub fn default_input() -> bool {
    DEFAULT_INPUT.load(Ordering::Relaxed)
}

// THE shared PipeWire node predicate for "is this ALSA node a selectable
// DEMIURGE interface node" (sync-layer mode). Used by aggregate (sink/source
// enumeration, sticky master, direct output), devices (live.conf header scan)
// and events (pw-mon watch) so they can never disagree. Any alsa_output.* /
// alsa_input.* node qualifies — USB and platform (I2S HAT, e.g. HiFiBerry)
// alike — EXCEPT:
//   - snd_aloop nodes: the aloop keeps its dedicated fallback-clock role
//     (aggregate::aloop_sink) and never appears as a selectable interface;
//   - HDMI nodes ("hdmi" in the name, case-insensitive): visible hardware but
//     never auto-adopted. Eligible only when the preferred interface name is
//     itself an HDMI device (explicit user intent — HDMI product names like
//     "vc4-hdmi-0" don't appear verbatim in PipeWire node names, so we match
//     on the HDMI-ness of the preference, not by substring) or when live.conf
//     `hdmi_default = on`.
pub fn is_selectable_audio_node(name: &str) -> bool {
    if !name.starts_with("alsa_output.") && !name.starts_with("alsa_input.") {
        return false;
    }
    if name.contains("snd_aloop") { return false; }
    if name.to_ascii_lowercase().contains("hdmi") {
        if hdmi_default() { return true; }
        return preferred_interface().is_some_and(|w| preference_is_hdmi(&w));
    }
    true
}

// ---------- /proc/asound/cards probe ----------
//
// Used when no interface is named (or a named one never shows up and
// `audio_fallback` allows it). Order: a USB card, else any other non-HDMI card
// (HiFiBerry / I2S HAT), else HDMI (vc4hdmi*). Loopback cards never qualify.

#[derive(Clone, Debug, PartialEq)]
pub struct ProcCard {
    pub id: String,      // "USB", "sndrpihifiberry", "vc4hdmi0"
    pub driver: String,  // "USB-Audio", "snd_rpi_hifiberry_dacplusadcpro", "vc4-hdmi"
    pub name: String,    // long name
    pub is_usb: bool,
    pub is_hdmi: bool,
}

// Parse /proc/asound/cards text. Entries are two lines:
//  " 2 [USB           ]: USB-Audio - Scarlett Solo USB"
//  "                      Focusrite Scarlett Solo USB at usb-0000:01:00.0-1.1, full speed"
pub fn parse_proc_cards(text: &str) -> Vec<ProcCard> {
    let mut out: Vec<ProcCard> = Vec::new();
    let mut detail_for: Option<usize> = None;
    for line in text.lines() {
        let t = line.trim();
        if t.is_empty() { continue; }
        let head = t.split_whitespace().next().unwrap_or("");
        if head.chars().all(|c| c.is_ascii_digit()) && t.contains('[') && t.contains("]:") {
            let id = t.split_once('[').and_then(|(_, r)| r.split_once(']'))
                .map(|(i, _)| i.trim().to_string()).unwrap_or_default();
            let rest = t.split_once("]:").map(|(_, r)| r.trim()).unwrap_or("");
            let (driver, name) = match rest.split_once(" - ") {
                Some((d, n)) => (d.trim().to_string(), n.trim().to_string()),
                None => (rest.to_string(), String::new()),
            };
            let is_usb = driver.to_ascii_lowercase().contains("usb");
            let is_hdmi = id.to_ascii_lowercase().starts_with("vc4hdmi")
                || driver.to_ascii_lowercase().contains("hdmi");
            out.push(ProcCard { id, driver, name, is_usb, is_hdmi });
            detail_for = Some(out.len() - 1);
        } else if let Some(i) = detail_for.take() {
            if t.to_ascii_lowercase().contains(" at usb-") { out[i].is_usb = true; }
        }
    }
    out
}

// Pick per the order above; `playable` filters out capture-only cards.
pub fn probe_pick<'a>(cards: &'a [ProcCard], playable: impl Fn(&str) -> bool)
    -> Option<(&'a ProcCard, &'static str)>
{
    let ok = |c: &&ProcCard| {
        c.id != "Loopback" && c.id != "DemiurgeLoop"
            && !c.driver.to_ascii_lowercase().contains("loopback")
            && playable(&c.id)
    };
    if let Some(c) = cards.iter().filter(ok).find(|c| c.is_usb && !c.is_hdmi) {
        return Some((c, "USB audio card"));
    }
    if let Some(c) = cards.iter().filter(ok).find(|c| !c.is_usb && !c.is_hdmi) {
        return Some((c, "I2S/other non-HDMI card"));
    }
    cards.iter().filter(ok).find(|c| c.is_hdmi).map(|c| (c, "only HDMI audio is present"))
}

fn card_has_playback(card_id: &str) -> bool {
    fs::read_dir(format!("/proc/asound/{card_id}"))
        .map(|rd| rd.flatten().any(|e| {
            let n = e.file_name().to_string_lossy().into_owned();
            n.starts_with("pcm") && n.ends_with('p')
        }))
        .unwrap_or(true)
}

fn probe_proc_cards() -> Option<(ProcCard, &'static str)> {
    let text = fs::read_to_string("/proc/asound/cards").unwrap_or_default();
    let cards = parse_proc_cards(&text);
    probe_pick(&cards, card_has_playback).map(|(c, why)| (c.clone(), why))
}

fn describe_cards() -> String {
    let text = fs::read_to_string("/proc/asound/cards").unwrap_or_default();
    let cards = parse_proc_cards(&text);
    if cards.is_empty() { return "none".into(); }
    cards.iter().map(|c| format!("{} ({})", c.id, c.name)).collect::<Vec<_>>().join(", ")
}

// Resolve the bypass-mode ALSA device string, bounded — never waits forever.
//
//  * Nothing named (no audio_interface / interface / global pick): a short
//    5s hot-plug grace for a normal card, then the /proc/asound/cards probe
//    (USB > I2S > HDMI), so a stock Pi with only HDMI audio just works.
//  * Something named: wait up to `audio_wait` secs (default 30; when a
//    preference exists, unlisted-but-attached hardware gets a 5s grace first
//    so the preferred device may enumerate late). If it never appears:
//    `audio_fallback = auto` -> probe; otherwise log an honest error naming
//    the missing device + existing cards and return None (caller exits
//    nonzero so systemd shows the failure).
//  * A non-CARD= `audio_interface` string (e.g. plughw:1,0) cannot be
//    verified against the card list; it is used as given at timeout.
fn resolve_bypass_iface(lc: &LiveConfig, configured: &str) -> Option<String> {
    let explicit = has_interface_preference(lc) || lc.audio_interface.is_some();
    let wait = if explicit { lc.audio_wait.unwrap_or(30).max(1) } else { 5 };
    let fallback_ok = lc.audio_fallback.unwrap_or(!explicit);
    let grace = if has_interface_preference(lc) { 5 } else { 0 };
    for i in 0..wait {
        if let Some((card, rule)) = resolve_interface(lc) {
            if rule != "attached" || i >= grace {
                log(&format!("interface: '{}' (card {}) via {rule}", card.name, card.id));
                return Some(format!("hw:CARD={},DEV=0", card.id));
            }
        }
        log("waiting for an audio interface to enumerate...");
        thread::sleep(Duration::from_secs(1));
    }
    let named = lc.interface.clone()
        .or_else(read_global_interface)
        .or_else(|| lc.audio_interface.clone())
        .unwrap_or_else(|| "(none named)".into());
    if explicit {
        if lc.audio_interface.as_deref().is_some_and(|d| !d.contains("CARD=")) && !fallback_ok {
            log(&format!("interface: '{configured}' not verifiable against the card list — using as configured"));
            return Some(configured.to_string());
        }
        log(&format!("ERROR: audio interface '{named}' did not appear within {wait}s; cards present: {}",
            describe_cards()));
    }
    if fallback_ok {
        if let Some((c, why)) = probe_proc_cards() {
            log(&format!("interface: probing /proc/asound/cards -> card {} ('{}'): {why}", c.id, c.name));
            return Some(format!("hw:CARD={},DEV=0", c.id));
        }
        log(&format!("ERROR: no usable audio card found (cards present: {})", describe_cards()));
        return None;
    }
    log("ERROR: audio_fallback is off — exiting so systemd shows the failure (set `audio_fallback = auto` to probe instead)");
    None
}

#[cfg(test)]
mod probe_tests {
    use super::*;

    const HDMI_ONLY: &str = " 0 [vc4hdmi0       ]: vc4-hdmi - vc4-hdmi-0
                      vc4-hdmi-0
 1 [vc4hdmi1       ]: vc4-hdmi - vc4-hdmi-1
                      vc4-hdmi-1
";
    const USB_RIG: &str = " 0 [vc4hdmi0       ]: vc4-hdmi - vc4-hdmi-0
                      vc4-hdmi-0
 1 [sndrpihifiberry]: RPi-simple - snd_rpi_hifiberry_dacplusadcpro
                      snd_rpi_hifiberry_dacplusadcpro
 2 [USB            ]: USB-Audio - Scarlett Solo USB
                      Focusrite Scarlett Solo USB at usb-0000:01:00.0-1.1, full speed
 3 [Loopback       ]: Loopback - Loopback
                      Loopback 1
";
    const I2S: &str = " 0 [vc4hdmi0       ]: vc4-hdmi - vc4-hdmi-0
                      vc4-hdmi-0
 1 [sndrpihifiberry]: RPi-simple - snd_rpi_hifiberry_dacplusadcpro
                      snd_rpi_hifiberry_dacplusadcpro
";

    #[test]
    fn parses_cards() {
        let c = parse_proc_cards(USB_RIG);
        assert_eq!(c.len(), 4);
        assert_eq!(c[2].id, "USB");
        assert!(c[2].is_usb && !c[2].is_hdmi);
        assert!(c[0].is_hdmi);
        assert_eq!(c[1].id, "sndrpihifiberry");
        assert!(!c[1].is_usb && !c[1].is_hdmi);
    }

    #[test]
    fn hdmi_only_picks_hdmi() {
        let c = parse_proc_cards(HDMI_ONLY);
        let (p, why) = probe_pick(&c, |_| true).unwrap();
        assert_eq!(p.id, "vc4hdmi0");
        assert!(why.contains("HDMI"));
    }

    #[test]
    fn usb_beats_i2s_and_hdmi() {
        let c = parse_proc_cards(USB_RIG);
        assert_eq!(probe_pick(&c, |_| true).unwrap().0.id, "USB");
    }

    #[test]
    fn i2s_beats_hdmi() {
        let c = parse_proc_cards(I2S);
        assert_eq!(probe_pick(&c, |_| true).unwrap().0.id, "sndrpihifiberry");
    }

    #[test]
    fn skips_loopback_and_unplayable() {
        let c = parse_proc_cards(USB_RIG);
        let p = probe_pick(&c, |id| id != "USB").unwrap();
        assert_eq!(p.0.id, "sndrpihifiberry");
        assert!(probe_pick(&parse_proc_cards(" 3 [Loopback       ]: Loopback - Loopback\n"), |_| true).is_none());
        assert!(probe_pick(&[], |_| true).is_none());
    }
}

static LOG_PATH: OnceLock<PathBuf> = OnceLock::new();
static SHUTDOWN: AtomicBool = AtomicBool::new(false);

pub fn log_init() {
    let home = std::env::var("HOME").unwrap_or_else(|_| "/tmp".into());
    let dir = PathBuf::from(format!("{home}/.demiurge/logs"));
    let _ = fs::create_dir_all(&dir);
    let _ = LOG_PATH.set(dir.join("launcher.log"));
}

pub fn log(msg: &str) {
    let stamp = timestamp();
    let line = format!("[{stamp}] {msg}\n");
    eprint!("{line}");
    if let Some(path) = LOG_PATH.get() {
        if let Ok(mut f) = OpenOptions::new().create(true).append(true).open(path) {
            let _ = f.write_all(line.as_bytes());
        }
    }
}

fn timestamp() -> String {
    // HH:MM:SS from system time. Good enough for a local log.
    use std::time::{SystemTime, UNIX_EPOCH};
    let secs = SystemTime::now().duration_since(UNIX_EPOCH).map(|d| d.as_secs()).unwrap_or(0);
    let h = (secs / 3600) % 24;
    let m = (secs / 60) % 60;
    let s = secs % 60;
    format!("{h:02}:{m:02}:{s:02}")
}

// ---------- Quantum safety ----------
//
// A `quantum` in live.conf is not automatically runnable. csound's -b must be a
// multiple of the patch's ksmps, and PipeWire cannot drive the graph below the
// device's api.alsa.period-size. Break either and csound exits during init —
// its JACK ports disappear, graph::apply finds nothing to link, and the rig
// presents as "the patcher disconnected from the interface" with no cause named
// anywhere. The launcher used to carry that config straight into a restart loop
// with no audio.
//
// The constraint logic is NOT duplicated here. It lives in one place —
// /opt/demiurge/bin/demiurge-quantum-limits — which the demiurge-audio CLI also
// calls, so the CLI's refusal and the launcher's fallback can never disagree
// about what is legal. This is the thin client for it.
const LIMITS_BIN: &str = "/opt/demiurge/bin/demiurge-quantum-limits";

pub enum QuantumVerdict {
    /// Runs as configured.
    Ok,
    /// Runs, but api.alsa.period-size has to come down first. The launcher does
    /// NOT do that itself — changing WirePlumber config mid-session bounces
    /// every device. It logs the fact and runs; the graph will be constrained
    /// to the device period until the user applies it via demiurge-audio.
    NeedsPeriod(String),
    /// Cannot work. Carries the nearest quantum that can.
    Bad { nearest: Option<u32>, message: String },
    /// A constraint could not be read (helper missing, PipeWire not up yet).
    /// Fail OPEN: an unverifiable constraint must not stop the rig booting.
    Unknown(String),
}

pub fn check_quantum(live_path: &str, quantum: u32) -> QuantumVerdict {
    if !std::path::Path::new(LIMITS_BIN).exists() {
        return QuantumVerdict::Unknown(format!("{LIMITS_BIN} not installed"));
    }
    let out = Command::new(LIMITS_BIN)
        .args(["check", &quantum.to_string(), "--machine", "--live", live_path])
        .stdin(Stdio::null())
        .output();
    let Ok(out) = out else {
        return QuantumVerdict::Unknown("could not run demiurge-quantum-limits".into());
    };
    let line = String::from_utf8_lossy(&out.stdout);
    // state|quantum|ksmps|period|nearest|period_target|message
    let f: Vec<&str> = line.trim().split('|').collect();
    if f.len() < 7 {
        return QuantumVerdict::Unknown("demiurge-quantum-limits gave no verdict".into());
    }
    let nearest = f[4].parse::<u32>().ok();
    let msg = f[6].to_string();
    match f[0] {
        "ok"     => QuantumVerdict::Ok,
        "period" => QuantumVerdict::NeedsPeriod(msg),
        "bad"    => QuantumVerdict::Bad { nearest, message: msg },
        _        => QuantumVerdict::Unknown(msg),
    }
}

fn last_good_path() -> PathBuf {
    let home = std::env::var("HOME").unwrap_or_else(|_| "/home/gorpi7".into());
    PathBuf::from(format!("{home}/demiurge/state/last-good-quantum"))
}

/// The last quantum this rig was observed running audio at. Written only after
/// the graph has been up and healthy for a while (see main.rs), so it is a
/// statement about reality rather than about config.
pub fn last_good_quantum() -> Option<u32> {
    fs::read_to_string(last_good_path()).ok()?.trim().parse().ok()
}

pub fn record_good_quantum(quantum: u32) {
    if last_good_quantum() == Some(quantum) { return; }
    let p = last_good_path();
    if let Some(dir) = p.parent() { let _ = fs::create_dir_all(dir); }
    if fs::write(&p, format!("{quantum}\n")).is_ok() {
        log(&format!("quantum: {quantum} confirmed good (graph healthy) — recorded as the fallback"));
    }
}

/// Decide what quantum to ACTUALLY run at, given what live.conf asks for.
/// Never returns a value known to be unrunnable, and is loud about substituting.
///
/// Preference order when the requested value is impossible:
///   1. the last quantum this rig was seen making audio at, if it still checks out
///   2. the nearest legal value the helper suggests
///   3. the requested value (only if we have nothing better — fail open rather
///      than refuse to boot)
pub fn vet_quantum(live_path: &str, requested: u32) -> u32 {
    match check_quantum(live_path, requested) {
        QuantumVerdict::Ok => requested,
        QuantumVerdict::NeedsPeriod(msg) => {
            log(&format!("quantum: {requested} — {msg}"));
            log("quantum: running anyway; the device period is a floor, so the graph will sit at the device period until 'demiurge-audio quantum' lowers it");
            requested
        }
        QuantumVerdict::Unknown(msg) => {
            log(&format!("quantum: {requested} NOT verified ({msg}) — proceeding unchecked"));
            requested
        }
        QuantumVerdict::Bad { nearest, message } => {
            log("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
            log(&format!("quantum: live.conf asks for {requested}, WHICH WOULD KILL THE AUDIO ENGINE"));
            log(&format!("quantum: {message}"));
            let good = last_good_quantum()
                .filter(|q| *q != requested)
                .filter(|q| matches!(check_quantum(live_path, *q),
                                     QuantumVerdict::Ok | QuantumVerdict::NeedsPeriod(_)));
            let chosen = good.or(nearest).unwrap_or(requested);
            if chosen == requested {
                log("quantum: no safe alternative known — proceeding with the requested value, audio may not start");
            } else if good == Some(chosen) {
                log(&format!("quantum: falling back to {chosen}, the last value this rig made audio at"));
            } else {
                log(&format!("quantum: falling back to {chosen}, the nearest workable value"));
            }
            log("quantum: live.conf was NOT modified — fix it with 'demiurge-audio quantum <n>'");
            log("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
            chosen
        }
    }
}

// ---------- ONE CODEC, ONE CLOCK DOMAIN ----------
//
// THE FAILURE THIS EXISTS TO MAKE IMPOSSIBLE.
//
// A duplex I2S codec (DAC and ADC on one chip, one crystal) is presented by
// ALSA's card profiles in two different shapes, and only one of them is true:
//
//   ACP "stereo-fallback"  two INDEPENDENT PipeWire nodes. Nothing in that
//                          model records that they are the same chip, so
//                          PipeWire makes capture a FOLLOWER of playback,
//                          targets a follower delay near 1.5x quantum, and
//                          RESYNCS whenever the delay leaves that window.
//   "pro-audio"            the card's raw PCMs, one clock domain, no follower.
//
// Every resync is a discontinuity on the input, and NOTHING downstream reports
// it: ALSA stays `state: RUNNING`, the capture hw_ptr keeps advancing at
// exactly 48000 frames/s, no xrun is logged, every link stays up. What the
// player hears is the guitar passing cleanly for a few seconds and then dying
// into a steady oscillating tone that never clears. MEASURED on this rig,
// 2026-08-11, quantum 64, ACP profile:
//
//   spa.alsa: hw:2c: follower delay:349 target:161 thr:128 resample:1, resync
//   spa.alsa: hw:2p: snd_pcm_avail after recover: Broken pipe
//
// and, once the resyncing had dragged the engine's client node long enough:
//
//   supervise: 'neptrPhase4' is ALIVE but has no ports — treating as failed
//
// WHY THIS IS RUST AND NOT ONLY CONFIG. The profile switch is also written as
// Rule 0 of config/wireplumber/50-demiurge.conf, and that rule is the primary
// mechanism. It is not sufficient on its own, for two reasons this rig
// demonstrated rather than theorised:
//
//   1. The config file only reaches /etc when someone re-runs
//      setup-demiurge.sh. On 2026-08-12 the deployed
//      /etc/wireplumber/wireplumber.conf.d/50-demiurge.conf was byte-identical
//      to commit 673d476 — FOUR commits before the Pro Audio rule was written.
//      The fix had been measured, committed, and never installed, and nothing
//      on the device could tell.
//   2. Even when the profile is correct, it can fall BACK. The launcher log
//      records the card running as `...sound.pro-output-0` from 12:49 to
//      14:16 and then:
//
//        aggregate: clock master changed: ...pro-output-0 → ...stereo-fallback
//
//      after which every run was ACP again. A profile nothing asserts is a
//      profile that drifts.
//
// So the invariant is asserted at every launcher start and on every device
// change, from the running graph, against what the card is ACTUALLY on. Fail
// OPEN and LOUD: an unverifiable profile must never stop the rig booting, but
// it must never be silent either — silence is what cost this bug two nights.
//
// api.alsa.use-acp = false WAS CONSIDERED AND REJECTED, so nobody re-tries it.
// It reaches the same raw PCMs, but the resulting nodes are named
// `alsa_output.platform-soc_..._sound` with NO profile suffix, and every
// period/ring/priority rule in 50-demiurge.conf and in the generated
// 60-demiurge-period.conf matches `..._sound\..*` — a trailing dot that is no
// longer there. The card would come up untuned, silently, which is the exact
// class of failure this whole file is written against. `device.profile =
// "pro-audio"` keeps the suffix (`...sound.pro-output-0`) and every regex
// keeps matching, which is why it is the form used here and there.
const PRO_AUDIO_PROFILE: &str = "pro-audio";
const WP_POLICY_FILE: &str = "/etc/wireplumber/wireplumber.conf.d/50-demiurge.conf";

pub fn enforce_duplex_clock_domain() {
    let Some((id, name)) = platform_duplex_device() else {
        // No platform/I2S card attached (USB-only rig, or PipeWire not up yet).
        // USB duplex boxes are already clock-coherent to PipeWire and are
        // deliberately left on ACP — see 50-demiurge.conf Rule 0 for why
        // (their mono `__Mic1__source` split nodes are what the numbered-input
        // chain syntax resolves against, and Pro Audio removes them).
        return;
    };

    match active_profile(&id) {
        Some(p) if p == PRO_AUDIO_PROFILE => {
            log(&format!("clock domain: {name} on '{PRO_AUDIO_PROFILE}' — capture and playback share one clock"));
            return;
        }
        Some(p) => log(&format!(
            "clock domain: {name} is on profile '{p}' — capture is a FOLLOWER of playback and will resync; switching to '{PRO_AUDIO_PROFILE}'"
        )),
        None => log(&format!(
            "clock domain: could not read {name}'s active profile — asserting '{PRO_AUDIO_PROFILE}' anyway"
        )),
    }

    let Some(index) = profile_index(&id, PRO_AUDIO_PROFILE) else {
        log("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        log(&format!("clock domain: {name} declares NO '{PRO_AUDIO_PROFILE}' profile"));
        log("clock domain: capture will run as a resampling follower of playback. Expect the input");
        log("clock domain: to pass for a few seconds and then resync continuously into a steady tone.");
        log("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        return;
    };

    let ok = Command::new("wpctl")
        .args(["set-profile", &id, &index.to_string()])
        .stdin(Stdio::null()).stdout(Stdio::null()).stderr(Stdio::null())
        .status().map(|s| s.success()).unwrap_or(false);
    if !ok {
        log(&format!("clock domain: 'wpctl set-profile {id} {index}' failed — card stays on its current profile"));
        warn_if_policy_stale();
        return;
    }

    // The device tears its nodes down and re-creates them under the new
    // profile. Give it a moment, then say what actually happened rather than
    // what was requested — a set-profile that reports success and does not
    // stick is exactly the shape of the 14:18 fallback above.
    thread::sleep(Duration::from_millis(600));
    match active_profile(&id) {
        Some(p) if p == PRO_AUDIO_PROFILE =>
            log(&format!("clock domain: {name} switched to '{PRO_AUDIO_PROFILE}' — follower resync eliminated")),
        Some(p) =>
            log(&format!("clock domain: {name} REJECTED the switch and is still on '{p}' — input will resync")),
        None =>
            log(&format!("clock domain: {name} profile unreadable after the switch")),
    }
    warn_if_policy_stale();
}

// The duplex platform/I2S card, as `(object id, device.name)`. Matched on the
// same identity 50-demiurge.conf uses — `alsa_card.platform-soc_*_sound` —
// which is a TOPOLOGY match, not a product match: any I2S audio HAT on any Pi
// is this card, and no brand is named anywhere. `_sound` excludes HDMI
// (`...hdmi`) and `soc_` excludes snd-aloop (`platform-snd_aloop`), which must
// keep its own ACP profile or its node name changes and
// 90-demiurge-aloop.conf's lowest-priority rule stops matching it.
fn platform_duplex_device() -> Option<(String, String)> {
    let listing = pw_cli(&["ls", "Device"]);
    let mut cur_id: Option<String> = None;
    for line in listing.lines() {
        let t = line.trim_start();
        if let Some(rest) = t.strip_prefix("id ") {
            cur_id = Some(rest.split(',').next().unwrap_or("").trim().to_string());
            continue;
        }
        if let Some(rest) = t.strip_prefix("device.name = ") {
            let name = rest.trim().trim_matches('"');
            if name.starts_with("alsa_card.platform-soc_") && name.ends_with("_sound") {
                return cur_id.clone().map(|id| (id, name.to_string()));
            }
        }
    }
    None
}

// Name of the profile the device is CURRENTLY on (`pw-cli e <id> Profile`).
fn active_profile(id: &str) -> Option<String> {
    profile_names(&pw_cli(&["e", id, "Profile"])).into_iter().next().map(|(_, n)| n)
}

// Index of a named profile among the ones the device OFFERS
// (`pw-cli e <id> EnumProfile`).
fn profile_index(id: &str, want: &str) -> Option<u32> {
    profile_names(&pw_cli(&["e", id, "EnumProfile"]))
        .into_iter()
        .find(|(_, n)| n == want)
        .map(|(i, _)| i)
}

// Parse (index, name) pairs out of a pw-cli Param:Profile dump. The shape is
//
//   Prop: key Spa:Pod:Object:Param:Profile:index (1), flags 00000000
//     Int 3
//   Prop: key Spa:Pod:Object:Param:Profile:name (2), flags 00000000
//     String "pro-audio"
//
// so the parse is: remember the Int that follows a `Profile:index` key, and
// emit a pair when a String follows a `Profile:name` key. Keyed on the Spa
// property names rather than on line positions, because the surrounding
// indentation and the other props in the object differ between profiles.
fn profile_names(dump: &str) -> Vec<(u32, String)> {
    let mut out: Vec<(u32, String)> = Vec::new();
    let mut pending_index: Option<u32> = None;
    let mut want: Option<&'static str> = None;
    for line in dump.lines() {
        let t = line.trim();
        if t.starts_with("Prop:") {
            want = if t.contains("Profile:index") {
                Some("index")
            } else if t.contains("Profile:name") {
                Some("name")
            } else {
                None
            };
            continue;
        }
        match want {
            Some("index") => {
                if let Some(v) = t.strip_prefix("Int ") {
                    pending_index = v.trim().parse().ok();
                    want = None;
                }
            }
            Some("name") => {
                if let Some(v) = t.strip_prefix("String ") {
                    let name = v.trim().trim_matches('"').to_string();
                    if let Some(i) = pending_index.take() {
                        out.push((i, name));
                    }
                    want = None;
                }
            }
            _ => {}
        }
    }
    out
}

// Config drift is INVISIBLE from the device unless something looks, and on
// this rig it hid a measured, committed fix for a full day. Cheap textual
// check: does the installed policy carry the Pro Audio rule at all? Only ever
// logged, never acted on — the runtime assertion above is what actually fixes
// the graph; this names the reason it had to.
fn warn_if_policy_stale() {
    let Ok(text) = fs::read_to_string(WP_POLICY_FILE) else { return };
    if text.contains(PRO_AUDIO_PROFILE) { return; }
    log(&format!("clock domain: {WP_POLICY_FILE} predates the Pro Audio rule — the card falls back to a"));
    log("clock domain: follower-capture profile on every WirePlumber start. Re-install the policy:");
    log("clock domain:   sudo install -m0644 config/wireplumber/50-demiurge.conf /etc/wireplumber/wireplumber.conf.d/");
}

fn pw_cli(args: &[&str]) -> String {
    Command::new("pw-cli")
        .args(args)
        .stdin(Stdio::null())
        .output()
        .map(|o| String::from_utf8_lossy(&o.stdout).into_owned())
        .unwrap_or_default()
}

// Reassert clock.force-rate and clock.force-quantum in the PipeWire `settings`
// metadata object. Idempotent. Called at startup and on every USB device
// change so a hot-plugged interface that opens at a non-48k rate cannot drag
// the graph with it.
pub fn pin_clock_metadata(rate: u32, quantum: u32) {
    let r = rate.to_string();
    let q = quantum.to_string();
    let rate_ok = Command::new("pw-metadata")
        .args(["-n", "settings", "0", "clock.force-rate", &r])
        .stdin(Stdio::null())
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .status()
        .map(|s| s.success())
        .unwrap_or(false);
    let quantum_ok = Command::new("pw-metadata")
        .args(["-n", "settings", "0", "clock.force-quantum", &q])
        .stdin(Stdio::null())
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .status()
        .map(|s| s.success())
        .unwrap_or(false);
    if rate_ok && quantum_ok {
        log(&format!("clock: pinned rate={r} quantum={q}"));
    } else {
        log(&format!(
            "clock: pin attempt — rate_ok={rate_ok} quantum_ok={quantum_ok} (pw-metadata available?)"
        ));
    }
}

// Called at launcher startup (before spawning any audio stage) and in the
// LiveConfigChanged rate-change path.
//
// Iterates enforce_usb_rates until no USB alsa node needs destroying, then
// returns. If nodes keep coming back at the wrong rate after MAX_ITERS
// attempts, the WirePlumber rule probably missed the device creation window
// entirely. At that point we do a hardware USB bounce (unbind + bind) via
// the privileged bounce script, wait for re-enumeration, and do one final
// enforce pass before handing control back to the caller.
//
// This completely replaces the retired boot-time bounce service/timer pair
// with a deterministic, condition-driven equivalent that is also effective at
// runtime (not just boot).
// Physical replug always fixes the wrong-rate bitcrush because it forces
// the USB host to re-present the device to WP, which re-applies
// monitor.alsa.rules (including audio.rate=48000). USB unbind/bind does
// the same at the software level. Do it unconditionally at every boot,
// then hammer enforce until nodes are stable.
pub fn ensure_usb_rates_stable(target_rate: u32) {
    // Stage 1: hardware bounce — USB unbind/bind causes WP to see a
    // device-remove + device-add event and recreate all nodes fresh under
    // the 48 kHz rate rule. Skip when no USB audio node exists: a platform
    // card (I2S HAT, HDMI, on-SoC codec) is permanently powered, cannot be
    // re-enumerated, and does not have the stuck-clock problem this clears.
    let has_usb = has_usb_audio_nodes();
    if has_usb {
        audio_bounce();
        log("clock: post-bounce wait 3s for WP re-enumeration");
        thread::sleep(Duration::from_secs(3));
    }

    // Stage 2: enforce loop — verify every node that opened is at the right
    // rate, destroy any that aren't, repeat until clean.
    for i in 1..=5u32 {
        let destroyed = enforce_usb_rates(target_rate);
        if destroyed == 0 {
            log(&format!("clock: USB rates stable after bounce + {i} enforce pass(es)"));
            return;
        }
        log(&format!("clock: enforce pass {i} — destroyed {destroyed} wrong-rate node(s), waiting 600ms"));
        thread::sleep(Duration::from_millis(600));
    }

    log("clock: USB rates did not converge — proceeding anyway");
}

// Single-pass USB rate enforcer. Returns the number of nodes destroyed.
//
// For each alsa_*.usb-* node:
//   1. If a negotiated Format exists: check its rate. Wrong rate → destroy.
//   2. If no Format (node idle): check EnumFormat to see if WirePlumber's
//      monitor.alsa.rules constrained the allowed rates to `target_rate`
//      only. If not constrained (still shows a wide range), the WP rule
//      missed this node — destroy so WP recreates it and re-applies rules.
//
// Called by ensure_usb_rates_stable and directly in the UsbDeviceChange
// handler (where a single pass is sufficient because the 750ms pw-mon
// debounce means WP has already had time to act on the new node).
pub fn enforce_usb_rates(target_rate: u32) -> usize {
    let listing = Command::new("pw-cli")
        .arg("ls")
        .arg("Node")
        .output()
        .map(|o| String::from_utf8_lossy(&o.stdout).into_owned())
        .unwrap_or_default();

    let mut cur_id: Option<String> = None;
    let mut usb_nodes: Vec<(String, String)> = Vec::new();
    for line in listing.lines() {
        let trimmed = line.trim_start();
        if let Some(rest) = trimmed.strip_prefix("id ") {
            let id_str = rest.split(',').next().unwrap_or("").trim().to_string();
            cur_id = Some(id_str);
            continue;
        }
        if trimmed.starts_with("node.name = ") {
            let name = trimmed.trim_start_matches("node.name = ").trim_matches('"');
            if name.starts_with("alsa_output.usb-") || name.starts_with("alsa_input.usb-")
                || name.starts_with("alsa_input.hw_")
            {
                if let Some(id) = cur_id.clone() {
                    usb_nodes.push((id, name.to_string()));
                }
            }
        }
    }

    let mut destroyed = 0usize;
    for (id, name) in &usb_nodes {
        let action = check_usb_node_rate(id, target_rate);
        match action {
            NodeRateStatus::Correct => {}
            NodeRateStatus::Wrong(r) => {
                log(&format!(
                    "rate: USB node {name} (id={id}) at {r}, expected {target_rate} — destroying for re-creation"
                ));
                destroy_node(id);
                destroyed += 1;
            }
            NodeRateStatus::Unconstrained => {
                log(&format!(
                    "rate: USB node {name} (id={id}) idle with unconstrained rate (WP rule missed) — destroying for re-creation"
                ));
                destroy_node(id);
                destroyed += 1;
            }
            NodeRateStatus::Unknown => {
                // Node is idle and EnumFormat couldn't be parsed — leave it alone.
                // The WP rule most likely applied; we just can't verify. If audio
                // comes out wrong the user will hear it and can replug.
            }
        }
    }
    destroyed
}

enum NodeRateStatus {
    Correct,
    Wrong(u32),      // negotiated at this rate (not target)
    Unconstrained,   // idle, WP rate-pin rule did not apply
    Unknown,         // idle, EnumFormat unreadable
}

fn check_usb_node_rate(id: &str, target_rate: u32) -> NodeRateStatus {
    // 1. Try negotiated Format first.
    let fmt = Command::new("pw-cli")
        .args(["e", id, "Format"])
        .output()
        .map(|o| String::from_utf8_lossy(&o.stdout).into_owned())
        .unwrap_or_default();

    let mut next_is_rate = false;
    for l in fmt.lines() {
        let t = l.trim();
        if next_is_rate {
            if let Some(rest) = t.strip_prefix("Int ") {
                if let Ok(r) = rest.trim().parse::<u32>() {
                    return if r == target_rate {
                        NodeRateStatus::Correct
                    } else {
                        NodeRateStatus::Wrong(r)
                    };
                }
            }
            next_is_rate = false;
            continue;
        }
        if t.contains("Format:Audio:rate") {
            next_is_rate = true;
        }
    }

    // 2. Node is idle — check EnumFormat to see if WP constrained it.
    enum_format_rate_status(id, target_rate)
}

// Inspect EnumFormat to determine whether WirePlumber's monitor.alsa.rules
// already pinned this idle node to target_rate.
//
// pw-cli output examples:
//   Constrained:    "Audio:rate = 48000"
//   Range (missed): "Audio:rate: { min: 44100, max: 96000 }"
//   Single-value choice: "Audio:rate: { default: 48000, min: 48000, max: 48000 }"
fn enum_format_rate_status(id: &str, target_rate: u32) -> NodeRateStatus {
    let out = Command::new("pw-cli")
        .args(["e", id, "EnumFormat"])
        .output()
        .map(|o| String::from_utf8_lossy(&o.stdout).into_owned())
        .unwrap_or_default();

    if out.trim().is_empty() {
        return NodeRateStatus::Unknown;
    }

    let target_str = target_rate.to_string();
    for line in out.lines() {
        let t = line.trim();
        if !t.starts_with("Audio:rate") { continue; }

        if !t.contains('{') {
            // Direct assignment: "Audio:rate = 48000"
            return if t.ends_with(&target_str) {
                NodeRateStatus::Correct
            } else {
                NodeRateStatus::Wrong(0)
            };
        }

        // Choice object: check if min == max == target (single-value choice)
        let min_ok = t.contains(&format!("min: {target_str}"));
        let max_ok = t.contains(&format!("max: {target_str}"));
        return if min_ok && max_ok {
            NodeRateStatus::Correct
        } else {
            NodeRateStatus::Unconstrained
        };
    }

    // No Audio:rate line found — can't determine; leave it alone.
    NodeRateStatus::Unknown
}

fn has_usb_audio_nodes() -> bool {
    let listing = Command::new("pw-cli")
        .args(["ls", "Node"])
        .output()
        .map(|o| String::from_utf8_lossy(&o.stdout).into_owned())
        .unwrap_or_default();
    listing.contains("alsa_output.usb-") || listing.contains("alsa_input.usb-")
}

fn destroy_node(id: &str) {
    let _ = Command::new("pw-cli")
        .args(["destroy", id])
        .stdin(Stdio::null())
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .status();
}

// Trigger a hardware unbind+bind on every attached hot-pluggable audio
// interface — whichever they are; the script derives the device set from the
// sound cards actually present and knows no vendor IDs. This forces ALSA to
// close the device file and WirePlumber to re-enumerate it from scratch,
// applying monitor.alsa.rules on the fresh node. Cards that cannot be
// re-enumerated (I2S HAT, HDMI, loopback) make it a silent no-op.
//
// Requires /usr/local/bin/demiurge-audio-bounce.sh to be installed and the
// service user to have NOPASSWD sudo for it (set up by setup-demiurge.sh).
// Kill any link originating from a hardware monitor port (*.split:monitor_*).
// WirePlumber auto-wires these to JACK clients when they register, creating
// a digital feedback loop through the Scarlett's direct monitor that sounds
// like bitcrush. Called on every poll tick during client registration.
pub fn scrub_monitor_ports() -> usize {
    let full = Command::new("pw-link")
        .arg("-l")
        .output()
        .map(|o| String::from_utf8_lossy(&o.stdout).into_owned())
        .unwrap_or_default();

    let mut current_port: Option<String> = None;
    let mut killed = 0usize;
    for line in full.lines() {
        if !line.starts_with(' ') && !line.starts_with('\t') && line.contains(':') {
            current_port = Some(line.trim().to_string());
            continue;
        }
        let Some(dst) = current_port.as_ref() else { continue; };
        let Some(src) = line.trim_start().strip_prefix("|<- ") else { continue; };
        if src.contains(":monitor_") {
            log(&format!("scrub monitor: {src} -X-> {dst}"));
            let _ = Command::new("pw-link").args(["-d", src, dst]).stderr(Stdio::null()).status();
            killed += 1;
        }
    }
    killed
}

pub fn audio_bounce() {
    log("clock: invoking hardware audio-interface bounce");
    let res = Command::new("sudo")
        .arg("/usr/local/bin/demiurge-audio-bounce.sh")
        .stdin(Stdio::null())
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .status();
    match res {
        Ok(s) if s.success() => log("clock: bounce script exited cleanly"),
        Ok(s)  => log(&format!("clock: bounce script exit {s}")),
        Err(e) => log(&format!("clock: bounce script failed to run: {e}")),
    }
}

pub fn install_signal_handlers() {
    let _ = &SHUTDOWN;
}

pub fn shutdown_requested() -> bool {
    SHUTDOWN.load(Ordering::Relaxed)
}


/// True when the first chain has no first program (a fresh install's live.conf).
pub fn chain_is_empty(lc: &LiveConfig) -> bool {
    lc.chains.get(0).and_then(|c| c.stages.get(0)).map(|p| p.trim().is_empty()).unwrap_or(true)
}

/// Empty chain: nothing to run, which is not a failure. Log once, then wait for
/// live.conf to change and re-exec this binary so the new chain is loaded the
/// normal way. Never returns.
pub fn idle_until_changed(live_path: &str) -> ! {
    use std::os::unix::process::CommandExt;
    log("chain is empty: nothing to run, idling until live.conf changes");
    let mtime = |p: &str| std::fs::metadata(p).and_then(|m| m.modified()).ok();
    let start = mtime(live_path);
    loop {
        std::thread::sleep(std::time::Duration::from_secs(2));
        if mtime(live_path) != start {
            log("live.conf changed: reloading");
            let exe = std::env::current_exe().unwrap_or_else(|_| "/opt/demiurge/bin/demiurge-launcher".into());
            let err = std::process::Command::new(exe).args(std::env::args().skip(1)).exec();
            log(&format!("re-exec failed ({err}); exiting so systemd restarts us"));
            std::process::exit(1);
        }
    }
}
