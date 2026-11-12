# PMOR Live dry run (permission-free)

Run later, with Live CLOSED (the script refuses otherwise):

    bash "/Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW/Pocket-OpGorator/DaisyExamples/seed/PMOR_V3_max/max/dryrun/run.sh"

`run.sh --check` validates the files (no Live/Max). `run.sh --dry` prints every command, runs nothing.
Exit: 0 pass, 1 checks failed, 2 refused, 3 timeout (120 s), 4 device ran but LiveAPI absent.

Flow: `open -a "Ableton Live 12 Suite" dryrun.als` -> Live starts Max for Live -> `PMOR dryrun.amxd` (Max Instrument on one MIDI track) runs
1.5-7.5 s after load -> writes `/tmp/pmor_dryrun_result.json`, then `/tmp/pmor_dryrun_done` -> script prints the JSON, then `kill -TERM` on the Live pid it
started (SIGKILL after 20 s). No AppleScript, no screenshots. Device never opens a serial/udp port or sends `hello`.

Result fields (`/tmp/pmor_dryrun_result.json`):
- `ok`: max_ok AND (inside_live ? live_ok : true) AND no errors. `inside_live`: LiveAPI `live_set` answered (id != 0). `max_ok` / `live_ok` (null outside Live).
- `max_checks`: patcher.filepath non-empty; set-folder resolution (copy of pmor_where.js logic; with dryrun.als beside the device, set_folder must equal the device dir);
  pmor.proto and pmor.core~ instantiate (then deleted; sent only `sessiondir`/`pool` and `reseed 0`); pmor_ui/lane/port/where/browser.js + schema.json resolve on the search path.
- `live_checks` (only inside Live): live_set reachable, `is_playing`, `tempo`, `current_song_time` readable, live_app version. `live.live_set_properties` lists the LOM properties Live reports.
- `checks`: all rows `{group,name,ok,detail}`. `device`: filepath/name/dirname/listing. `set_folder`, `session_dir`: resolved cache location. `externals`, `helpers`, `max`, `live`, `errors`, `elapsed_ms`.

Files: `make_dryrun.py` -> `PMOR dryrun.maxpat` + `.amxd`; `pmor_dryrun.js` (logic, copies pmor_where.js resolver as of 2026-11-09);
`make_set.py` -> `dryrun.als` from `template/one_midi_track_instrument.xml` (cut down from Emory's Live-saved `marimb comp.als`); `check.py` validators.
After editing the amxd run `python3 make_dryrun.py && python3 make_set.py` (the set records the amxd size/CRC; `run.sh --check` fails if stale).
`make_set.py --userlib` points the set at the User Library copy (copy the amxd + pmor_dryrun.js there first).

NOT verified (could not open Live): that Live accepts dryrun.als and loads the device from the recorded path; whether the set loads clean (not dirty) so SIGTERM quits
without a save sheet; that Live exits on SIGTERM (else SIGKILL, which makes Live offer crash recovery next launch); that `live_app` version calls
and the LOM property names exist as probed; that the amxd container (modelled on CHORUS.amxd) loads in Live. Headless Max verified the amxd + js (result file, externals, search path).
