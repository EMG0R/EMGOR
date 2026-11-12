// Internal session graph types + boot-config reader.
//
// The user-facing config format is `live.conf`, parsed in `live.rs`.
// `Session`/`Program`/`Edge` are the *internal* graph the launcher
// actually runs — `live::to_session` is the only place that builds them.
// There is no longer a user-facing `run` / `patch` session-file format;
// live.conf is the single surface, the Rust layer is hidden.
//
// Boot config (/boot/firmware/demiurge.conf) still has one key:
//
//   launch = <absolute path to a live.conf>
//
// Reserved endpoints (used by `live::to_session` when building edges):
//   in1 → hardware input 1 (mono) — Scarlett Solo Input 1, or first
//         alsa_input node matching `__Mic1__source:capture_MONO`.
//   in2 → hardware input 2 (mono) — Scarlett Solo Input 2, likewise.
//   out → demiurge-sink (loopback-linked to physical sinks by the
//         aggregation pass).
//
// Inputs are always numbered and always mono. Stereo is not a concept at
// the input layer — if a stage needs two hardware channels, declare both
// `in1 ->` and `in2 ->` at the head of the chain. The graph module then
// routes in1 to the stage's first input port and in2 to the second.

use std::fs;
use std::io;

#[derive(Clone, Debug)]
pub struct Program {
    pub id: String,
    pub lang: String,
    pub file: String,
    // True when the program is in the `midi =` block of live.conf: spawned
    // and joined to the MIDI bus but never wired into the JACK audio graph.
    // Used by spawn_program to pick a MIDI-only invocation for languages
    // that have one (e.g. Strudel's `--midi` flag).
    pub midi_only: bool,
}

#[derive(Clone, Debug)]
pub struct Edge {
    pub from: String,
    pub to: String,
}

#[derive(Clone, Debug)]
pub struct Session {
    pub programs: Vec<Program>,
    pub edges: Vec<Edge>,
}

pub fn parse_boot_launch(path: &str) -> io::Result<String> {
    let contents = fs::read_to_string(path).unwrap_or_default();
    for raw in contents.lines() {
        let line = strip_comment(raw).trim().to_string();
        if let Some(rest) = line.strip_prefix("launch") {
            if let Some(eq) = rest.find('=') {
                return Ok(rest[eq + 1..].trim().to_string());
            }
        }
    }
    Ok(String::new())
}

fn strip_comment(s: &str) -> &str {
    match s.find('#') {
        Some(i) => &s[..i],
        None => s,
    }
}
