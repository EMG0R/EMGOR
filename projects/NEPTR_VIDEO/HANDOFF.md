# NEPTR_VIDEO — handoff

Written 2026-08-30. Replaces the previous HANDOFF.md entirely.

This is the fractal-and-feedback video engine: NEPTR's VIDEO mode, and the host
of the HOME mode selector. C++17 + GLSL ES 3.0, one platform seam
(`platform_glfw.cpp` on macOS / `platform_drm.cpp` on the Pi).

**Read in this order:** this file → `ARCHITECTURE.md` (what exists, §17 is the
invariant list) → `ARCHITECTURE_V2.md` (the two-channel jit.op plan, not built)
→ `README.md` → `deploy/README.md`.

---

## 1. State right now

**Mac: 21/21 ctest green.** `journey_probe.py` passes (journey recurrence
31.3, control 2.85).

**Pi (`demiurge.local`): verified 2026-08-29** — 21/21 ctest (886 s), GCC 14.2
with zero warnings, source byte-identical to the Mac, 61 fps at 480x800, HOME
on the panel.

Since that Pi run, two Mac-only changes landed and are **not yet on hardware**:

1. `kUnseenDepthCap` 3.0 → **1.0**, applied bank-wide (see §3).
2. `journey_probe.py` now requires 80% frame coverage (see §4).

Neither can plausibly break the engine — one is a constant, one is a test — but
neither has been run on the Pi. **That is the next job.**

---

## 2. What this session built: the endless zoom journey

The generators used to oscillate on a closed path and never advance. The user's
words: *"dancing back and forth both ways not really advancing."*

`MotionClock` (`motion.h`) is four sinusoids of one phase, wrapped to 12π so 1,
½ and ⅓ realign exactly — mathematically a closed circle that returns to its
start every ~12.5 s forever. Measured: **recurrence 0.135**, i.e. it revisits an
earlier frame almost exactly.

The replacement is `Journey` (`journey.h`): a monotone `logZoom` integrated off
the same signed scene clock, so SPEED forward flies in, reverse flies back out,
zero freezes. **Recurrence 31.3.** A new global `g.journey` (JRNY, bipolar,
default +0.25 decades/s) sets the rate.

Four things about it are load-bearing:

- **It is a pure function of accumulated travel.** No state machine, no "am I
  recycling" flag. Reversibility is therefore structural, not implemented:
  rewinding recomputes the dive rather than remembering it. `motion_proofs.py`
  asserts rewound frames are **byte-identical**, and they are (mean|Δ| 0.000000).
- **Iterations, not precision, are the depth limit.** The hi/lo float centre
  pair (`uCenterHi`/`uCenterLo`, split at full double precision in
  `renderer.cpp`) is good to ~1e-14 and works on V3D. But at the 512-iteration
  cap the frame goes solid black past ~7 decades, so `kSpan` is 5.
  `Journey::detailBoost()` raises iterations with depth or the dive descends
  into a black screen.
- **A dive ends in a dissolve, never a cut.** Two views, blended by `uXfade`,
  with the incoming one WIDE (negative depth) so a dive opens out into somewhere
  new. The two-view branch is a runtime *uniform* `if`, deliberately not a
  `#define` — a compile-time variant would recompile the shader at the dive
  boundary, stalling exactly where the dissolve exists to hide.
- **Every dive carries its own golden-angle rotation** (`JourneyView::rot`).
  With a finite aim table dives eventually reuse an aim and reproduce a frame
  exactly — caught as recurrence 1.256 when a probe span happened to be exactly
  8 dives. The golden angle is an irrational fraction of a turn, so orientations
  never repeat, and the view turns *about* the aim rather than off it.

---

## 3. The aims — and the one open defect

**16 of 22 escape kernels were parked as "no dive available."** That was an
artefact: every kernel was aimed at MANDELBROT coordinates, and 21 of them are
not that map. Ten are Julia-seeded, where `c` is a live knob, so no fixed
coordinate can ever be right for those.

`tests/target_search.py` searches each map's *own* plane — beam search down the
depths, seeded at two view scales, aim refined every half decade, scored on live
fraction and edge energy. Results are baked into `kernels.cpp` as `kAims`
(11 kernels, up to 6 aims each; several per kernel, because one aim would make
every dive land in the same place and the instrument would loop).

