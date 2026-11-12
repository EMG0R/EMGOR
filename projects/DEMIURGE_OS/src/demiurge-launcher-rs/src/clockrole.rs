// Device-presence-AND-transport-driven clock-role switching.
//
// Today `demiurge-sync opgorator|demiurge` is 100% manual: a human decides
// whether the OpGorator (Pocket OpGorator / "PMOR" / Daisy Seed groovebox)
// or Demiurge is clock master, and runs the CLI by hand. This module makes
// that automatic.
//
// TRANSPORT, NOT PRESENCE (2026-10 rework). The original version of this
// module handed master to PMOR the instant it was merely PLUGGED IN,
// whether or not it was making any sound — wrong for "two-way clock
// negotiation": a silent PMOR sitting on the USB bus should never steal
// the clock from a NEPTR loop that's actually playing. The election now
// keys on TRANSPORT:
//
//   - PMOR present but SILENT, NEPTR has a loop running -> DEMIURGE master.
//   - PMOR SOUNDING, NEPTR empty                        -> DEMIURGE follower.
//   - Both running                                       -> whoever already
//     holds it keeps it (checked against sync.conf's current `role`, so
//     this never flaps every heartbeat just because both are active).
//   - Presence with no transport information yet          -> stay MASTER
//     (safe default — a device we know nothing about must never take the
//     clock just by existing).
//   - Device absent entirely                              -> DEMIURGE master.
//
// EVIDENCE OF AN EXTERNAL CLOCK (2026-10-03). A CC117 "playing" announce is
// not enough: the election also requires `ext_clock = yes` in
// clock_status.conf, i.e. the daemon has heard a sustained 0xF8 stream that
// is not its own Midi Through echo. Without it PMOR counts as silent.
//
// Transport state comes from `~/demiurge/clock_status.conf`, written every
// ~2s by the daemon (src/demiurge-clock.cpp) from two live MIDI signals:
// PMOR's own ch16 CC117 transport announce (sounding/silent), and a
// CC118-steer-recency proxy for "NEPTR has a loop actively driving tempo"
// (chase_bliss_clock.orc only writes CC118 while it believes the loop is
// authoritative, so recent CC118 traffic already means "a loop is
// running"). Reading this small file is cheap and non-blocking — unlike
// re-probing the MIDI bus directly from here (shelling to aseqdump the
// way demiurge-sync's own `probe_clock` does blocks for a multi-second
// capture window, far too slow for a 2s heartbeat poll).
//
// ONE WRITE PATH. This module never touches ~/demiurge/sync.conf directly —
// it always shells out to `demiurge-sync --auto <role>`, the same CLI a
// human runs by hand. That is deliberate, not laziness: sync.conf already
// has exactly one writer (demiurge-sync) per the project's established
// doctrine (see interface.conf / live.conf, same pattern), and giving this
// module a second direct writer would immediately create the two-mechanisms
// problem this project has hit before. `--auto` exists solely so the CLI can
// tell an automatic call apart from a human one and stamp `source = auto` in
// the file it writes (see below) — the file format doesn't change, only who
// gets credit for the write.
//
// EXPLICIT USER CHOICE WINS. A human who runs `demiurge-sync demiurge` (no
// --auto) is stamped `source = manual` by the CLI. Auto-detection must never
// steal master back from a human who deliberately claimed it — so before
// shelling out we read the CURRENT source ourselves and skip the call
// entirely when it says `manual` (in addition to demiurge-sync enforcing the
// same rule server-side — belt and suspenders, since two independent checks
// beat one when the failure mode is "silently overrode the user").
//
// EDGE-TRIGGERED, NOT LEVEL-TRIGGERED. `PresenceTracker` only returns an
// action the instant presence actually changes. Re-observing "still
// present"/"still absent" is silent. This matters because the CLI's CC116
// injection is not idempotent-free: firing it every 2s heartbeat (or on
// every unrelated USB hot-plug) would spam the shared MIDI bus and fight the
// clock daemon's own 2s re-announce (see demiurge/docs/clock.md, "Role
// negotiation"). One transition, one CC116 send.

use std::process::Command;

/// A clock-role change to apply, derived purely from a presence transition.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum RoleAction {
    /// The device just appeared — Demiurge should become follower.
    SetOpgorator,
    /// The device just disappeared — Demiurge should become master.
    SetDemiurge,
}

impl RoleAction {
    pub fn role_arg(self) -> &'static str {
        match self {
            RoleAction::SetOpgorator => "opgorator",
            RoleAction::SetDemiurge => "demiurge",
        }
    }
}

