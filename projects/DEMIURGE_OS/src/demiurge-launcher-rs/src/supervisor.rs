// Child process supervisor. Launches each program via the
// /opt/demiurge/bin/demiurge-run-<lang> wrapper, tracks PIDs, and resolves
// the JACK / ALSA-seq client names for graph wiring.

use std::collections::HashMap;
use std::fs::{self, OpenOptions};
use std::path::{Path, PathBuf};
use std::process::{Child, Command, Stdio};
use std::thread;
use std::time::{Duration, Instant};

use crate::config::{Program, Session};
use crate::util;

// Stage respawn. Until 2026-08-11 nothing here restarted a dead stage:
// Event::ChildExited called mark_dead() and re-applied the graph, and that was
// the end of it. demiurge.service's Restart=on-failure restarts the LAUNCHER,
// and the launcher does not exit when one stage of several dies (the clock
// keeps the child-watcher's pid set non-empty). So any engine death was a
// silent instrument until a human noticed and restarted by hand — which
// happened three times on 2026-08-10, twice on the standalone Csound backend.
//
// Restarts are capped and backed off on purpose. A stage that dies instantly
// and forever (bad csd, missing plugin, quantum its ksmps can't divide) must
// not be respawned in a tight loop: every wrapper starts its engine at
// SCHED_FIFO 74 on the isolated core, and a respawn storm at RT priority is
// precisely the shape that hard-reset this board twice the same night. After
// MAX_RESTARTS the stage stays down and says so, loudly, once.
const MAX_RESTARTS: u32 = 5;

// Consecutive heartbeats (2 s each) a resolved client may be missing from the
// PipeWire graph before we treat the stage as failed. THIS IS THE CASE PROCESS
// SUPERVISION MISSES: the observed failure was not a crash. The process stayed
// alive and healthy-looking at 0% CPU with every JACK port deregistered — no
// exit, no signal, nothing for try_wait() to see, and no audio. Watching only
// for process exit would never have caught it.
//
// Three strikes rather than one: resolve_clients can legitimately race a
// restarting client, and PipeWire briefly drops ports during a quantum change.
const GRAPH_ABSENT_STRIKES: u32 = 3;

// How long a freshly spawned stage is exempt from the absence scan.
//
// MEASURED THE HARD WAY: without this, the scan fires DURING normal startup.
// Csound takes several seconds to compile a large orchestra before it registers
// a JACK client, the scan counted three strikes in 6 s, killed a perfectly
// healthy engine that was still compiling, and restarted it — burning three
// restart attempts in 18 seconds. That is the restart storm this whole module
// is supposed to prevent, caused by the detector meant to help.
//
// It is made worse by a real interaction: demiurge-run-csound destroys stale
// `csound6` PipeWire nodes at startup, so an overlapping respawn can delete the
// node of the engine that is currently healthy. The grace window covers that
// overlap too.
//
// 45 s sits well past the ~6 s init of the NEPTR patch and past
// resolve_client's own 10 s deadline. A stage genuinely portless for 45 s is
// broken; 45 s of silence is bad, a kill-loop at SCHED_FIFO 74 is worse.
const GRAPH_ABSENT_GRACE: Duration = Duration::from_secs(45);

// How long a stage that has NEVER registered gets before we call it failed.
// Deliberately generous: this covers "engine is still compiling a large
// orchestra", and the cost of waiting is silence on ONE stage, while the cost
// of being too eager is killing a healthy engine mid-startup.
const REGISTER_DEADLINE: Duration = Duration::from_secs(90);

// A stage that has held its ports this long has proven itself; give it a fresh
// restart budget so a long session cannot slowly exhaust the cap.
const HEALTHY_RESET_AFTER: Duration = Duration::from_secs(120);

fn restart_backoff(attempt: u32) -> Duration {
    Duration::from_secs(match attempt { 0 => 2, 1 => 5, 2 => 15, 3 => 30, _ => 60 })
}

const WRAPPER_DIR: &str = "/opt/demiurge/bin";
const CLOCK_BIN:   &str = "/opt/demiurge/bin/demiurge-clock";

pub struct RunningProg {
    pub child: Option<Child>,
    pub pid: u32,
}

pub struct Supervisor {
    pub session: Session,
    pub running: Vec<RunningProg>,
    pub clients: HashMap<String, String>, // program id → resolved client name
    pub dead: Vec<bool>,
    pub rate: u32,                        // injected into every child as DEMIURGE_RATE
    pub quantum: u32,                     // injected into every child as DEMIURGE_QUANTUM
    // Per-stage respawn bookkeeping. Kept sized to session.programs by
    // sync_supervision_state(), which is cheaper than threading resizes
    // through every path that rebuilds `dead` (start_all, reconcile, the
    // quantum fallback).
    restarts: Vec<u32>,
    next_try: Vec<Option<Instant>>,
    absent_strikes: Vec<u32>,
    spawned_at: Vec<Option<Instant>>,
    seen_present: Vec<bool>,
}

