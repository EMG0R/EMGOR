// live.conf — the user-facing abstraction over the Rust session format.
//
// One file, edited in place on the running Pi (~/demiurge/live.conf). The
// launcher polls its mtime once a second; any save triggers a hot-reload
// that diffs the old vs new chain and reconciles (kill removed stages,
// start new ones, re-apply the graph, re-wire MIDI).
//
// Shape:
//
//   # optional auto-managed device block between these markers:
//   # DEMIURGE-DEVICES-BEGIN
//   # audio:  Scarlett Solo ..., UMC404HD ...
//   # midi:   Launchkey Mini MK3
//   # DEMIURGE-DEVICES-END
//
//   link = off            # on = join any Ableton Link peer on the LAN
//   bpm  = 110            # only used when link is off
//
//   chain =
//     ~/patterns/drive.strudel
//     ~/fx/vibrato.pd
//     ~/fx/delay.dsp
//
//   chain =                # a second, parallel chain — both feed `out`
//     ~/patterns/pad.strudel
//     ~/fx/reverb.dsp
//
// Rules:
//   - Language is inferred from file extension (no `run` prefix, no lang).
//   - `.dsp` and `.cpp` are compiled on demand by the launcher.
//   - Chain stages are piped in order, terminating at `out` (the virtual
//     sink). `out` is always appended automatically — you never write it.
//   - Multiple `chain =` blocks run in parallel; their terminal stages all
//     feed `demiurge-sink:playback_FL/FR`, which sums them for free.
//   - `in1 ->` / `in2 ->` as leading lines of a chain pull hardware input
//     1 / 2 (mono) into that chain's first stage. Inputs are always
//     addressed by number; there is no stereo input pair on DEMIURGE.
//     The legacy bare `in ->` form is accepted as an alias for `in1 ->`.
//   - `clock` and `pdclock` (when any stage is pd) are implicit — users
//     never list them. The launcher always supervises them.
//   - Blank lines and `#` comments are ignored.
//
// Translation to the internal session graph happens in `to_session`.

use std::collections::HashSet;
use std::fs;
use std::io;

use crate::config::{Edge, Program, Session};

#[derive(Clone, Debug, Default)]
pub struct ChainBlock {
    pub stages: Vec<String>,        // audio stages, piped in order
    pub hw_input_1_first: bool,     // chain begins with `in1 ->` (or legacy `in ->`)
    pub hw_input_2_first: bool,     // chain begins with `in2 ->`
}

#[derive(Clone, Debug, Default)]
pub struct LiveConfig {
    pub link: bool,
    pub bpm: Option<f64>,
    pub rate: Option<u32>,           // PipeWire clock.force-rate + JACK client srate, Hz. Default 48000.
    pub quantum: Option<u32>,        // PipeWire clock.force-quantum + JACK client bufsize, in frames @ rate
    pub chains: Vec<ChainBlock>,     // one or more parallel chains, all feeding `out`
    pub midi: Vec<String>,           // MIDI-only sidecars (launched, joined to bus, not in audio graph)
    pub inputs: Vec<String>,         // stage paths that also receive in1+in2 as extra edges
    pub sync_layer_off: bool,        // `sync_layer = off` (legacy `lowlatency=on`/`bypass=on`):
                                     // skip the PipeWire graph; run the chain's first program
                                     // directly on `audio_interface` (raw ALSA, lowest latency).
    pub audio_interface: Option<String>,  // legacy ALSA device for sync_layer=off, e.g. hw:CARD=USB,DEV=0
    pub interface: Option<String>,        // per-set explicit interface by product NAME (e.g. "Volt 2");
                                          // resolved to a card at launch, see util::resolve_interface
}

impl LiveConfig {
    // Total number of audio stages across every parallel chain.
    pub fn total_stages(&self) -> usize {
        self.chains.iter().map(|c| c.stages.len()).sum()
    }
}

pub const DEVICES_BEGIN: &str = "# DEMIURGE-DEVICES-BEGIN";
pub const DEVICES_END:   &str = "# DEMIURGE-DEVICES-END";