/// Who last set the current role: a human (`demiurge-sync opgorator|demiurge`
/// typed directly) or this automation (`demiurge-sync --auto ...`).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Source {
    Auto,
    Manual,
}

/// Pure edge-detection state machine over device presence.
///
/// Starts "absent" (the safe default: Demiurge is master until proven
/// otherwise). Feed it a fresh presence reading on every poll/event via
/// `observe()`; it returns `Some(action)` only on the tick where presence
/// actually flips, `None` on every repeat. This is what keeps hot-plug
/// bursts and the 2s heartbeat from re-asserting CC116 on every tick.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct PresenceTracker {
    last_present: bool,
}

impl PresenceTracker {
    pub fn new() -> Self {
        Self { last_present: false }
    }

    /// Feed the latest presence reading. Returns the action to apply on an
    /// edge, or None if presence didn't change since the last observation.
    pub fn observe(&mut self, present: bool) -> Option<RoleAction> {
        if present == self.last_present {
            return None;
        }
        self.last_present = present;
        Some(if present { RoleAction::SetOpgorator } else { RoleAction::SetDemiurge })
    }
}

impl Default for PresenceTracker {
    fn default() -> Self {
        Self::new()
    }
}

/// Parse the `source = auto|manual` line out of sync.conf's contents.
/// Missing file, missing key, or any value other than "manual" all mean
/// Auto — this is the backward-compatible default for a sync.conf written
/// before this feature existed (plain `role = ...`, no `source` line at
/// all), so upgrading in place does not silently freeze auto-detection off.
pub fn parse_source(contents: &str) -> Source {
    for line in contents.lines() {
        let t = line.trim();
        if t.is_empty() || t.starts_with('#') {
            continue;
        }
        let Some(rest) = t.strip_prefix("source") else { continue };
        let rest = rest.trim_start();
        let Some(val) = rest.strip_prefix('=') else { continue };
        let val = val.trim();
        return if val.eq_ignore_ascii_case("manual") { Source::Manual } else { Source::Auto };
    }
    Source::Auto
}

/// The rule: auto-detection may only ever apply on top of an `auto` role.
/// A `manual` role always wins — this function can never return true for
/// Source::Manual, by construction, which is exactly the guarantee the
/// design requires ("manual always wins").
pub fn auto_may_apply(source: Source) -> bool {
    matches!(source, Source::Auto)
}

const SYNC_CLI: &str = "/usr/local/bin/demiurge-sync";

fn sync_conf_path() -> String {
    let home = std::env::var("HOME").unwrap_or_else(|_| "/tmp".into());
    format!("{home}/demiurge/sync.conf")
}

fn current_source() -> Source {
    match std::fs::read_to_string(sync_conf_path()) {
        Ok(contents) => parse_source(&contents),
        Err(_) => Source::Auto, // no file yet — nothing manual has been set
    }
}

/// Apply a role action by shelling out to `demiurge-sync --auto <role>`.
/// Skips the call (and logs why) when the current sync.conf source is
/// `manual` — see the module doc for the "explicit choice wins" rule.
/// Fail-safe: a missing binary or a failed spawn is logged, never panics —
/// a wedged automation must not be able to take the launcher down with it.
pub fn apply_action(action: RoleAction) {
    let source = current_source();
    if !auto_may_apply(source) {
        crate::util::log(&format!(
            "clockrole: device presence changed (would set role={}) but sync.conf source=manual — leaving the user's explicit choice alone",
            action.role_arg()
        ));
        return;
    }
    let bin = if std::path::Path::new(SYNC_CLI).exists() {
        SYNC_CLI
    } else {
        "demiurge-sync" // dev/macOS fallback: whatever's on PATH
    };
    crate::util::log(&format!("clockrole: device presence changed — auto-setting role={}", action.role_arg()));
    match Command::new(bin).arg("--auto").arg(action.role_arg()).status() {
        Ok(status) if status.success() => {}
        Ok(status) => crate::util::log(&format!("clockrole: demiurge-sync --auto {} exited {status}", action.role_arg())),
        Err(e) => crate::util::log(&format!("clockrole: failed to run demiurge-sync: {e}")),
    }
}

/// Device-presence oracle: is the OpGorator/PMOR/Daisy device attached
/// right now, by ANY signal we have?
///
///   - PipeWire audio node (substring match, same patterns
///     supervisor::resolve_opgorator_client already uses for graph wiring).
///   - ALSA-seq MIDI client (devices::scan_usb_midi) — covers the device
///     showing up MIDI-only, with no audio interface, which pw-mon's
///     ALSA-audio-node watch (events::spawn_pwmon) never sees at all. This
///     is the "MIDI-only presence gap" fix: rather than depend on a udev
///     hotplug event reaching the launcher, this function is re-evaluated
///     on every heartbeat (2s) regardless of what triggered it, so absence
///     of a hotplug signal just means a couple of seconds of extra latency,
///     never a stuck role.
fn device_physically_present() -> bool {
    audio_node_present() || midi_client_present(&crate::devices::scan_usb_midi())
}

