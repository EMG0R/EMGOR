# Cardinal on the Pi 5

STATUS: plan + install script staged, NOT built, NOT run. Nothing here has been verified on the rig.

**NEVER build, download or install Cardinal during a performance or while the engine is live.** A build pins the CPU and xruns the rig. The script refuses unless `CONFIRM_NOT_LIVE_AUDIO=yes`.

## Why Cardinal (not "VCV in Csound")
Cardinal is the GPL fork of VCV Rack 2 (DISTRHO/Cardinal), built for ARM64. It is the same engine as the Mac, so one `.vcv` file sounds identical on both. Re-implementing Rack modules in Csound would be a permanent, lossy rewrite that drifts from the Mac. Cardinal hosts the real modules.

## Sample-locked with Csound
Cardinal's standalone is a JACK client. On Demiurge, JACK is pipewire-jack at the `live.conf` rate/quantum (48000 / 64), the same graph Csound's rtjack client joins. Same period, same rate, same clock: sample-locked, no resampler. Never open a second audio device for it.

## Names (researched, unverified on hardware)
- Upstream has NO binary called `cardinal-headless`.
- JACK standalone: `bin/Cardinal` (also `CardinalNative`, `CardinalMini`). Needs GL/X11 libs, so on a display-less Pi it runs under `xvfb-run`.
- `make HEADLESS=true` builds GUI-less plugin formats (lv2/vst2/vst3/clap) and the static `loader`, not the JACK standalone.
- Prebuilt: GitHub release 26.02 has `Cardinal-linux-aarch64-26.02.tar.gz` (~1 GB). So ARM64 is available prebuilt.
- `deploy-cardinal.sh` installs the real binary to `/opt/demiurge/cardinal/bin/Cardinal` and a wrapper `/opt/demiurge/bin/cardinal-headless` (adds xvfb when no DISPLAY) so the milestone command exists.

Open question: if xvfb + standalone costs too much CPU, the alternative is a truly GUI-less host (Carla or a JACK-hosted LV2 `Cardinal.lv2` built with HEADLESS=true). Decide by measurement.

## Install
```
DRY_RUN=1 ./setup-script/deploy-cardinal.sh                        # plan
CONFIRM_NOT_LIVE_AUDIO=yes ./setup-script/deploy-cardinal.sh       # real run, idle rig only
```
Prefers the prebuilt tarball, falls back to a shallow recursive source build (`make jack`, -j2, `nice -n19 ionice -c3`). Idempotent via `/opt/demiurge/cardinal/.installed-version`. Before-NYC milestone: `cardinal-headless --version` works on TWINK. The `--version` flag is unconfirmed (DPF standalone); the script falls back to `--help` + `ldd`.

## Loading a .vcv
Unverified. Expected: `cardinal-headless /path/to/patch.vcv` (Rack-style patch argument), with the app auto-connecting to JACK. Confirm on first run, then wrap it as `demiurge-run-cardinal` in `src/wrappers/` following `demiurge-run-csound` (source `demiurge-env.sh`, honour `DEMIURGE_RATE`/`DEMIURGE_QUANTUM`).

## Later: core pinning + RT
Not part of the install. Target: pin Cardinal's engine thread(s) to cores separate from Csound's engine (`taskset`/cpuset), SCHED_FIFO below pipewire's 83 and relative to Csound's `chrt --fifo 74` (e.g. 70), same pattern as the csound wrapper. Re-measure latency after.

## Module budget method
The ceiling is measured, not guessed.
1. Run the Csound rig as for a gig (live.conf quantum 64).
2. Load a patch in Cardinal, add modules a few at a time.
3. Watch `jack_cpu_load` and the xrun count over a few minutes per step (`pw-top` for ERR column).
4. Budget = the module count/patch where xruns stay at 0 and `jack_cpu_load` stays under the agreed headroom. The first xrun is the ceiling; back off one step.
Record per-patch numbers beside the `.vcv`. Do all of this off-stage.
