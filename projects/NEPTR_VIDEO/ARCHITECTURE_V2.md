# NEPTR_VIDEO V2 — `jit.op` Two-Channel Architecture

**Written 2026-08-16. This is a handoff document.** It is written so that a model or a
person with *no prior context* can read this file plus the repo and start building.

It supersedes nothing in [`ARCHITECTURE.md`](./ARCHITECTURE.md) (V1) as a *description of
what exists* — V1 remains the reference for today's engine, and this document cites it
constantly. Where V2 changes one of V1's invariants, that is called out explicitly in
[§14](#14-invariants--what-v2-keeps-and-what-it-supersedes).

Read in this order if you are new: **§1 (current state) → §2 (verification standard) →
§3 (operational handoff) → §4–§9 (the design) → §10 (weaknesses) → §11 (phases) →
§12 (dispatch) → §13 (open questions).**

---

# PART I — WHERE THINGS STAND

## 1. Current state, for someone with zero context

### 1.1 What this is

NEPTR is a Raspberry Pi 5 instrument with four **modes**: AUDIO, VIDEO, GAMES,
MULTI-DIM-DSP. This repo is **VIDEO** — a fractal-and-feedback video synthesis engine in
C++17 on GLES 3.0 — and it also hosts the HOME mode selector that launches the other
three. ~9,600 lines of C++. The full description of the engine as it stands is
`ARCHITECTURE.md` in this directory; **read it fully before writing code.** This section
covers only what that document cannot: what is in flight, what is unverified, and what is
known-uncertain as of today.

Two hard constraints shape everything (V1 §1):

- **The Pi 5 has no hardware video *encoder*.** (It does have an H.265 hardware
  *decoder*.) This is the central fact for the NDI-output and recording questions in §9.
- **DRM/KMS allows exactly one master.** HOME is a view inside this binary, not a
  separate program, for that reason alone.

### 1.2 Build and test status — verified today

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j8
cd build && ctest
```

**The tree builds clean, 0 errors. `ctest` is GREEN: 16/16 pass, 0 failed, ~134–149 s
total.** An earlier report in this project's history that the suite was red was a
truncated-shell artifact, not a real failure. Do not carry that claim forward.

```
 1..5  compiler palette patch control preset .... Passed   0.00 sec each
 6..11 home rotate lifecycle mod motion governor  Passed   0.00 sec each
12/16  mode_games ....................... Passed    1.02 sec
13/16  control_bus ...................... Passed    0.30 sec
14/16  motion_bank ...................... Passed   77.39 sec
15/16  motion_time ...................... Passed   65.72 sec
16/16  list_bus ......................... Passed    4.57 sec
```

The suite grew from 12 to 16 and from ~1.5 s to ~134 s because `motion_bank` and
`motion_time` **render real frames from the real binary and compare pixels.** That is the
project's verification standard working as intended (§2), not a regression. Registration
is in `CMakeLists.txt:248-277`, all three GL-driven suites inside `if(NOT
NV_SKIP_GL_TESTS)`.

### 1.3 In flight: intrinsic generator motion + bipolar SPEED

This work is **largely landed and asserted**, contrary to earlier reports that it was
abandoned mid-step. The agent doing it did finish wiring its suites into ctest.

**What exists** (`src/motion.h`, 78 lines, GL-free, header-only):

- `class nv::MotionClock` — a single `double phase_` (`motion.h:75`) advanced by
  `phase_ += dtScene * 2π * 0.08` (`kBaseHz = 0.08`, `motion.h:47`) and wrapped modulo
  `kPeriod = 12π` (`motion.h:43`), sign-correct in both directions (`motion.h:51-55`).
- `vector(float amount, float out[4])` (`motion.h:67-72`) →
  `amount * (cos φ, sin φ, sin φ/2, sin φ/3)`. `amount` is the **EVOLVE** knob; at 0 it
  returns exact zeros, so EVOLVE 0 is a bit-exact identity.
- Wired: `MotionClock motionClock;` lives in `main()`'s loop scope
  (`src/main.cpp:1355`), deliberately **outside `App`** so a kernel change, a shader
  recompile or a preset load cannot reset it (comment at `main.cpp:1350-1354`). Advanced
  at `main.cpp:1554-1556`, pushed to the renderer at `main.cpp:1662-1666`, uploaded as
  `uMotion` once per frame at `renderer.cpp:364`, declared at
  `shader_source.cpp:198`.
- Start phase is **randomised per run** from `steady_clock` (`main.cpp:1362-1371`);
  `NV_MOTION_PHASE` pins it for tests.
- **Escape kernels receive `uMotion` but NOT `uTime`** (`nvStep(s, c, uKernelP,
  uMotion)`, `shader_source.cpp:345-347`). Field kernels receive raw `uTime`
  (`nvField(p, uTime, uKernelP, uMotion)`, `shader_source.cpp:287-291`). This is the
  whole point of the work: escape kernels were previously frozen because they are pure
  functions of position, and now they animate off a *bounded, wrapping* clock rather than
  an unbounded one.
- Params: `g.speed` is `Curve::BipolarCube`, −8..8, default 1 (`state.cpp:214`);
  `g.evolve` is Linear 0..2, default 1 (`state.cpp:220`).

**What is actually asserted, per suite.** This matters — a passing suite proves the
assertions its author wrote, nothing more:

| claim | `test_motion.cpp` (ctest `motion`) | `motion_probe.py` (`motion_bank`) | `motion_proofs.py` (`motion_time`) | `motion_cost.py` (**not in ctest**) |
|---|---|---|---|---|
| every kernel animates | no — links no kernels | **yes, all 30**, enumerated from the `[nv] kernels N` banner (`main.cpp:831`); asserts mean-abs pixel diff ≥ 0.5 between frames 40 and 220 | no — **kernel 0 only** | no — 6 hardcoded |
| reverse truly reverses | yes, arithmetically to 1e-5 (`:167-182`) | no | **yes — byte-identical frames** at 60 and 180 with `g.speed=-1` at frame 121, plus a control run proving the test *can* fail (`:91-119`) — kernel 0 only | no |
| speed 0 is a true freeze | yes, bit-exact vector over 100 zero-advances (`:154-165`) | no | **yes — byte-identical frames 90/91 and 90/150**, scene time `==` exactly (`:121-134`) — kernel 0 only | no |
| continuity across the wrap | yes, worst single-step delta < 0.025 across 12π (`:184-211`) | no | dense 40-frame run and a zero-crossing sweep (`:136-173`) | no |
| continuity across kernel switch | no | no | **yes** (`:175-191`) | no |
| fps / cost | no | no | no | **yes — ms/frame + % deltas** |

**The gap between what is asserted and what was asked for:** reverse, freeze and
continuity are proven for **kernel 0 only**. `motion_proofs.py` has a `--kernel` flag
(`:85`) that CMake never passes (`CMakeLists.txt:265-267`). Every-kernel coverage
(`motion_bank`) proves only *movement*, at EVOLVE 1 / SPEED 1. So "does reverse work on
kernel 17" is currently unknown. Closing that is cheap: pass `--kernel` in a loop, or add
a second registration over a sampled subset.

### 1.4 Known-open: the frame-jump report, and a hypothesis that does NOT hold

**The user reported frames sometimes jumping on animating fractals.** This is open. No
test covers it and nobody has confirmed a fix on hardware.

A prime suspect had been proposed: `sceneTime` is a `double` (`main.cpp:1349`) but is
narrowed to `float` for the `uTime` uniform (`renderer.cpp:357`, and again per chain pass
at `renderer.cpp:532`). **This hypothesis was checked with numbers and does not survive:**

- float32 ulp at `sceneTime = 600 s` is 2⁻¹⁴ ≈ **61 µs**. A 16.67 ms frame delta is
  **≈273 ulps** — quantisation jitter ~0.37 % of one frame's motion. Not a visible jump.
- The cast only becomes visibly steppy (ulp ≈ 10 % of a frame) around `sceneTime ≈
  16,384 s ≈ 4.5 h`, and jump-level (ulp > one frame) around ~73 h. At SPEED 8 those
  wall-clock thresholds shrink 8×, i.e. steppiness at ~34 min of wall time at maximum
  speed — which is the only regime where this is even a candidate.
- It also cannot explain jumps on **escape** kernels at all, because `nvStep` is never
  handed `uTime`.

Record this as *disproven at normal speed*, not as *fixed*. Better candidates, in order,
all unverified:

1. **The governor re-planning mid-run.** `detailScale()` / `resScale()` are clamped into
   the frame at `renderer.cpp:330-338`. A detail or resolution step *is* a visible jump.
   Critically, **both Python motion suites set `NV_GOVERNOR=0`**
   (`motion_probe.py:105`, `motion_proofs.py:44`), so neither suite can catch a
   governor-induced step. This is the single most suspicious gap.
2. **Async chain recompiles going live between frames** (`pumpCompiler()`, called from
   the frame loop ~`main.cpp:1552`; swap at `renderer.cpp:281-288`).
3. **A raw `dt` spike** fed straight into `sceneTime` (`main.cpp:1385-1394`) unless
   `NV_FIXED_DT` is set.

**Reproduce before fixing** (§2). The obvious first experiment is a long run with the
governor *enabled* and per-frame pixel deltas logged, looking for an outlier.

### 1.5 Known-open: the fps cost of motion is unmeasured

`tests/motion_cost.py` exists, works, and prints real numbers — but is **not registered
in CMake** and has never been reported. It runs the binary with
`NV_FRAMETIME=1 NV_VSYNC=0 NV_GOVERNOR=0 NV_MAX_FRAMES=600`, scrapes `[ft] <ms>` lines
(`main.cpp:1393`, printed *before* the `NV_FIXED_DT` override so it is genuine wall-clock
interval, with `glFinish()` at end of frame so it is real GPU time), drops the first
quarter, takes the median, and takes the minimum of three medians. It reports
**milliseconds per frame, not fps**: per kernel, STATIC / EVOLVE0 / INSTR % / EVOLVE1 /
MOVING %, plus a bit-exactness verdict, plus a summary
`instruction cost across those kernels: median +X.X%, worst +Y.Y%`.

Default kernels are `0,4,10,13,20,28` at detail 256, 1280×720, 600 frames
(`motion_cost.py:78-81`). Running it costs several minutes. **Someone should run it and
paste the output into this document.** It is also the tool that will answer the single
biggest V2 question (§10.1).

### 1.6 The kernel-names wire verb: it exists, and here is its exact format

This was requested so surfaces can show a button per fractal instead of only next/prev.
**It is implemented and passing** (ctest `list_bus`). It is a general list bus, not a
kernel-specific one, which is exactly right for V2.

**Request** — `receiver.cpp:171-178`:

```
/nv/list ,s <what:s>      # one bank
/nv/list                  # no args = every bank
```

`what` ∈ `"kernel" | "palette" | "color" | "trap" | "mode"` (`main.cpp:433-471`). Any
`/nv/list` parses successfully; an unknown `what` simply yields an empty list.

**Reply** — `receiver.cpp:287-314`, sent immediately to the last-heard-from peer via
`sendToPeer` (`:316-324`), bypassing the 10 Hz broadcast interval:

```
/nv/list_item ,siss  <what:s> <index:i> <id:s> <name:s>
/nv/list_end  ,si    <what:s> <count:i>
```

Encoders at `osc.cpp:204-216` and `osc.cpp:183-191`. Messages accumulate into `#bundle`s
capped at `kMaxDatagram = 1200` bytes (`receiver.cpp:23`); `list_end` is always last, so a
surface counts items until it sees `list_end` and re-requests on a count mismatch.
Only `kernel` uses distinct `id` and `name` (`{k.id, k.name}`, `main.cpp:439`);
palette/color/trap/mode put the name in both fields.

**This is the single most important existing seam for V2's web UI**, because the operator
grid the user asked for is `/nv/list op` with **zero new wire machinery** — see §8.2.

### 1.7 Tree divergence, and the absence of version control

- **The Mac `DEMIURGE_OS` tree lacks control-plane patches 1 and 2 that are live on the
  Pi.** A naive `deploy` would revert them. Verify before deploying: press ARCADE on the
  phone panel at `/neptr/panel`; `b1` should go 1 then 0. If it goes 1 and **stays** 1,
  patch 1 is missing. If nothing appears at all, patch 2 or 4 is missing
  (`deploy/README.md:499-501`).
- `~/_______DEMIURGE/web/demiurge_web.py` on the Pi is behind `/opt` (pre-existing).
- **Nothing is committed anywhere; every tree is dirty by instruction.**
- **`NEPTR_VIDEO/` is not under version control at all.** `git rev-parse --show-toplevel`
  resolves to `/Users/emgor/Documents/_EMGOR_SYNTH`, and `git ls-files .` inside
  `NEPTR_VIDEO/` returns **zero tracked files**. There is no branch, no worktree, no
  diff, no revert. This is the largest *process* risk in the project and it dominates the
  dispatch strategy in §12.

---

## 2. Verification standard — what "done" means here

This project has been burned repeatedly by agents reporting success from reading their own
code. V1 §14 states the standard; this section makes it operational and adds the two rules
learned since.

### 2.1 The four rules

1. **Claims come from running the binary and measuring.** Not from reading the diff. A
   claim about pixels is settled with frame captures and a statistic —
   pixel-difference or brightness — never by inspection. Precedents: particles-only mean
   brightness 95.04 vs the synth's 96.21 (V1 §8); death-at-max mean |Δ| 4.719 against a
   baseline *re-run* differing by 4.721 (V1 §8); white-pixel fraction 0.0000 at DEPOSIT 1
   over 900 frames (V1 §9). Copy that shape.
2. **Reproduce the bug before fixing it.** At least one "fix" in this project's history
   was proposed against a non-bug because the reading was stale. If you cannot make it
   happen on demand, you cannot know you stopped it.
3. **Mutation-check every new test.** Deliberately break the logic under test and confirm
   the assertion fires. `motion_proofs.py:116-119` does this properly — it runs a
   *control* capture at the overshoot frame to prove the reverse test is capable of
   failing. A test that has never been seen to fail is not evidence.
4. **State plainly what is estimated.** Fill-rate figures, NDI latency, VRAM budgets in
   this document are estimates and say so. An estimate presented as a measurement is
   worse than no number.

### 2.2 Test runs must not steal the user's screen — **on the Mac**

The user works on the Mac while agents run. The suite is now **~134 seconds**; with
visible windows that is over two minutes of stolen focus per run.

- **Mac (GLFW backend): never put a window on screen.** `NV_SHOT` and `NV_SELFTEST`
  **imply headless** — `GLFW_VISIBLE=FALSE` plus `GLFW_FOCUSED=FALSE` and
  `GLFW_FOCUS_ON_SHOW=FALSE` — because every caller of those is a capture or a test, never
  someone watching. `NV_HEADLESS=1` hides a window explicitly. `NV_WINDOW=1` forces a
  visible window back for the times you genuinely want to watch. (All in
  `src/platform/platform_glfw.cpp`; the worker-context creation was also fixed to stop
  restoring `GLFW_VISIBLE=TRUE` unconditionally, which would un-hide any window created
  after it.) Verified: `NV_SELFTEST=1 NV_SHOT=/tmp/hl.png ./build/neptr-video` renders a
  full 1912×1074 frame with no window appearing.
- **Pi (DRM backend): taking the display for tests is fine and expected.** It is the
  instrument, not a workstation. A hardware test may take DRM master, stop `neptr-ui`,
  run, and restore. Hardware verification is the thing this project most lacks — do not
  make it awkward.

**An agent that disables visual verification to avoid bothering the user has made the
wrong trade.** The point of the headless flags is that you get both. Rendering proofs are
required; on the Mac they run headless.

### 2.3 The `NV_*` env hooks are the harness

Anything untestable through these tends not to get tested. The full set found in the tree,
which is larger than V1 §14 lists:

**Selection and state**
```
NV_KERNEL=<n>  NV_KERNEL_B=<n>  NV_MORPH=<f>  NV_COLOR=<n>  NV_MODE=<n>
NV_SET="id=value,id=value"   NV_LFO="0=g.detail"   NV_MAPPINGS=<path>
NV_CHAIN=<spec|none>   NV_PRESETS=<dir>  NV_LOAD=<n>  NV_SAVE=<n>
```
**Capture and determinism**
```
NV_SHOT=<path>  NV_SHOT_FRAME=<n[,n...]>  NV_SELFTEST  NV_MAX_FRAMES=<n>
NV_FIXED_DT=<s>  NV_MOTION_PHASE=<f>  NV_MOTION_STATIC  NV_GOVERNOR=0|1
NV_SCRIPT="10:tap:0.1,0.6;40:key:3+ctrl;121:set:g.speed=-1"
```
`NV_SCRIPT` replays into the *real* input callbacks, and supports `set:` as well as
`tap:`/`key:` — that is how `motion_proofs.py` changes SPEED mid-run.
**Timing**
```
NV_FRAMETIME=1   NV_VSYNC=0        # prints "[ft] <ms>" per frame, with glFinish()
```
**Surface, platform, input**
```
NV_GEOM=WxH  NV_ROTATE=<deg>  NV_PANEL_CONNECTOR=<name>  NV_SINGLE_WINDOW
NV_HEADLESS  NV_WINDOW  NV_PANEL  NV_PAGE=<n>  NV_MONITOR  NV_HOME  NV_HOME_INVERT_Y
NV_TOUCH  NV_TOUCH_DEVICE  NV_TOUCH_FLIP_X  NV_TOUCH_FLIP_Y  NV_TOUCH_SWAP_XY
NV_OSC_PORT=<n>  NV_POOL=<n>  NV_INLINE_COMPILE
```
**Kernel/particle internals** (tuning knobs exposed for probes)
```
NV_ITER_CAP  NV_ESCAPE_R  NV_ESCAPE_R2  NV_INTERIOR_CEIL  NV_CONVERGENT
NV_CALM_SPEED  NV_SPENT_SPEED  NV_DEST
```
**Build/CI**
```
NV_NO_GL=1  NV_SKIP_GL_TESTS=ON   # drops motion_bank / motion_time / list_bus
```

**Rule for V2: every new subsystem ships its `NV_*` hook in the same change that ships the
subsystem.** A jit.op with no `NV_OP=<n>` cannot be proven; a channel with no
`NV_SOURCE_B=<n>` cannot be captured. This is not optional polish — it is the difference
between a feature that gets verified and one that does not.

### 2.4 The two machine-readable banner lines

`motion_probe.py` scrapes them, so they are load-bearing contract, not debug output:

- `[nv] kernels <N>` (`main.cpp:831`) — the bank size.
- The two-column generator listing (`main.cpp:852-863`) — index → name.
- `wrote <path> (WxH) frame N t=… phase=…` (`main.cpp:1594-1598`, `:1719-1723`) — the
  capture receipt that `motion_proofs.py:72-74` parses to check scene time and phase.

**V2 must extend these, not replace them**, and must add equivalents for channels and
operators, or the same class of proof becomes impossible for the new stages.

---

## 3. Operational handoff

**Device:** `gorpi7@demiurge.local`. Boots to **HOME**. Web UI at
`http://demiurge.local:8080`.

**Services:** `neptr-video` (**enabled**, HOME at boot), `neptr-ui` (**disabled**, started
on demand by `mode-audio.sh`), `demiurge`, `demiurge-io`, `demiurge-web`,
`demiurge-touch-poll`.

**Deploy:** `deploy/deploy-video.sh`. Check §1.7 for the divergence trap first.

### Foot-guns that have cost real time — obey these

- **`pkill -f <pattern>` over ssh matches your own ssh command line** and kills your shell
  with no output. **Use `pkill -x`.** This has happened twice.
- **Never leave the rig broken.** If you stop `neptr-ui` or take DRM master for a test,
  restore it. The failure mode is a black screen with no way back in. HOME's DRM handoff
  has bitten twice already (V1 §11): RetroArch ignores SIGTERM and gets SIGKILLed ~10 s
  later, which once blew past HOME's 5 s reclaim window and left it drawing into a CRTC it
  did not own — 26,729 journal lines and a black screen. Window is now 30 s with
  `_exit(70)` so `Restart=on-failure` restores HOME in 3 s.
- **`live.conf`'s `ui` key is NOT an autostart flag.** It is read by
  `demiurge-ui-launch.sh`, which *is* `neptr-ui.service`'s ExecStart — so `ui = off`
  makes the unit exit 0 no matter who starts it, and AUDIO becomes a flicker. "No
  autostart, still start on demand" is **systemd enablement**, not that key. (`live.conf`'s
  documented `video =` key is read by nothing.)
