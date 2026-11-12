// Patch graph applier. Given a session and resolved client map, walk the
// edge list, skip dead nodes (graceful passthrough upstream-to-downstream),
// and call pw-link for each surviving edge. Scrubs undeclared auto-links
// on a second pass.

use std::collections::{HashMap, HashSet};
use std::process::{Command, Stdio};

use crate::config::Session;
use crate::util;

const DEMIURGE_SINK_L: &str = "demiurge-sink:playback_FL";
const DEMIURGE_SINK_R: &str = "demiurge-sink:playback_FR";
// Source end of the input loopback (the sink end is demiurge-source-input,
// which aggregate feeds from the hardware).
const DEMIURGE_SOURCE: &str = "demiurge-source";

// Where the chain's `out` endpoint terminates. Single wired USB device and
// nothing else → straight to that device's hardware playback ports (direct
// mode: no loopback, one quantum less latency on output). Otherwise → the
// demiurge-sink loopback, which is the mixing point the aggregate / BT path
// needs. Decided by aggregate::direct_output_device so routing and aggregation
// never disagree about which mode we're in.
//
// The device's two ports are DERIVED (util::device_port_pair), not named: a
// card on the Pro Audio profile has no playback_FL at all, and asking pw-link
// for a port that does not exist fails silently, so hardcoding the names cost
// the whole output path with no error anywhere. A device that is present but
// exposes no playback ports yet (still enumerating) falls back to the
// loopback rather than to invented port names.
fn out_sink_ports() -> (String, String) {
    if let Some(dev) = crate::aggregate::direct_output_device() {
        if let Some(pair) = util::device_port_pair(&pw_link_list("-i"), &dev, "playback_") {
            return pair;
        }
    }
    (DEMIURGE_SINK_L.into(), DEMIURGE_SINK_R.into())
}

// Where the chain's default hardware input comes FROM — the mirror of
// out_sink_ports. Single wired capture device and nothing else → that device's
// hardware capture ports (direct mode). Otherwise → the demiurge-source
// loopback, which is the merge point a multi-interface or no-interface (aloop
// clock) rig needs. Decided by aggregate::direct_input_device so routing and
// aggregation can never disagree about which mode we are in.
//
// A card that exposes only capture_MONO (single-input devices) feeds BOTH
// destination channels from that one port, matching resolve_prog_out_ports'
// treatment of mono sources.
fn in_source_ports() -> Option<(String, String)> {
    let outs = pw_link_list("-o");
    let have = |p: &str| outs.lines().any(|l| l == p);

    if let Some(dev) = crate::aggregate::direct_input_device() {
        // Derived, not named — capture_FL/FR under ACP, capture_AUX0/AUX1
        // under Pro Audio, capture_MONO on single-input devices, all handled
        // by the one rule. See util::device_port_pair.
        if let Some(pair) = util::device_port_pair(&outs, &dev, "capture_") {
            return Some(pair);
        }
        // Device is present but has no capture ports yet (still enumerating,
        // or genuinely output-only like an HDMI sink). Fall through to the
        // loopback rather than inventing ports that do not exist.
    }

    for (l, r) in [("capture_FL", "capture_FR"), ("output_FL", "output_FR")] {
        let (pl, pr) = (format!("{DEMIURGE_SOURCE}:{l}"), format!("{DEMIURGE_SOURCE}:{r}"));
        if have(&pl) && have(&pr) { return Some((pl, pr)); }
    }
    None
}

