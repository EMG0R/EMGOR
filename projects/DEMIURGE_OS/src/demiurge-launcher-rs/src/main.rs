// DEMIURGE launcher — Rust rewrite.
//
// Reads /boot/firmware/demiurge.conf for `launch = <path>` (defaults to
// ~/demiurge/live.conf), parses the live.conf, supervises child processes
// via the /opt/demiurge/bin/demiurge-run-* wrappers, drives the PipeWire
// patch graph through pw-link, and dynamically aggregates every USB-class
// audio interface macOS-style.
//
// live.conf is the *only* user-facing config format. The internal Session
// graph (Program/Edge in config.rs) is built by `live::to_session` and is
// not touched by users — the Rust layer stays hidden behind the simple
// `chain = …` interface.
//
// Live-reload model:
//   - When the launcher is pointed at a live.conf, it parses it, builds
//     the internal session graph implicitly (clock always present, pdclock
//     added if any stage is .pd, chain stages piped in order), then starts
//     a file watcher that polls the live.conf mtime every second.
//   - On save, the launcher parses the new file, diffs it against the
//     running set (by id + file), kills removed stages, starts new ones,
//     keeps matching stages alive, and reapplies the graph.
//   - `link = on/off` toggles trigger a demiurge-clock respawn with the
//     right --link flag. When off, demiurge-clock never constructs the
//     Link object — zero overhead.
//
// Zero external crates. std only.

use std::env;
use std::process;
use std::sync::mpsc;
use std::thread;
use std::time::{Duration, Instant};

mod config;
mod graph;
mod aggregate;
mod midi;
mod supervisor;
mod events;
mod live;
mod devices;
mod util;
mod clockrole;
mod params;

use events::Event;

const BOOT_CONFIG: &str = "/boot/firmware/demiurge.conf";