/// THE election function. Kept under the name `pmor_present` because
/// main.rs calls it directly at three sites (boot settle, UsbDeviceChange,
/// the 2s Heartbeat) and feeds its result straight into
/// `PresenceTracker::observe()` — main.rs is owned by another agent and
/// is out of scope for this change, so the transport-aware rework has to
/// live entirely inside what this function returns, not in a new name or
/// a new call site. Despite the name, what this now answers is NOT "is
/// the device plugged in" (see `device_physically_present()` for that) —
/// it's "should the external device hold clock master right now", which
/// `PresenceTracker` treats exactly the way it used to treat raw
/// presence: an edge from false->true fires `RoleAction::SetOpgorator`
/// (follower), true->false fires `RoleAction::SetDemiurge` (master), and
/// repeats are silent. See the module doc above for the full election
/// table.
pub fn pmor_present() -> bool {
    should_follow_external(
        device_physically_present(),
        read_clock_status(),
        current_role_is_follower(),
    )
}

/// Pure decision function — see the module doc's election table. Kept
/// separate from `pmor_present()` so it's testable without touching the
/// filesystem or shelling out.
///
/// `current_is_follower` is only consulted in the "both active" and
/// "both silent" cases, where the rule is "whoever already holds it
/// keeps it" — i.e. don't flip. Reading the CURRENT decision back out of
/// sync.conf (rather than, say, threading extra state through
/// `PresenceTracker`) keeps this function pure and keeps `sync.conf` the
/// single place "what's the role right now" is answered from, matching
/// every other reader in this module (`current_source`).
fn should_follow_external(
    physically_present: bool,
    status: ClockStatus,
    current_is_follower: bool,
) -> bool {
    if !physically_present {
        return false; // device gone entirely -> DEMIURGE master, no exceptions
    }
    match (status.pmor_playing, status.loop_active) {
        (None, None) => false, // presence with no transport info at all -> stay master (safe default)
        (pmor, loop_active) => {
            // EVIDENCE, not presence/announce: PMOR only counts as sounding
            // when its CC117 says playing AND real external 0xF8 is on the
            // bus (ext_clock). Unknown/absent ext_clock == no evidence.
            let pmor_sounding = pmor.unwrap_or(false) && status.ext_clock.unwrap_or(false);
            let loop_running = loop_active.unwrap_or(false);
            match (pmor_sounding, loop_running) {
                (true, false) => true,                  // PMOR sounding, NEPTR empty -> follower
                (false, true) => false,                 // PMOR silent, NEPTR running -> master
                (true, true) => current_is_follower,     // both running -> whoever started first keeps it
                (false, false) => current_is_follower,   // both silent -> don't churn the role for nothing
            }
        }
    }
}

const MATCH_PATTERNS: [&str; 3] = ["OpGorator", "Daisy", "PMOR"];

/// Three-state transport reading for one side: `None` means "no fresh
/// signal" (either nothing has ever been seen, or the daemon itself
/// marked it stale past its own `ROLE_SIGNAL_FRESH_S` window) — the
/// daemon has already done the staleness math in
/// `clock_status.conf`'s `pmor_transport`/`loop_active` fields (`unknown`
/// vs `playing`/`stopped`/`yes`), so this parser just trusts those
/// strings rather than re-deriving freshness from the `*_age_s` fields
/// itself. Two independent sources of truth for "is this stale" would be
/// exactly the kind of parallel-mechanism drift this project avoids
/// elsewhere.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
struct ClockStatus {
    pmor_playing: Option<bool>,
    loop_active: Option<bool>,
    /// `ext_clock = yes|no` from the daemon: a sustained stream of 0xF8
    /// that is NOT our own Midi Through echo was heard within the signal
    /// freshness window. This is the EVIDENCE half of the election: a
    /// PMOR CC117 "playing" with no external ticks on the bus does not
    /// make PMOR a clock master (2026-10-03: auto-election flipped to
    /// opgorator with PMOR plugged in and emitting zero 0xF8). Absent
    /// (older daemon) parses as None and counts as "no evidence".
    ext_clock: Option<bool>,
}