BURNING SHIP went from dead-by-one-decade to a five-decade descent with edge
energy rising 1.1 → 68.8. Confirmed on the Pi: `0.61/0.8 … 0.30/66.1`, same
shape as the Mac, so the precision pair survives V3D's compiler.

### OPEN: the depth cap is set by eyeball, and only one kernel has been seen

**The metric cannot see the failure that matters.** Looked at on the panel,
BURNING SHIP degrades into green/red salt-and-pepper — precision breakdown in
the iteration loop — and *noise scores well on edge energy* (66.1), because
noise is the maximum-edge signal there is. Downsample coherence does not
separate them either (0.68 clean vs 0.56 speckled). It is also platform
specific: **the Mac renders these depths cleanly and will tell you everything is
fine.**

Measured on the panel, speckle fraction (pixels >60 from their 3×3 median):

| decades | 0 | 1 | 2 | 3 |
|---|---|---|---|---|
| burningship | 0.13% clean | 0.24% onset | 1.26% band | 8.29% failed |

So `kUnseenDepthCap = 1.0` in `kernels.cpp`, applied to the **whole bank** by
`capJourneyDepths()`. It is applied bank-wide rather than inside the aim applier
because `julia` and `kifs` take their depth from the *older* table and an
exception living in the aim applier let them straight past the ceiling.

**Only `mandelbrot` is exempt (4.5)** — the one kernel inspected at depth on the
panel, where it showed clean filaments, spirals and mini-brots at 4 and 5
decades.

**To raise any kernel: eyeball its dive at depth ON THE PANEL, then add a
one-line exception.** Not on a Mac. This is the single largest piece of value
left on the table — most kernels are probably good well past 1.0, but nobody has
looked.

The cap is known to be conservative. A second panel inspection at BURNING
SHIP's aim found 2 and 3 decades rendering as dense *structure*, not as the
salt-and-pepper of the first run, and measured a different speckle floor
(2.20 / 3.21 / 8.38 / 8.17% against 0.13 / 0.24 / 1.26 / 8.29%) — the two runs
used different median/threshold bases, so the absolute numbers are not
comparable. **The shape agrees in both: flat through 1 decade, a step change at
2.** So 1.0 is on the right side of the step either way, and the disagreement is
about how much headroom is being left, not about where the cliff is. Whoever
does the eyeball pass should fix ONE speckle metric first and write down its
basis, or the numbers will not be comparable across sessions either.

---

## 4. Landmines

- **No version control.** The repo has zero tracked files. Nothing here is
  recoverable if overwritten.
- **`measureFocus` must stay gated.** Running it ungated takes a **SIGBUS**
  inside libgallium at `~Renderer()` on the Pi. Confirmed by A/B: `NV_STEER=1`
  crashes at ≥20 frames (`kFocusEvery` is 15), unset is clean. It only bites at
  process exit, so the live instrument never sees it — but it truncates buffered
  stdout, which silently failed `motion_time` by losing the `wrote …` lines of
  frames that had rendered fine.
- **`NV_HOME=0` or you are testing the menu.** `main.cpp:1742` short-circuits to
  the HOME screen and never calls `renderScene`, and `measureFocus` lives inside
  it. A whole A/B was run without it, measured nothing, and looked like a pass.
- **Runtime steering is OFF and should stay off.** `focus.h` + `NV_STEER` finds
  the interior/exterior boundary (the shader writes interior as exact black, so
  that classification is free and exact). Measured, it left BURNING SHIP as dead
  and cost MANDELBROT its deepest decade: **not every boundary point is a good
  target** — a generic one sits on a smooth arc and converges to a straight
  edge. It refines an aim already in the right region; it cannot rescue one that
  is not.
- **`b.*` collides with morph** at `patch.cpp:45` — matters for V2.
- **`pkill -f` has killed the caller's own ssh.** Use systemctl or precise PIDs.
- **`live.conf`'s `ui` key is not an autostart flag** — it is read by the script
  that *is* `neptr-ui.service`'s ExecStart.
- **`-Wall` is not in CMakeLists.** Run it separately; the Pi's GCC catches what
  libc++ tolerates (`std::fabs` without `<cmath>` is a hard error there).
- **The web VISUALS page is a hash route**: `#/video/visuals`. The path form
  404s. Its parameter list is **hand-written** (`video.js` `PAGES[0].params`),
  so a new global is invisible there until added — that is how JRNY went missing
  from the web while showing on the panel.