fn main() {
    util::log_init();
    util::log("=== DEMIURGE launcher (rust) starting ===");

    let home = env::var("HOME").unwrap_or_else(|_| "/home/gorpi7".into());
    let default_live = format!("{home}/demiurge/live.conf");

    // Resolve the launch target. Precedence:
    //   1. /boot/firmware/demiurge.conf `launch = ...` if set
    //   2. ~/demiurge/live.conf
    let launch_path_boot = config::parse_boot_launch(BOOT_CONFIG).unwrap_or_default();
    util::log(&format!("boot launch = '{launch_path_boot}'"));

    let launch_path = if !launch_path_boot.is_empty() {
        launch_path_boot
    } else {
        default_live.clone()
    };

    util::log(&format!("loading live config: {launch_path}"));
    let lc = match live::parse(&launch_path) {
        Ok(lc) => lc,
        Err(e) => {
            util::log(&format!("live.conf load failed ({launch_path}): {e}"));
            process::exit(0);
        }
    };

    // sync_layer = off → the launcher itself bypasses the PipeWire graph and runs
    // the chain's first program directly on the audio interface (lowest latency).
    // ONE flag in live.conf, handled right here — there is no separate mode-select
    // service. run_bypass execs into the program and does not return on success.
    if lc.sync_layer_off {
        // A fresh install ships an empty chain. Exiting 1 there made systemd
        // restart the launcher forever (thousands of restarts, 2026-10-07 on a
        // fresh public install). Nothing to run is not a failure: idle until
        // live.conf changes, then re-exec to pick up the new chain.
        if util::chain_is_empty(&lc) {
            util::idle_until_changed(&launch_path);
        }
        util::run_bypass(&lc, &launch_path);
        util::log("sync_layer=off bypass did not start — exiting (systemd will retry)");
        process::exit(1);
    }

    // sync_layer = on: make sure this user's PipeWire is up (a prior bypass run
    // may have stopped it), then build the graph as normal.
    util::ensure_pipewire_running();
    // BEFORE anything opens the card. A duplex I2S codec left on an ACP
    // profile is presented as two independent nodes, so PipeWire makes capture
    // a resampling FOLLOWER of playback and resyncs it forever — the input
    // passes for a few seconds and then dies into a steady tone, with every
    // health check still green. See util::enforce_duplex_clock_domain for the
    // measured journal lines and for why this is asserted here rather than
    // trusted to config alone.
    util::enforce_duplex_clock_domain();
    util::set_preferred_interface(util::preferred_interface_name(&lc));
    util::set_hdmi_default(lc.hdmi_default);
    util::set_default_input(!lc.hw_input_off);

    let session = live::to_session(&lc);
    let mut current_rate = lc.rate.unwrap_or(util::DEFAULT_RATE);
    // vet_quantum, not a bare read: live.conf can legally contain a quantum the
    // rest of the stack cannot honour (ksmps mismatch, or below the device's
    // ALSA period-size), and carrying that into the graph kills csound at init
    // and leaves a disconnected patcher with nothing naming the cause. See
    // util::vet_quantum — it substitutes the last known-good value, loudly, and
    // never edits live.conf behind the user's back.
    let mut current_quantum = util::vet_quantum(&launch_path, lc.quantum.unwrap_or(util::DEFAULT_QUANTUM));
    util::log(&format!(
        "live: {} chain(s), {} stage(s), link={}, rate={current_rate}, quantum={current_quantum}",
        lc.chains.len(),
        lc.total_stages(),
        lc.link,
    ));
    let live_path_for_watcher: Option<String> = Some(launch_path.clone());

    if session.programs.is_empty() {
        util::log("session has no programs — nothing to run");
        process::exit(0);
    }

    util::log(&format!(
        "session: {} programs, {} patch edges",
        session.programs.len(),
        session.edges.len()
    ));

    // First pass: pin the graph clock to 48 kHz / 128 quantum, then
    // aggregate whatever USB devices are already plugged in before we
    // launch anything. Pinning before aggregation matters: ALSA nodes
    // created by aggregate honor the clock that's in effect when they
    // join the graph.
    // Boot rate-pin sequence — three stages so the Scarlett can't open at
    // the wrong rate regardless of firmware state:
    //   Stage 1: hardware USB bounce forces WP to re-enumerate the device
    //            fresh under monitor.alsa.rules (audio.rate=48000), then
    //            enforce loop verifies every node.
    //   Stage 2: aggregate (links sink-output → hardware), enforce on the
    //            live streaming node.
    //   Stage 3: after JACK clients start, one final enforce under load.
    util::pin_clock_metadata(current_rate, current_quantum);
    util::ensure_usb_rates_stable(current_rate);   // stage 1: bounce + verify
    aggregate::apply();
    thread::sleep(Duration::from_millis(400));
    util::enforce_usb_rates(current_rate);          // stage 2: live node check

    let mut sup = supervisor::Supervisor::new(session, current_rate, current_quantum);
    sup.start_all();

    // resolve_clients polls every 250ms for JACK client registration; each tick
    // also calls util::scrub_monitor_ports to kill WP's monitor_AUX auto-wires
    // before they can create a feedback loop.
    sup.resolve_clients();
    util::enforce_usb_rates(current_rate);          // stage 3: under-load check
    graph::apply(&sup.session, &sup.clients);
    midi::wire_bus(&sup.session, &sup.clients);

    // Clock-role auto-switch: evaluate PMOR/OpGorator presence once now that
    // the graph has settled, so a device already attached at boot is picked
    // up immediately rather than waiting on the NEXT hot-plug event to
    // arrive (requirement: no boot-time race). See clockrole.rs for the
    // edge-detection design and the manual-override rule.
    let mut clock_presence = clockrole::PresenceTracker::new();
    if let Some(action) = clock_presence.observe(clockrole::pmor_present()) {
        clockrole::apply_action(action);
    }

    // Event wiring.
    let (tx, rx) = mpsc::channel::<Event>();
    events::spawn_pwmon(tx.clone());
    events::spawn_child_watcher(sup.child_pids(), tx.clone());
    events::spawn_heartbeat(tx.clone());
    if let Some(ref p) = live_path_for_watcher {
        events::spawn_live_watcher(p.clone(), tx.clone());
        // Also start the device-header writer on the same file.
        devices::spawn_header_writer(p.clone());
    }

    util::install_signal_handlers();

    let mut last_state = sup.state_fingerprint();

    // ---- quantum probation ----
    //
    // A quantum is only "known good" once the graph has actually RUN at it with
    // nothing dead — config that parses proves nothing. Until the probation
    // window elapses we keep the previous known-good value in reserve, and if a
    // stage dies inside the window the quantum we just started at is the prime
    // suspect. Reverting there is the difference between a rig that recovers on
    // its own and systemd restart-looping a silent instrument in front of an
    // audience.
    //
    // 25 s is chosen to sit well past csound's ~6 s init on this patch (a
    // startup crash lands inside the window) while staying short enough that a
    // healthy quantum is banked before anyone thinks to change anything.
    const QUANTUM_PROBATION: Duration = Duration::from_secs(25);
    let mut quantum_started_at = Instant::now();
    let mut quantum_on_probation = true;

    loop {
        let ev = match rx.recv_timeout(Duration::from_secs(2)) {
            Ok(e) => e,
            Err(mpsc::RecvTimeoutError::Timeout) => Event::Heartbeat,
            Err(mpsc::RecvTimeoutError::Disconnected) => break,
        };

        if util::shutdown_requested() {
            util::log("shutdown requested");
            sup.shutdown();
            process::exit(0);
        }

        match ev {
            Event::UsbDeviceChange => {
                util::log("event: usb device change");
                // Re-assert the duplex card's clock domain on every device
                // event, not only at boot. The launcher log has the card
                // silently falling BACK mid-session —
                //   aggregate: clock master changed: ...pro-output-0 → ...stereo-fallback
                // — after which capture was a follower again and nothing said
                // so. Early-returns when the profile is already correct, so
                // this costs two pw-cli calls per hot-plug.
                util::enforce_duplex_clock_domain();
                util::pin_clock_metadata(current_rate, current_quantum);
                aggregate::apply();
                util::enforce_usb_rates(current_rate);
                // Give the JACK bridge time to register ports on the
                // freshly-enumerated device before we relink the graph.
                thread::sleep(Duration::from_millis(500));
                sup.resolve_clients();
                graph::apply(&sup.session, &sup.clients);
                midi::wire_bus(&sup.session, &sup.clients);
                if let Some(action) = clock_presence.observe(clockrole::pmor_present()) {
                    clockrole::apply_action(action);
                }
            }
            Event::ChildExited(pid) => {
                util::log(&format!("event: child exited pid={pid}"));
                sup.mark_dead(pid);
                graph::apply(&sup.session, &sup.clients);
                // Schedule the restart immediately rather than waiting for a
                // heartbeat to notice. respawn_dead() owns the backoff, so
                // calling it early is safe and just shortens the silence.
                restart_downed_stages(&mut sup, &tx, &mut last_state);

                // Died during probation → suspect the quantum we just started
                // at. This is the exact shape of the `quantum = 32` failure:
                // csound exits during init, its ports vanish, the graph goes
                // dark, and nothing in the system connects that to the number
                // the user changed. Fall back to a value this rig has actually
                // made audio at, say so loudly, and keep playing.
                if quantum_on_probation && quantum_started_at.elapsed() < QUANTUM_PROBATION {
                    if let Some(good) = util::last_good_quantum().filter(|g| *g != current_quantum) {
                        util::log("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
                        util::log(&format!(
                            "quantum: a stage died {} s after starting at quantum {current_quantum}",
                            quantum_started_at.elapsed().as_secs()
                        ));
                        util::log(&format!(
                            "quantum: FALLING BACK to {good}, the last quantum this rig made audio at"
                        ));
                        util::log("quantum: live.conf still says otherwise — fix it with 'demiurge-audio quantum <n>'");
                        util::log("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
                        current_quantum = good;
                        sup.quantum = good;
                        util::pin_clock_metadata(current_rate, good);
                        sup.shutdown();
                        let n = sup.session.programs.len();
                        sup.running = (0..n).map(|_| supervisor::RunningProg { child: None, pid: 0 }).collect();
                        sup.dead = vec![false; n];
                        sup.clients.clear();
                        sup.start_all();
                        // The watcher thread exits once its pid set empties, so
                        // the new children need a fresh one or nothing would
                        // notice the NEXT death.
                        events::spawn_child_watcher(sup.child_pids(), tx.clone());
                        thread::sleep(Duration::from_millis(500));
                        sup.resolve_clients();
                        graph::apply(&sup.session, &sup.clients);
                        midi::wire_bus(&sup.session, &sup.clients);
                        // Not on probation any more: `good` is by definition a
                        // value that ran, and re-arming would let a genuinely
                        // unrelated crash trigger a pointless second restart.
                        quantum_on_probation = false;
                        quantum_started_at = Instant::now();
                        last_state = sup.state_fingerprint();
                    }
                }
            }
            Event::LiveConfigChanged => {
                let Some(path) = live_path_for_watcher.clone() else { continue; };
                util::log("event: live.conf changed — reloading");
                match live::parse(&path) {
                    Ok(new_lc) => {
                        // Switched to sync_layer = off on a live edit: tear down the
                        // graph and hand off to direct mode (execs, no return).
                        // (Going back to sync_layer = on needs a service restart,
                        // since the launcher is no longer running in bypass.)
                        if new_lc.sync_layer_off {
                            util::log("event: sync_layer=off — bypassing PipeWire, switching to direct mode");
                            sup.shutdown();
                            util::run_bypass(&new_lc, &path);
                            process::exit(1);
                        }
                        util::set_preferred_interface(util::preferred_interface_name(&new_lc));
                        util::set_hdmi_default(new_lc.hdmi_default);
                        util::set_default_input(!new_lc.hw_input_off);
                        let new_rate    = new_lc.rate.unwrap_or(util::DEFAULT_RATE);
                        // Vet on every reload, not just at boot: editing
                        // live.conf by hand (or any writer that skips
                        // demiurge-audio) is the other way an impossible
                        // quantum reaches the graph.
                        let new_quantum = util::vet_quantum(&path, new_lc.quantum.unwrap_or(util::DEFAULT_QUANTUM));
                        let rate_changed    = new_rate    != current_rate;
                        let quantum_changed = new_quantum != current_quantum;
                        // The clock's `file` marker ("link=on"/"link=off") changes on
                        // toggle, so reconcile's (id, file) match naturally kills and
                        // respawns the clock with the new --link flag.
                        let new_session = live::to_session(&new_lc);
                        if rate_changed || quantum_changed {
                            // Rate and quantum are baked into JACK clients at spawn time
                            // (chuck --srate/--bufsize, csound -r/-b). Reconcile would
                            // keep matching stages alive with the old values; force a
                            // full restart so every wrapper picks up the new env vars.
                            // Re-pin metadata first so newly-spawned clients negotiate
                            // at the new graph rate/quantum, and re-enforce USB rates
                            // so any device that opened at the old rate is bounced.
                            util::log(&format!(
                                "graph: rate {current_rate}→{new_rate} quantum {current_quantum}→{new_quantum}, restarting all stages"
                            ));
                            current_rate    = new_rate;
                            current_quantum = new_quantum;
                            sup.rate    = new_rate;
                            sup.quantum = new_quantum;
                            util::pin_clock_metadata(new_rate, new_quantum);
                            util::enforce_usb_rates(new_rate);
                            sup.shutdown();
                            sup.session = new_session;
                            let n = sup.session.programs.len();
                            sup.running = (0..n).map(|_| supervisor::RunningProg { child: None, pid: 0 }).collect();
                            sup.dead = vec![false; n];
                            sup.clients.clear();
                            sup.start_all();
                            // New quantum, new probation: the value that was
                            // banked belongs to the graph we just tore down.
                            if quantum_changed {
                                quantum_started_at = Instant::now();
                                quantum_on_probation = true;
                            }
                        } else {
                            sup.reconcile(new_session);
                        }
                        thread::sleep(Duration::from_millis(500));
                        sup.resolve_clients();
                        graph::apply(&sup.session, &sup.clients);
                        midi::wire_bus(&sup.session, &sup.clients);
                        last_state = sup.state_fingerprint();
                    }
                    Err(e) => {
                        util::log(&format!("live.conf reload failed: {e} — keeping current graph"));
                    }
                }
            }
            Event::Heartbeat => {
                // Re-run aggregate every heartbeat. It's idempotent (diff-based,
                // a no-op once converged), so this lets the snd-aloop fallback
                // link self-heal when the aloop is enumerated AFTER the launcher's
                // first pass — the cold-boot race: the aloop is not a USB node, so
                // pw-mon never re-triggers aggregate for it, and without this the
                // graph would stay on the HDMI fallback (suspended) forever.
                aggregate::apply();
                // Bank the current quantum once the graph has survived
                // probation with every stage alive. This is the ONLY thing that
                // writes the fallback value, so the fallback can only ever be a
                // configuration that was observed working on this hardware.
                if quantum_on_probation
                    && quantum_started_at.elapsed() >= QUANTUM_PROBATION
                    && !sup.dead.iter().any(|d| *d)
                {
                    util::record_good_quantum(current_quantum);
                    quantum_on_probation = false;
                }
                let state = sup.state_fingerprint();
                if state != last_state {
                    util::log("state change detected by heartbeat");
                    sup.resolve_clients();
                    graph::apply(&sup.session, &sup.clients);
                    midi::wire_bus(&sup.session, &sup.clients);
                    last_state = state;
                }

                // Catch the stage that is alive but has dropped out of the
                // graph (process up, ports gone, no audio, nothing for
                // try_wait to see), then restart anything that is down. Order
                // matters: the scan marks such a stage dead so the respawn in
                // the same tick picks it up.
                sup.scan_graph_presence();
                restart_downed_stages(&mut sup, &tx, &mut last_state);

                // Re-check PMOR/OpGorator presence every heartbeat too, not
                // just on pw-mon's UsbDeviceChange. pw-mon only watches ALSA
                // *audio* nodes (events::spawn_pwmon), so a device that shows
                // up MIDI-only (no audio interface) never fires that event at
                // all — this is the "MIDI-only presence gap". pmor_present()
                // itself checks both signals (PipeWire audio node substring
                // match AND devices::scan_usb_midi's ALSA-seq client list),
                // and PresenceTracker::observe is a no-op unless presence
                // actually changed, so this costs one cheap poll per 2s tick
                // and never re-fires CC116 while the device sits steady.
                if let Some(action) = clock_presence.observe(clockrole::pmor_present()) {
                    clockrole::apply_action(action);
                }
            }
        }
    }

    util::log("event channel closed, exiting");
    sup.shutdown();
}

// Bring back any stage that is down, then re-establish everything that is keyed
// to a stage's pid or client name. Each of these is load-bearing:
//
//   * resolve_clients — a restarted engine gets a NEW JACK client registration;
//     the old name in the map may be stale or absent.
//   * graph::apply    — a fresh client has no links at all. Without this the
//     stage is running and inaudible, which is the failure we are fixing.
//   * midi::wire_bus  — MIDI subscriptions die with the old process.
//   * spawn_child_watcher — the watcher's pid set is FIXED when it is created
//     and its thread exits once that set empties. A respawned stage would
//     otherwise never be watched again, so the second death would go unnoticed
//     even though the first was handled.
//
// last_state is refreshed so the heartbeat's change detector does not
// immediately re-fire on the transition we just performed.
fn restart_downed_stages(
    sup: &mut supervisor::Supervisor,
    tx: &mpsc::Sender<events::Event>,
    last_state: &mut String,
) {
    let revived = sup.respawn_dead();
    if revived.is_empty() { return; }
    util::log(&format!("respawn: restarted {} stage(s): {}", revived.len(), revived.join(", ")));
    thread::sleep(Duration::from_millis(500));
    sup.resolve_clients();
    graph::apply(&sup.session, &sup.clients);
    midi::wire_bus(&sup.session, &sup.clients);
    events::spawn_child_watcher(sup.child_pids(), tx.clone());
    *last_state = sup.state_fingerprint();
}