fn clock_status_path() -> String {
    let home = std::env::var("HOME").unwrap_or_else(|_| "/tmp".into());
    format!("{home}/demiurge/clock_status.conf")
}

/// Parse the `key = value` lines demiurge-clock.cpp's `write_status_file`
/// writes. Pure (given file contents) so it's testable without a live
/// daemon or filesystem.
fn parse_clock_status(contents: &str) -> ClockStatus {
    let mut status = ClockStatus::default();
    for line in contents.lines() {
        let t = line.trim();
        if t.is_empty() || t.starts_with('#') {
            continue;
        }
        let Some((key, val)) = t.split_once('=') else { continue };
        let key = key.trim();
        let val = val.trim();
        match key {
            "pmor_transport" => {
                status.pmor_playing = match val {
                    "playing" => Some(true),
                    "stopped" => Some(false),
                    _ => None, // "unknown" or anything unrecognized
                };
            }
            "loop_active" => {
                status.loop_active = match val {
                    "yes" => Some(true),
                    // 2026-10-01: the daemon DOES write "no" now. It
                    // consumes NEPTR's ch16 CC115 loop-held announce, which
                    // is a positive fact ("no loop") rather than the old
                    // CC118-recency inference that could only say "seen" or
                    // "not seen". This defensive arm became the live path.
                    "no" => Some(false),
                    _ => None,
                };
            }
            "ext_clock" => {
                status.ext_clock = match val {
                    "yes" => Some(true),
                    "no" => Some(false),
                    _ => None,
                };
            }
            _ => {}
        }
    }
    status
}

/// Reads `~/demiurge/clock_status.conf`. Missing file (daemon never ran,
/// or hasn't written its first status yet) or a parse yielding nothing
/// both degrade to `ClockStatus::default()` (both fields `None`), which
/// `should_follow_external`'s `(None, None)` arm already treats as "no
/// transport information yet -> stay master" — there is no separate
/// error path to maintain here.
fn read_clock_status() -> ClockStatus {
    match std::fs::read_to_string(clock_status_path()) {
        Ok(contents) => parse_clock_status(&contents),
        Err(_) => ClockStatus::default(),
    }
}

/// Parse the `role = opgorator|demiurge` line out of sync.conf's
/// contents — the CURRENT decision, as last persisted (by a human or by
/// this module's own previous `apply_action` call). Mirrors
/// `parse_source`'s shape/tolerance (missing key, comments, whitespace)
/// for the same file.
fn parse_role(contents: &str) -> bool {
    for line in contents.lines() {
        let t = line.trim();
        if t.is_empty() || t.starts_with('#') {
            continue;
        }
        let Some(rest) = t.strip_prefix("role") else { continue };
        let rest = rest.trim_start();
        let Some(val) = rest.strip_prefix('=') else { continue };
        return val.trim().eq_ignore_ascii_case("opgorator");
    }
    false // missing file/key -> same "demiurge" default demiurge-sync itself uses
}

fn current_role_is_follower() -> bool {
    match std::fs::read_to_string(sync_conf_path()) {
        Ok(contents) => parse_role(&contents),
        Err(_) => false,
    }
}

fn audio_node_present() -> bool {
    let Ok(out) = Command::new("pw-link").arg("-o").output() else { return false };
    if !out.status.success() {
        return false;
    }
    let text = String::from_utf8_lossy(&out.stdout);
    text.lines()
        .filter_map(|l| l.split_once(':').map(|(c, _)| c))
        .any(|client| MATCH_PATTERNS.iter().any(|pat| client.contains(pat)))
}

/// Pure (given a device-name list) so the MIDI-only-presence logic is
/// hermetically testable without shelling out to aconnect.
fn midi_client_present(names: &[String]) -> bool {
    names.iter().any(|n| MATCH_PATTERNS.iter().any(|pat| n.contains(pat)))
}

#[cfg(test)]
mod tests {
    use super::*;

    // ---- PresenceTracker edge detection ----

    #[test]
    fn no_action_while_absent() {
        let mut t = PresenceTracker::new();
        assert_eq!(t.observe(false), None);
        assert_eq!(t.observe(false), None);
    }

    #[test]
    fn absent_to_present_fires_once() {
        let mut t = PresenceTracker::new();
        assert_eq!(t.observe(true), Some(RoleAction::SetOpgorator));
        // Repeated "still present" observations must be silent.
        assert_eq!(t.observe(true), None);
        assert_eq!(t.observe(true), None);
    }