pub fn apply(session: &Session, clients: &HashMap<String, String>) {
    util::log(&format!("graph: apply {} edges", session.edges.len()));
    match crate::aggregate::direct_output_device() {
        Some(dev) => util::log(&format!("graph: OUT direct → {dev} (demiurge-sink loopback bypassed)")),
        None => util::log("graph: OUT via demiurge-sink loopback (aggregate/BT mixing point)"),
    }

    let mut desired: HashSet<(String, String)> = HashSet::new();
    for edge in &session.edges {
        let live_to = walk_to_live(session, clients, &edge.to);
        if !endpoint_live(session, clients, &edge.from) { continue; }

        // Mono hardware inputs: in1 (Mic1 / Input 1) and in2 (Mic2 / Input 2)
        // create a SINGLE link from the numbered capture_MONO source into the
        // matching input channel of the destination stage. in1 → first input
        // port, in2 → second. Stereo is not assumed anywhere — every input
        // is addressed by its 1-indexed channel number.
        if let Some(ch) = mono_input_channel(&edge.from) {
            let Some(src) = resolve_hw_input_mono(ch) else {
                util::log(&format!("graph: {} unavailable (no capture_MONO for input {ch})", edge.from));
                continue;
            };
            let Some(dst) = resolve_prog_input_port(session, clients, &live_to, (ch - 1) as usize) else {
                continue;
            };
            pw_link(&src, &dst);
            desired.insert((src.clone(), dst.clone()));
            util::log(&format!("link: {src} → {dst}"));
            continue;
        }

        let Some((sl, sr)) = out_ports_for(session, clients, &edge.from) else { continue; };
        let Some((dl, dr)) = in_ports_for(session, clients, &live_to) else { continue; };

        pw_link(&sl, &dl);
        pw_link(&sr, &dr);
        desired.insert((sl.clone(), dl.clone()));
        desired.insert((sr.clone(), dr.clone()));
        util::log(&format!("link: {sl} → {dl}  |  {sr} → {dr}"));
    }

    // DEFAULT HARDWARE INPUT.
    //
    // The selected interface supplies BOTH directions. Until this existed the
    // sync layer wired output and nothing else: with no `in1 ->` line in
    // live.conf the chain's first stage had ZERO incoming links, so the user's
    // instrument never reached the engine at all in sync mode. Meanwhile the
    // hardware capture was being pulled into demiurge-source-input, a loopback
    // that nothing downstream consumed. Output got a direct-path optimisation
    // in 2026-06; input did not even get a path.
    //
    // Rules:
    //   - Applies to the HEAD of every chain (a program with no incoming edge).
    //   - A chain that declared `in1 ->` / `in2 ->` (or was named in `inputs =`)
    //     is skipped: the numbered mono form is the more specific statement of
    //     intent and already ran above, and adding the stereo pair on top would
    //     double-feed the stage.
    //   - `input = off` in live.conf disables it wholesale.
    //
    // Both links go into `desired`, which is what stops scrub_stray_session_links
    // from deleting them on the very next tick — the output path shipped with
    // exactly this bug and it was fixed the same way, by threading `&desired`
    // through the scrubber.
    if util::default_input() {
        match in_source_ports() {
            Some((sl, sr)) => {
                let direct = crate::aggregate::direct_input_device();
                let mut wired = 0;
                for head in chain_heads(session, clients) {
                    if has_declared_input(session, &head) { continue; }
                    let Some((dl, dr)) = in_ports_for(session, clients, &head) else { continue; };
                    pw_link(&sl, &dl);
                    pw_link(&sr, &dr);
                    desired.insert((sl.clone(), dl.clone()));
                    desired.insert((sr.clone(), dr.clone()));
                    util::log(&format!("link: {sl} → {dl}  |  {sr} → {dr}"));
                    wired += 1;
                }
                if wired > 0 {
                    match &direct {
                        Some(dev) => util::log(&format!("graph: IN direct → {dev} (demiurge-source loopback bypassed)")),
                        None => util::log("graph: IN via demiurge-source loopback (multi-interface / aloop clock)"),
                    }
                }
            }
            None => util::log("graph: IN unavailable (no capture device and no demiurge-source ports)"),
        }
    } else {
        util::log("graph: IN disabled (live.conf input = off)");
    }

    // MIDI: feed the shared "Midi Through" pool into every rnbo stage's
    // midiin1. Physical devices (Teensy) and demiurge_clock are merged INTO
    // Midi Through at the ALSA-seq layer (demiurge-midi-connect.sh + midi.rs);
    // RNBO instances are JACK-MIDI only, so they can't join that ALSA bus
    // directly — we bridge the pool's PipeWire port into the instance here.
    // Crucially this edge goes into `desired` so scrub_stray_session_links
    // (which deletes every non-desired incoming link to a session client's
    // ports) does NOT strip it on the next tick.
    if let Some(pool) = midi_pool_capture() {
        let in_ports = pw_link_list("-i");
        for prog in &session.programs {
            if prog.lang != "rnbo" { continue; }
            let Some(client) = clients.get(&prog.id) else { continue; };
            let dst = format!("{client}:midiin1");
            if in_ports.lines().any(|l| l == dst) {
                pw_link(&pool, &dst);
                desired.insert((pool.clone(), dst.clone()));
                util::log(&format!("midi: {pool} → {dst}"));
            }
        }
    }

    // USER OVERRIDES. `demiurge-graph connect <src> <dst>` records the link in
    // ~/demiurge/graph.conf so it can outlive the next graph event; without
    // this pass the launcher's scrubbers delete it within a tick, and the CLI
    // says so plainly rather than pretending otherwise. Merging the file into
    // `desired` here is the launcher half of that handoff, and it is what turns
    // "live now, probably gone soon" into a link that actually persists.
    //
    // Deliberately merged LAST, after every derived edge, so a user override
    // can add to the graph but the ports the launcher owns are already claimed.
    // Overrides are additive only — there is no `disconnect =` form, because a
    // persistent subtraction from a graph the launcher re-derives every tick
    // would be indistinguishable from the bug this whole day was spent fixing.
    for (src, dst) in read_graph_overrides() {
        pw_link(&src, &dst);
        util::log(&format!("link: {src} → {dst}  (graph.conf override)"));
        desired.insert((src, dst));
    }

    scrub_stray_session_links(session, clients, &desired);
    scrub_stray_sink_inputs(session, clients, &desired);
    scrub_hw_playback(&desired);
    scrub_sink_feedback(session, clients);
}

