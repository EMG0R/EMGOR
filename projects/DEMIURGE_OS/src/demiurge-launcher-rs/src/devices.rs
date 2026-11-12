// Device-header writer for ~/demiurge/live.conf.
//
// A background thread that every ~5 seconds:
//   1. Snapshots the current audio (USB + platform) / MIDI device set from
//      PipeWire + ALSA.
//   2. Formats it as a comment block delimited by DEMIURGE-DEVICES-BEGIN /
//      DEMIURGE-DEVICES-END markers.
//   3. Rewrites live.conf ONLY when:
//        - the device snapshot differs from the last-written block, AND
//        - the file has been idle for at least 10 seconds (mtime >= 10s old).
//
// The idle-guard is the point. Users editing live.conf are saving every
// few seconds. We never want to clobber a live edit with a stale header
// write — so we back off until the user is clearly done. If the user is
// editing "live" via an editor that writes on every keystroke, our
// rewrite simply never fires. Devices still show up on the next natural
// idle window (e.g. when the editor is closed).
//
// Atomic write: temp file + rename. We re-check mtime immediately before
// the rename; if the file changed between read and rename, we abort.

use std::fs;
use std::io::Write;
use std::thread;
use std::time::{Duration, SystemTime};

use crate::live::{DEVICES_BEGIN, DEVICES_END};
use crate::util;

const SCAN_INTERVAL: Duration = Duration::from_secs(5);
const IDLE_THRESHOLD: Duration = Duration::from_secs(10);

pub fn spawn_header_writer(live_path: String) {
    thread::spawn(move || {
        let mut last_block = String::new();
        loop {
            thread::sleep(SCAN_INTERVAL);
            let block = format_device_block();
            if block == last_block { continue; }

            // Idle guard: only touch the file if the user hasn't saved in IDLE_THRESHOLD.
            let Ok(meta) = fs::metadata(&live_path) else { continue; };
            let Ok(modified) = meta.modified() else { continue; };
            let Ok(age) = SystemTime::now().duration_since(modified) else { continue; };
            if age < IDLE_THRESHOLD { continue; }

            match rewrite_header(&live_path, &block, modified) {
                Ok(true)  => {
                    util::log("devices: refreshed live.conf header");
                    last_block = block;
                }
                Ok(false) => { /* aborted due to race; try again next tick */ }
                Err(e) => util::log(&format!("devices: rewrite failed: {e}")),
            }
        }
    });
}

// Returns Ok(true) if we wrote, Ok(false) if we safely aborted.
fn rewrite_header(path: &str, block: &str, expected_mtime: SystemTime) -> std::io::Result<bool> {
    let contents = fs::read_to_string(path)?;

    // Strip any existing DEMIURGE-DEVICES block.
    let stripped = strip_block(&contents);
    let mut new_contents = String::new();
    new_contents.push_str(block);
    if !stripped.trim_start().is_empty() {
        if !block.ends_with('\n') { new_contents.push('\n'); }
        new_contents.push_str(stripped.trim_start_matches('\n'));
    }

    // Re-check mtime immediately before writing.
    let meta = fs::metadata(path)?;
    if meta.modified()? != expected_mtime {
        return Ok(false);
    }

    // Atomic write: temp file next to the target, then rename.
    let tmp = format!("{path}.tmp");
    {
        let mut f = fs::File::create(&tmp)?;
        f.write_all(new_contents.as_bytes())?;
        f.sync_all()?;
    }
    fs::rename(&tmp, path)?;
    // Restore the mtime to match the old one so we don't wake the watcher.
    let _ = set_mtime(path, expected_mtime);
    Ok(true)
}

fn set_mtime(path: &str, t: SystemTime) -> std::io::Result<()> {
    // Poor man's utime via `touch -d`. No libc dep.
    let dur = t.duration_since(SystemTime::UNIX_EPOCH)
        .map_err(|e| std::io::Error::new(std::io::ErrorKind::Other, e))?;
    let secs = dur.as_secs();
    std::process::Command::new("touch")
        .arg("-d")
        .arg(format!("@{secs}"))
        .arg(path)
        .status()?;
    Ok(())
}

fn strip_block(contents: &str) -> String {
    let mut out = String::new();
    let mut skipping = false;
    for line in contents.lines() {
        let tr = line.trim();
        if tr == DEVICES_BEGIN { skipping = true; continue; }
        if tr == DEVICES_END   { skipping = false; continue; }
        if skipping { continue; }
        out.push_str(line);
        out.push('\n');
    }
    out
}