    #[test]
    fn present_to_absent_fires_once() {
        let mut t = PresenceTracker::new();
        assert_eq!(t.observe(true), Some(RoleAction::SetOpgorator));
        assert_eq!(t.observe(false), Some(RoleAction::SetDemiurge));
        // Repeated "still absent" observations must be silent.
        assert_eq!(t.observe(false), None);
        assert_eq!(t.observe(false), None);
    }

    #[test]
    fn full_sequence_produces_exactly_one_action_each_way() {
        // absent -> present -> present -> absent
        let mut t = PresenceTracker::new();
        let mut actions = Vec::new();
        for present in [false, true, true, false] {
            if let Some(a) = t.observe(present) {
                actions.push(a);
            }
        }
        assert_eq!(actions, vec![RoleAction::SetOpgorator, RoleAction::SetDemiurge]);
    }

    #[test]
    fn flapping_still_fires_one_action_per_genuine_edge() {
        let mut t = PresenceTracker::new();
        let readings = [false, false, true, true, true, false, true, false, false];
        let actions: Vec<RoleAction> = readings.iter().filter_map(|&p| t.observe(p)).collect();
        assert_eq!(
            actions,
            vec![
                RoleAction::SetOpgorator, // false->true
                RoleAction::SetDemiurge,  // true->false
                RoleAction::SetOpgorator, // false->true
                RoleAction::SetDemiurge,  // true->false
            ]
        );
    }

    #[test]
    fn boot_settle_with_device_already_present_fires_present_action() {
        // Requirement 5: PMOR already attached at startup must be picked up
        // by one settle-time evaluation, not just future hot-plug events.
        let mut t = PresenceTracker::new();
        assert_eq!(t.observe(true), Some(RoleAction::SetOpgorator));
    }

    #[test]
    fn boot_settle_with_device_absent_fires_nothing() {
        let mut t = PresenceTracker::new();
        assert_eq!(t.observe(false), None);
    }

    // ---- manual-vs-auto override rule ----

    #[test]
    fn auto_source_allows_auto_apply() {
        assert!(auto_may_apply(Source::Auto));
    }

    #[test]
    fn manual_source_blocks_auto_apply() {
        assert!(!auto_may_apply(Source::Manual));
    }

    #[test]
    fn manual_always_wins_regardless_of_which_action_is_pending() {
        // The rule must not depend on what action was about to be applied —
        // manual blocks both SetOpgorator and SetDemiurge alike.
        for source in [Source::Manual] {
            assert!(!auto_may_apply(source));
        }
        for source in [Source::Auto] {
            assert!(auto_may_apply(source));
        }
    }

    // ---- sync.conf `source` parsing ----

    #[test]
    fn parse_source_manual() {
        let s = "role = demiurge\nsource = manual\n";
        assert_eq!(parse_source(s), Source::Manual);
    }

    #[test]
    fn parse_source_auto() {
        let s = "role = opgorator\nsource = auto\n";
        assert_eq!(parse_source(s), Source::Auto);
    }

    #[test]
    fn parse_source_missing_key_defaults_to_auto() {
        // Legacy sync.conf predating this feature: no `source` line at all.
        let s = "role = opgorator\n";
        assert_eq!(parse_source(s), Source::Auto);
    }

    #[test]
    fn parse_source_missing_file_defaults_to_auto() {
        assert_eq!(parse_source(""), Source::Auto);
    }

    #[test]
    fn parse_source_ignores_comments_and_blank_lines() {
        let s = "# comment\n\n   \nrole = demiurge\n# source = manual (commented out)\nsource=manual\n";
        assert_eq!(parse_source(s), Source::Manual);
    }

    #[test]
    fn parse_source_is_case_insensitive_for_manual() {
        assert_eq!(parse_source("source = MANUAL\n"), Source::Manual);
        assert_eq!(parse_source("source = Manual\n"), Source::Manual);
    }

    #[test]
    fn parse_source_garbage_value_defaults_to_auto() {
        assert_eq!(parse_source("source = whatever\n"), Source::Auto);
    }

    // ---- MIDI-only presence detection (pure, no shelling out) ----

    #[test]
    fn midi_presence_matches_opgorator_substring() {
        let names = vec!["Pocket OpGorator MIDI 1".to_string()];
        assert!(midi_client_present(&names));
    }

    #[test]
    fn midi_presence_matches_daisy_substring() {
        let names = vec!["Daisy Seed".to_string()];
        assert!(midi_client_present(&names));
    }

    #[test]
    fn midi_presence_matches_pmor_substring() {
        let names = vec!["PMOR-1".to_string()];
        assert!(midi_client_present(&names));
    }

    #[test]
    fn midi_presence_false_when_absent() {
        let names = vec!["Midi Through".to_string(), "System".to_string()];
        assert!(!midi_client_present(&names));
    }