// The shared MIDI pool's PipeWire capture port ("Midi Through" bridged by
// pipewire's ALSA-seq monitor). Everything joined to the ALSA-seq "Midi
// Through" bus (Teensy, demiurge_clock) shows up here, so linking this into a
// stage feeds it the whole pool.
fn midi_pool_capture() -> Option<String> {
    pw_link_list("-o")
        .lines()
        .find(|l| l.contains("Midi Through") && l.ends_with("(capture)"))
        .map(|s| s.to_string())
}

// Kill any auto-connected link from a session program's output INTO
// demiurge-sink:playback_FL/FR that isn't in the desired edge set.
// WirePlumber speculatively wires every JACK client's dac output to the
// sink, so a mid-chain stage (e.g. ChucK as stage 0) ends up playing
// dry in parallel with its intended downstream route. The patch graph
// owns every session client's contribution to the sink: only programs
// whose edge terminates at `out` may drive demiurge-sink playbacks.
fn scrub_stray_sink_inputs(
    session: &Session,
    clients: &HashMap<String, String>,
    desired: &HashSet<(String, String)>,
) {
    let session_clients: HashSet<String> = session.programs.iter()
        .filter_map(|p| clients.get(&p.id).cloned())
        .collect();

    let full = Command::new("pw-link")
        .arg("-l")
        .output()
        .map(|o| String::from_utf8_lossy(&o.stdout).into_owned())
        .unwrap_or_default();

    let mut current_port: Option<String> = None;
    for line in full.lines() {
        if !line.starts_with(' ') && !line.starts_with('\t') && line.contains(':') {
            current_port = Some(line.to_string());
            continue;
        }
        let Some(dst) = current_port.as_ref() else { continue; };
        if dst != DEMIURGE_SINK_L && dst != DEMIURGE_SINK_R { continue; }
        let Some(src) = line.trim_start().strip_prefix("|<- ") else { continue; };

        let src_client = src.split(':').next().unwrap_or("");
        if !session_clients.contains(src_client) { continue; }

        if desired.contains(&(src.to_string(), dst.to_string())) { continue; }

        util::log(&format!("scrub sink-in: {src} -X-> {dst}"));
        let _ = Command::new("pw-link").args(["-d", src, dst]).stderr(Stdio::null()).status();
    }
}

// The first audio stage of every chain: a live program that no edge points AT.
// Chains are built as a linear pipe (live::to_session), so the head is exactly
// the stage with no incoming edge. Excludes the implicit clock program and any
// MIDI-only sidecar — neither has audio inputs and neither is part of a chain.
//
// Uses the session's own edge list rather than re-deriving chain structure,
// because `inputs =` and the mono `in1/in2` forms both add edges that must
// count as "this stage already has an input".
fn chain_heads(session: &Session, clients: &HashMap<String, String>) -> Vec<String> {
    session.programs.iter()
        .filter(|p| !p.midi_only && p.lang != "clock")
        .filter(|p| clients.contains_key(&p.id))
        .filter(|p| !session.edges.iter().any(|e| e.to == p.id))
        .map(|p| p.id.clone())
        .collect()
}

// Did the session explicitly declare an input for this stage? True when any
// edge from `in1` / `in2` terminates on it — set either by a leading `in1 ->`
// line in the chain or by naming the stage under `inputs =`.
fn has_declared_input(session: &Session, id: &str) -> bool {
    session.edges.iter().any(|e| e.to == id && (e.from == "in1" || e.from == "in2"))
}