impl Supervisor {
    pub fn new(session: Session, rate: u32, quantum: u32) -> Self {
        let n = session.programs.len();
        Self {
            session,
            running: (0..n).map(|_| RunningProg { child: None, pid: 0 }).collect(),
            clients: HashMap::new(),
            dead: vec![false; n],
            rate,
            quantum,
            restarts: vec![0; n],
            next_try: vec![None; n],
            absent_strikes: vec![0; n],
            spawned_at: vec![None; n],
            seen_present: vec![false; n],
        }
    }

    fn sync_supervision_state(&mut self) {
        let n = self.session.programs.len();
        self.restarts.resize(n, 0);
        self.next_try.resize(n, None);
        self.absent_strikes.resize(n, 0);
        self.spawned_at.resize(n, None);
        self.seen_present.resize(n, false);
    }

    pub fn start_all(&mut self) {
        self.sync_supervision_state();
        for idx in 0..self.session.programs.len() {
            self.spawn_one(idx);
        }
    }

    // Start (or restart) exactly one stage. Extracted from start_all so respawn
    // and first launch cannot drift apart — a restarted stage must get the
    // identical wrapper, env and compile treatment as the original, or the
    // recovery path is a second, less-tested code path that only runs when
    // something is already going wrong.
    fn spawn_one(&mut self, idx: usize) -> bool {
        {
            let resolved_lang = resolve_lang(&self.session.programs[idx].lang, &self.session.programs[idx].file);
            self.session.programs[idx].lang = resolved_lang;
            // `opgorator`: no process to spawn — the Pocket OpGorator/Daisy
            // Seed USB capture device is (or isn't) already a node in the
            // PipeWire graph. resolve_clients() finds it by substring match;
            // graph::apply skips its edges gracefully if it never resolves.
            if self.session.programs[idx].lang == "opgorator" {
                util::log(&format!("opgorator: device passthrough (no process) — {}", self.session.programs[idx].id));
                self.running[idx] = RunningProg { child: None, pid: 0 };
                return true;
            }
            // .dsp / .cpp stages: ensure the compiled binary exists, and
            // rewrite the Program.file to point at it so the wrappers run
            // the right thing. Compile failures leave the program dead.
            if let Err(e) = ensure_compiled(&mut self.session.programs[idx]) {
                util::log(&format!("compile failed: {}: {e}", self.session.programs[idx].id));
                self.dead[idx] = true;
                return false;
            }
            let prog = &self.session.programs[idx];
            let plan = crate::params::plan_for(&self.session.programs, idx);
            match spawn_program(prog, self.rate, self.quantum, plan.as_ref()) {
                Ok(child) => {
                    let pid = child.id();
                    util::log(&format!("launch: {} ({}) → {} [pid {pid}]", prog.id, prog.lang, prog.file));
                    self.running[idx] = RunningProg { child: Some(child), pid };
                    self.dead[idx] = false;
                    self.absent_strikes[idx] = 0;
                    self.spawned_at[idx] = Some(Instant::now());
                    self.seen_present[idx] = false;
                    true
                }
                Err(e) => {
                    util::log(&format!("launch failed: {} ({}): {e}", prog.id, prog.lang));
                    self.dead[idx] = true;
                    false
                }
            }
        }
    }