    #[test]
    fn midi_presence_false_when_empty() {
        let names: Vec<String> = vec![];
        assert!(!midi_client_present(&names));
    }

    // ---- end-to-end role-handoff composition (pure logic, no hardware) ----
    //
    // These compose PresenceTracker + the manual/auto sync.conf rule as ONE
    // sequence, the way apply_action() actually gates them at runtime, to
    // prove the FULL "device appears -> auto role set -> device vanishes ->
    // reverts" story rather than each half in isolation. A small in-test
    // model stands in for demiurge-sync's own file (apply_action() itself
    // shells out, which isn't testable here without hardware/binaries) --
    // it mirrors exactly the same two rules apply_action() enforces:
    //   1. auto_may_apply(current_source) gates whether the pending action
    //      is actually written.
    //   2. A successfully applied SetOpgorator/SetDemiurge action updates
    //      what "current role" is, for the next iteration's gate check.
    // `source` never changes on its own here (only a human editing
    // sync.conf changes it, which apply_action() never does) -- these tests
    // set it directly at each step, exactly like a human intervening.
    struct FakeSyncConf {
        source: Source,
        role: &'static str,
        write_log: Vec<&'static str>,
    }

    impl FakeSyncConf {
        fn new() -> Self {
            Self { source: Source::Auto, role: "demiurge", write_log: Vec::new() }
        }
        /// Mirrors apply_action(): only writes (and logs) if auto may apply.
        fn apply(&mut self, action: RoleAction) {
            if !auto_may_apply(self.source) {
                return; // manual wins -- silently skipped, same as apply_action()
            }
            self.role = action.role_arg();
            self.write_log.push(action.role_arg());
        }
    }

    #[test]
    fn end_to_end_device_appears_then_vanishes_round_trips_role() {
        // device plugged in -> Demiurge becomes follower (opgorator) ->
        // device unplugged -> Demiurge reverts to master (demiurge).
        let mut tracker = PresenceTracker::new();
        let mut conf = FakeSyncConf::new();

        for present in [false, true, false] {
            if let Some(action) = tracker.observe(present) {
                conf.apply(action);
            }
        }

        assert_eq!(conf.write_log, vec!["opgorator", "demiurge"]);
        assert_eq!(conf.role, "demiurge");
    }

    #[test]
    fn end_to_end_plug_unplug_replug_churn_writes_one_action_per_edge() {
        // Simulates a flaky USB connection: plugged, unplugged, replugged,
        // unplugged again, replugged and left in. Every genuine edge must
        // write exactly once, in order, and land on the right final role.
        let mut tracker = PresenceTracker::new();
        let mut conf = FakeSyncConf::new();
        let readings = [true, false, true, false, true];

        for present in readings {
            if let Some(action) = tracker.observe(present) {
                conf.apply(action);
            }
        }

        assert_eq!(
            conf.write_log,
            vec!["opgorator", "demiurge", "opgorator", "demiurge", "opgorator"]
        );
        assert_eq!(conf.role, "opgorator");
    }

    #[test]
    fn end_to_end_manual_override_blocks_mid_churn_then_auto_resumes() {
        // Device present -> auto sets opgorator. A human then explicitly
        // claims master (source flips to Manual) -- e.g. they want the
        // Demiurge session in charge even with the OpGorator attached.
        // While manual, unplugging/replugging the device must NOT touch
        // the role at all (no writes). Once the human releases control
        // (source flips back to Auto -- e.g. they run `demiurge-sync auto`
        // or edit the file back), the very next presence EDGE (not a
        // no-op re-observation) must resume driving the role again.
        let mut tracker = PresenceTracker::new();
        let mut conf = FakeSyncConf::new();

        // Device appears -- auto picks it up.
        assert_eq!(tracker.observe(true), Some(RoleAction::SetOpgorator));
        conf.apply(RoleAction::SetOpgorator);
        assert_eq!(conf.role, "opgorator");

        // Human claims manual control, explicitly wants Demiurge master.
        conf.source = Source::Manual;
        conf.role = "demiurge"; // the human's direct edit, not via apply()

        // Device flaps while manual -- PresenceTracker still tracks edges
        // internally (it doesn't know about manual/auto), but apply()
        // must silently refuse to act on any of them.
        let writes_before_churn = conf.write_log.len();
        for present in [false, true, false] {
            if let Some(action) = tracker.observe(present) {
                conf.apply(action);
            }
        }
        assert_eq!(conf.write_log.len(), writes_before_churn, "manual source must block every auto write during churn");
        assert_eq!(conf.role, "demiurge", "manual role must survive the churn untouched");

        // Human releases control back to auto. Tracker's last_present is
        // now false (from the loop above) -- the NEXT device-present edge
        // must resume auto behaviour immediately.
        conf.source = Source::Auto;
        assert_eq!(tracker.observe(true), Some(RoleAction::SetOpgorator));
        conf.apply(RoleAction::SetOpgorator);
        assert_eq!(conf.write_log, vec!["opgorator", "opgorator"]);
        assert_eq!(conf.role, "opgorator");
    }