fn mono_input_channel(id: &str) -> Option<u8> {
    match id {
        "in1" => Some(1),
        "in2" => Some(2),
        _ => None,
    }
}

// WirePlumber's default routing policy will speculatively wire up any
// JACK-shim client's inputs to "convenient" sources — other chain programs'
// outputs, the hardware mic capture, even the hardware sink's monitor ports.
// Two real symptoms we hit because of this:
//
//   1. Bitcrush-on-boot. Whichever chain program the launcher resolved first
//      (typically ChucK as stage 0) gets auto-linked into every other stage's
//      inputs that come up later, so raw ChucK hits cpp_crush directly
//      instead of the clean delayed signal from faust_delay.
//
//   2. Crackle that gets worse over time. `alsa_output.<hw>:monitor_FL` gets
//      speculatively routed into a stage's input, closing a loop:
//      stage → csound → demiurge-sink → hw playback → hw monitor → stage.
//
// Fix: the launcher owns every incoming audio link on every session
// program. After applying the desired graph, for every session program's
// input port, delete every incoming link that isn't in the desired set —
// no matter where it came from. Hardware capture is only allowed in if
// the session explicitly declared `in ->` (in which case it's in desired).
fn scrub_stray_session_links(
    session: &Session,
    clients: &HashMap<String, String>,
    desired: &HashSet<(String, String)>,
) {
    let session_clients: HashSet<String> = session.programs.iter()
        .filter_map(|p| clients.get(&p.id).cloned())
        .collect();

    let full = Command::new("pw-link")
        .arg("-l")
        .output()
        .map(|o| String::from_utf8_lossy(&o.stdout).into_owned())
        .unwrap_or_default();

    let mut current_port: Option<String> = None;
    for line in full.lines() {
        if !line.starts_with(' ') && !line.starts_with('\t') && line.contains(':') {
            current_port = Some(line.to_string());
            continue;
        }
        let Some(dst) = current_port.as_ref() else { continue; };
        let Some(src) = line.trim_start().strip_prefix("|<- ") else { continue; };

        let dst_client = dst.split(':').next().unwrap_or("");
        if !session_clients.contains(dst_client) { continue; }

        if desired.contains(&(src.to_string(), dst.to_string())) { continue; }

        util::log(&format!("scrub stray: {src} -X-> {dst}"));
        let _ = Command::new("pw-link").args(["-d", src, dst]).stderr(Stdio::null()).status();
    }
}

fn walk_to_live(session: &Session, clients: &HashMap<String, String>, start: &str) -> String {
    // Follow the edge chain starting at `start` through dead nodes until
    // we hit a live one or `out`. Mono input endpoints (in1/in2) are only
    // valid as sources and never traversed here; `out` is the only valid
    // terminal destination.
    let mut cur = start.to_string();
    loop {
        if cur == "out" { return cur; }
        if endpoint_live(session, clients, &cur) { return cur; }
        if let Some(next) = session.edges.iter().find(|e| e.from == cur) {
            cur = next.to.clone();
        } else {
            return "out".into();
        }
    }
}

fn endpoint_live(session: &Session, clients: &HashMap<String, String>, id: &str) -> bool {
    if id == "out" || id == "in1" || id == "in2" { return true; }
    let Some(_prog) = session.programs.iter().find(|p| p.id == id) else { return false; };
    clients.get(id).is_some()
}

fn out_ports_for(session: &Session, clients: &HashMap<String, String>, id: &str) -> Option<(String, String)> {
    if id == "out" {
        return Some(out_sink_ports());
    }
    // in1/in2 are mono and never flow through out_ports_for — apply()
    // handles them on a dedicated mono path.
    if id == "in1" || id == "in2" { return None; }
    let _prog = session.programs.iter().find(|p| p.id == id)?;
    let client = clients.get(id)?;
    resolve_prog_out_ports(client)
}

fn in_ports_for(session: &Session, clients: &HashMap<String, String>, id: &str) -> Option<(String, String)> {
    if id == "out" {
        return Some(out_sink_ports());
    }
    if id == "in1" || id == "in2" { return None; }
    let _prog = session.programs.iter().find(|p| p.id == id)?;
    let client = clients.get(id)?;
    resolve_prog_in_ports(client)
}