    // Respawn any stage that is down and due a retry. Returns the ids brought
    // back up, so the caller can re-resolve clients, re-apply the graph and
    // re-arm the child watcher (whose pid set is fixed at spawn time and would
    // otherwise never see the NEXT death of a restarted stage).
    pub fn respawn_dead(&mut self) -> Vec<String> {
        self.sync_supervision_state();
        let mut revived = Vec::new();
        for idx in 0..self.session.programs.len() {
            if !self.dead[idx] { continue; }
            // No process by design — never "dead" in a way respawning fixes.
            if self.session.programs[idx].lang == "opgorator" { continue; }

            if self.restarts[idx] >= MAX_RESTARTS {
                continue; // already reported when the cap was hit
            }
            if let Some(at) = self.next_try[idx] {
                if Instant::now() < at { continue; }
            } else {
                // First observation of this death: schedule, don't fire. One
                // heartbeat of delay lets an orderly shutdown (reconcile,
                // quantum change) settle instead of racing it.
                self.next_try[idx] = Some(Instant::now() + restart_backoff(0));
                continue;
            }

            // Reap the corpse so we do not leak a zombie per restart.
            if let Some(mut ch) = self.running[idx].child.take() {
                let _ = ch.kill();
                let _ = ch.wait();
            }
            self.running[idx] = RunningProg { child: None, pid: 0 };

            let attempt = self.restarts[idx] + 1;
            self.restarts[idx] = attempt;
            let id = self.session.programs[idx].id.clone();
            util::log(&format!("respawn: '{id}' is down — restart attempt {attempt}/{MAX_RESTARTS}"));

            if self.spawn_one(idx) {
                self.clients.remove(&id);
                revived.push(id);
            } else if self.restarts[idx] >= MAX_RESTARTS {
                util::log("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
                util::log(&format!("respawn: GIVING UP on '{id}' after {MAX_RESTARTS} attempts"));
                util::log("respawn: this stage stays DOWN — there is no audio from it until fixed");
                util::log("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
            }
            self.next_try[idx] = Some(Instant::now() + restart_backoff(attempt));
        }
        revived
    }

    // Catch the stage that is ALIVE but has fallen out of the graph.
    //
    // This is the failure that actually bit us: process up, try_wait() reports
    // running, 0% CPU, every JACK port deregistered, no audio. Process
    // supervision cannot see it. So: if a stage's resolved client has no ports
    // in `pw-link -o` for GRAPH_ABSENT_STRIKES consecutive heartbeats, kill it
    // and let respawn_dead() bring it back.
    pub fn scan_graph_presence(&mut self) {
        self.sync_supervision_state();
        let present = pw_client_names();
        // An empty listing means pw-link failed or PipeWire is not answering —
        // that is a broken probe, not evidence that every stage vanished.
        // Killing the whole chain on a failed probe would be catastrophic.
        if present.is_empty() { return; }

        for idx in 0..self.session.programs.len() {
            if self.dead[idx] { continue; }
            let lang = self.session.programs[idx].lang.clone();
            // clock is ALSA-seq only and never owns JACK audio ports;
            // opgorator is a device we did not spawn. Neither can be judged
            // by this test, and killing the clock on it would be a own-goal.
            if lang == "clock" || lang == "opgorator" { continue; }

            let id = self.session.programs[idx].id.clone();
            let Some(client) = self.clients.get(&id).cloned() else { continue; };

            if present.contains(&client) {
                self.absent_strikes[idx] = 0;
                self.seen_present[idx] = true;
                // Proven healthy for a good while → restore its restart budget.
                if let Some(t) = self.spawned_at[idx] {
                    if t.elapsed() >= HEALTHY_RESET_AFTER && self.restarts[idx] > 0 {
                        util::log(&format!(
                            "supervise: '{id}' healthy for {}s — restart budget reset",
                            t.elapsed().as_secs()
                        ));
                        self.restarts[idx] = 0;
                        self.next_try[idx] = None;
                    }
                }
                continue;
            }

            // TWO DIFFERENT FAULTS, TWO DIFFERENT DEADLINES.
            //
            //   seen_present == true  — it registered, played, then lost its
            //     ports. Startup is provably over, so absence is immediately
            //     suspicious and three strikes (6 s) is enough.
            //
            //   seen_present == false — it has never registered since this
            //     spawn. That is either an engine still compiling a large
            //     orchestra, or one that will never come up. Only time
            //     separates them, so it gets REGISTER_DEADLINE.
            //
            // Both must be handled. An earlier version skipped the second case
            // entirely on the theory that "never registered" was somebody
            // else's problem — and a respawned Csound then sat alive with no
            // ports for 6 minutes and the rig stayed silent, because nothing
            // was watching that case at all. Backoff and the retry cap are what
            // make policing it safe, not refusing to police it.
            let since_spawn = self.spawned_at[idx].map(|t| t.elapsed());
            let deadline = if self.seen_present[idx] { GRAPH_ABSENT_GRACE } else { REGISTER_DEADLINE };
            if !self.seen_present[idx] {
                match since_spawn {
                    Some(e) if e >= deadline => {}   // never came up — treat as failed
                    _ => continue,
                }
            }
            self.absent_strikes[idx] += 1;
            let strikes = self.absent_strikes[idx];
            util::log(&format!(
                "supervise: '{id}' client '{client}' missing from the graph ({strikes}/{GRAPH_ABSENT_STRIKES})"
            ));
            if strikes < GRAPH_ABSENT_STRIKES { continue; }

            util::log(&format!(
                "supervise: '{id}' is ALIVE but has no ports ({}) — treating as failed and restarting",
                if self.seen_present[idx] { "registered earlier, then vanished" }
                else { "never registered" }
            ));
            if let Some(mut ch) = self.running[idx].child.take() {
                let _ = ch.kill();
                let _ = ch.wait();
            }
            self.running[idx] = RunningProg { child: None, pid: 0 };
            self.dead[idx] = true;
            self.absent_strikes[idx] = 0;
            // Do NOT reset an existing backoff. Overwriting it with `now` was
            // the second half of the storm: each absence detection re-armed an
            // immediate retry, so the backoff computed by respawn_dead() never
            // actually delayed anything. Take whichever is LATER.
            let now = Instant::now();
            self.next_try[idx] = Some(match self.next_try[idx] {
                Some(pending) if pending > now => pending,
                _ => now,
            });
        }
    }

    // Reconcile the running set against a new session. Matching is done
    // by (id, file): a stage with the same id + same file is considered
    // unchanged and kept alive. Everything else is torn down and restarted.
    // Edges are replaced wholesale — graph::apply is idempotent so it'll
    // converge on the next call.
    pub fn reconcile(&mut self, new_session: Session) {
        let mut new_session = new_session;

        // Normalize .dsp / .cpp stages in the new session to their compiled
        // binary paths so the (id, file) match against already-running
        // programs (which were normalized at start_all time) actually hits.
        // ensure_compiled is idempotent when the binary is newer than the source.
        for idx in 0..new_session.programs.len() {
            new_session.programs[idx].lang = resolve_lang(&new_session.programs[idx].lang, &new_session.programs[idx].file);
            let _ = ensure_compiled(&mut new_session.programs[idx]);
        }

        let mut keep: Vec<Option<(Child, u32, String)>> = (0..new_session.programs.len()).map(|_| None).collect();

        // For each new program, find a matching old program.
        for (new_idx, new_prog) in new_session.programs.iter().enumerate() {
            for (old_idx, old_prog) in self.session.programs.iter().enumerate() {
                if self.dead[old_idx] { continue; }
                if old_prog.id == new_prog.id && old_prog.file == new_prog.file && old_prog.midi_only == new_prog.midi_only {
                    if let Some(child) = self.running[old_idx].child.take() {
                        let pid = self.running[old_idx].pid;
                        let client = self.clients.get(&old_prog.id).cloned().unwrap_or_default();
                        keep[new_idx] = Some((child, pid, client));
                    }
                    break;
                }
            }
        }

        // Kill everything from the old session that wasn't claimed.
        for (idx, r) in self.running.iter_mut().enumerate() {
            if let Some(child) = r.child.as_mut() {
                util::log(&format!("reconcile: stopping '{}'", self.session.programs[idx].id));
                let _ = child.kill();
            }
        }
        for r in self.running.iter_mut() {
            if let Some(mut child) = r.child.take() {
                let _ = child.wait();
            }
        }

        // Move to the new session.
        let n = new_session.programs.len();
        self.session = new_session;
        self.running = (0..n).map(|_| RunningProg { child: None, pid: 0 }).collect();
        self.dead = vec![false; n];
        let mut new_clients: HashMap<String, String> = HashMap::new();

        // Restore kept children and start the rest.
        for idx in 0..self.session.programs.len() {
            let resolved_lang = resolve_lang(&self.session.programs[idx].lang, &self.session.programs[idx].file);
            self.session.programs[idx].lang = resolved_lang;
            if self.session.programs[idx].lang == "opgorator" {
                self.running[idx] = RunningProg { child: None, pid: 0 };
                util::log(&format!("reconcile: opgorator passthrough '{}'", self.session.programs[idx].id));
                continue;
            }
            if let Some((child, pid, client)) = keep[idx].take() {
                self.running[idx] = RunningProg { child: Some(child), pid };
                if !client.is_empty() {
                    new_clients.insert(self.session.programs[idx].id.clone(), client);
                }
                util::log(&format!("reconcile: kept '{}'", self.session.programs[idx].id));
                continue;
            }
            if let Err(e) = ensure_compiled(&mut self.session.programs[idx]) {
                util::log(&format!("reconcile: compile failed {}: {e}", self.session.programs[idx].id));
                self.dead[idx] = true;
                continue;
            }
            let plan = crate::params::plan_for(&self.session.programs, idx);
            let prog = &self.session.programs[idx];
            match spawn_program(prog, self.rate, self.quantum, plan.as_ref()) {
                Ok(child) => {
                    let pid = child.id();
                    util::log(&format!("reconcile: launched '{}' (pid {pid})", prog.id));
                    self.running[idx] = RunningProg { child: Some(child), pid };
                }
                Err(e) => {
                    util::log(&format!("reconcile: launch failed {}: {e}", prog.id));
                    self.dead[idx] = true;
                }
            }
        }
        self.clients = new_clients;
    }


    pub fn child_pids(&self) -> Vec<u32> {
        self.running.iter().map(|r| r.pid).filter(|p| *p > 0).collect()
    }

    pub fn resolve_clients(&mut self) {
        for (idx, prog) in self.session.programs.iter().enumerate() {
            if self.dead[idx] { continue; }
            let pid = self.running[idx].pid;
            if let Some(client) = resolve_client(prog, pid) {
                self.clients.insert(prog.id.clone(), client);
            }
        }
    }

    pub fn mark_dead(&mut self, pid: u32) {
        for (idx, r) in self.running.iter().enumerate() {
            if r.pid == pid {
                self.dead[idx] = true;
                util::log(&format!("program '{}' marked dead", self.session.programs[idx].id));
                break;
            }
        }
    }

    pub fn state_fingerprint(&mut self) -> String {
        // Include live/dead status of every child. Cheap to compute, used
        // by the heartbeat loop as a change detector.
        let mut out = String::new();
        for (idx, r) in self.running.iter_mut().enumerate() {
            let alive = if let Some(ref mut ch) = r.child {
                match ch.try_wait() {
                    Ok(Some(_)) => false,
                    Ok(None) => true,
                    Err(_) => false,
                }
            } else if self.session.programs[idx].lang == "opgorator" {
                // No process by design — "alive" here just means "still a
                // no-spawn passthrough stage", not "has a live PID". Keeping
                // this true (never marking it dead[idx]) is what lets a
                // late-plugged OpGorator get re-resolved on the next
                // hot-plug instead of being permanently skipped.
                true
            } else { false };
            if !alive { self.dead[idx] = true; }
            out.push_str(&format!("{}={};", self.session.programs[idx].id, alive as u8));
        }
        out
    }

    pub fn shutdown(&mut self) {
        for r in self.running.iter_mut() {
            if let Some(child) = r.child.as_mut() {
                let _ = child.kill();
            }
        }
        for r in self.running.iter_mut() {
            if let Some(mut child) = r.child.take() {
                let _ = child.wait();
            }
        }
    }
}

// Dispatch a program to the right executable:
//   - "clock" has no wrapper — we exec the binary directly with --link=...
//   - everything else goes through /opt/demiurge/bin/demiurge-run-<lang>
//
// stdout+stderr for every child are captured to ~/.demiurge/logs/<id>.log.
// That log is the only place a dying stage's real error message shows up —
// the launcher's own journal only prints "program 'foo' marked dead".
fn spawn_program(prog: &Program, rate: u32, quantum: u32, plan: Option<&crate::params::LaunchPlan>) -> std::io::Result<Child> {
    let (out, err) = child_log_files(&prog.id)?;
    let r = rate.to_string();
    let q = quantum.to_string();
    if prog.lang == "clock" {
        // prog.file is "link=on|off bpm=<val>" encoded by live::to_session.
        let link_on = prog.file.contains("link=on");
        let link_flag = if link_on { "--link=on" } else { "--link=off" };
        let bpm_val: f64 = prog.file.split_whitespace()
            .find(|s| s.starts_with("bpm="))
            .and_then(|s| s[4..].parse().ok())
            .unwrap_or(120.0);
        let mut cmd = Command::new(CLOCK_BIN);
        cmd.arg(link_flag);
        cmd.arg(format!("--bpm={:.1}", bpm_val));
        cmd.env("DEMIURGE_RATE", &r);
        cmd.env("DEMIURGE_QUANTUM", &q);
        return cmd
            .stdin(Stdio::null())
            .stdout(out)
            .stderr(err)
            .spawn();
    }
    let wrapper = format!("{WRAPPER_DIR}/demiurge-run-{}", prog.lang);
    let mut cmd = Command::new(&wrapper);
    // Parameter-pool contract: env for every stage, plus per-language argv
    // (ChucK hook and Pd -path/-send before the file; Csound macros after it).
    // Wrappers are `exec <engine> ... "$FILE" "$@"`, so pre_args ride in as
    // the wrapper's first arguments and the real file follows them.
    if let Some(p) = plan {
        cmd.args(&p.pre_args);
    }
    cmd.arg(&prog.file);
    cmd.env("DEMIURGE_RATE", &r);
    cmd.env("DEMIURGE_QUANTUM", &q);
    if let Some(p) = plan {
        for (k, v) in &p.env { cmd.env(k, v); }
        cmd.args(&p.post_args);
    }
    // Strudel in a `midi =` block runs in MIDI-only mode: the strudel-runner
    // emits MIDI to the shared bus instead of hosting a cpal JACK client.
    if prog.midi_only && prog.lang == "strudel" {
        cmd.arg("--midi");
    }
    cmd.stdin(Stdio::null())
        .stdout(out)
        .stderr(err)
        .spawn()
}

// Open ~/.demiurge/logs/<id>.log (truncated) and return two handles for the
// child's stdout and stderr. Truncating on each spawn matches user expectation:
// if a stage is restarted you want the fresh failure, not an append pile.
fn child_log_files(id: &str) -> std::io::Result<(Stdio, Stdio)> {
    let home = std::env::var("HOME").unwrap_or_else(|_| "/tmp".into());
    let dir = PathBuf::from(format!("{home}/.demiurge/logs"));
    fs::create_dir_all(&dir)?;
    let path = dir.join(format!("{id}.log"));
    let f = OpenOptions::new().create(true).write(true).truncate(true).open(&path)?;
    let out = f.try_clone()?;
    Ok((Stdio::from(out), Stdio::from(f)))
}

fn compile_log_path(id: &str) -> PathBuf {
    let home = std::env::var("HOME").unwrap_or_else(|_| "/tmp".into());
    PathBuf::from(format!("{home}/.demiurge/logs")).join(format!("{id}.compile.log"))
}

fn compile_log_handles(path: &Path) -> std::io::Result<(Stdio, Stdio)> {
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)?;
    }
    let f = OpenOptions::new().create(true).write(true).truncate(true).open(path)?;
    let out = f.try_clone()?;
    Ok((Stdio::from(out), Stdio::from(f)))
}

