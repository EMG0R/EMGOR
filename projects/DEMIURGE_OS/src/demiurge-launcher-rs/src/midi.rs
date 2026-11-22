// Shared ALSA seq MIDI bus wiring. Every live program that registers an
// ALSA-seq client gets bidirectionally connected to "Midi Through" via
// aconnect. Physical MIDI devices are merged into Midi Through separately
// by /opt/demiurge/bin/demiurge-midi-connect.sh.
//
// The ALSA seq client namespace is *not* the same as the JACK client
// namespace. csound registers in JACK as `csound6` but in ALSA seq as
// `Csound`; Pd is `pure_data` in JACK and `Pure Data` in seq. This module
// owns the mapping from language to probable seq client name, probes
// `aconnect -l` for a match, and only calls aconnect if the target
// actually exists — otherwise the journal fills with `invalid destination
// address` for every raw-midi client (ChucK, etc.) on every boot.

use std::collections::{HashMap, HashSet};
use std::process::{Command, Stdio};

use crate::config::{Program, Session};
use crate::util;

pub fn wire_bus(session: &Session, clients: &HashMap<String, String>) {
    util::log("midi: wire bus");

    // Merge physical MIDI first.
    let _ = Command::new("/opt/demiurge/bin/demiurge-midi-connect.sh").status();

    let seq_clients = list_seq_clients();

    for prog in &session.programs {
        if clients.get(&prog.id).is_none() { continue; }

        // Find which (if any) candidate seq client name is actually live.
        let seq_name = candidate_seq_names(prog)
            .into_iter()
            .find(|n| seq_clients.contains(n));

        let Some(name) = seq_name else {
            util::log(&format!("midi: skip {} (no ALSA seq client registered)", prog.id));
            continue;
        };

        connect("Midi Through", &format!("{name}:0"));
        connect(&format!("{name}:0"), "Midi Through");
    }
}

// Candidate ALSA seq client names for a given program. Ordered by
// likelihood — the first one found in `aconnect -l` wins. Languages that
// use rawmidi (ChucK's default MidiIn, Pd with -alsamidi) return empty
// lists and are skipped entirely.
fn candidate_seq_names(prog: &Program) -> Vec<String> {
    match prog.lang.as_str() {
        "csound"                       => vec!["Csound".into()],
        "pd"                           => vec!["Pure Data".into()],
        "sc" | "sclang" | "supercollider" => vec!["SuperCollider".into()],
        // Faust binaries built with `-midi` register their basename as
        // the seq client name.
        "faust" | "cpp" => {
            let base = prog.file.rsplit('/').next().unwrap_or(&prog.file).to_string();
            vec![base]
        }
        // Python sidecars — rtmidi-backed scripts show up by whatever
        // name they pass to MidiOut. Fall back to the program id.
        "python" => vec![prog.id.clone()],
        _ => vec![],
    }
}

// Parse `aconnect -l` for the set of ALSA seq client *names*. Lines look like:
//   client 14: 'Midi Through' [type=kernel]
//   client 128: 'Csound' [type=user,pid=5344]
// We match the single-quoted name. ChucK's rawmidi usage means it never
// shows up here — which is exactly why we probe instead of blind-calling.
fn list_seq_clients() -> HashSet<String> {
    let out = Command::new("aconnect")
        .arg("-l")
        .output()
        .map(|o| String::from_utf8_lossy(&o.stdout).into_owned())
        .unwrap_or_default();
    let mut set = HashSet::new();
    for line in out.lines() {
        if !line.starts_with("client ") { continue; }
        let Some(start) = line.find('\'') else { continue; };
        let rest = &line[start + 1..];
        let Some(end) = rest.find('\'') else { continue; };
        // aconnect right-pads client names to 16 chars inside the quotes;
        // strip trailing whitespace so the exact name comparison matches.
        let name = rest[..end].trim_end().to_string();
        if !name.is_empty() { set.insert(name); }
    }
    set
}

fn connect(from: &str, to: &str) {
    let status = Command::new("aconnect")
        .args([from, to])
        .stderr(Stdio::null())
        .status();
    if let Ok(s) = status {
        if s.success() {
            util::log(&format!("midi: {from} → {to}"));
        }
    }
}