// Resolve a hardware mono capture source by 1-indexed input number.
//
// Every USB audio input on DEMIURGE is addressed as a mono PipeWire source.
// The Scarlett Solo exposes its two inputs as
//   alsa_input.<...>.HiFi__Mic1__source:capture_MONO   (input 1)
//   alsa_input.<...>.HiFi__Mic2__source:capture_MONO   (input 2)
// "Stereo pairs" don't exist at the input layer — sessions always declare
// inputs by number (in1, in2, …). The matcher looks for `__Mic<n>__source`
// on any alsa_input node that isn't a DEMIURGE virtual.
fn resolve_hw_input_mono(channel: u8) -> Option<String> {
    let outs = pw_link_list("-o");
    let tag = format!("__Mic{channel}__source");
    outs.lines()
        .find(|l| l.contains(&tag) && l.ends_with(":capture_MONO") && !l.contains("demiurge"))
        .map(|s| s.to_string())
}

// Pick the Nth (0-indexed) input port of a destination stage. Used by the
// mono-edge path to route in1 → first input, in2 → second input.
fn resolve_prog_input_port(
    session: &Session,
    clients: &HashMap<String, String>,
    id: &str,
    index: usize,
) -> Option<String> {
    let (a, b) = in_ports_for(session, clients, id)?;
    Some(if index == 0 { a } else { b })
}

fn resolve_prog_out_ports(client: &str) -> Option<(String, String)> {
    let outs = pw_link_list("-o");
    const PAIRS: &[(&str, &str)] = &[
        ("output_FL", "output_FR"),
        ("output_1", "output_2"),
        ("output1", "output2"), // csound JACK driver (csound6:output1/output2)
        ("out_0", "out_1"),
        ("out_1", "out_2"),
        ("out1", "out2"),       // RNBO runner instance audio outs (untitled-0:out1/out2)
        ("out_l", "out_r"),
        ("outL", "outR"),
        ("outport 0", "outport 1"),
        // OpGorator/Daisy USB capture device (stereo): the "stage" IS the
        // hardware node, so its "outputs" are its ALSA capture ports.
        ("capture_FL", "capture_FR"),
    ];
    let prefix = format!("{client}:");
    let ports: Vec<&str> = outs.lines().filter(|l| l.starts_with(&prefix)).collect();
    for (l, r) in PAIRS {
        let pl = format!("{prefix}{l}");
        let pr = format!("{prefix}{r}");
        if ports.iter().any(|p| *p == pl) && ports.iter().any(|p| *p == pr) {
            return Some((pl, pr));
        }
    }
    // Mono USB capture (OpGorator/Daisy in mono mode, or any single-channel
    // input device resolved as a "program" output): one capture_MONO port
    // feeds both destination channels.
    let mono = format!("{prefix}capture_MONO");
    if ports.iter().any(|p| *p == mono) {
        return Some((mono.clone(), mono));
    }
    let mut first_two = ports.iter().take(2);
    let a = first_two.next()?.to_string();
    let b = first_two.next()?.to_string();
    Some((a, b))
}

fn resolve_prog_in_ports(client: &str) -> Option<(String, String)> {
    let ins = pw_link_list("-i");
    const PAIRS: &[(&str, &str)] = &[
        ("input_FL", "input_FR"),
        ("input_1", "input_2"),
        ("input1", "input2"),   // csound JACK driver (csound6:input1/input2)
        ("in_0", "in_1"),
        ("in_1", "in_2"),
        ("in1", "in2"),         // RNBO runner instance audio ins (untitled-0:in1/in2)
        ("in_l", "in_r"),
        ("inL", "inR"),
        ("playback_FL", "playback_FR"),
        ("inport 0", "inport 1"),
    ];
    let prefix = format!("{client}:");
    let ports: Vec<&str> = ins.lines().filter(|l| l.starts_with(&prefix)).collect();
    for (l, r) in PAIRS {
        let pl = format!("{prefix}{l}");
        let pr = format!("{prefix}{r}");
        if ports.iter().any(|p| *p == pl) && ports.iter().any(|p| *p == pr) {
            return Some((pl, pr));
        }
    }
    let mut first_two = ports.iter().take(2);
    let a = first_two.next()?.to_string();
    let b = first_two.next()?.to_string();
    Some((a, b))
}