pub fn parse(path: &str) -> io::Result<LiveConfig> {
    let raw_text = fs::read_to_string(path)?;

    let mut lc = LiveConfig::default();

    // Which block, if any, indented lines belong to:
    //   Some(idx) into lc.chains — currently collecting a `chain =` block
    //   None                      — collecting `midi =` when in_midi is true
    let mut cur_chain: Option<usize> = None;
    let mut in_midi  = false;
    let mut in_inputs = false;
    let mut skip_devices_block = false;

    for raw in raw_text.lines() {
        // Skip the auto-managed devices block entirely.
        let tr = raw.trim();
        if tr == DEVICES_BEGIN { skip_devices_block = true; continue; }
        if tr == DEVICES_END   { skip_devices_block = false; continue; }
        if skip_devices_block  { continue; }

        let line = strip_comment(raw);
        if line.trim().is_empty() {
            // A blank line doesn't end a block — users leave space for
            // readability. A block ends at the next non-indented key=value.
            continue;
        }

        let is_indented = line.starts_with(' ') || line.starts_with('\t');
        let trimmed = line.trim();

        if is_indented {
            if let Some(ci) = cur_chain {
                // Stage line. Accept `in1 ->` / `in2 ->` (or legacy `in ->`),
                // `-> out` (noise — we always append out), or a raw path.
                if trimmed == "in1 ->" || trimmed == "in1->" || trimmed == "in1"
                    || trimmed == "in ->" || trimmed == "in->" || trimmed == "in"
                {
                    lc.chains[ci].hw_input_1_first = true;
                    continue;
                }
                if trimmed == "in2 ->" || trimmed == "in2->" || trimmed == "in2" {
                    lc.chains[ci].hw_input_2_first = true;
                    continue;
                }
                if trimmed == "-> out" || trimmed == "->out" || trimmed == "out" {
                    continue;
                }
                let path = trimmed.trim_end_matches("->").trim().to_string();
                if !path.is_empty() { lc.chains[ci].stages.push(path); }
                continue;
            }
            if in_midi {
                lc.midi.push(trimmed.to_string());
                continue;
            }
            if in_inputs {
                lc.inputs.push(trimmed.to_string());
                continue;
            }
        }

        // Non-indented: a new key or end of block.
        cur_chain = None;
        in_midi   = false;
        in_inputs = false;

        if let Some((k, v)) = trimmed.split_once('=') {
            let k = k.trim();
            let v = v.trim();
            match k {
                "link"    => lc.link = parse_bool(v),
                "bpm"     => lc.bpm  = v.parse().ok(),
                "rate"    => lc.rate = v.parse().ok(),
                "quantum" => lc.quantum = v.parse().ok(),
                // Sync layer flag. `sync_layer = off` bypasses the PipeWire graph.
                // Legacy aliases `lowlatency = on` / `bypass = on` mean the same.
                "sync_layer"          => lc.sync_layer_off = !parse_bool(v),
                "lowlatency" | "bypass" => if parse_bool(v) { lc.sync_layer_off = true; },
                "audio_interface"     => if !v.is_empty() { lc.audio_interface = Some(v.to_string()); },
                "interface"           => if !v.is_empty() { lc.interface = Some(v.to_string()); },
                "chain" => {
                    // Open a new parallel chain block.
                    lc.chains.push(ChainBlock::default());
                    let idx = lc.chains.len() - 1;
                    cur_chain = Some(idx);
                    if !v.is_empty() {
                        for part in v.split(',') {
                            let p = part.trim();
                            if !p.is_empty() { lc.chains[idx].stages.push(p.to_string()); }
                        }
                    }
                }
                "midi" => {
                    in_midi = true;
                    if !v.is_empty() {
                        for part in v.split(',') {
                            let p = part.trim();
                            if !p.is_empty() { lc.midi.push(p.to_string()); }
                        }
                    }
                }
                "inputs" => {
                    // Stages listed here receive in1 + in2 as extra parallel
                    // edges, in addition to whatever the chain already wires
                    // into their inputs. Lets a mid-chain csound stage take
                    // hardware capture directly without forcing the upstream
                    // ChucK stage to pass adc through.
                    in_inputs = true;
                    if !v.is_empty() {
                        for part in v.split(',') {
                            let p = part.trim();
                            if !p.is_empty() { lc.inputs.push(p.to_string()); }
                        }
                    }
                }
                _ => {}
            }
        }
    }

    // Prune empty chain blocks (user wrote `chain =` but no stages under it).
    lc.chains.retain(|c| !c.stages.is_empty());

    Ok(lc)
}