// Ensure the runnable artifact for a program exists on disk. For .dsp
// (Faust) and .cpp (C++ JACK) sources this means compiling the sibling
// binary on demand. Idempotent: skips the build when the binary is
// already newer than the source.
fn ensure_compiled(prog: &mut Program) -> std::io::Result<()> {
    match prog.lang.as_str() {
        "faust" => {
            // Faust stages may be listed as either the binary or the .dsp
            // source. Normalize: drop the .dsp extension to get the binary.
            let src = if prog.file.ends_with(".dsp") {
                prog.file.clone()
            } else {
                // Already a binary path; nothing to do.
                return Ok(());
            };
            let bin = src.trim_end_matches(".dsp").to_string();
            if needs_rebuild(&bin, &src) {
                let dir = std::path::Path::new(&src).parent()
                    .map(|p| p.to_string_lossy().to_string())
                    .unwrap_or_else(|| ".".into());
                let file_name = std::path::Path::new(&src).file_name()
                    .map(|n| n.to_string_lossy().to_string())
                    .unwrap_or_else(|| src.clone());
                let log_path = compile_log_path(&prog.id);
                util::log(&format!("faust: compiling {file_name} in {dir} (log: {})", log_path.display()));
                let (out, err) = compile_log_handles(&log_path)?;
                // -midi always; -osc only if a manifest names this stage (params.rs)
                let home = std::env::var("HOME").unwrap_or_else(|_| "/tmp".into());
                let manifests = crate::params::read_manifests(&Path::new(&home).join("demiurge/params"));
                let flags = crate::params::faust_build_flags(&manifests, prog);
                let status = Command::new("faust2jackconsole")
                    .args(&flags)
                    .arg(&file_name)
                    .current_dir(&dir)
                    .stdin(Stdio::null())
                    .stdout(out)
                    .stderr(err)
                    .status()?;
                if !status.success() {
                    return Err(std::io::Error::new(std::io::ErrorKind::Other,
                        format!("faust2jackconsole exited with {status}; see {}", log_path.display())));
                }
            }
            prog.file = bin;
            Ok(())
        }
        "cpp" => {
            // C++ stages may be listed as either the binary or the .cpp source.
            let src = if prog.file.ends_with(".cpp") {
                prog.file.clone()
            } else {
                // If prog.file is a binary path and the binary exists, we're done.
                if Path::new(&prog.file).exists() { return Ok(()); }
                // Otherwise, treat it as a basename: try a sibling .cpp.
                format!("{}.cpp", prog.file)
            };
            let bin = src.trim_end_matches(".cpp").to_string();
            if needs_rebuild(&bin, &src) {
                let log_path = compile_log_path(&prog.id);
                util::log(&format!("cpp: compiling {src} (log: {})", log_path.display()));
                let (out, err) = compile_log_handles(&log_path)?;
                let status = Command::new("g++")
                    .arg("-O2")
                    .arg("-o").arg(&bin)
                    .arg(&src)
                    .arg("-ljack").arg("-lasound").arg("-lm").arg("-lpthread")
                    .stdin(Stdio::null())
                    .stdout(out)
                    .stderr(err)
                    .status()?;
                if !status.success() {
                    return Err(std::io::Error::new(std::io::ErrorKind::Other,
                        format!("g++ exited with {status}; see {}", log_path.display())));
                }
            }
            prog.file = bin;
            Ok(())
        }
        _ => Ok(()),
    }
}

