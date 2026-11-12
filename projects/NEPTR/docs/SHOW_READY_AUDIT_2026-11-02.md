# Show-ready audit, 2026-11-02

Goal: a dirty power cut comes back sounding as it was, including preset selection.
Status: orchestra changes are on disk and syntax-checked but NOT live (need an engine restart, Emory's call). NAM C++ fix is source-only, not built, not installed.

## 1. NAM startup crash

Evidence: three engine segfaults in a row, each preceded by
`[NAMLoader] Failed to load '32_super_reverb_1977.nam' (L): No config parser registered for architecture: SlimmableContainer`. The 4th launch loaded profile 5 then 11. The child exited ~9 s after launch (journal, 13:23:47 to 13:23:56) while the HAT stream was being reset by PipeWire. The crashed runs' csound stderr is not retained (log rewritten per launch), so this is inference, not a captured trace.

Mechanism (best fit to the code):
- The parser registry is `nam::ConfigParserRegistry::instance()`, a function-local static `unordered_map` (NAM/model_config.h). `SlimmableContainer` is registered by a static initialiser in container.cpp at dlopen. Nothing inserts afterwards, so a healthy process can never report it missing, and the first load of the same file family succeeds.
- The only time it is missing is when the registry has been destroyed: process teardown (exit() runs static destructors) while the NamLoader thread is mid-`get_dsp`. `nam_deinit` only joins the thread if csound runs deinit; on an exit/crash path it does not. The loader thread then parses against a destroyed map (message), then dereferences freed memory (segfault).
- Trigger: csound exiting early at boot (audio device not ready, HAT stream reset), which is the boot-ordering problem in section 3. The 4th launch survived because PipeWire was ready.
- Aggravator: the loader thread starts at opcode init with the compiled-in default (profile 5), and the persisted profile arrives a few k-cycles later, so boot did two heavy loads back to back (widening the window).

Fix (csound/nam/src/nam_loader.cpp/.h, `nam_loader.cpp` passes `g++ -fsyntax-only`):
1. `NamLoader::shutdown_all()` registered with `atexit` on first init: stops and joins every loader thread. It runs before the NAM-core statics are destroyed (registered later than their construction), so the registry outlives the thread.
2. Loader defers its first load until the persisted profile has been requested (max ~1 s): one load at boot instead of two.
3. A failed load with no model active retries 3x at 500 ms, never a hot loop. The audio thread already passes audio through when no model pair exists, and load exceptions are caught (std::exception and `...`), so failure means bypass plus a log line.

Not fixed: a hard SIGSEGV/SIGKILL elsewhere still kills the process; this removes the teardown race only.

Install (needs Emory; replaces the live plugin, requires an engine restart):
```
cd ~/NEPTR_phase4/csound/nam && nice -n 19 ./build-on-pi.sh   # or: cd build && nice -n 19 make -j2
sudo cp build/csound-nam.so /usr/lib/aarch64-linux-gnu/csound/plugins64-6.0/csound-nam.so.new
sudo mv /usr/lib/aarch64-linux-gnu/csound/plugins64-6.0/csound-nam.so.new /usr/lib/aarch64-linux-gnu/csound/plugins64-6.0/csound-nam.so
# backup first: sudo cp -a the old .so to csound-nam.so.bak-20261102
```
Then restart demiurge at a time Emory chooses.

## 2. Persistence audit

Save path: instr 97 scans every 2 s (`metro 0.5`); on any change it writes the table and `ftsavek`s alternately to `neptr_settings.ftab` / `.b.ftab` in `~/demiurge/state` (SD, ext4, not tmpfs). Observed: both files rewritten 4 s apart at 13:30:44/48 during use.

| State | Persisted | Where | Verified by |
|---|---|---|---|
| Every effect bypass + params | yes | slots 0..~301 | state_unpack.inc.orc read |
| NAM row/variant/bypass/in/out gain | yes | 272-280; profile idx recomputed from row+variant | unpack lines 290-310 |
| nam_size (FULL/MICRO) | yes | 340 (+100) | unpack |
| Vocal chain voc_* (12 + voc_pre_db) | yes | 319-330, 342 (+100) | unpack |
| voc_gate (x0/x1 pre-gain) | NEW | 347 (+100), boot-only | syntax check; needs restart |
| Mixer mx_* + looper master | yes | 331-339, 341 | unpack |
| Master out / vol, manual BPM | yes | 305, 296, 306 | unpack |
| Current menu, last toggle, pedal owner (LastToggle/29) | yes | 302-304 | unpack 336-346 |
| Blooper | yes | 225-229 | unpack |
| hyperGuitar lanes | yes | 309-318 | unpack |
| gkDuoFx, gkDuoNam (DUODECIMUS radio) | NEW | 344, 345 (+100), boot-only | syntax check; needs restart |
| gkPrLoaded (last preset) | NEW | 346 (+100), boot-only | syntax check; needs restart |
| Preset bank / activation sequences | yes | `presets/<ctx>/bank*.ftab`, `seq*.ftab`, A/B + generation | existing code |
| SoundMagic rack | yes | `state/sm_*.ftab` A/B | existing code |
| Looper audio/lengths | no, by design | blank slate since 2026-08-12 | looper_persist.orc header |
| mx_sel, lp_relock* chn | no | UI navigation / transient | low value |

New slots are boot-only (written in state_persist.orc after the unpack include, not in `state_unpack.inc.orc`) so a preset recall can never rewrite them. Old files read 0 = absent and are skipped. Layout stamp 77000302 unchanged, so existing files still load. Slot 348 = save generation. Free slots now 349-510.

Corrupt/truncated fallback: stamp lives at slot 511, the end of the file, so a truncated file fails the stamp and the other copy is used. Bug found and fixed: boot took A whenever A was valid, but A is one save older than B half the time, so the last change (e.g. preset selection) was lost after a cut. Boot now loads both, picks the higher generation (slot 348), and sends the next save to the older copy.

Time to SD: the engine write lands in the page cache; the kernel flushes after `vm.dirty_expire_centisecs=3000`, so a cut can lose up to ~30-35 s, not "a few seconds". Staged fix: `scripts/prime-state-sync.sh` + `prime-state-sync.service` (idle-class loop, fsync of changed state/bank files every 2 s). Install (no audio restart needed, but it is a new system unit):
```
sudo cp ~/NEPTR_phase4/scripts/prime-state-sync.service /etc/systemd/system/ && sudo systemctl daemon-reload && sudo systemctl enable --now prime-state-sync
```

## 3. Boot ordering

Failure: demiurge.service has `After=sound.target user@1000.service` only. It does not wait for the HAT to exist as a PipeWire node, so the launcher can link before `alsa_output.platform-soc_107c000000_sound.pro-output-0` exists and fall back to a virtual sink. prime-watch recovers it after the fact. Launcher (DEMIURGE_OS_work, not deployed) improvements:
- `ExecStartPre` that polls `pw-cli ls Node` (or `wpctl status`) for the HAT output and input nodes, 40 s timeout, then proceeds anyway. Lowest risk, no Rust change.
- In the launcher, refuse to link to a virtual sink while the HAT is expected; retry linking with backoff.
- Delay the csound launch until the quantum/rate lockstep (`demiurge-quantum-limits`) reports done. Journal shows a "HAT DROP -> hat-reset" at the same second as a restart, so an early csound start overlaps a HAT stream reset.

## 4. What needs an engine restart / still risky

- Restart needed: state_persist.orc changes (new slots, newest-copy pick, generation), and the NAM plugin (after build and install).
- Not live-tested: no audio or render was run. Static checks only (`csound --syntax-check-only`: completed, 0 errors; `g++ -fsyntax-only` on nam_loader.cpp). First boot after install should be watched: expect `[state] ...` lines and exactly one `[NAMLoader] Loaded profile`.
- First save after the restart writes gen 1 (old files have 0). Files from before are still accepted.
- Hard power cut during an SD write can still corrupt a file; the A/B copy covers one. With the sync unit absent, up to ~30 s of changes are lost.
- Looper contents never survive a boot (by design).
- NAM audio-thread detail: the audio thread holds shared_ptr copies of models, so the last release of an old model can run its destructor on the audio thread after a swap. Not a crash, a possible small glitch on profile change; unchanged.