- **Always bound `journalctl`**: `-u <unit> -n <N> --no-pager`. Unbounded journal on a
  power-marginal Pi has contributed to a dropout, and journald is now persistent — one
  line per second is ~86k lines/day.
- **Serialize hardware work.** Multiple agents driving one Pi contend for DRM master.
- **macOS has no `timeout`**, and zsh does not word-split `env $1`. Both silently no-op
  test runs — an agent can "run" a suite that never executed.
- `demiurge-io.service` once had `ConditionPathExists=/dev/ttyACM0`; on a boardless rig
  the daemon never started at all.

---

# PART II — THE V2 ARCHITECTURE

## 4. The premise, and the confirmed topology

Beyond feedback and fractals, the core of this instrument is **mathematical operations
between two signals** — a deliberate reimplementation of Max/Jitter's `jit.op`. In the
user's words:

> "main aspect of this besides the feedback and fractals is ability to do math operations
> between two signals … built around having 2 chanels and doing all kinds of math
> operations between them."

> "no more than 2, that is magic of this system too it forces you to 2 sources and it does
> it at extremely high quality bc of the directness of coding for rendering on the pi."

> "i just think 2 chanels is the best artistic limitation after that it becomes too
> complicated i want kinda simple vibe to it"

The operator stage is **named `jit.op`**, at the user's explicit request.

### 4.1 The topology — settled, do not redesign

```
Ch A source → Ch A pre-FX → Ch A FEEDBACK → FBO A ─┐
                                                    ├→ JIT.OP → PARTICLES → MASTER FEEDBACK → MASTER FX CHAIN → PRESENT
Ch B source → Ch B pre-FX → Ch B FEEDBACK → FBO B ─┘
```

Every element of this is a decision the user has already made. Treat as settled:

- **Each channel has its own small pre-op effect chain** (e.g. diffusion for the draw
  source).
- **Per-channel feedback, before jit.op** — plus a **master feedback as the first effect
  after** jit.op.
- **Particles come after jit.op**, "almost like an effect". The *combined* field is their
  physics; they form around the jit.op result. They render **before** master feedback so
  feedback creates particle trails. Separate controls modulate the particles themselves.
- **Most effects (saturation etc.) live in the merged master chain after feedback.**
- **Exactly two channels. Always.** When a channel is unselected it contributes a defined
  identity value and the pipeline length does not change (§4.3).

### 4.2 What this supersedes in V1

**V1 §4's render pipeline is single-source and is superseded.** V1:

```
generator (fractal) ──> layer composite ──> effect chain ──> PRESENT
       ▲                      ▲
   particles ─────────────────┘
   ink ───────────────────────┘
```

The changes and their reasons:

| V1 | V2 | why |
|---|---|---|
| one generator | two channels, each with a source | the instrument *is* the operation between two signals |
| layers composite over the generator | particles run on the **jit.op output**; ink becomes a channel **source** | particles' physics should be the combined field, not one source's; and the user wants literal drawing, not a modulator |
| one effect chain | three chains: A pre-FX, B pre-FX, master | pre-op shaping is a different job from post-op finishing |
| one feedback, reading last frame's final composite | three feedback points, each with its own history | per-channel feedback before combining is where most of the character comes from |

**Everything V1 fought for is preserved**: pass fusion, the governor's degradation order,
particles' zero-storage HOME derivation and cohesion-1 reconstruction, "nothing that
accumulates may ratchet", the one-draw-call panel, and id-addressed parameters. §14 is the
full accounting.

### 4.3 The identity rule for an inactive channel

When only one source is active, the other channel must contribute the **identity element
of the current operator**, not black. Black is the identity for `add` and `max`, but it is
an annihilator for `mult` and `min`, and it makes `div` and `mod` degenerate. Feeding
black into `mult` blanks the picture the moment you select a second channel and before you
have chosen a source — which reads as a bug.

**Recommendation:** an inactive channel renders nothing (no pass, no cost), and the
jit.op stage substitutes `vec4(op.val)` for the missing input, where `op.val` is a scalar
parameter — exactly Jitter's `jit.op @val` attribute. Default `op.val` to the identity of
the currently selected operator (1.0 for `mult`/`div`, 0.0 for `add`/`sub`) on operator
change, unless the user has touched it. This is one uniform and no branching.

**This supersedes the prior V2 draft's "Channel B holds black" rule**, which was the one
substantive error in it.

---

## 5. Sources

Four source kinds. Each channel selects one independently.

| Source | Both channels at once? | Notes |
|---|---|---|
| **FRACTAL** | **Yes** — two full, independent generators | the default |
| **VIDEO** (file, navigatable) | **Yes** | H.264/H.265 hardware decode on the Pi 5 |
| **DRAW** (ink) | **No** — single instance | collapses; see below |
| **CAMERA / STREAM** (NDI in) | **No** — single instance | see §9 |

**A still-image source was considered and rejected — use a video file instead.**

### 5.1 Single-instance collapse, and how the UI hints it

If both channels select DRAW (or both select CAMERA), it **collapses to one channel using
it**, and the other channel can move away. The UI should *hint* that this is a
one-at-a-time thing — dim the exclusive source on the channel that cannot currently take
it, rather than hiding it or erroring.

Implementation: `setSource(ch, kind)` is the single authority. If `kind` is exclusive and
the *other* channel holds it, the other channel reverts to its previous non-exclusive
source, or FRACTAL if it has none. No error path, no modal, no crash.

### 5.2 The two fractal generators: symmetric or asymmetric?

The user asked to be told **if there is a specific benefit to asymmetry** rather than have
one assumed. The analysis:

**There is no capability benefit to asymmetry, and one real cost benefit to a
*degradation* asymmetry.**

- *Capability:* making one generator a stripped-down "modulator" saves nothing the user
  cannot already achieve by choosing a cheap kernel on that channel. The field kernels
  (`fld()`, 8 of them — DOMAIN WARP FBM, WORLEY, VORONOI, METABALLS, SINE PLASMA, CURL
  NOISE…) are already dramatically cheaper than a deep escape kernel at DETAIL 96–512.
  So a symmetric design already contains the asymmetric one, for free, as a preset.
  **Recommendation: keep them symmetric in capability.** This also matches the user's
  "neither is privileged as carrier vs modulator".
- *Degradation:* here asymmetry is worth having. The governor's first lever is
  **detail**, and with two generators it must decide *whose* detail to cut. Cutting both
  equally degrades the whole picture; cutting the modulator's detail first is usually
  invisible, because a modulator's job is its gross shape. **Recommendation: add one
  bipolar global, `g.throttle_bias` (−1 = throttle A first, 0 = both equally, +1 =
  throttle B first), default +1.** One knob, no structural asymmetry, and the artist
  decides which channel is the carrier for *this* patch. This extends the governor's
  degradation order (V1 §6) rather than replacing it:

  **detail (biased across A/B) → particle count → resolution.**