fn format_device_block() -> String {
    let audio = scan_usb_audio();
    let midi  = scan_usb_midi();

    let mut s = String::new();
    s.push_str(DEVICES_BEGIN);
    s.push('\n');
    s.push_str("# devices detected by DEMIURGE (read-only, auto-refreshed when idle)\n");
    s.push_str("# audio:\n");
    if audio.is_empty() {
        s.push_str("#   (none)\n");
    } else {
        for d in &audio {
            s.push_str(&format!("#   - {d}\n"));
        }
    }
    s.push_str("# midi:\n");
    if midi.is_empty() {
        s.push_str("#   (none)\n");
    } else {
        for d in &midi {
            s.push_str(&format!("#   - {d}\n"));
        }
    }
    s.push_str(DEVICES_END);
    s.push('\n');
    s
}

fn scan_usb_audio() -> Vec<String> {
    // Parse `pw-cli ls Node` output for selectable ALSA audio nodes (USB and
    // platform alike — util::is_selectable_audio_node) and extract a friendly
    // description. Fall back to `pw-link -o` if pw-cli is missing.
    let out = std::process::Command::new("pw-cli")
        .arg("ls").arg("Node")
        .output();
    let Ok(out) = out else { return vec![]; };
    let text = String::from_utf8_lossy(&out.stdout);

    // Very naive scan: for each node, if its name starts with alsa_output.usb-
    // or alsa_input.usb-, record its node.description.
    let mut names: Vec<(String, String)> = Vec::new();
    let mut cur_name: Option<String> = None;
    let mut cur_desc: Option<String> = None;
    let mut cur_dir:  Option<String> = None;
    for line in text.lines() {
        let t = line.trim();
        if t.starts_with("id ") && cur_name.is_some() {
            flush_audio(&mut names, &mut cur_name, &mut cur_desc, &mut cur_dir);
        }
        if let Some(v) = t.strip_prefix("node.name = \"") {
            let v = v.trim_end_matches('"').to_string();
            // Same predicate as aggregate/events: USB + platform audio nodes,
            // minus aloop and non-preferred HDMI.
            if util::is_selectable_audio_node(&v) {
                cur_dir = Some(if v.starts_with("alsa_output.") { "out" } else { "in" }.into());
                cur_name = Some(v);
            }
            else { cur_name = None; cur_dir = None; }
        } else if let Some(v) = t.strip_prefix("node.description = \"") {
            cur_desc = Some(v.trim_end_matches('"').to_string());
        }
    }
    flush_audio(&mut names, &mut cur_name, &mut cur_desc, &mut cur_dir);

    // Merge duplicates (same description appearing as both in and out).
    let mut seen = std::collections::BTreeMap::<String, (bool, bool)>::new();
    for (desc, dir) in names {
        let entry = seen.entry(desc).or_insert((false, false));
        if dir == "in"  { entry.0 = true; }
        if dir == "out" { entry.1 = true; }
    }
    seen.into_iter().map(|(desc, (i, o))| {
        let tag = match (i, o) {
            (true, true)  => "in + out",
            (true, false) => "in only",
            (false, true) => "out only",
            _ => "",
        };
        format!("{desc}  ({tag})")
    }).collect()
}

fn flush_audio(
    out: &mut Vec<(String, String)>,
    cur_name: &mut Option<String>,
    cur_desc: &mut Option<String>,
    cur_dir:  &mut Option<String>,
) {
    if let (Some(_), Some(desc), Some(dir)) = (cur_name.take(), cur_desc.take(), cur_dir.take()) {
        out.push((desc, dir));
    }
}

// pub: also used by clockrole::pmor_present() as the MIDI-side presence
// signal for the OpGorator/PMOR/Daisy device (covers it showing up
// MIDI-only, with no audio interface — see clockrole.rs's module doc).
pub fn scan_usb_midi() -> Vec<String> {
    // `aconnect -l` lists every ALSA seq client. We pick out anything that
    // isn't System / Midi Through / demiurge_clock and report its name.
    let out = std::process::Command::new("aconnect").arg("-l").output();
    let Ok(out) = out else { return vec![]; };
    let text = String::from_utf8_lossy(&out.stdout);

    let mut names = Vec::new();
    for line in text.lines() {
        if let Some(rest) = line.strip_prefix("client ") {
            if let Some((_id, name_and_type)) = rest.split_once(": ") {
                if let Some(name) = name_and_type.split('\'').nth(1) {
                    let skip = name == "System"
                        || name == "Midi Through"
                        || name == "demiurge_clock"
                        || name.starts_with("RtMidi")
                        || name.starts_with("Pure Data")
                        || name.starts_with("csound")
                        || name.starts_with("ChucK")
                        || name.starts_with("SuperCollider");
                    if !skip {
                        names.push(name.to_string());
                    }
                }
            }
        }
    }
    names
}