fn needs_rebuild(bin: &str, src: &str) -> bool {
    let bm = std::fs::metadata(bin).and_then(|m| m.modified());
    let sm = std::fs::metadata(src).and_then(|m| m.modified());
    match (bm, sm) {
        (Ok(b), Ok(s)) => b < s,
        (Err(_), Ok(_)) => true,
        _ => true,
    }
}

pub fn resolve_lang(lang: &str, file: &str) -> String {
    let normalized = match lang {
        "sclang" | "supercollider" => "sc",
        other => other,
    };
    if normalized != "auto" { return normalized.to_string(); }
    // Infer from file extension.
    let ext = file.rsplit('.').next().unwrap_or("").to_lowercase();
    match ext.as_str() {
        "csd" => "csound",
        "ck" => "chuck",
        "pd" => "pd",
        "scd" => "sc",
        "dsp" => "faust",
        "py" => "python",
        "cpp" => "cpp",
        "rnbo" => "rnbo",
        "mjs" | "strudel" => "strudel",
        "nam" => "nam",
        _ => "faust", // fallthrough: precompiled binary treated as faust
    }.to_string()
}

fn candidate_client_names(prog: &Program) -> Vec<String> {
    match prog.lang.as_str() {
        // USS guests (program id `uss-<name>`) register their own JACK client named after
        // the id (demiurge-run-csound), so they are never confused with NEPTR's `csound6`.
        "csound" if prog.id.starts_with("uss-") => vec![prog.id.clone()],
        "csound" | "nam" => vec!["csound6".into(), "csound".into(), "Csound".into()],
        "chuck" => vec!["ChucK".into(), "chuck".into(), "Chuck".into()],
        "pd" => vec!["pure_data".into()],
        "sc" | "sclang" | "supercollider" => vec!["SuperCollider".into()],
        "faust" | "cpp" => {
            let base = basename(&prog.file);
            vec![base.clone(), format!("cpp_{base}")]
        }
        // RNBO stages don't spawn their own JACK client — the always-on
        // rnbo-runner does, and loading a patcher creates a JACK client
        // named "<patchername>-<index>". The demiurge-run-rnbo wrapper
        // loads the patcher (file basename, sans .rnbo) into instance 0,
        // so the client to wire is "<patchername>-0".
        "rnbo" => {
            let patcher = basename_no_ext(&prog.file);
            vec![format!("{patcher}-0")]
        }
        "python" => vec!["python".into(), "python3".into()],
        "clock" => return vec!["demiurge_clock".into()],
        // opgorator has no spawned process and no exact client name — its
        // node name is whatever the ALSA monitor gave the USB device (e.g.
        // "alsa_input.usb-...Pocket_OpGorator..."). resolve_client special-
        // cases this lang and does a substring search instead of using
        // these candidates directly; kept here for documentation/consistency.
        "opgorator" => vec!["OpGorator".into(), "Daisy".into()],
        // strudel-runner.mjs hosts its audio graph on cpal → JACK, and that
        // client always registers as `cpal_client_out` (out_0/out_1). The
        // old bash launcher had a dedicated wire_strudel_audio step; here
        // we just treat it as another candidate name so the normal graph
        // application path links it like any other stage.
        "strudel" => vec!["cpal_client_out".into()],
        _ => vec![prog.lang.clone()],
    }
}