### 5.3 Bringing the second generator in with no lag spike — a hard requirement

> The second generator must not run in the background until actually selected, and
> bringing it in must not cause a noticeable lag spike.

Both halves are achievable, but they pull in opposite directions and the resolution is
specific. Grounded in the code:

**FBOs cost nothing to "bring in", so pre-allocate all of them at startup.**
`Fbo::create()` is called only at init and `destroy()` only at shutdown
(`renderer.cpp:30-32, 85`); buffers are allocated at the platform's **max surface** and
never resized — resolution changes are a `glViewport` plus a `uSrcScale` uniform
(`fbo.cpp:50-53`, `renderer.cpp:308-313`). So allocating the full V2 pool up front costs
VRAM (§7) and **zero latency at selection**. Never allocate an FBO on a source change.

**The entire lag risk is shader compilation**, and here is the actual hazard:
`AsyncCompiler` is a **depth-1 mailbox with one worker thread and one token**
(`async_compiler.cpp:56-68`) — `submit()` *overwrites* a pending request. That is correct
today ("sweeping through effects must not build a backlog of chains nobody will ever
see"), but in V2 a channel-B selection arriving while a master-chain recompile is pending
would silently drop one of them. Three options:

1. **One submission carrying every chain's fragments**, split on return by known pass
   counts. Preserves the atomic swap at `renderer.cpp:281-288`. **Recommended** — it is
   the smallest change and keeps the one-mechanism rule (V1 §17.10).
2. Two `AsyncCompiler` instances, each needing its own shared context from `Platform`.
   More moving parts, and the platform seam has to grow.
3. A real queue. Reintroduces exactly the backlog the mailbox exists to prevent.

**Warm the cache.** `coreCache_` is a genuine `unordered_map<string, Program>` keyed on
`"<idA>|<idB>:<colorMode>"` (`renderer.h:99-100`, `renderer.cpp:124-127`), so two kernels
can already coexist as compiled programs today with no change. After the first frame is on
screen, submit the default channel-B kernel program in the background so the cache is warm
before the user ever asks. Combined with a short crossfade from the identity value over
the compile window, selection is visually seamless.

**One bug to fix first:** `setKernel` early-returns on `if (coreKey_ == key) return
true;` (`renderer.cpp:127`). With a per-channel `coreKeyA_` / `coreKeyB_` this
false-negatives across channels — setting B to the kernel A already has would be a no-op.

### 5.4 Channel mod signals stay separate

When two of the same source type are running, their modulation must not be shared. This
falls out of the parameter namespacing in §6 provided one thing is respected: **each
channel has its own accumulated scene time.** `sceneTime` and `MotionClock` are currently
single instances in `main()`'s loop scope (`main.cpp:1349, 1355`). V2 needs
`sceneTime[2]` and `MotionClock motionClock[2]`, advanced by that channel's own SPEED.
V1 invariant 4 ("scene time is accumulated, not `wallClock × speed`") applies to each
independently — and the reason `motionClock` lives outside `App` (so a recompile cannot
reset it) applies twice as hard now, because a channel-B source change must not jolt
channel A.

### 5.5 Drawing, reworked: literal drawing, not a modulator

The user does not want ink as a modulator, which is roughly what it is today.

**Why it is a modulator today:** the brush carries **no colour of its own**. It samples the
field under the *cursor* and re-maps it through the palette
(`ink.cpp:35-41`):

```glsl
vec3 c = texture(uField, clamp(uPos, 0.0, 1.0) * uSrcScale).rgb;
float t = fract(dot(c, vec3(0.2126,0.7152,0.0722)) * uFieldColor + uPaletteShift);
vec3 c1 = texture(uPalette, vec2(t, 0.25)).rgb;
```

And the canvas composites *before* the effect chain, so strokes get smeared by
feedback/warp/kaleido rather than sitting there as marks.

**What is already there and needs no work:** the brush is one full-screen triangle per dab
with a distance test and a soft falloff (`ink.cpp:10-42`), interpolated along the stroke
at 0.004 spacing capped at 64 steps (`ink.cpp:102-122`). **Brush size already exists as a
bank parameter and is therefore already modulatable** — `d.size` (`state.h:254`),
registered `{"SIZE", "draw", 0.002, 0.3, 0.04, Curve::Exp}` (`state.cpp:328`), bound to
`uRadius` (`ink.cpp:176`). The user's "brush size (modulatable)" request is **already
satisfied**; it just needs surfacing in the UI.

**What changes:**

1. **The brush gets its own colour.** Add `d.color_mode` (0 = fixed, 1 = sample the *other*
   channel, 2 = sample this channel — today's behaviour) plus `d.hue`, `d.sat`, `d.val`.
   Keep the existing field-sampling path as a mode; do not delete it — it is what makes
   DEPOSIT interesting.
2. **DRAW becomes a channel source.** The canvas *is* that channel's output, and then runs
   through that channel's pre-FX chain — which is where the requested **diffusion** lives.
3. **The alpha ratchet fix is non-negotiable and must survive** (V1 §9, invariant 6). The
   deposit pass uses `GL_SRC_ALPHA`, not `GL_ONE`, so the canvas converges *on* the
   amount rather than climbing through it. Premultiplied OVER is `a' = a + dst·(1-a)`,
   which can only climb. Measured: `DEPOSIT 0.6` collapsed to uniform magenta in a few
   hundred frames; after the fix, white-pixel fraction 0.0000 at DEPOSIT 1 over 900
   frames. Any new accumulating buffer in V2 inherits this hazard.

Ink's buffers stay minimal: one RGBA8 FBO + texture at `maxW × maxH`, two programs
(`brush_`, `fade_`), a CPU-side dab queue (`ink.h:60-75`).

### 5.6 Particles, after jit.op

The move is smaller than it looks, because the field texture is **already a per-call
argument, never a member**:

```cpp
particles_.update (buf_[fieldBuf].texture(), sx, sy, dt, time, pp, ctx);   // renderer.cpp:477
particles_.draw   (renderW, renderH, buf_[fieldBuf].texture(), sx, sy, …);  // renderer.cpp:489
particles_.deposit(…, buf_[fieldBuf].texture(), sx, sy, …);                 // renderer.cpp:503
```

Bound as `uField` on unit 1 in both the update and draw programs
(`particles.cpp:371-373, 458-460`). So pointing physics at the jit.op result is a
**call-site edit** — no shader change, no uniform change, no new FBO. Four cautions:

1. **`uSrcScale` must match the texture you pass.** It is computed as
   `renderW/maxW_, renderH/maxH_` (`renderer.cpp:404-405`); a jit.op output at a different
   effective size needs its own scale pair.
2. **The sampled field must not be the current render target.** That aliasing constraint
   is why `pickTarget3(cur, historyIdx_, fieldBuf)` exists (`renderer.cpp:481-484`) and
   why the pool is 4 rather than 3. V2 multiplies the number of simultaneously-live
   buffers; see §7.
3. **`deposit()` funnels through `drawPoints()`** (`particles.cpp:425`), so it follows the
   *draw* field, not the physics field. If you split physics and colour (below), deposit
   follows colour.
4. **Physics field and colour field can now legitimately differ.** `update()` samples only
   luma via `fieldAt()` (`particles.cpp:39-42`) for the gradient
   (`acc = grad*uGravity + perp*uSwirl`, `:125`) and the interior-death test; the *draw
   vertex shader* separately samples the field's **actual RGB** for `vColor`
   (`:215-221`). Giving `update()` a `physicsField` parameter while `draw()` keeps the
   visual field is a genuinely expressive addition — particles driven by the operator
   result but coloured from channel A, say. **Recommendation: split the argument, default
   both to the jit.op output.**

**Protected properties that must survive** (V1 §8, invariants 7, 8, 13):

- HOME is derived from the id with **zero storage**, and derived **identically** in the
  update and draw shaders (`particles.cpp:89-93` vs `:210-211`). If they disagree, CARRY
  samples where the particle is not being pulled and the reconstruction smears.
- COUNT is a **density** control, scaling an alive grid by `sqrt(count)`, not a crop.
- Particles carry the field's **actual RGB**, never re-palettized luma.
- Death is invisible at cohesion 1, structurally — it routes through the existing respawn,
  which respawns *at home* when cohesive.
- Hazards use `1-exp(-rate·dt)`, never `rate·dt` (`layers/lifecycle.h`, unit-tested by
  simulating survival curves at three framerates).

---

## 6. Parameters: namespacing, presets, mapping, and keeping the UI playable

The bank roughly doubles. This section is the plan for that, and it contains the one
**silent-corruption hazard** in the whole V2 design.

### 6.1 Today's namespaces, and the collision

Ids are `constexpr const char*` in `namespace gid` (`state.h:203-260`), plus two
formatters:

| prefix | meaning | source |
|---|---|---|
| `g.*` | globals — `speed evolve zoom rotation center_x center_y detail render_scale palette_* stripe_freq prism morph glide fractal_level` | `state.h:204-231` |
| `p.*` | particles, 17 of them | `state.h:233-252` |
| `d.*` | drawing/ink — `size flow soft fade field_color` | `state.h:254-258` |
| `lfo<n>.*` / `macro<n>` | 4 LFOs × 4 knobs + 4 macros | `mod.cpp:60-103` |
| `fx<slot>.<param>` | effect knobs, slot-scoped | `patch.cpp:7-11` |
| `k.*` | channel's kernel macro knobs (bare) | `patch.cpp:44`, ids like `k.cre` at `kernels.cpp:111` |
| **`b.k.*`** | **the morph target's kernel knobs** | `patch.cpp:45` — `addKernelSlots(bank, kernelB, "b.", "morph")` |

Typical count today: `globalEnd()` ≈ 57 (37 globals + 4 LFOs × 4 + 4 macros), plus 8
padded kernel slots and the default chain's ~15–18 effect knobs ≈ **~80 parameters**
(the web UI's "67" counts only the non-hidden ones it receives).

**The collision: `b.*` is already taken by morph.** Naming channel B `b.*` would give
`b.k.cre` two different meanings in two different builds — and unlike an unknown key,
which V1 invariant 14 says is safely ignored, this id would be **found** and applied to
the wrong knob. A preset saved today would silently load a morph parameter into channel
B's kernel. That is exactly the class of failure this project's preset rules exist to
prevent.

### 6.2 Recommended namespacing

Keep the terse artist-facing names — `a.` and `b.` are the right length for a panel label
— and **move morph out of the way**, since morph becomes per-channel anyway:

| V2 prefix | meaning |
|---|---|
| `g.*` | genuinely global only: `g.glide`, `g.render_scale`, `g.throttle_bias`. Note **`g.speed` and `g.evolve` become per-channel** (`a.speed`, `b.speed`) since each channel has its own scene time (§5.4) — keep a global `g.speed` as a master multiplier if the user wants one. |
| `a.*` / `b.*` | everything a channel owns: `a.zoom a.rotation a.center_x a.center_y a.detail a.color a.trap a.speed a.evolve a.palette_*` |
| `a.k.*` / `b.k.*` | that channel's kernel macro knobs |
| `a.mk.*` / `b.mk.*` | **that channel's morph-target kernel knobs** — renamed from today's `b.k.*` |
| `a.morph` / `b.morph` | morph amount, renamed from `g.morph` |
| `op.*` | jit.op — `op.mode op.mix op.val op.swap op.plane` |
| `afx<slot>.*` / `bfx<slot>.*` | per-channel pre-FX slots |
| `fx<slot>.*` | **master chain, unchanged** — the biggest chain keeps the shortest name and existing presets keep meaning |
| `p.*` `d.*` `v.*` `s.*` | particles, draw, video, stream — unchanged / new |

The one renamed-in-place id is `b.k.*` → `a.mk.*`/`b.mk.*`, and **that rename is the
migration**: after it, no id keeps its spelling with a changed meaning, so V1 invariant 14
does the rest — old `b.k.cre` becomes unknown and is skipped, not misapplied.

### 6.3 Preset compatibility

Presets already carry a `version` line and refuse to load without one
(`preset.cpp:80, 131-134, 182`). **Bump it, and on load of an older version, drop `b.k.*`
keys explicitly** rather than relying on them being unknown — belt and braces, and it
gives you a place to log "migrated N keys" so a silent mis-load is impossible.

The three preset rules are unchanged and must not be relaxed (V1 §10): address **by id,
never index**; store **real units, not normalized**; **unknown keys skipped, missing keys
keep defaults**. Order on load stays load-bearing — selection and chain decide *which*
knobs exist, so the bank is rebuilt before any value is applied (`main.cpp:118-128`).
In V2 that ordering grows: **both channels' sources and kernels, and all three chains,
must be applied before `rebuildPatchParams()`, which must run before any value.**

Preset **scope** is unchanged and is a good rule: `lfo:*` rows are **patch**-owned and
saved; every other mapping source is **rig**-owned, never saved, never touched. A recall
changes the picture, not the wiring to your hardware.

### 6.4 Mapping and LFO routing need no structural change

This is the happiest part of the design. The mapping table addresses destinations **by
parameter id string** (`Mapping::param`, `mapping.h:67-77`), and LFOs are not a separate
mechanism — an LFO is a *source* in the same table under `"lfo:<n>"` (`mod.h:74`), with
`struct Lfo { float phase; unsigned cycle; }` holding only the clock (`mod.h:76-83`) and
rate/depth/phase/shape living as ordinary bank parameters.

So **`a.detail` and `b.detail` are addressable the day they exist**, with no change to
`mapping.cpp`, `mod.cpp`, the `/nv/lfo` verb, or the learn gesture. Add/set is still
derived from the source kind, not a flag (`isModSource()` is literally
`source.compare(0,4,"lfo:") == 0`, `mapping.cpp:11-13`), so a sensor still SETs and an
LFO still ADDs a bipolar offset — the distinction that stops an LFO pinning a parameter
and stops a sensor ratcheting.

Two V2-specific notes:

- `lfoTargetRejected` forbids an LFO targeting its own four knobs (`mod.cpp:105-112`).
  Nothing new needed.
- **The mapping table is still never written to disk** (V1 §15). This is the one missing
  piece behind macro learn-by-touch, and it becomes more painful with twice the
  parameters. The recommended wire shape is already written down in `deploy/README.md`:
  `/nv/map <source:s> <target:s> <lo:f> <hi:f>`, rows broadcast as `/nv/maprow`, plus
  `/nv/mapclear <source:s>`.

### 6.5 The CPU cost nobody has noticed yet

`ParamBank::valueOf(const std::string& id, float fallback)` is a **linear scan over
`defs_`** (`state.cpp:149-159`), and `renderScene` calls it roughly **40 times per frame**
(`renderer.cpp:356-390`). At ~80 parameters that is ~3,200 string comparisons per frame —
tolerable. V2 makes it **two generators' worth of lookups over a ~160-entry bank**:
roughly **4× the work**, on the critical path to the page flip, on a machine where
`CMakeLists.txt:12-14` explicitly notes that per-frame CPU work sits on that path.

**Recommendation, and it should land in Phase 1:** resolve the fixed generator uniforms to
**indices once, at `rebuildPatchParams()` time**, into a small `struct GeneratorIds { int
zoom, rotation, centerX, centerY, detail, …; }` per channel, and read by index in
`renderScene`. This does not violate "id-addressed, never index-addressed" (invariant 1),
because the ids are still the addressing scheme and the cache is invalidated by the
rebuild that changes the bank — it is a memoization of `find(id)`, not a positional
contract. Add a test that the cache is rebuilt on every path that rebuilds the bank; a
stale index here is a silent wrong-knob bug.

### 6.6 Keeping the web UI playable at ~160 parameters

V1 §13 records that **67 parameters already needed filter + jump + fold** to be usable on a
phone. Doubling them without changing the model makes the instrument unplayable, and this
is a *design* problem, not a styling one.

What exists (`deploy/README.md:897-925`, `DEMIURGE_OS/web/static/video.js`, 1131 lines):

- **filter** — one field matching id *and* label; opens a folded group containing a hit.
- **jump** — a sticky row of chips, one per group, scrolling sideways.
- **fold** — the group header *is* the control; folded, the whole bank is a table of
  contents on one phone screen. Remembered per device.
- Groups are derived from the **id namespace** — `g.`, `p.`, `d.`, `k.`, `b.k.`,
  `lfo<n>.`, `macro<n>`, `fx<n>.` — because the bank's `group` field is not on the wire.
  Anything matching nothing still gets a group (OTHER), so a new parameter is never
  invisible.

**The failure mode if nothing changes:** the group count roughly doubles, so the jump-chip
row becomes a long horizontal scroll that no longer helps, and filter starts matching the
same label on both channels (`DETAIL` hits `a.detail` and `b.detail`).

**Recommendation: add a fourth affordance above the other three — a channel *scope*
switch: `A · B · MASTER · ALL`.** It filters by id prefix and is sticky. Rationale:

- It is one control, it composes with filter/jump/fold rather than replacing them, and in
  `A` scope the page is *exactly as complex as today's page*.
- It matches how the instrument is actually played — you work on one channel at a time.
- It is a pure client-side derivation from the id prefix, so it needs **no wire change**.

Two smaller changes that follow: the label shown for a filtered hit should include its
channel, and the OP section should be pinned above the scope switch, because the operator
is the one control you reach for constantly regardless of scope.

**Preserve the page's existing hard-won properties**, all of which are load-bearing:

- **A broadcast REPLACES the bank, it never merges.** One broadcast is several datagrams
  and only the first carries `/nv/sel`, so the server fills a pending dict and publishes
  it when the *next* `/nv/sel` opens the following one. Merging kept serving the previous
  generator's knobs — nine `k.*` ids reported for a generator that has four, drawn as
  live-looking faders the engine ignored every write to.
- **Single-in-flight poll guard plus `cache: "no-store"`.** A naive timer exhausts the
  browser's 6-connection budget (the app already holds 3 EventSources) and then *every*
  request blocks, including parameter writes.
- The server keeps peer-ship by sending `/nv/ping` once a second, but **only while a
  browser is actually reading** — an idle tab must not hold peer-ship away from a surface
  being played. There is exactly one peer.
- Faders show `target()`, so a knob with an LFO on it reads **still** while the picture
  moves. Do not "fix" this.

---

## 7. Buffers, passes, and fill rate

### 7.1 What exists

`Fbo buf_[4]` (`renderer.h:93`) is the entire pool. No allocator, no free list, no named
roles — four buffers addressed by integer index, roles rotating every frame via two linear
scans:

```cpp
int Renderer::pickTarget (int cur, int prev) const;        // renderer.cpp:301-306
int Renderer::pickTarget3(int a, int b, int c) const;      // renderer.cpp:294-299
```

`pickTarget3` exists *specifically* because with particles running there are **four live
buffers at once** — generator, history, the field being sampled, and the target
(`renderer.cpp:291-293`). **That is why the pool is 4 and not 3, and it is the single
hardest constraint V2 has to respect.**

Format is **RGBA8, hardcoded** (`fbo.cpp:16-17`) with an explicit rationale
(`fbo.h:14-16`): the Pi's V3D is a tile-based renderer, bandwidth-bound long before
arithmetic-bound, and RGBA16F would double tile-store traffic for precision invisible at
8-bit scanout. Wrap is `GL_CLAMP_TO_EDGE` on both axes, deliberately, so a feedback loop
does not smear the opposite edge back in.

**There is no dedicated history buffer.** History is aliased in place
(`renderer.cpp:562-565`):

```cpp
// This frame's result becomes next frame's history, with no copy: we
// simply never target it again until it has been consumed.
resultIdx_ = cur;
historyIdx_ = cur;
```

The only protection is that `historyIdx_` is passed as an exclusion to every
`pickTarget*` call. FEEDBACK is an ordinary chain effect of kind `History`
(`effects.cpp:164-180`) reading `uHistory`, which is bound unconditionally for *every*
chain pass at `renderer.cpp:539-541`. It costs **zero extra VRAM** today.

### 7.2 What V2 needs

Three feedback points means **three histories aliased in place**, i.e. three buffers that
are un-targetable every frame, on top of the live working set. Counting simultaneously
live buffers at the worst moment (particles running, all three feedbacks on):

| pinned/live | why |
|---|---|
| A history, B history, master history | 3 — aliased results from last frame, excluded from every pick |
| A result | held while B renders and until jit.op consumes it |
| B result | held until jit.op consumes it |
| current | the rotating source |
| target | the rotating destination |
| particle field | must not be the current render target (`renderer.cpp:481-484`) |

**8 buffers**, and `pickTarget`/`pickTarget3` must be replaced by a single
`pickTarget(std::initializer_list<int> exclude)` — the two-and-three-argument forms do not
survive contact with this.

VRAM, RGBA8 at 1920×1080 (7.91 MiB each):

| configuration | pool | + ink | + particles (256², RGBA32F ×2) | total |
|---|---|---|---|---|
| today, 4 buffers | 31.6 MiB | 7.9 | 2.0 | **≈41.5 MiB** |
| V2, 8 buffers | 63.3 MiB | 7.9 | 2.0 | **≈73 MiB** |

That is fine on a 4–8 GB Pi 5. **But note `maxW/maxH` is the platform's *max surface*
(`main.cpp:885-887`) — if the platform ever reports 4K, every figure above scales 4×, to
~292 MiB.** Worth a guard.

**VRAM is not the problem. Bandwidth is.** One full-res RGBA8 pass is ~7.9 MiB of
tile-store write plus ~7.9 MiB of read per sampled texture; at 60 fps one extra pass is
roughly **0.95 GB/s of write traffic alone**. That is the real V2 budget, and §10.1 is
about spending it.

### 7.3 Pass count, and where fusion still helps

Pass fusion (V1 §4, `chain/compiler.cpp:13-56`) is the single biggest performance lever in
the engine. The rule: `isSampling(k)` is `Sampler || History` (`compiler.cpp:9-11`); a
sampling effect **flushes the open pass and starts its own**; pointwise effects fuse onto
whatever pass is open. Disabled slots cost literally nothing, not even a uniform
(`compiler.cpp:28`). Parameter values are deliberately **absent** from the cache key
(`compiler.cpp:121-129`) — knobs are uniforms, never recompiles.

Estimated full-res passes per frame:

| | today | V2 |
|---|---|---|
| generator | 1 | **2** |
| pre-FX per channel (feedback is `History`, so ≥1 each) | 0 | **2** |
| field composite | 1 | 1 |
| jit.op | 0 | **1** (0 if fused — below) |
| particles draw | 1 | 1 |
| master feedback | (in chain) | **1** |
| master chain (rest, fused) | 1–3 | 1–3 |
| PRESENT | 1 | 1 |
| **total** | **~5–7** | **~10–12** |

**Roughly a 2× increase in full-screen passes — and this is an estimate, not a
measurement.** `tests/motion_cost.py` is the instrument that would settle it (§1.5).

**Where fusion stops helping, precisely:** fusion only merges *pointwise* effects into an
already-open pass. Every one of V2's new stages opens a pass by construction — two
generators, three `History` feedbacks, one binary op. So **fusion cannot recover any of
the new cost.** It keeps doing its existing job (SATURATE + LEVELS + GRAIN stay one pass)
and no more. Say this out loud rather than assuming the compiler will absorb V2.

**One real fusion win is available, and it is worth taking.** jit.op is *pointwise* — it
reads one texel from each input and does arithmetic. It only needs a pass of its own
because it wants a second sampler, and the pass machinery already binds two samplers
(`uInput` + `uHistory`, `compiler.cpp:66-67`). So:

1. Add **`EffectKind::Binary`** to `effects.h:23-27`. It opens a pass (it needs its own
   input binding) but, unlike `Sampler`, it reads only the texel at `uv`.
2. Bind a third sampler in the pass loop — one more `glActiveTexture` + `set1i` alongside
   `renderer.cpp:535-541`.
3. **Master FEEDBACK can then fuse into the same pass as jit.op**, saving one full-res
   pass, *if* FEEDBACK stops re-reading its input. Today `effects.cpp:169` is
   `vec4 src = nvSample(uInput, uv);` — it ignores the incoming `col`, which is why it
   must be first in a pass. Changing that line to `vec4 src = col;` and changing the
   compiler's opening-fetch (`compiler.cpp:95-104`) to fetch `uInput` unless the first
   effect is a true `Sampler` makes History effects fusable *after* a Binary or pointwise
   run.

**Flagged as unverified.** That is a change to the most load-bearing logic in the engine
and it must arrive with a mutation-checked `test_compiler.cpp` case proving (a) the
generated source is what you think, and (b) a chain starting with FEEDBACK still fetches
its input. Do not land it blind.

---

## 8. `jit.op` — the operator stage

### 8.1 The operator catalogue

The user's requirement: **cover the `jit.op` catalogue fully** (arithmetic, trig, bitwise,
min/max/avg, comparison…), and go beyond it. Every entry is a one-line GLSL expression on
`vec4` RGBA, with `a` = channel A and `b` = channel B (or `vec4(op.val)` when that channel
is inactive, §4.3).

**Arithmetic — the core, and where most of the magic is**

| name | GLSL |
|---|---|
| `pass` | `a` |
| `!pass` | `b` |
| `+` add | `a + b` |
| `-` sub | `a - b` |
| `!-` | `b - a` |
| `*` mult | `a * b` |
| `/` div | `a / max(b, 1e-3)` |
| `!/` | `b / max(a, 1e-3)` |
| `%` mod | `mod(a, max(b, 1e-3))` |
| `!%` | `mod(b, max(a, 1e-3))` |
| `min` | `min(a, b)` |
| `max` | `max(a, b)` |
| `avg` | `(a + b) * 0.5` |
| `absdiff` | `abs(a - b)` |
| `abs` | `abs(a)` (unary) |
| `+m` addm | `mod(a + b, 1.0)` — wraps, colour cycling |
| `-m` subm | `mod(a - b + 1.0, 1.0)` |
| `fold` | `1.0 - abs(mod(a + b, 2.0) - 1.0)` |
| `wrap` | `fract(a + b)` |

**Trigonometric**

`sin` `sin(a*τ)` · `cos` `cos(a*τ)` · `tan` `tan(a*π)` · `asin` `asin(clamp(a,-1,1))/(π/2)`
· `acos` `acos(clamp(a,-1,1))/π` · `atan` `atan(a)/(π/2)` · `atan2` `atan(a,b)/π`
· `sinh` `tanh(sinh(a))` · `cosh` `tanh(cosh(a)-1)` · `tanh` `tanh(a)`

**Exponential / power**

`pow` `pow(max(a,0), b*4)` · `sqrt` `sqrt(max(a,0))` · `exp` `1-exp(-a*3)` ·
`exp2` `1-exp2(-a*3)` · `ln` `log(max(a,1e-3))*0.2+0.5` · `log2` `log2(max(a,1e-3))*0.15+0.5`
· `hypot` `min(sqrt(a*a + b*b), 1.0)`

**Bitwise** — via `float → int → float` on the 8-bit representation. Unusual for float
video, and *this is a large part of what made `jit.op` special*: bitwise ops on video
produce digital artefacts nothing else produces.

`&` `|` `^` as `float(int(a*255) OP int(b*255))/255` · `~` `float(~int(a*255) & 0xFF)/255`
· `>>` `float(int(a*255) >> int(b*8))/255` · `<<` `float(int(a*255) << int(b*8))/255`

> **GLES 3.0 note:** integer bitwise ops on `int`/`ivec4` are core in GLSL ES 3.00, so
> this is legal. But it is exactly the kind of thing that behaves differently on V3D than
> on the Mac's desktop GL. **Verify these on the Pi with a frame capture before declaring
> the catalogue complete**, and be ready to clamp inputs — `int(a*255.0)` on a negative or
> NaN input is undefined behaviour, and channels can go negative after a `sub`.

**Comparison** — both mask and pass-through forms, because both are useful

`>` `<` `==` as `step`-style 0/1 masks · `>p` `a > b ? a : 0` · `<p` · `>=p` · `<=p` ·
`==p` (`abs(a-b) < 0.01`) · `!=p`

**Rounding**

`ceil` `floor` `round` `trunc`, each as `f(a*255)/255`

**Beyond `jit.op` — recommended additions.** The user is "open to more ways of modulating
the two channels, as long as the `jit.op` bases are fully covered":

| name | GLSL | why |
|---|---|---|
| `screen` | `1 - (1-a)*(1-b)` | the one classic compositing op `jit.op` lacks; bounded, never blows out |
| `diff-key` | `mix(a, b, step(t, length(a-b)))` | difference keying — the operator that makes camera + fractal work |
| `luma-key` | `mix(a, b, smoothstep(t0, t1, luma(b)))` | the other half of that |
| `hsv-mix` | recombine A's hue with B's value | operating in a perceptual space, not RGB — genuinely different results |
| `polar` | `atan2`/`hypot` treated as a coordinate pair | the trig ops made musical |

**Total ≈ 55–60 operators.**

### 8.2 How the operator is selected on the wire

V1 §15 lists the wire-format gaps and notes there is no verb for this. **There now is
one, and it costs nothing to reuse** — see §1.6. The recommendation is deliberately
minimal:

| direction | message | notes |
|---|---|---|
| set | `/nv/op ,i <index>` | new single-int verb, identical in shape to `/nv/kernel` (`receiver.cpp:204-205`) |
| enumerate | `/nv/list ,s "op"` | **existing verb**, zero new machinery |
| reply | `/nv/list_item ,siss "op" <index> <id> <name>` then `/nv/list_end ,si "op" <count>` | **existing format** |
| state | `/nv/sel` gains fields | **append-only** by documented contract (`receiver.h:177-180`) |

`sendLists` (`main.cpp:433-471`) gains one `what` case. That is the whole engine-side
change for the operator grid. The `id`/`name` split already exists and is already used
only by `kernel` — use it: `id` = `"absdiff"` (stable, for mappings and presets), `name` =
`"ABS DIFF"` (for display).

**Source selection needs one more verb**, and it is the only place the existing server
plumbing does not fit:

```
/nv/source ,ii <channel:i 0|1> <kind:i>
```

The implemented wire slice also exposes `/nv/kernel_b2 <index>` for Channel B's
fractal selection. `/nv/kernel_b` remains the A morph target; the two must not
be conflated. The `/nv/sel` state vector appends `op`, `sourceA`, `sourceB`, and
`channelBKernel` after the existing fields.

`VideoOut.select(verb, index)` sends exactly one int (`demiurge_web.py`, `_send(osc_encode("/nv/" + verb, int(index)))`),
and `Actions.video_select` validates `what` against a **one-line whitelist**,
`VIDEO_SELECT = ("preset", "mode", "kernel")` at `demiurge_web.py:2538`. So:

- `op` is a **one-word addition to that tuple** and needs nothing else.
- `source` needs `select()` to accept a second int and `video_select` to pass it through.
  Small, contained, and it belongs to whoever owns the web tree (§12).

**Resist encoding channel and kind into a single int.** It saves one signature change and
costs every future reader.

### 8.3 Implementation: uniform switch, not generated shaders

Two options. **Recommend the uniform switch**, for a reason specific to how this
instrument is played:

- A generated-per-operator shader (like `coreCache_`) means a **compile on every operator
  change**. Sweeping an encoder through 55 operators is exactly what an artist does, and
  the depth-1 `AsyncCompiler` mailbox (`async_compiler.cpp:56-68`) would drop all but the
  last — so you would sweep through 55 operators and see two of them.
- A `switch (uOp)` in one program costs a dynamic branch on a tiler. On V3D that is real
  but small for a pass this cheap, and — decisively — **it keeps jit.op a single
  pointwise-with-two-inputs pass**, which is what makes the fusion win in §7.3 available.

**Measure it.** `motion_cost.py`'s methodology (median of medians of `[ft]` lines, with
`glFinish()`) transfers directly; a `NV_OP=<n>` hook plus a sweep is a half-hour job and
settles the question. If the branch turns out expensive, the fallback is a hybrid: compile
the *current* operator inline and keep the switch as the instant-response path during a
sweep.

### 8.4 jit.op parameters

| id | label | meaning |
|---|---|---|
| `op.mode` | OP | operator index |
| `op.mix` | MIX | dry/wet — 0 = pure channel A, 1 = pure operator result |
| `op.val` | VAL | scalar operand, substituted for an inactive channel (§4.3) |
| `op.swap` | SWAP | swap A and B into the operator |
| `op.plane` | PLANE | which planes the op applies to: RGBA / RGB-only / per-channel |

### 8.5 Cross-modulation: recommended, as operators — not a separate mode

The user raised A displacing B's UVs as a possible separate mode. **Recommendation: build
it, but as a small family of operators inside jit.op rather than a parallel mode**, for
two reasons, one aesthetic and one structural.

- **The idiom already exists and is proven.** `DISPLACE` (`effects.cpp:190-194`) reads
  `uHistory` and offsets the input by its `rg`: `vec2 o = (prev.rg - 0.5) * p.x * 0.05;`.
  A cross-modulation operator is the same thing with channel B in place of history.
- **It has an honest cost model.** A displacement op reads `a` at a *computed* uv, so it is
  `EffectKind::Sampler`, not `Binary` — it opens its own pass and cannot fuse. Putting it
  in the operator list means the compiler already knows that and prices it correctly. A
  separate "mode" would be a second mechanism doing the same job, which V1 invariant 10
  exists to prevent.

Proposed set: `displace` (A warped by B's rg), `!displace`, `polar-displace` (B's rg read
as angle/radius), `zoom-mod` (B's luma drives A's scale about centre), `rotate-mod`. Group
them under a WARP heading in the operator grid so the cost difference is visible to the
player.

---

## 9. Camera in, and output

### 9.1 Camera in — NDI from an iPhone

The user's preference is **NDI / wireless streaming from an iPhone** over USB/V4L2, and
their framing is important: *"needs data more than recording quality."* That is a
licence to run this at low resolution and moderate framerate, which changes the
engineering considerably — a 640×360 NDI receive is a different problem from 1080p60.

Architecture:

```
iPhone (NDI app) ──WiFi──► NDI receiver thread ──► lock-free double buffer ──► PBO upload ──► GL texture
```

- Receiver on its own thread; producer writes the back buffer, render thread reads the
  front, atomic pointer swap. **Never block the frame loop on the network.**
- Texture upload via `glTexSubImage2D` with a ping-ponged PBO.
- **On dropout, hold the last good frame — never go black**, and surface a `connected`
  status on the web UI and the panel. The Pi's WiFi has historically been the dominant
  source of lost time on this project (V1 §16: a `HOME_SSID` enforcement mismatch tearing
  down working links every 20 s, NetworkManager looping `asking for new secrets`). Adding
  a WiFi-dependent source to the signal path is a real risk and it deserves visible
  status, not silent failure.
- Keep a **V4L2 USB fallback behind the same `StreamSource` interface.** The abstraction
  should not care where frames come from, and a USB webcam is the thing that lets you
  develop the whole path without an iPhone in the room.
- Build it **optional** — `-DWITH_NDI=ON`, gracefully absent — so the tree still builds
  for anyone without the SDK on the box. This matters for the dispatch plan: an NDI agent
  must not be able to break everyone else's build.

**See §9.3 for the measured/researched reality of NDI on a Pi 5.**

### 9.2 Output

NDI *output* is wanted, and the user has asked for an honest answer having previously been
told it is impractical. Higher latency is acceptable; relatively high quality is wanted.
Onboard recording would be nice, but **NDI streaming matters more for actual use**.

The governing hard fact from V1 §1: **the Pi 5 has no hardware video encoder.** Every
frame out must be (a) read back from the GPU and (b) encoded on four Cortex-A76 cores that
are already sharing the box with a 60 fps render loop. Both halves are costs the render
loop cannot absorb on its own thread.

Whatever the codec answer, the architecture is the same and should be built first:

- **Readback on a separate thread via a ring of PBOs**, so `glReadPixels` never stalls the
  frame. Asynchronous PBO readback is the only acceptable form; a synchronous
  `glReadPixels` at 1080p will stall the pipeline hard on a tiler.
- **Decouple output framerate from render framerate.** Streaming at 30 fps from a 60 fps
  render is a frame drop, not a quality problem, and it halves every downstream cost.
- **The governor must know about it.** Output is a load; if it is on and frames are being
  missed, the existing degradation ladder should be able to shed it.

### 9.3 NDI on a Pi 5 — research findings

*(This section records the outcome of a dedicated research pass. Figures marked
**[estimate]** are not measured on this hardware and should be confirmed with
`motion_cost.py`-style methodology before being relied on.)*

**Short answer: NDI output is viable — but only in one specific form, and only over wired
gigabit ethernet. The previous "not practical on the Pi" was too pessimistic; "just turn it
on" would be too optimistic.**

#### The three hardware facts

1. **No hardware H.264/H.265 encoder on Pi 5.** Confirmed by a Raspberry Pi engineer:
   *"There is no hardware H264 or H265 encoder on Pi5."* BCM2712 has **HEVC 4Kp60 decode
   only**. ([forums.raspberrypi.com t=378329](https://forums.raspberrypi.com/viewtopic.php?t=378329))
2. **No hardware JPEG encoder either** — *"The jpeg hardware is unchanged across Pi0–Pi4
   (and missing on Pi5)."*
   ([t=365783](https://forums.raspberrypi.com/viewtopic.php?t=365783)) This kills "just use
   MJPEG in hardware" as an escape hatch.
3. **Memory bandwidth ≈ 17 GB/s theoretical**, ~10–11 GB/s of practically achievable bus
   traffic (tinymembench copy ~4.8–5.7 GB/s, counting read+write).
   ([sbc-reviews #21](https://github.com/geerlingguy/sbc-reviews/issues/21))

The anchor measurement for what software encoding costs here: a user encoding 1080p**50**
H.264 measured **~60 % of one core on a Pi 4** (hardware encoder) versus **~80 % of all
four cores on a Pi 5** (software), calling the system "nearly unusable."

#### NDI output — what is actually possible

- **There is an official arm64 Linux SDK.** NDI SDK v6 ships x86_64, **aarch64** and
  armv7l. Needs `avahi-daemon` for discovery, and NEON.
  ([platform considerations](https://docs.ndi.video/all/developing-with-ndi/sdk/platform-considerations))
- **On Linux/ARM, NDI is 100 % CPU.** Hardware codec paths exist only on Windows, macOS,
  iOS and tvOS. NDI's own docs say the library is "heavily optimized (much of it written in
  assembly)" — but that assembly is x86-centric; the ARM path is NEON and less tuned.
- **Full-bandwidth NDI is SpeedHQ2** (8-bit 4:2:2 DCT intra-frame), CPU or FPGA only.
  Crucially, NDI's own documentation says: ***"NDI speed limitations relate more to system
  memory bandwidth than CPU processing performance."*** On a 17 GB/s machine that is a real
  ceiling, not a footnote.
- **NDI|HX / HX3 send is effectively dead here.** HX is H.264/H.265, requires the separate
  **Advanced SDK**, whose docs say plainly that you may use those codecs *"if you are
  developing using devices that already have hardware compressors."* We do not.
- **Bandwidth:** full NDI 1080p60 ≈ **150 Mbit/s**
  ([NDI bandwidth white paper](https://docs.ndi.video/all/getting-started/white-paper/bandwidth)).
  On gigabit that is 16 % of the link — fine. On the Pi 5's 1×1 Wi-Fi 5 radio, where
  iperf3 measures roughly 92–360 Mbit/s depending on conditions, sustained 150 Mbit/s with
  NDI's bursty per-frame delivery is **not a serious proposition**. **Wired GbE is
  mandatory for NDI out.**
- **Prior art on weaker hardware:**
  [lplassman/V4L2-to-NDI](https://github.com/lplassman/V4L2-to-NDI) runs full NDI on a
  **Pi 4**, listing UYVY 1080p60 → 5 client connections, and ~110 ms glass-to-glass at
  1080p60. (Caveat: those "connections" figures may describe the GbE bandwidth limit
  rather than the encode limit. But a Pi 4 shipping 1080p60 SpeedHQ at all is real
  evidence.)
- **[estimate] Encode cost on a Pi 5: ~1.5–3 A76 cores at 1080p60** (40–75 % of the
  machine), plus ~0.3 core for packetisation. Derivation: an A76 at 2.4 GHz is roughly
  2–2.5× a Pi 4 A72 core; if a Pi 4 was near-saturated at 1080p60, a Pi 5 has about 2×
  headroom, not 4×. **This is the number to measure first — it decides the whole design.**

#### The readback path is where this is won or lost

- **[estimate]** 1080p RGBA = 8.29 MB/frame → at 60 fps that is **~498 MB/s written by the
  GPU plus ~498 MB/s read by the CPU**. Add UYVY conversion and the network stack and the
  whole capture→NDI chain is **≈1.5–1.6 GB/s of DRAM traffic**, roughly **15–20 % of
  practically achievable memory bandwidth** — significant, but not the wall. The wall is
  CPU.
- **No published `glReadPixels` benchmark exists for the Pi 5's VideoCore VII.** I looked.
  Whoever implements this **will be the source of that number** — measure it and put it in
  this document.
- V3D stores render targets in a **tiled/UIF layout**, so `glReadPixels` implies a
  detiling pass. Budget for it.
- **Three non-negotiable implementation details**, each of which roughly halves a cost:
  1. **Do the RGBA→UYVY conversion in the fragment shader** (render to a half-width RGBA
     target that *is* UYVY). Halves readback bytes 498→249 MB/s, and NDI's docs say UYVY
     "avoids internal color conversions" whereas BGRA/BGRX "incur a performance penalty."
  2. **A 2-deep PBO ring** — read frame N−1 while rendering N. GLES 3.1 on the Pi 5 has
     PBOs; a synchronous `glReadPixels` serialises the GPU and will destroy the frame rate.
  3. **`NDIlib_send_send_video_async_v2`** — NDI's docs say async submission "almost always
     yields significant performance benefits."

#### Recommendation for output

**Build full NDI over wired gigabit, with resolution and framerate as runtime knobs, and
design for 1080p30 or 720p60 rather than 1080p60.**

| option | bitrate | CPU **[estimate]** | verdict |
|---|---|---|---|
| full NDI 1080p60 | ~150 Mbit | 1.5–3 cores | stretch goal; measure before committing |
| **full NDI 1080p30** | ~75 Mbit | 0.8–1.5 cores | **the pragmatic default** |
| **full NDI 720p60** | ~65 Mbit | 0.7–1.3 cores | best when motion matters more than detail — which, for fractal/feedback work, it often does |
| MJPEG over HTTP | 70–140 Mbit | 1.5–2.5 cores | simpler and universally consumable, but worse quality-per-bit and no cheaper. Only wins on simplicity |
| x264 → SRT/RTP | 8–20 Mbit | ~3.2 cores **measured** at 1080p50 | not viable at 1080p60; viable at 720p30. **The only option that works over Wi-Fi or WAN** |
| raw UYVY over UDP | 1.99 Gbit | ~0.5 core | **does not fit gigabit.** 1080p30 raw is 995 Mbit against a 932 Mbit measured ceiling |

Keep an x264/SRT path at 720p30 as an explicitly-labelled *low-bandwidth remote* mode. It
is the only thing that will ever leave the local network.

#### Camera in — the honest recommendation is **not** NDI

- **The iPhone sends NDI|HX2 = H.264 or H.265**, with HEVC now the default in the NDI
  Camera app. **iOS caps the output** — NDI's FAQ states iOS "sets the max resolution of
  either the X or Y axis to 1920 and scales the other axis accordingly." Expect ≤1080p at
  30 or 60 fps. Bandwidth 2–16 Mbit/s, so **receive is a bandwidth non-issue over Wi-Fi**;
  it is a CPU and latency issue.
- **Software decode is proven but tight:** one developer reports **a single 1080p60 NDI|HX
  stream decoding on a stock Pi 5 without dropped frames** via KMSDRM/SDL2, no overclock —
  described as "just about holding on," with 4K60 unusable.
- **The Pi 5's one hardware block is HEVC decode**, and it is real: Frigate users run
  `ffmpeg -hwaccel drm` on `/dev/video19` + `/dev/media2` with
  `dtoverlay=vc4-kms-v3d,cma-512` and report CPU dropping 45 %→20 %.
  ([frigate #18431](https://github.com/blakeblackshear/frigate/discussions/18431))
  **But NDI's Linux SDK has no hooks into it** — using it via NDI means the Advanced SDK
  and compressed pass-through (`NDIlib_compressed_packet_t`), decoding the bitstream
  yourself.
- **[estimate] End-to-end latency, iPhone → Wi-Fi → Pi: 150–350 ms.** HX2 is not a
  low-latency format (NDI's own band for it is 100–300 ms of encoder buffering alone) plus
  Wi-Fi jitter buffer plus 1–2 frames of decode. **Fine for a source layer in a video
  synth. Useless for anything needing sync to live audio.** Given the user's framing —
  *"needs data more than recording quality"* — this is acceptable, but it should be stated
  on the UI, because a 300 ms camera will feel broken to anyone expecting a mirror.

**Recommendation: default the camera source to RTSP/HEVC, not NDI.** It drops the NDI SDK
dependency from the receive side entirely, lands directly on the one hardware block the Pi
5 actually has, and needs no licence. Latency is comparable (200–850 ms typical, tunable
toward ~200 ms with small buffers). The cost is losing auto-discovery — you configure a
fixed IP and port. Keep NDI-in as a second `StreamSource` implementation for when the SDK
is already linked for output.

**Ruled out: the iPhone as a USB UVC device.** No iPhone presents itself as a UVC camera to
a Linux host; Continuity Camera is macOS-only and the Camo/EpocCam class of apps need a
vendor host driver that does not exist for arm64 Linux.

#### Recording — do not encode

**[estimate]** Software x264 at 1080p60 ultrafast is ~3.5–4 cores — the entire machine —
and libx265 is described by Pi engineers as "too intensive at any significant resolution."
Recording that coexists with the engine tops out at 720p60 or 1080p30 (~1.3–2 cores).

**The strategy that actually works is not to encode at all:**

- 1080p60 **UYVY raw = 249 MB/s = ~15 GB/min**.
- The Pi 5's NVMe measures **~723–732 MB/s sequential write at PCIe Gen 3**, ~450 MB/s at
  the default Gen 2.
  ([Geerling](https://www.jeffgeerling.com/blog/2023/forcing-pci-express-gen-30-speeds-on-pi-5/))
- So **raw UYVY 1080p60 to NVMe fits comfortably even at Gen 2, at ~0.3–0.5 core** —
  essentially just the readback and the write. Add LZ4 (~1 core, ~2:1) for 7 GB/min.
  Transcode offline on a real machine.

This is the only recording strategy that leaves the CPU budget intact for the synth. It
costs disk, and disk is the cheap resource here. It also shares the entire readback path
with NDI output, so **build the readback once and fan it out to both consumers.**

#### The three numbers to measure before committing to any of this

1. SpeedHQ 1080p60 encode cost, in cores, on the actual Pi 5.
2. `glReadPixels`-to-PBO latency for a 1080p frame off V3D. **No published figure exists.**
3. Whether NDI's arm64 send path saturates on memory bandwidth before CPU, as NDI's own
   documentation warns it does.

---

# PART III — RISK, PLAN, AND PROCESS

## 10. Weaknesses — a candid assessment

The user asked for this directly: *"what are weakness of the system, how should we
archetect this out into the future."* This section is deliberately unflattering. Items are
ordered by how much damage they can do.

### 10.1 Fill rate is the ceiling, and V2 spends roughly 2× of it — unmeasured

**This is the most serious weakness and everything else is secondary to it.**

The engine is fill-rate bound on a tile-based renderer — `CMakeLists.txt:12-14` says so
outright, and `fbo.h:14-16` chose RGBA8 over RGBA16F for exactly that reason. V2 roughly
doubles the number of full-screen passes (§7.3: ~5–7 → ~10–12), and worse, it **doubles
the single most expensive pass**: the escape-kernel inner loop at DETAIL 96–512 dwarfs
every blur and warp in the chain combined.

**Nobody has measured this.** The figure above is an estimate from counting passes, which
is the weakest kind of performance claim. `tests/motion_cost.py` is the instrument that
would settle it and it is not even registered in ctest.

**What the governor can rescue, and what it cannot.** Its degradation order is *detail →
particle count → resolution* (V1 §6). Detail is genuinely the right first lever, because
two escape generators are dominated by iteration count. So the governor **can** hold 60 fps
— by cutting DETAIL roughly in half across both channels. That is not free: DETAIL is a
visible aesthetic control, and an instrument that silently halves its own detail the moment
you select a second fractal will read as "the second channel looks bad." `g.throttle_bias`
(§5.2) mitigates but does not remove this.

What the governor **cannot** rescue: it has no lever for *pass count*. Three feedback
points and a binary op are structural, not scalable. If ten passes is too many, the answer
is fewer passes, not a smaller number.

**Mitigations, in order of value:**

1. **Measure first, with `motion_cost.py`'s methodology.** Do not build all of V2 and then
   discover the budget.
2. **Take the fusion win in §7.3** — jit.op + master feedback in one pass.
3. **Let each channel render at a lower resolution than the master.** The buffers are
   already sub-viewported with `uSrcScale` (`renderer.cpp:308-313`), so a per-channel scale
   is nearly free to implement and is a *much* less visible degradation than losing detail:
   a modulator at 0.5× resolution is usually indistinguishable after an operator and a
   feedback smear. **This is the strongest available lever and it does not exist yet.**
   Consider making it a governor stage: *detail (biased) → channel resolution → particle
   count → master resolution.*
4. **Skip a channel's pre-FX pass entirely when its chain is empty.** Already free —
   disabled slots cost nothing (`compiler.cpp:28`) and an empty chain produces no pass.

### 10.2 Three aliased histories triple the surface for the ratchet class of bug

V1 invariant 6 — **"nothing that accumulates may ratchet"** — is the invariant this project
has been burned by most. The alpha ratchet collapsed the ink canvas to uniform magenta in a
few hundred frames; FEEDBACK saturating to white via `max()` is the same bug wearing a
different hat.

V2 has **three independent feedback loops instead of one**, and each is an accumulating
buffer aliased in place (`renderer.cpp:562-565`). Every one of them is a place this can go
wrong, and two of them now feed a *multiplier* rather than a compositor: channel A
ratcheting toward white and then going through `mult` with channel B does not look like a
runaway — it looks like channel B. **The failure mode gets harder to diagnose, not easier.**

Specific hazards:

- The `mix()`-not-`max()` discipline in FEEDBACK (`effects.cpp:170-176`) must hold in all
  three instances. If the fusion change in §7.3 rewrites `vec4 src = nvSample(uInput, uv)`
  to `vec4 src = col`, that touches the exact line the bound depends on.
- `TRAILS` is *deliberately* still `max()` — it is the ratchet look kept as its own effect.
  With three chains there are now three places someone can put TRAILS in a loop with
  something that adds energy.
- The `historyIdx_` aliasing is protected only by exclusion arguments to `pickTarget*`.
  With three histories and a variadic exclusion set, a single missed exclusion means a
  buffer is written while being read — which on a tiler produces *intermittent* corruption,
  the worst possible signature.

**Mitigation: a bounded-energy assertion in the test harness.** Run each feedback point at
maximum with a bright source for 900 frames and assert the white-pixel fraction stays at
0.0000, exactly as was done for DEPOSIT. Do it once per feedback point, not once total.

### 10.3 The parameter bank doubles, and `valueOf()` is a linear scan

Covered in §6.5. `ParamBank::valueOf` scans `defs_` linearly (`state.cpp:149-159`) and
`renderScene` calls it ~40× per frame; V2 makes that two generators' worth of lookups over
a bank twice as long — **roughly 4× the CPU work, on the critical path to the page flip.**

This is a weakness of the *existing* design that V2 exposes rather than creates, and it has
a clean fix (memoize `find(id)` at rebuild time). But it must land in Phase 1, not later,
because it is exactly the kind of cost that gets attributed to the wrong thing: someone
will measure V2 as slow and blame the GPU.

### 10.4 The web UI does not survive ~160 parameters without a new affordance

V1 §13 records that 67 parameters *already* needed filter + jump + fold to be playable on a
phone. §6.6's channel-scope switch is the proposed answer, and it is cheap — but note this
is a genuine design risk, not a styling detail. The user's stated priority is the web UI
("hardware controls need to be mapped later anyway so focus on getting the web ui up to
speed w ability to be modulated"), so a V2 whose engine is beautiful and whose UI is a wall
of 160 faders has failed at the thing the user asked for.

The specific failure to watch for: the jump-chip row derives groups from the id namespace,
so doubling the namespaces doubles the chips and turns a helpful sticky row into a long
horizontal scroll. **Verify on an actual phone, not a desktop browser window narrowed.**

### 10.5 Preset and mapping implications of a channel concept

- **The `b.k.*` id-meaning collision (§6.1) is a silent-corruption bug** if `b.*` is chosen
  for channel B without renaming morph. It is the only place in the V2 design where an
  existing id would keep its spelling and change its meaning, and V1 invariant 14 —
  "unknown ids are ignored or kept, never fatal" — actively works *against* you here,
  because the id is not unknown.
- **Every existing preset becomes a single-channel preset.** That is fine and correct
  (missing keys keep defaults, so channel B lands at its defaults with a null source), but
  it means the *first* thing a user does after V2 lands is load a preset and see it look
  right only if the inactive-channel identity rule (§4.3) is correct. If the identity is
  black and the default operator is `mult`, every old preset loads as a black screen. **The
  identity rule is not a nicety; it is what makes the upgrade non-destructive.**
- **Mappings survive untouched** (§6.4) — genuinely, because they address by id string and
  are rig-owned. This is the one place the existing architecture pays off completely.
- **The mapping table is still never persisted** (V1 §15). With twice the parameters and a
  new operator to map, this gets more painful every phase it is deferred.

### 10.6 `AsyncCompiler` is a depth-1 mailbox and V2 has three chains plus two generators

Detailed in §5.3. `submit()` overwrites a pending request (`async_compiler.cpp:56-68`).
Today there is one chain and one generator program, so the mailbox is correct. V2 has five
independent compile triggers, and the failure mode of the current design is not a stall —
it is a **silently dropped compile**, i.e. a chain edit that appears not to take. That is a
bug users report as "the UI doesn't work," and it will be intermittent.

### 10.7 NDI and the Pi's Wi-Fi

§9.3 has the numbers. Summarising the risk rather than the engineering: **NDI output is
gigabit-only, and camera input over Wi-Fi carries 150–350 ms of latency**, on a machine
whose Wi-Fi has historically been the dominant source of lost time on this project (V1
§16). Adding two network-dependent stages to a live instrument's signal path is a real
reliability regression, and the mitigation — hold the last good frame, show a connection
status, never go black — must be built in from the first commit, not added after the first
show goes wrong.

Also: NDI's own docs warn its throughput is limited by **memory bandwidth**, and §7.2
shows V2's buffer traffic is already the dominant cost. These two systems compete for the
same resource. **[estimate]** They have not been measured together and probably interact
worse than either does alone.

### 10.8 Process fragility: no version control, and a monolithic `main.cpp`

- **`NEPTR_VIDEO/` has zero tracked files** (§1.7). There is no diff, no branch, no
  revert, no bisect. For a project that runs many concurrent agents, this is the largest
  process risk that exists, and it is also the cheapest to fix.
- **`src/main.cpp` is 1,760 lines and `struct App` spans lines 69–718** — 650 lines holding
  the renderer, panel, compiler, receiver, mappings, bank, LFOs, macros, learn state,
  chain, selection, view, HOME, and preset I/O. Nearly every V2 workstream must touch it.
  It is the single largest merge magnet in the tree.
- `src/ui/panel.cpp` (1,128 lines) is the second.
- `CMakeLists.txt` is one flat file where every test registers itself. Two agents adding
  tests collide by construction — this has already happened in this project.

§12 is the plan for all four.

### 10.9 Smaller things that are nonetheless fragile

- **`renderer.cpp:348`**: the core pass target is `int target = (historyIdx_ == 0) ? 1 :
  0;` — hardcoded for a single generator. Two generators in one frame need proper
  allocation, and this line will silently do the wrong thing rather than fail.
- **`FrameStats::iterations`** (`renderer.h:21-26`) is a scalar and becomes meaningless
  with two generators at different DETAIL. The governor consumes it.
- **`paletteTex_` is one 2-row LUT** shared by the generator, particles and ink
  (`renderer.cpp:45-56, 97-113`). Per-channel palettes need either two textures or a 4-row
  LUT, and both particles and ink sample it — so this decision reaches further than it
  looks.
- **Bitwise operators on V3D** (§8.1) are legal GLES 3.00 but untested on this hardware,
  and `int(a*255.0)` on a negative input (easy to produce with `sub`) is undefined
  behaviour.
- **`motion_cost.py` writes into `/tmp/nvtest`** and `motion_probe.py` starts thirty
  processes each compiling a shader. Two agents running suites concurrently will fight.
- **`motion_probe.py:111` falls back to a hardcoded `30`** if the `[nv] kernels N` banner
  regex misses — so a broken banner silently under- or over-enumerates rather than failing.

---

## 11. Phased implementation plan

Each phase is **independently shippable and independently verifiable**. No phase depends on
network code or video decode to be provable. The ordering is chosen so that the riskiest
unknown (fill rate) is answered in Phase 1, before any of the expensive work is built on
top of it.

### Phase 0 — Foundations (no user-visible change)

**Purpose: make the tree safe to work on concurrently, and answer the fill-rate question
before designing around a guess.**

1. **`git init` inside `NEPTR_VIDEO/`** with a `.gitignore` for `build/` and `.cache/`, and
   one baseline commit. Nothing else in this plan is safe without it.
2. **Extract `struct App` from `main.cpp` into `src/app.{h,cpp}`.** Pure move, no
   behaviour change. This is what converts most later merge conflicts into no-conflicts.
3. **Split `CMakeLists.txt`**: keep the top-level for the binary, move test registration to
   `tests/CMakeLists.txt`, and register tests from a list so two agents adding tests touch
   different lines.
4. **Register `motion_cost.py`** in ctest as an opt-in (not in the default suite — it takes
   minutes), **run it, and record the numbers in §1.5 of this document.**
5. **Memoize the generator's parameter lookups** (§6.5) with a test that the cache is
   rebuilt on every bank rebuild.
6. Add a `pickTarget(std::initializer_list<int>)` overload alongside the existing two, with
   unit tests. Do not delete the old ones yet.

**Verifiable by:** `ctest` still 16/16 green; a recorded `motion_cost.py` baseline;
`git log` exists.

### Phase 1 — Dual fractal channels + `jit.op` core

**Self-contained and fully testable before any video decode or networking. This is the
phase that proves or disproves the whole design.**

1. `enum class SourceKind { Fractal, Video, Draw, Stream };` and a `ChannelState` holding
   source kind, `Selection` fields, scene time and `MotionClock`.
2. Split `struct Selection` (`state.h:278-293`) into per-channel sub-structs; keep palette
   global for now (§10.9).
3. Second generator: per-channel `coreKeyA_`/`coreKeyB_`, fix the `setKernel` early-return
   (`renderer.cpp:127`), proper target allocation replacing `renderer.cpp:348`.
4. Grow the FBO pool to 8 and switch every call site to the exclusion-set `pickTarget`.
5. **`jit.op`**: `EffectKind::Binary`, the full operator catalogue as a `switch (uOp)`,
   `op.*` parameters, and the identity rule of §4.3.
6. Parameter namespacing per §6.2, including the `b.k.*` → `*.mk.*` rename and the preset
   version bump.
7. Wire: `/nv/op`, `/nv/source`, `/nv/list "op"`, `/nv/sel` appended fields.
8. Governor: `g.throttle_bias` and biased detail degradation.
9. `NV_*` hooks: `NV_SOURCE_A`, `NV_SOURCE_B`, `NV_KERNEL_A`, `NV_KERNEL_B2`, `NV_OP`.

**Verifiable by** (all headless on the Mac, per §2.2):
- A frame capture with `NV_OP=<n>` for **every** operator, asserting each produces a
  different image from `pass` — the operator equivalent of `motion_bank`.
- Arithmetic identities as pixel proofs: `sub(a, a)` is black; `mult(a, 1)` is bit-identical
  to `pass`; `swap` of a commutative op is bit-identical, and of `sub` is not.
- The inactive-channel identity: with channel B off, every operator equals `pass` when
  `op.val` is at the operator's identity.
- `motion_cost.py` re-run: **the measured cost of the second generator**, against the
  Phase 0 baseline.
- `ctest` green, and a mutation check on every new assertion.

### Phase 2 — Per-channel pre-FX and the three-chain compiler

1. Three `CompiledChain`s; one `AsyncCompiler` submission carrying all three (§5.3).
2. Per-channel FEEDBACK, with its own history buffer per channel.
3. The fusion change in §7.3, **only if** the mutation-checked compiler tests pass.
4. `afx<n>.*` / `bfx<n>.*` parameters.

**Verifiable by:** `test_compiler.cpp` cases for the new grouping; a bounded-energy proof
per feedback point (§10.2); a pass-count assertion so a regression in fusion is loud.

### Phase 3 — Web UI

Done here rather than last, because it is the user's stated priority and because Phases 1–2
are unplayable without it.

1. Channel scope switch (§6.6), operator grid from `/nv/list "op"`, source selectors with
   the exclusivity hint.
2. Server: `"op"` added to `VIDEO_SELECT` (`demiurge_web.py:2538`); `select()` and
   `video_select` extended for the two-int `/nv/source`.
3. Per-channel modulation UI — the user explicitly wants parameters modulatable from the
   web.

**Verifiable by:** on an actual phone. Route and API tests do not prove a layout.

### Phase 4 — Draw rework

Ink as a channel source, brush colour modes, pre-FX diffusion (§5.5). Small, high
aesthetic return, and independent of everything above once Phase 1's source abstraction
exists.

### Phase 5 — Particles after jit.op

Move the field argument, optionally split physics from colour (§5.6). Deliberately after
the chains, because the particle stage sits between jit.op and master feedback and wants
that topology settled.

**Verifiable by:** re-running the cohesion-1 reconstruction proof (V1 §8) — particles-only
mean brightness must still track the synth's, and death at max must still be invisible.

### Phase 6 — Video file source

FFmpeg demux → V4L2 M2M HEVC decode → DMA-BUF → EGL image → GL texture on the Pi; FFmpeg
software decode + `glTexSubImage2D` on macOS. One-frame readahead; if the decoder falls
behind, **hold the previous frame** — video must never cause a render-loop drop.

### Phase 7 — Camera in

`StreamSource` interface, **RTSP/HEVC first** (§9.3), NDI-in second, V4L2 USB fallback.
Behind `-DWITH_NDI=ON` so the tree builds without the SDK.

### Phase 8 — Output

Shared readback path (shader-side UYVY → PBO ring), fanned out to NDI send and raw-to-NVMe
recording. Measure the three numbers in §9.3 first.

---

## 12. Subagent dispatch strategy

The user asked for this specifically:

> "start archetecting how to dispatch subagents for the things we are doing total overal in
> a way that will result in fastest outcome after merging / not create fucked up merge
> conflict that requires manual sorting of stuff."

This project has run many concurrent agents and **real collisions have occurred**: two
agents both editing `CMakeLists.txt`; one agent's new file missing from the build, blocking
another. The plan below is built around the specific files that caused those.

### 12.1 The four collision magnets, named

| file | lines | why it collides | fix |
|---|---|---|---|
| `src/main.cpp` | 1,760 (`struct App` = 69–718) | holds every subsystem; every workstream touches it | **Phase 0: extract `App` into `src/app.{h,cpp}`** |
| `CMakeLists.txt` | 278 | every agent adds a test at the bottom | **Phase 0: move tests to `tests/CMakeLists.txt`, list-driven** |
| `src/state.h` (`namespace gid`, 203–260) | — | every agent adds parameter ids | **Phase 0: one owner, batched** — see 12.3 |
| `src/renderer.cpp` | 593 | the render graph itself | **serialize** — never two agents at once |

Plus, in the *other* tree: `DEMIURGE_OS/web/demiurge_web.py` (2,707 lines) and
`DEMIURGE_OS/web/static/video.js` (1,131 lines). And note `src/platform/platform_glfw.cpp`
is currently being edited for the headless-window work.

### 12.2 Landing gates — what must be serial

**Nothing parallel starts until Phase 0 lands.** It is four mechanical changes and it is
what makes everything after it parallelisable. Run it as **one agent, alone**.

Then, strictly ordered — each is a gate, and work behind it must not start early:

```
GATE 0  Phase 0 foundations (git, App extraction, CMake split, pickTarget, memoization)
           │
GATE 1  ChannelState + Selection split + FBO pool of 8      ← ONE agent, owns renderer.cpp + state.h
           │                                                   Everything downstream reads these types.
           ├──────────────┬──────────────┬─────────────────┐
GATE 2   jit.op          three-chain    parameter          wire verbs
         operators       compiler       namespacing        (/nv/op, /nv/source, list "op")
           │              │              │                  │
           └──────────────┴──────────────┴──────────────────┘
                          │
GATE 3  Web UI (needs the wire verbs to exist and be stable)
                          │
GATE 4  Draw · Particles · Video · Camera · Output  ← genuinely parallel, disjoint files
```

**Why GATE 1 is serial:** `ChannelState`, the `Selection` split and the FBO pool are *type*
changes that every other workstream compiles against. Two agents inventing them
concurrently produces two incompatible designs and a manual merge of the exact kind the
user wants to avoid. One agent, one commit, then everyone rebases onto it.

### 12.3 Territory table — file-level ownership, not "areas"

Within a wave, **an agent owns files exclusively.** If two agents need the same file, they
are not in the same wave.

| agent | owns exclusively | may read | must NOT touch |
|---|---|---|---|
| **A0 · Foundations** (alone) | `CMakeLists.txt`, `tests/CMakeLists.txt`, `src/app.{h,cpp}`, `src/main.cpp`, `src/state.{h,cpp}`, `src/renderer.{h,cpp}`, `.gitignore` | all | — |
| **A1 · Channels** (alone, GATE 1) | `src/renderer.{h,cpp}`, `src/state.{h,cpp}`, `src/gfx/fbo.{h,cpp}`, `src/app.cpp` | all | `chain/*`, `layers/*`, `control/*` |
| **A2 · jit.op operators** | `src/chain/ops.{h,cpp}` **(new)**, `src/chain/effects.{h,cpp}`, `tests/test_ops.cpp` **(new)** | `chain/compiler.*`, `renderer.*` | `renderer.cpp`, `state.*` |
| **A3 · Chain compiler** | `src/chain/compiler.{h,cpp}`, `src/async_compiler.{h,cpp}`, `tests/test_compiler.cpp` | `chain/effects.h` | `chain/effects.cpp`, `renderer.cpp` |
| **A4 · Parameters & presets** | `src/patch.{h,cpp}`, `src/preset.{h,cpp}`, `tests/test_patch.cpp`, `tests/test_preset.cpp` | `state.h` | `state.cpp` |
| **A5 · Wire** | `src/control/receiver.{h,cpp}`, `src/control/osc.{h,cpp}`, `tests/test_control.cpp`, `tests/test_list_bus.py` | `state.h`, `app.h` | `control/mapping.*` |
| **A6 · Web UI** | `DEMIURGE_OS/web/static/video.js`, `DEMIURGE_OS/web/demiurge_web.py` | this repo, read-only | anything in `NEPTR_VIDEO/src` |
| **A7 · Device panel** | `src/ui/panel.{h,cpp}`, `src/ui/theme.h` | `state.h` | everything else |
| **A8 · Draw** | `src/layers/ink.{h,cpp}` | `state.h`, `renderer.h` | `renderer.cpp` |
| **A9 · Particles** | `src/layers/particles.{h,cpp}`, `src/layers/lifecycle.h` | `renderer.h` | `renderer.cpp` |
| **A10 · Video source** | `src/sources/video.{h,cpp}` **(new)** | `platform/platform.h` | `renderer.cpp` |
| **A11 · Camera in** | `src/sources/stream.{h,cpp}`, `src/sources/rtsp.{h,cpp}`, `src/sources/ndi_recv.{h,cpp}` **(all new)** | `platform/platform.h` | `renderer.cpp` |
| **A12 · Output** | `src/output/readback.{h,cpp}`, `src/output/ndi_send.{h,cpp}`, `src/output/record.{h,cpp}` **(all new)** | `renderer.h` | `renderer.cpp` |

### 12.4 The rules that actually prevent the collisions

1. **`src/renderer.cpp` has exactly one owner at a time, ever.** It is the render graph. If
   your change needs a call added there, write everything else, then hand a **one-line
   patch description** to whoever holds it — do not edit it yourself.
2. **New subsystems arrive as new files with a header, and are wired in by the file's
   owner.** A9 writes `particles.cpp`; A1 or the renderer owner adds the call. This is what
   prevents the "agent's file missing from the build" failure, and it is why the CMake
   split in Phase 0 matters — the new file must be *listed* by its own agent, in a file
   nobody else is editing.
3. **Parameter ids are batched, not added ad-hoc.** Everyone who needs new ids writes them
   into a single shared list (a scratch file, not `state.h`), and **A4 lands them all in one
   commit.** `namespace gid` (`state.h:203-260`) is a dense block that conflicts on almost
   any concurrent edit.
4. **Tests register themselves in `tests/CMakeLists.txt`, one `add_test` per agent, in an
   agent-named section.** Never at the bottom of the file — that is precisely where two
   agents collide.
5. **Never run two GL suites concurrently.** `motion_bank` starts thirty processes each
   compiling a shader; `motion_cost.py` writes to `/tmp/nvtest`. Serialize, or give each
   agent its own build dir and temp dir.
6. **Only one agent touches the Pi at a time.** DRM master is exclusive (V1 §11).
7. **Cross-tree work is a hand-off, not a shared edit.** A6 owns the `DEMIURGE_OS` web
   files and nobody in the `NEPTR_VIDEO` tree touches them; A5 owns the wire in this tree
   and publishes the format to A6 as a spec. The two trees also diverge (§1.7), so an agent
   that edits both is an agent that can silently revert a live patch.

### 12.5 What is genuinely parallel, and what only looks it

**Genuinely parallel** (disjoint files, no shared types beyond stable headers):
A2 + A3 + A4 + A5 after GATE 1 · A8 + A9 + A10 + A11 + A12 after GATE 2 · A7 alongside
almost anything · A6 alongside anything in this tree once the wire is frozen.

**Looks parallel and is not:**

- **jit.op operators (A2) and the chain compiler (A3).** They share `EffectKind` in
  `chain/effects.h`. Resolution: A3 lands `EffectKind::Binary` **first**, as a one-line
  enum change plus the compiler handling, then A2 fills the catalogue. Twenty minutes of
  serialization saves a nasty merge.
- **Parameters (A4) and anything that adds a parameter.** See rule 3.
- **Web UI (A6) and the wire (A5).** A6 cannot start until `/nv/op` and `/nv/source` are
  frozen — not implemented, but *frozen*. Publish the spec early, implement in parallel.
- **Anything and the governor.** The governor is small but read by the renderer and the
  panel. Fold governor changes into whoever owns `renderer.cpp` in that wave.

### 12.6 Merge hygiene

- **One commit per agent per gate**, with a message naming the files it owns.
- **Rebase onto the gate, never merge sideways.** Sideways merges between agents in the
  same wave are exactly the manual sorting the user wants to avoid.
- **The tree must be green at every gate**: `cmake --build build -j8` clean and `ctest`
  16/16 (plus whatever each agent added). An agent that lands red blocks every other agent,
  because they all rebase onto it.
- **Verification claims are per-agent and must follow §2.** "It compiles" is not a report.

---

## 13. Open questions

These are the ones that genuinely change the design. Each has a recommendation so it can be
answered with a yes/no rather than an essay.

**Q1 — Per-channel palettes, or one shared palette?**
Today one 2-row LUT is shared by the generator, particles and ink
(`renderer.cpp:45-56, 97-113`). Two channels with independent palettes would let you
multiply a warm fractal by a cold one, which is a real `jit.op` idiom. Cost: a 4-row LUT
and a decision about which palette particles and ink read.
**Recommendation: yes, per-channel, via a 4-row LUT** — the colour interaction between the
two channels is a large part of what the operator is for. Particles read the jit.op result's
colour, so they are unaffected; ink reads whichever channel it is the source of.

**Q2 — Does each channel keep MORPH?**
MORPH interpolates the *iteration itself*: `z ← mix(fA(z,c), fB(z,c), m)` — mathematically
different from blending two images with `avg`. With two channels you can now do the image
blend directly.
**Recommendation: keep MORPH per-channel.** It is not redundant; it makes shapes that no
image blend can. But it does mean a channel can hold two kernels, so the bank grows by
another 4 knobs per channel — which is the honest cost of saying yes.

**Q3 — Per-channel render resolution as a governor stage?**
§10.1 argues this is the strongest available lever against V2's fill-rate cost, and that a
modulator at 0.5× is far less visible than a detail cut.
**Recommendation: yes, build it in Phase 1** — the `uSrcScale` plumbing already exists, so
it is cheap, and it is the thing most likely to make the difference between V2 running at
60 fps and not.

**Q4 — Should particles be able to read a different field for physics than for colour?**
§5.6: a one-argument split. Particles driven by the operator result but coloured from
channel A, say.
**Recommendation: yes, split the argument, default both to the jit.op output.** It costs
one parameter and unlocks a genuinely different behaviour.

**Q5 — Camera: RTSP/HEVC or NDI?**
§9.3 concludes RTSP/HEVC is lower-complexity, lands on the Pi 5's one hardware block, needs
no SDK or licence, and has comparable latency. The cost is losing auto-discovery — you type
in an IP.
**Recommendation: RTSP/HEVC as the default, NDI-in as a second implementation behind the
same `StreamSource` interface.** But this is genuinely the user's call, because "type in an
IP address" versus "it just appears" is a real ergonomic difference on stage.

**Q6 — NDI output resolution target: 1080p60, or 1080p30 / 720p60?**
§9.3: 1080p60 SpeedHQ is **[estimate]** 1.5–3 cores and gigabit-only; 1080p30 or 720p60
roughly halves everything.
**Recommendation: build the knob, design for 1080p30, and treat 1080p60 as a stretch goal
gated on measurement.** Also: **is a wired ethernet connection to the Pi acceptable when
streaming?** If the answer is no, NDI output is off the table and the honest alternative is
x264/SRT at 720p30.

**Q7 — Is `g.speed` per-channel, global, or both?**
§5.4 requires per-channel scene time. The question is whether a single master SPEED that
scales both should also exist.
**Recommendation: both** — `a.speed` and `b.speed` per channel, plus `g.speed` as a master
multiplier. It is one extra parameter and it is the control you actually want on a knob.

**Q8 — Should the operator be modulatable by an LFO?**
`op.mode` is an index, not a continuous quantity. An LFO on it would step through operators
— which is either the best or the worst idea in this document.
**Recommendation: allow it, but make `op.mode` a `Curve::Linear` parameter with a stepped
readout**, so the mapping table needs no special case (V1 invariant 10: one mechanism per
job). Then let the user find out whether they like it.

**Q9 — Does anything ever need a third channel?**
The user has been clear that two is the artistic limit, and this document takes that as
settled. Flagging only that the design in §7.2 (8 buffers, exclusion-set `pickTarget`)
happens to generalise, so a future third channel would be expensive but not architecturally
blocked.
**Recommendation: no. Two is the instrument.**

---

## 14. Invariants — what V2 keeps, and what it supersedes

### Kept, unchanged, and load-bearing

1. Parameters are **id-addressed**; the bank's tail is dynamic. *(§6.5's memoization is a
   cache of `find(id)`, invalidated by rebuild — not a positional contract.)*
2. `/nv/param` and `/nv/state` are **normalized**; presets are **real units**.
3. The state broadcast sends **target**, not the modulated value.
4. Scene time is **accumulated**, not `wallClock × speed` — **now per channel** (§5.4).
5. Exponential forms everywhere a rate meets a variable `dt` (`1-exp(-x)`), never linear.
6. **Nothing that accumulates may ratchet.** *Now three times as much surface* (§10.2).
7. Particles carry the field's **actual RGB**; re-palettizing luma is a second colouring.
8. Update and draw shaders must derive particle **HOME identically**.
9. The panel is **one draw call from one atlas**.
10. **One mechanism per job.** *(Why cross-modulation is operators, not a mode — §8.5.)*
11. Only PRESENT rotates.
12. VAOs are per-context.
13. Death must be invisible at cohesion 1.
14. Unknown ids are **ignored or kept, never fatal**. *(And note §6.1: this invariant does
    not protect you when an id keeps its spelling and changes its meaning.)*

### Superseded, explicitly

**S1. V1 §4's render pipeline is single-source.** Superseded by §4.1. *Reason:* the
instrument's central idea is the operation between two signals; a single-source graph
cannot express it. Pass fusion, the governor and every other property of §4 are preserved.

**S2. Layers composite over the generator.** Superseded: particles now run on the **jit.op
output** and ink becomes a channel **source** (§5.5, §5.6). *Reason:* the user's stated
design — the combined field IS the particles' physics — and the user does not want ink as a
modulator.

**S3. One effect chain.** Superseded by three (§4.1). *Reason:* pre-op signal shaping and
post-op finishing are different jobs; the user asked for a small pre-FX chain per channel.

**S4. One feedback point, reading last frame's final composite.** Superseded by three, each
with its own aliased history (§7.2). *Reason:* per-channel feedback before combining is
where the user says most of the character comes from. *Cost accepted knowingly:* §10.2.

**S5. Prior V2 draft's "an inactive channel holds black."** Superseded by the operator
identity rule (§4.3). *Reason:* black is the identity for `add` but an annihilator for
`mult` — the draft's rule would blank the picture on the most-used operator.

### New invariants proposed for V2

15. **The pipeline is always two channels, one operator, one master chain.** An inactive
    channel costs no passes, but the operator always sees a defined value — the operator's
    identity, or `op.val`.
16. **Nothing runs until selected.** An unselected source compiles no shader, decodes no
    video, opens no socket. **But every FBO is allocated at startup** — buffers are
    pre-warmed, work is not (§5.3).
17. **`jit.op` is pointwise unless it says otherwise.** The arithmetic/trig/bitwise/
    comparison operators read one texel from each input. Warp operators are declared
    `Sampler` so the compiler prices them correctly (§8.5).
18. **Channels are symmetric in capability, asymmetric only in degradation.** Neither is
    stripped down. `g.throttle_bias` decides who loses detail first (§5.2).
19. **Each channel owns its scene time and its `MotionClock`.** A change on one channel —
    source, kernel, recompile, preset — must never jolt the other.
20. **No id may keep its spelling and change its meaning.** If a parameter's semantics
    change, it gets a new id and the preset version is bumped (§6.1, §6.3).
21. **Every new subsystem ships its `NV_*` hook in the same change that ships the
    subsystem** (§2.3). Untestable through the harness means untested.

---

## 15. Quick reference for an implementer

```bash
# build + test (Mac, headless — no windows will appear)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j8
cd build && ctest --output-on-failure          # 16/16 expected, ~134 s

# a deterministic frame capture
NV_KERNEL=4 NV_FIXED_DT=0.0166666 NV_MOTION_PHASE=0 NV_GOVERNOR=0 \
NV_SHOT=/tmp/nvtest/a.ppm NV_SHOT_FRAME=40,220 ./build/neptr-video

# frame timing (real GPU time; glFinish per frame)
NV_FRAMETIME=1 NV_VSYNC=0 NV_GOVERNOR=0 NV_MAX_FRAMES=600 ./build/neptr-video

# force a visible window when you actually want to watch
NV_WINDOW=1 ./build/neptr-video
```

| you want | read |
|---|---|
| what exists today | `ARCHITECTURE.md` (V1), all 17 sections |
| the render graph | `src/renderer.cpp` — `renderScene` at `:315`, `present` at `:569` |
| pass fusion | `src/chain/compiler.cpp` — `groupIntoPasses` at `:13` |
| the effect catalogue | `src/chain/effects.cpp` — 23 effects, 3 of kind `History` |
| the parameter store | `src/state.h:62-182`, ids at `:203-260` |
| the bank rebuild | `src/patch.cpp:36-65` |
| the wire | `src/control/receiver.cpp` — ingress `:80-217`, egress `:287-383` |
| particles | `src/layers/particles.cpp` — HOME at `:89-93` and `:210-211` |
| ink | `src/layers/ink.cpp` — brush at `:10-42` |
| deploy and the Pi | `deploy/README.md` |
| the web page | `DEMIURGE_OS/web/static/video.js`, `demiurge_web.py:2514-2557` |