// Kill any auto-connected link from demiurge-sink-output (the passive
// loopback side of demiurge-sink) INTO a chain program's input ports.
// WirePlumber sometimes speculatively links these up on startup, which
// closes a feedback loop: csound → sink → sink-output → csound-in → ...
// Only demiurge-sink-output → hardware (alsa_output.*) is legitimate;
// everything else is a loop we need to break.
fn scrub_sink_feedback(session: &Session, clients: &HashMap<String, String>) {
    let full = Command::new("pw-link")
        .arg("-l")
        .output()
        .map(|o| String::from_utf8_lossy(&o.stdout).into_owned())
        .unwrap_or_default();

    let session_clients: std::collections::HashSet<String> = session.programs.iter()
        .filter_map(|p| clients.get(&p.id).cloned())
        .collect();

    let mut current_port: Option<String> = None;
    for line in full.lines() {
        if !line.starts_with(' ') && !line.starts_with('\t') && line.contains(':') {
            current_port = Some(line.to_string());
            continue;
        }
        let Some(port) = current_port.as_ref() else { continue; };
        let Some(src) = line.trim_start().strip_prefix("|<- ") else { continue; };
        if !src.starts_with("demiurge-sink-output:") { continue; }
        let dst_client = port.split(':').next().unwrap_or("");
        if session_clients.contains(dst_client) {
            util::log(&format!("scrub fb: {src} -X-> {port}"));
            let _ = Command::new("pw-link").args(["-d", src, port]).stderr(Stdio::null()).status();
        }
    }
}

// Parse ~/demiurge/graph.conf — the user-override file written by
// `demiurge-graph connect`. Format, one per line:
//
//     connect = csound6:output1 -> some-node:input_FL
//
// Blank lines and `#` comments ignored. A malformed line is skipped rather
// than aborting the pass: this file is edited by hand as often as by the CLI,
// and one bad line must not cost the user their whole graph.
fn read_graph_overrides() -> Vec<(String, String)> {
    let home = std::env::var("HOME").unwrap_or_else(|_| "/home/gorpi7".into());
    let path = std::env::var("DEMIURGE_GRAPH_CONF")
        .unwrap_or_else(|_| format!("{home}/demiurge/graph.conf"));
    let Ok(text) = std::fs::read_to_string(&path) else { return Vec::new(); };

    let mut out = Vec::new();
    for raw in text.lines() {
        let line = raw.split('#').next().unwrap_or("").trim();
        if line.is_empty() { continue; }
        let Some((key, val)) = line.split_once('=') else { continue; };
        if key.trim() != "connect" { continue; }
        let Some((src, dst)) = val.split_once("->") else { continue; };
        let (src, dst) = (src.trim(), dst.trim());
        // Both halves must look like `client:port`. Anything else is a typo,
        // and a typo that reached pw-link would just fail silently.
        if src.contains(':') && dst.contains(':') {
            out.push((src.to_string(), dst.to_string()));
        } else {
            util::log(&format!("graph.conf: ignoring malformed override '{line}'"));
        }
    }
    out
}

fn pw_link(src: &str, dst: &str) {
    let _ = Command::new("pw-link").args([src, dst]).stderr(Stdio::null()).status();
}

fn pw_link_list(flag: &str) -> String {
    Command::new("pw-link")
        .arg(flag)
        .output()
        .map(|o| String::from_utf8_lossy(&o.stdout).into_owned())
        .unwrap_or_default()
}

// Kill any auto-connected link from a program's output directly to the
// physical hardware playback ports. Only demiurge-sink-output is allowed to
// drive the physical sinks — EXCEPT in direct mode, where the chain's last
// stage terminates straight on the single wired device (that link is in
// `desired`, so it's preserved here). Everything else must route through
// demiurge-sink via the patch graph.
fn scrub_hw_playback(desired: &HashSet<(String, String)>) {
    let full = Command::new("pw-link")
        .arg("-l")
        .output()
        .map(|o| String::from_utf8_lossy(&o.stdout).into_owned())
        .unwrap_or_default();

    let mut current_port: Option<String> = None;
    for line in full.lines() {
        if line.starts_with("alsa_output.") && line.contains(":playback_") {
            current_port = Some(line.to_string());
            continue;
        }
        if let Some(port) = current_port.as_ref() {
            if let Some(src) = line.trim_start().strip_prefix("|<- ") {
                if !src.starts_with("demiurge-sink-output:")
                    && !desired.contains(&(src.to_string(), port.to_string()))
                {
                    util::log(&format!("scrub hw: {src} -X-> {port}"));
                    let _ = Command::new("pw-link").args(["-d", src, port]).stderr(Stdio::null()).status();
                }
            } else if !line.starts_with("  ") {
                current_port = None;
            }
        }
    }
}
