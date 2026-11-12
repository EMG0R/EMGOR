# NEPTR multipath mixer — plan (2026-10-08, from Emory)

Owner: pmor (NEPTR lane). Target: Prime NEPTR (8in/8out), and twink must keep working (stereo, 1 path).
Builds on DEMIURGE_OS 1e1cef5 (per-node mode, NxN routing/gain matrix).

## Paths (parallel, each with its own FX)
| path | NY patch (preset 1) | notes |
|---|---|---|
| Guitar | in 1-2 linked (mono guitar; 6-input hex is a separate patch) | full FX suite |
| PMOR | in 3-4 stereo | no FX = clean pass ("chill") |
| hyperTrumpet | in 5-6 STEREO | own FX path |
| Vocal | in 7-8 STEREO | Emory's custom vocal chain (not on PMOR) |

- **FX per path:** every path shows the full FX menu (blank by default), but **only effects that are switched on compute anything**. Minimal CPU. Separate settings per path.
- **Merge:** all paths feed the looper, looper mod and glitch mod. Emory asked about parallel loopers (open question).
- **Patches:** each patch saves its input layout, path FX and levels.
  1. **NY patch**: the table above.
  2. **Surf patch**: separate; layout TBD.
  3. **Hex / MC patch**: hyperGuitar hex on in 1-6, PMOR on 7 (hardware-summed to mono), vox/trumpet on 8 via a hardware switch.

## UI (Python pygame UI AND hardware: both required)
- A mixer page shows a volume strip per path, with a level meter.
- Encoders 1-4 set the levels of paths 1-4.
- Clicking an encoder opens that path's FX path (the current menu system, scoped to that path).
- Holding the next/previous encoder goes back to the mixer page.
- The web mixer page mirrors it (two-way via the pool).

## CPU / cores (Emory asked: pin instruments to different cores?)
- Today one csound runs pinned to isolated core 3 (FIFO 74) at ~34% CPU.
- Proposal:
  - Paths stay in the main csound, on-demand.
  - If the measured load needs it, split the looper/glitch section (or heavy paths) into a second csound/JACK client pinned to another core, in the same JACK cycle so no latency is added.
  - Measure first on Prime with all 4 paths + typical FX; split only if the period budget is tight.
- Csound `-j` multithreading has per-block overhead at small ksmps. A separate process per core is the predictable route.

## Open questions
- Surf patch: input layout?
- Loopers: one looper per path (parallel, clock-synced) feeding the mods, or one looper on the sum?

## Answers (Emory, 2026-10-08)
- **Loopers:** one per path, all clock-synced. Channels group into **linked instruments**: guitar 1-2 is one linked group, the hex guitar 1-6 is one linked 6ch group with one 6ch looper, and so on. A path = a linked group of N channels.
- **Surf patch:** mono guitar, summed from ins 1+2 into one mono path, stereo out.
- **Hex / MC patch:** hex guitar 1-6 as a 6ch linked group, PMOR on 7 (hardware-summed mono), vox/trumpet hardware switch on 8.
- **Cores:**
  - The synthesis engine (USS) runs on its own core.
  - Clocked cores split for parallel DSP: every audio path is an independent chain at the lowest latency, distributed dynamically across cores.
  - No clicks, fast.

## Architecture (locked, from the answers)
- **Each path is its own engine instance (JACK client):** the SAME neptrPhase4.csd launched per path, with PATH_ID, input group (channels, linked count), OSC/param port and state dir passed as macros. One codebase gives the full FX suite per path. FX are blank until switched on, and only enabled FX compute. The path's own looper lives inside the instance.
- **A master instance** takes every path's output and runs looper mod + glitch mod + master out.
- **Parallelism:**
  - The engines are separate JACK clients in one graph. JACK2 runs independent clients in parallel across cores in the SAME cycle, so there is no added latency and the work spreads dynamically across cores.
  - Order: paths in parallel, then master. USS is a separate client on its own core.
  - Needs sync_layer=on (graph) on Prime, not the single-process bypass.
- **Sync:** every instance follows demiurge-clock (tempo/start), so the loopers are bar-locked.
- **No clicks:**
  - Patch and layout switches crossfade path gains (mixer smoothing, ~10-20 ms).
  - A new path instance starts muted and fades in.
  - FX on/off already ramps.
- **Mixer:** the 1e1cef5 NxN matrix becomes the routing between JACK ports / path inputs. Level per path; per-path looper send.
- **UI:**
  - pygame mixer page: strips + meters.
  - Encoders 1-4: path levels.
  - Click: open that path's FX menus (the existing menus, pointed at that path's port).
  - Hold next/prev: back to the mixer.
  - Web mixer mirrors it.
  - The hardware and Python UI share ONE state via the pool.

## Build order
1. Make neptrPhase4.csd instantiable per path (macros, per-path ports/state). Offline-render 4 instances. CPU per instance at idle and with FX.
2. Master/mods instance + per-path looper sync.
3. Patch files: NY / surf / hex as layout presets (linked groups -> paths -> outputs).
4. Mixer page in pygame + encoder mapping + web mixer.
5. Bench on Prime (Emory present): sync_layer=on, JACK2 parallel, measure xruns/CPU.