    #[test]
    fn end_to_end_clock_disappearing_entirely_still_reverts_once() {
        // "Clock disappearing entirely" mid-stream: present for a long
        // stretch (repeated identical observations, e.g. every 2s
        // heartbeat), then gone for good. Must fire nothing on the
        // repeats and exactly one revert on the actual disappearance --
        // never a wedge (stuck "opgorator" forever because nothing ever
        // told the tracker presence changed).
        let mut tracker = PresenceTracker::new();
        let mut conf = FakeSyncConf::new();

        assert_eq!(tracker.observe(true), Some(RoleAction::SetOpgorator));
        conf.apply(RoleAction::SetOpgorator);

        // 10 heartbeats of "still here" -- must be totally silent.
        for _ in 0..10 {
            assert_eq!(tracker.observe(true), None);
        }
        assert_eq!(conf.write_log, vec!["opgorator"]);

        // Gone for good.
        assert_eq!(tracker.observe(false), Some(RoleAction::SetDemiurge));
        conf.apply(RoleAction::SetDemiurge);
        assert_eq!(conf.role, "demiurge");

        // And subsequent "still absent" heartbeats stay silent too --
        // no re-triggering, no wedge.
        for _ in 0..10 {
            assert_eq!(tracker.observe(false), None);
        }
        assert_eq!(conf.write_log, vec!["opgorator", "demiurge"]);
    }

    // ---- transport-aware election (should_follow_external) ----
    //
    // The core of the 2026-10 rework: presence alone must never be
    // enough. Every case in the module doc's election table gets a test
    // here, plus the regression case that motivated the rework in the
    // first place (device present + sounding == false must NOT imply
    // follower -- the old bug).

    #[test]
    fn absent_device_is_always_master_regardless_of_status() {
        // Even a status file claiming PMOR is sounding must not matter
        // if the device itself isn't there -- presence is still a
        // necessary (just no longer sufficient) condition.
        let status = ClockStatus { pmor_playing: Some(true), loop_active: Some(false), ext_clock: Some(true) };
        assert!(!should_follow_external(false, status, false));
        assert!(!should_follow_external(false, status, true));
    }

    #[test]
    fn present_with_no_transport_info_stays_master() {
        // THE core bug this rework fixes: a device that is merely
        // plugged in, with no transport signal yet at all, must not
        // become master just by existing.
        let status = ClockStatus { pmor_playing: None, loop_active: None, ext_clock: Some(true) };
        assert!(!should_follow_external(true, status, false));
        // Even if DEMIURGE currently happens to be following (e.g. a
        // stale sync.conf from a previous session), "no information at
        // all" is still the safe-default case and does not get decided
        // by stickiness -- it's checked before the sticky arms below.
        assert!(!should_follow_external(true, status, true));
    }

    #[test]
    fn pmor_sounding_neptr_empty_is_follower() {
        let status = ClockStatus { pmor_playing: Some(true), loop_active: Some(false), ext_clock: Some(true) };
        assert!(should_follow_external(true, status, false));
    }

    #[test]
    fn pmor_playing_flag_without_external_clock_is_not_follower() {
        // The 2026-10-03 bug: PMOR plugged in, CC117 says playing, but no
        // 0xF8 from it on the bus -> must NOT elect follower.
        for ext in [Some(false), None] {
            let status = ClockStatus { pmor_playing: Some(true), loop_active: Some(false), ext_clock: ext };
            assert!(!should_follow_external(true, status, false));
        }
    }

    #[test]
    fn parse_clock_status_ext_clock() {
        assert_eq!(parse_clock_status("ext_clock = yes\n").ext_clock, Some(true));
        assert_eq!(parse_clock_status("ext_clock = no\n").ext_clock, Some(false));
        assert_eq!(parse_clock_status("pmor_transport = playing\n").ext_clock, None);
    }