// Turn a LiveConfig into the internal Program+Edge graph.
//
// Always-present programs (not listed by the user):
//   - clock    (demiurge_clock, MIDI only)
//   - pdclock  (pd_clock_bridge.py, only if any stage is .pd)
//
// The clock program's `file` field encodes the Link state as a flag — the
// spawner reads this to pass --link=on|off to demiurge-clock.
pub fn to_session(lc: &LiveConfig) -> Session {
    let mut programs = Vec::new();
    let mut edges = Vec::new();

    // Implicit clock. file field encodes flags for the spawner: "link=on|off bpm=<val>".
    let bpm_part = lc.bpm.unwrap_or(120.0);
    let link_part = if lc.link { "link=on" } else { "link=off" };
    programs.push(Program {
        id:   "clock".into(),
        lang: "clock".into(),
        file: format!("{} bpm={:.1}", link_part, bpm_part),
        midi_only: false,
    });

    // Pd clock bridge, only when needed.
    let has_pd = lc.chains.iter().flat_map(|c| c.stages.iter()).any(|p| has_ext(p, "pd"));
    if has_pd {
        programs.push(Program {
            id:   "pdclock".into(),
            lang: "python".into(),
            file: expand_tilde("~/demiurge/bridges/pd_clock_bridge.py"),
            midi_only: true,
        });
    }

    // Chain stages. Each gets a synthetic id from its basename, uniquified.
    // Every chain block terminates at `out` — the virtual sink sums them
    // automatically because multiple pw-link edges into demiurge-sink:playback_FL/FR
    // are mixed for free.
    let mut ids_used: HashSet<String> = programs.iter().map(|p| p.id.clone()).collect();

    for block in &lc.chains {
        let mut last_id: Option<String> = None;
        for raw_path in &block.stages {
            let path = expand_tilde(raw_path);
            let lang = resolve_lang_from_ext(&path);
            let base = basename_no_ext(&path);
            let id   = uniquify(&mut ids_used, &base);
            programs.push(Program { id: id.clone(), lang, file: path, midi_only: false });
            if let Some(prev) = last_id.take() {
                edges.push(Edge { from: prev, to: id.clone() });
            } else {
                if block.hw_input_1_first {
                    edges.push(Edge { from: "in1".into(), to: id.clone() });
                }
                if block.hw_input_2_first {
                    edges.push(Edge { from: "in2".into(), to: id.clone() });
                }
            }
            last_id = Some(id);
        }
        if let Some(last) = last_id {
            edges.push(Edge { from: last, to: "out".into() });
        }
    }

    // `inputs =` directive: attach in1 + in2 as extra edges to listed stages.
    // Resolved by expanded file path against already-registered chain programs.
    for raw_path in &lc.inputs {
        let want = expand_tilde(raw_path);
        let Some(prog) = programs.iter().find(|p| p.file == want) else { continue; };
        edges.push(Edge { from: "in1".into(), to: prog.id.clone() });
        edges.push(Edge { from: "in2".into(), to: prog.id.clone() });
    }

    // MIDI-only sidecars: spawned and auto-joined to the MIDI bus by
    // midi::wire_bus, but never wired into the JACK audio graph. Used for
    // pattern generators (Strudel, Python drivers) that emit MIDI to steer
    // the audio chain.
    for raw_path in &lc.midi {
        let path = expand_tilde(raw_path);
        let lang = resolve_lang_from_ext(&path);
        let base = basename_no_ext(&path);
        let id   = uniquify(&mut ids_used, &base);
        programs.push(Program { id, lang, file: path, midi_only: true });
    }

    Session { programs, edges }
}

fn resolve_lang_from_ext(path: &str) -> String {
    let ext = path.rsplit('.').next().unwrap_or("").to_lowercase();
    match ext.as_str() {
        "csd"             => "csound",
        "ck"              => "chuck",
        "pd"              => "pd",
        "scd"             => "sc",
        "dsp"             => "faust",
        "py"              => "python",
        "cpp"             => "cpp",
        "rnbo"            => "rnbo",
        "mjs" | "strudel" => "strudel",
        "nam"             => "nam",
        _ => "auto",
    }.to_string()
}

fn has_ext(path: &str, ext: &str) -> bool {
    path.rsplit('.').next().map(|e| e.eq_ignore_ascii_case(ext)).unwrap_or(false)
}

fn basename_no_ext(path: &str) -> String {
    let base = path.rsplit('/').next().unwrap_or(path);
    match base.rsplit_once('.') {
        Some((stem, _)) => stem.to_string(),
        None => base.to_string(),
    }
}

fn uniquify(used: &mut HashSet<String>, base: &str) -> String {
    if !used.contains(base) {
        used.insert(base.to_string());
        return base.to_string();
    }
    for n in 2..1000 {
        let cand = format!("{base}_{n}");
        if !used.contains(&cand) {
            used.insert(cand.clone());
            return cand;
        }
    }
    base.to_string()
}

fn expand_tilde(path: &str) -> String {
    if let Some(rest) = path.strip_prefix("~/") {
        let home = std::env::var("HOME").unwrap_or_else(|_| "/home/gorpi7".into());
        format!("{home}/{rest}")
    } else {
        path.to_string()
    }
}

fn strip_comment(s: &str) -> &str {
    match s.find('#') {
        Some(i) => &s[..i],
        None => s,
    }
}

fn parse_bool(v: &str) -> bool {
    matches!(v.to_ascii_lowercase().as_str(), "on" | "true" | "yes" | "1")
}