fn basename(path: &str) -> String {
    path.rsplit('/').next().unwrap_or(path).to_string()
}

fn basename_no_ext(path: &str) -> String {
    let base = path.rsplit('/').next().unwrap_or(path);
    match base.rsplit_once('.') {
        Some((stem, _)) => stem.to_string(),
        None => base.to_string(),
    }
}

// Client names that currently own at least one output port in the graph.
// Deliberately `pw-link -o`: a stage that has fallen out of the graph has no
// ports of any kind, and every engine we supervise is a source of audio.
fn pw_client_names() -> std::collections::HashSet<String> {
    let mut set = std::collections::HashSet::new();
    let Ok(out) = Command::new("pw-link").arg("-o").output() else { return set };
    if !out.status.success() { return set; }
    for line in String::from_utf8_lossy(&out.stdout).lines() {
        if let Some((client, _)) = line.split_once(':') {
            set.insert(client.trim().to_string());
        }
    }
    set
}

fn resolve_client(prog: &Program, pid: u32) -> Option<String> {
    // clock uses a virtual ALSA-seq-only client name (no JACK audio).
    if prog.lang == "clock" { return Some("demiurge_clock".into()); }

    // opgorator is a passive USB device, not a client we spawned — match
    // by substring on the live PipeWire node name instead of the generic
    // exact-name candidate loop below.
    if prog.lang == "opgorator" { return resolve_opgorator_client(); }

    let candidates = candidate_client_names(prog);
    if candidates.is_empty() { return None; }

    let deadline = Instant::now() + Duration::from_secs(10);
    while Instant::now() < deadline {
        if pid > 0 && !pid_alive(pid) {
            util::log(&format!("resolve_client: {} (pid {pid}) died before registration", prog.id));
            return None;
        }
        // Scrub before the client check so we catch monitor_AUX auto-wires on
        // the same tick that the client appears in pw-link output.
        util::scrub_monitor_ports();
        let outs = Command::new("pw-link")
            .arg("-o")
            .output()
            .ok()?
            .stdout;
        let outs = String::from_utf8_lossy(&outs);
        for cand in &candidates {
            for line in outs.lines() {
                if let Some((client, _)) = line.split_once(':') {
                    if client == cand {
                        util::log(&format!("resolve_client: {} → {client}", prog.id));
                        // Give WP ~100ms to run its auto-connect policy on the
                        // newly-registered client, then scrub any monitor_AUX
                        // links it created before returning.
                        thread::sleep(Duration::from_millis(100));
                        util::scrub_monitor_ports();
                        return Some(client.to_string());
                    }
                }
            }
        }
        thread::sleep(Duration::from_millis(250));
    }
    util::log(&format!("resolve_client: FAILED for {} ({})", prog.id, prog.lang));
    None
}