    #[test]
    fn pmor_sounding_loop_unknown_is_follower() {
        // Partial info: PMOR known sounding, loop signal simply absent
        // (never seen any CC118 at all) -- unwrap_or(false) treats
        // "unknown" as "not running", so this still resolves to follower,
        // matching "this already works" in the task spec.
        let status = ClockStatus { pmor_playing: Some(true), loop_active: None, ext_clock: Some(true) };
        assert!(should_follow_external(true, status, false));
    }

    #[test]
    fn pmor_silent_neptr_running_is_master() {
        let status = ClockStatus { pmor_playing: Some(false), loop_active: Some(true), ext_clock: Some(true) };
        assert!(!should_follow_external(true, status, true));
    }

    #[test]
    fn pmor_unknown_neptr_running_is_master() {
        // Partial info the other way: no PMOR transport signal, but a
        // loop is known to be running -- unknown PMOR treated as "not
        // sounding", so DEMIURGE stays/becomes master.
        let status = ClockStatus { pmor_playing: None, loop_active: Some(true), ext_clock: Some(true) };
        assert!(!should_follow_external(true, status, false));
    }

    #[test]
    fn both_active_is_sticky_to_current_role() {
        // "Whoever started first keeps it" -- approximated here as
        // "whoever currently holds it keeps it", which is exactly
        // correct as long as this function is re-evaluated on every
        // heartbeat / edge-driven poll (it is -- see main.rs's three
        // call sites) rather than recomputed from scratch each time: the
        // role never flips BACK just because both happen to be active on
        // a given poll, which is what "started first" actually requires.
        let status = ClockStatus { pmor_playing: Some(true), loop_active: Some(true), ext_clock: Some(true) };
        assert!(should_follow_external(true, status, true), "was already follower -> stays follower");
        assert!(!should_follow_external(true, status, false), "was already master -> stays master");
    }

    #[test]
    fn both_silent_is_sticky_to_current_role() {
        // No requirement in the spec for "both silent" -- the safest
        // choice is the same stickiness as "both active": don't churn
        // the role (and spam CC116) for a transition that isn't actually
        // happening on either side.
        let status = ClockStatus { pmor_playing: Some(false), loop_active: Some(false), ext_clock: Some(true) };
        assert!(should_follow_external(true, status, true), "was already follower -> stays follower");
        assert!(!should_follow_external(true, status, false), "was already master -> stays master");
    }

    // ---- clock_status.conf parsing ----

    #[test]
    fn parse_clock_status_playing_and_loop_active() {
        let s = "role = follower\nbpm = 124.0\npmor_transport = playing\npmor_transport_age_s = 0.3\nloop_active = yes\nloop_active_age_s = 4.1\n";
        let status = parse_clock_status(s);
        assert_eq!(status.pmor_playing, Some(true));
        assert_eq!(status.loop_active, Some(true));
    }

    #[test]
    fn parse_clock_status_stopped_and_unknown_loop() {
        let s = "pmor_transport = stopped\nloop_active = unknown\n";
        let status = parse_clock_status(s);
        assert_eq!(status.pmor_playing, Some(false));
        assert_eq!(status.loop_active, None);
    }

    #[test]
    fn parse_clock_status_missing_keys_default_to_none() {
        let status = parse_clock_status("role = master\nbpm = 124.0\n");
        assert_eq!(status.pmor_playing, None);
        assert_eq!(status.loop_active, None);
    }

    #[test]
    fn parse_clock_status_empty_file_defaults_to_none() {
        let status = parse_clock_status("");
        assert_eq!(status, ClockStatus::default());
    }

    #[test]
    fn parse_clock_status_ignores_comments_and_blank_lines() {
        let s = "# demiurge-clock live status\n\n   \npmor_transport = playing\n# loop_active = yes (commented out)\nloop_active = yes\n";
        let status = parse_clock_status(s);
        assert_eq!(status.pmor_playing, Some(true));
        assert_eq!(status.loop_active, Some(true));
    }

    // ---- sync.conf `role` parsing (current_role_is_follower's parser) ----

    #[test]
    fn parse_role_opgorator_is_follower() {
        assert!(parse_role("role = opgorator\nsource = auto\n"));
    }

    #[test]
    fn parse_role_demiurge_is_not_follower() {
        assert!(!parse_role("role = demiurge\nsource = auto\n"));
    }

    #[test]
    fn parse_role_missing_key_defaults_to_not_follower() {
        assert!(!parse_role("source = auto\n"));
    }

    #[test]
    fn parse_role_missing_file_defaults_to_not_follower() {
        assert!(!parse_role(""));
    }

    #[test]
    fn parse_role_is_case_insensitive() {
        assert!(parse_role("role = OpGorator\n"));
        assert!(parse_role("role = OPGORATOR\n"));
    }
}
