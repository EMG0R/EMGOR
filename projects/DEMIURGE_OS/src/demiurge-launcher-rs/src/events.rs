// Event sources for the main loop. Three threads, one channel.
//
//   spawn_pwmon    — shells out to `pw-mon` and parses its event stream.
//                    Fires UsbDeviceChange on BOTH addition and removal of a
//                    selectable ALSA audio node (`alsa_output.*` /
//                    `alsa_input.*`, USB and platform alike — see
//                    util::is_selectable_audio_node).
//                    `added:` carries full properties (node.name); `removed:`
//                    carries only `id:`, so we remember each USB node's id at
//                    add time and fire when that id is removed. (Needed so the
//                    graph re-applies on unplug — e.g. dropping from an
//                    aggregated pair back to a single interface re-engages the
//                    direct-output fast path.)
//   spawn_child_watcher — snapshots the tracked child pids and checks
//                    /proc/<pid> existence every 500ms. When one dies,
//                    sends ChildExited.
//   spawn_heartbeat — periodic tick, used by the main loop to detect
//                     state drift that the other channels missed.
//
// This is pragmatic: a real implementation might use pipewire-rs or
// signalfd to avoid polling. For DEMIURGE's single-user runtime budget
// this is well under 1% CPU.

use std::collections::HashSet;
use std::io::{BufRead, BufReader};
use std::process::{Command, Stdio};
use std::sync::mpsc::{Sender, channel};
use std::thread;
use std::time::{Duration, Instant};

#[derive(Debug)]
pub enum Event {
    UsbDeviceChange,
    ChildExited(u32),
    LiveConfigChanged,
    Heartbeat,
}

// Polls ~/demiurge/live.conf mtime every second and posts
// LiveConfigChanged when it moves. Chill by design — no inotify, no
// filesystem events, no realtime CPU cost. A 1 Hz stat() call is free on
// the Pi and keeps the implementation trivial.
pub fn spawn_live_watcher(path: String, tx: Sender<Event>) {
    thread::spawn(move || {
        let mut last_mtime: Option<std::time::SystemTime> = std::fs::metadata(&path)
            .ok()
            .and_then(|m| m.modified().ok());
        loop {
            thread::sleep(Duration::from_secs(1));
            let m = match std::fs::metadata(&path) {
                Ok(m) => m.modified().ok(),
                Err(_) => None,
            };
            if m != last_mtime && m.is_some() {
                last_mtime = m;
                if tx.send(Event::LiveConfigChanged).is_err() { return; }
            }
        }
    });
}

pub fn spawn_pwmon(tx: Sender<Event>) {
    // Raw feed from pw-mon lands in `raw_tx`. A debouncer thread reads it,
    // discards the initial flood (~2s), then coalesces subsequent bursts
    // so we only emit one UsbDeviceChange per quiet window.
    let (raw_tx, raw_rx) = channel::<()>();

    thread::spawn(move || {
        loop {
            let mut child = match Command::new("pw-mon")
                .stdout(Stdio::piped())
                .stderr(Stdio::null())
                .spawn()
            {
                Ok(c) => c,
                Err(_) => { thread::sleep(Duration::from_secs(2)); continue; }
            };
            let Some(stdout) = child.stdout.take() else { continue; };
            let reader = BufReader::new(stdout);

            // Which node ids are USB audio devices, so we fire on BOTH add and
            // remove. Rebuilt each pw-mon (re)spawn: the initial dump re-announces
            // every existing object as `added:`, repopulating the set (those
            // startup fires are drained by the debouncer's settle window below).
            let mut usb_ids: HashSet<u32> = HashSet::new();
            let mut ev_added = false;   // inside an `added:` block
            let mut ev_removed = false; // inside a `removed:` block
            let mut cur_id: Option<u32> = None;
            for line in reader.lines().flatten() {
                let t = line.trim_start();
                if t.starts_with("added:") {
                    ev_added = true; ev_removed = false; cur_id = None;
                } else if t.starts_with("removed:") {
                    ev_added = false; ev_removed = true; cur_id = None;
                } else if t.starts_with("changed:") {
                    ev_added = false; ev_removed = false; cur_id = None;
                } else if let Some(rest) = t.strip_prefix("id:") {
                    cur_id = rest.trim().trim_end_matches(',').parse::<u32>().ok();
                    // `removed:` carries only the id — decide here, since no
                    // node.name follows to tell us what kind of node it was.
                    if ev_removed {
                        if let Some(id) = cur_id {
                            if usb_ids.remove(&id) { let _ = raw_tx.send(()); }
                        }
                    }
                } else if t.starts_with("node.name")
                    && crate::util::is_selectable_audio_node(t.split('"').nth(1).unwrap_or(""))
                {
                    // Only on `added:` (ignore property re-dumps on `changed:`).
                    // The `id:` line printed earlier in this same block.
                    if ev_added {
                        if let Some(id) = cur_id { usb_ids.insert(id); }
                        let _ = raw_tx.send(());
                    }
                }
            }
            let _ = child.wait();
            thread::sleep(Duration::from_secs(1));
        }
    });

    thread::spawn(move || {
        let startup_end = Instant::now() + Duration::from_secs(2);
        // Drain everything during the startup settle window.
        while Instant::now() < startup_end {
            let _ = raw_rx.recv_timeout(Duration::from_millis(100));
        }
        loop {
            // Block until the first event, then debounce 750ms.
            if raw_rx.recv().is_err() { return; }
            loop {
                match raw_rx.recv_timeout(Duration::from_millis(750)) {
                    Ok(_) => continue,
                    Err(_) => break,
                }
            }
            let _ = tx.send(Event::UsbDeviceChange);
        }
    });
}

pub fn spawn_child_watcher(mut pids: Vec<u32>, tx: Sender<Event>) {
    thread::spawn(move || {
        loop {
            pids.retain(|&pid| {
                let alive = std::path::Path::new(&format!("/proc/{pid}")).exists();
                if !alive {
                    let _ = tx.send(Event::ChildExited(pid));
                }
                alive
            });
            if pids.is_empty() { break; }
            thread::sleep(Duration::from_millis(500));
        }
    });
}

pub fn spawn_heartbeat(tx: Sender<Event>) {
    thread::spawn(move || {
        loop {
            thread::sleep(Duration::from_secs(2));
            if tx.send(Event::Heartbeat).is_err() { break; }
        }
    });
}