- **Pi wifi is unstable.** It dropped three times in one session and once moved
  subnets (mDNS recovered it). Run anything long under `tmux`/`nohup` **on the
  Pi** — two runs were lost to a foreground ssh timeout.

---

## 5. Verification conventions — not optional

This project has repeatedly produced green results that meant nothing. Each of
these rules exists because it failed:

1. **Measure by running.** Not by reading source.
2. **Mutation-check every test.** A probe was written to catch the oscillation
   bug and *passed the bug* at 8.3 — it sampled too sparsely to land near the
   loop phase. It now samples densely and runs a control that must register as a
   loop.
3. **Never judge on partial data.** The same probe scored a PASS on 35 of 96
   frames on the Pi. Recurrence is a *minimum*, so missing frames can only
   flatter it. It now requires 80% coverage (`--mincover`).
4. **Separate VERIFIED from INFERRED** in every report.
5. **Never open a window on the user's Mac.** Headless only there
   (`NV_HEADLESS`/`NV_SHOT`/`NV_SELFTEST`). The Pi's display is fine and
   expected. Note `NV_HEADLESS` grants nothing on the DRM backend, and DRM
   ignores `NV_GEOM` — every Pi shot is a full-panel PPM.
6. **One agent on the hardware at a time.** Concurrent work has killed another
   agent's engine, collided on `CMakeLists.txt`, and built a duplicate
   mechanism (`journeyRate` vs `journeyDepth`) for one job.
7. **Never leave the rig without a UI.** The panel was left dark twice. If you
   stop early, restore `neptr-video` *first* and confirm HOME.

---

## 6. Test harness

`NV_HOME=0` (synth, not menu) · `NV_KERNEL` · `NV_CHAIN=none` ·
`NV_FIXED_DT` · `NV_SET k=v,...` · `NV_SHOT` + `NV_SHOT_FRAME` (single frame
writes the plain path; a comma list writes `.0`, `.1`, …) · `NV_GEOM` ·
`NV_GOVERNOR=0` · `NV_MOTION_PHASE` · `NV_STEER` · `NV_LIST_KERNELS` ·
`NV_JOURNEY_DEPTH` / `NV_JOURNEY_TRAVEL` / `NV_JOURNEY_AIM="x,y"` /
`NV_JOURNEY_TARGET`.

**`NV_MAX_FRAMES` is a render count; `NV_MAX_SECONDS` is wall clock.** Frames
for determinism, seconds for a safety net — a headless run loses vsync, so 600
frames went from ~10 s to ~0.2 s and a harness that slept 2.5 s was talking to a
process that had already exited.

Tools: `depth_sweep.py` (how far a kernel rewards zooming) · `target_search.py`
(find aims) · `target_sweep.py` (score the seed table) · `journey_probe.py`
(does it advance) · `motion_probe.py` (does every generator move) ·
`motion_proofs.py` (reverse/freeze/continuity/8-hour clock).

Two traps in the tools themselves: **negative travel does not zoom out**
(`diveIndex` floors it into the previous dive, so −0.7 renders at depth 4.3 —
use the zoom knob), and the **8-hour clock proof parks the journey**, because it
runs at 8 scene-seconds per frame ≈ 2 decades of zoom per frame, where "steps
evenly" is not a property those frames can have.

---

## 7. Not done, roughly in priority order

1. **Deploy the two Mac-only changes and re-verify** (§1).
2. **Eyeball each kernel's dive at depth on the panel and raise its cap** (§3).
   Biggest available win.
3. `julia` and `newton3` find no aim: both lack an **interior** (Julia sets are
   often dust, Newton is convergent) and the boundary criterion assumes one
   exists. A first attempt to accommodate them gave Julia a **false pass at 5
   decades** while its edge energy sat flat at 0.30 — a smooth wash. Real
   structure grows edge energy tenfold; that is now the acceptance test.
4. `git init`.
5. The governor holds 60 fps by settling at **192×320 on a 480×800 panel** — a
   real sharpness cost. Worth deciding whether a lower fps goal and a sharper
   picture is the better trade.
6. Nobody has ever **tapped the HOME touchscreen**; every agent drove it over
   OSC.
7. ARCHITECTURE_V2 (two-channel jit.op) is designed, not built.