// Find the Pocket OpGorator's (Daisy Seed) USB capture node by substring
// match on its PipeWire node name — real names look like
// "alsa_input.usb-...Pocket_OpGorator-00..." or similar, never a bare
// "OpGorator" client token, so the generic exact-match candidate loop in
// resolve_client() doesn't apply here.
//
// Deliberately NOT a long retry loop like resolve_client's 10s deadline:
// there's no spawned process to wait on, so blocking that long here would
// stall graph re-application on every unrelated USB hot-plug event whenever
// the OpGorator simply isn't attached. A couple of quick passes (with the
// caller's own settle sleep already applied in main.rs before calling
// resolve_clients()) is enough to catch ALSA-monitor enumeration lag; if the
// device isn't there, this returns None immediately and the NEXT hot-plug
// (OpGorator's own alsa_input.usb-* add event, which pw-mon already fires
// on) re-invokes resolve_clients() and picks it up then.
fn resolve_opgorator_client() -> Option<String> {
    for attempt in 0..3 {
        if attempt > 0 { thread::sleep(Duration::from_millis(300)); }
        util::scrub_monitor_ports();
        let Ok(output) = Command::new("pw-link").arg("-o").output() else { continue; };
        let outs = String::from_utf8_lossy(&output.stdout);
        for pat in ["OpGorator", "Daisy"] {
            if let Some(client) = outs.lines()
                .filter_map(|l| l.split_once(':').map(|(c, _)| c))
                .find(|c| c.contains(pat))
            {
                util::log(&format!("resolve_client: opgorator → {client}"));
                thread::sleep(Duration::from_millis(100));
                util::scrub_monitor_ports();
                return Some(client.to_string());
            }
        }
    }
    util::log("resolve_client: opgorator device not present (will retry on next hot-plug)");
    None
}

fn pid_alive(pid: u32) -> bool {
    std::path::Path::new(&format!("/proc/{pid}")).exists()
}

#[cfg(test)]
mod uss_client_tests {
    use super::*;

    fn prog(id: &str, lang: &str) -> Program {
        Program { id: id.into(), lang: lang.into(), file: format!("/x/{id}"), midi_only: false }
    }

    #[test]
    fn uss_csound_guest_is_never_neptrs_csound6() {
        assert_eq!(candidate_client_names(&prog("uss-nonlinear_daylight", "csound")),
                   vec!["uss-nonlinear_daylight".to_string()]);
        // NEPTR's own engine keeps resolving by the stock names.
        assert_eq!(candidate_client_names(&prog("neptrPhase4", "csound"))[0], "csound6");
        // ChucK USS guests keep the stock ChucK names (one chuck at a time).
        assert_eq!(candidate_client_names(&prog("uss-supersaw", "chuck"))[0], "ChucK");
    }
}
