# VCV worker notes (gorpi7v5)
Updated 2026-10-22.

## State
- Cardinal 26.02 aarch64 prebuilt installed: /opt/demiurge/cardinal/bin/Cardinal (+ resources/). Tarball+extracts in ~/vcv/_dl (4 GB; tarball sha256 b36b6d5b...f7a98). xvfb apt-installed. ldd clean.
- CardinalJACK/Native are self-extracting zips (unzip once, don't run the SFX: unpacks 390 MB to /tmp each launch).
- Wrapper /opt/demiurge/bin/demiurge-run-cardinal (source: ~/DEMIURGE_OS_work src/wrappers, commit 008734b). Launcher source knows *.vcv -> lang cardinal; launcher NOT rebuilt (cargo build is heavy; needs Emory go / idle window).
- Checker ~/vcv/bin/vcv-check (venv python, stdlib; copy in vcv/vcv-check.py). Patches go in ~/vcv/patches/ (empty; Emory's are not here yet).
- Audio path: RESOLVED by Emory: sync_layer=on, pipewire-jack graph, csound6 client, 8ch HAT ports playback_AUX0-7. Cardinal joins as JACK client; links only via /opt/demiurge/bin/demiurge-graph.

## Decisions
- Patch load: no CLI arg upstream -> private HOME + autosave/patch.vcv copy (UNVERIFIED path).
- Guest scheduling: cores 0-2, FIFO 40 (isolcpus=3 holds csound). Later: coremap role cardinal core 2 / FIFO 70.
- Param hook: OSC /host-param /param /load (port 2228, GUI toggle in prebuilt) or MIDI-CC module via Midi Through.

## Blocked / gate
- Auto-mode classifier denied my test launch of Cardinal into the live graph (expected: needs Emory). Not run. Ports names/autosave path/CPU cost all unmeasured.

## Next
1. Emory go -> first run (see report for command), measure pw-top ERR + CPU, verify autosave load + port names.
2. Patches from Emory -> vcv-check each.
3. Launcher rebuild + chain token test; param manifest ~/demiurge/params/cardinal.json.
