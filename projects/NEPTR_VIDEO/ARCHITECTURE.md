# NEPTR_VIDEO — Architecture Reference

**Status as of 2026-08-16.** Written for planning future architecture. This is a *description of what exists*, not a design doc for what should exist. Where something is unverified or in flight, it says so — treat unmarked claims as verified by running code, and marked ones as claims.

NEPTR is a Raspberry Pi 5 instrument. It has four **modes**: AUDIO, VIDEO, GAMES, MULTI-DIM-DSP. NEPTR is the object; the modes are things it does. This repo is the **VIDEO** mode — a fractal-and-feedback video synthesis engine — and it also hosts the HOME mode selector that launches the others.

---

## 1. Context and constraints

| Constraint | Consequence |
|---|---|
| Pi 5, GLES 3.0, no hardware video encoder | Everything is a fragment shader. No compositor, no engine, no video decode path. |
| 60 fps is the goal, always | A governor gives up quality to defend it. See §6. |
| DRM/KMS allows exactly **one** master | HOME is a *view* inside this binary, not a separate program. See §11. |
| Touchscreen panel + projector/TV on two HDMI ports | Outputs have **roles** (Scene / Panel), not indices. See §3. |
| The user has **no footswitch board** | The web UI is a primary control surface, not a convenience. |
| Personal artistic instrument, not a product | No IP concerns, no compatibility burden, no users but one. |

The two aesthetic commitments that drive real technical decisions:

1. **Colour and behaviour come from the fractal's own mathematics**, not from post-processing. Palettes read quantities the iteration actually produces.
2. **Everything is secretly a particle system.** Behaviour is modulatable from "you cannot tell it is particles" to total chaos.

---

## 2. Repo layout

```
src/
  main.cpp              frame loop, App struct, env hooks, input dispatch
  state.{h,cpp}         ParamBank — the one flat parameter store
  patch.{h,cpp}         rebuilds the bank when kernel/chain changes
  preset.{h,cpp}        save/load
  mod.{h,cpp}           LFOs, macros, learn gesture
  motion.h              intrinsic generator motion (IN FLIGHT)
  renderer.{h,cpp}      render graph, pass execution, palette LUT
  shader_source.cpp     GLSL preamble, noise lib, core template
  governor.{h,cpp}      the 60fps defender
  async_compiler.{h,cpp}  off-thread shader compilation
  core/kernels.cpp      ~30 generators
  chain/effects.cpp     ~25 effects
  chain/compiler.cpp    pass fusion
  layers/particles.*    GPU particle system
  layers/ink.*          drawing layer
  layers/lifecycle.h    particle death hazards (GL-free, testable)
  palette/palette.*     palette bank + dual LUT
  control/osc.*         OSC encode/decode
  control/receiver.*    UDP 8004 ingress + state broadcast
  control/mapping.*     the ONE mapping table
  ui/panel.*            device control surface (one draw call)
  ui/theme.h            the H=296 palette
  ui/font.*             bitmap atlas
  home/{home,nav,launcher}.*   mode selector
  platform/platform.h            THE SEAM
  platform/platform_glfw.cpp     macOS dev (GL 4.1, two windows)
  platform/platform_drm.cpp      Pi (libdrm+GBM+EGL, direct scanout)
  platform/evdev_touch.*         touch input
  gfx/                  fbo, shader, quad, gl helpers
tests/                  12 ctest suites + python probes
deploy/                 Pi service, deploy script, mode launchers, games bridge
```

~9,600 lines of C++.

---

## 3. The platform seam

`src/platform/platform.h` is the only file both backends implement. Everything above it is portable.

```
Output              a display with a ROLE, not an index
OutputRole          Scene | Panel
currentContext()    which GL context is current
contextCount()      how many exist
releaseDisplay()    give up DRM master
acquireDisplay()    take it back
rotation()          panel mounted sideways
```

**Role assignment** (`assignRoles()`): one display → Scene. Otherwise the *smallest* display is the Panel, with `NV_PANEL_CONNECTOR` as an override. Nothing hardcodes HDMI-A-1.

**Non-obvious and load-bearing:** VAOs are **not** shared between GL contexts (textures, buffers and programs are). Anything holding a VAO keeps one per context — see `Panel::flush(ctx)` and `Quad::create(contextCount)`. Forgetting this produces a blank surface on the second output only.

**Rotation** is applied only in the PRESENT pass. Rotating the shared fullscreen vertex shader would turn every offscreen pass too, and each effect would then sample a sideways image. The panel handles its own rotation separately, and the *viewport* must be swapped under a quarter turn because the framebuffer is physical while the layout is logical.

macOS runs two windows (SCENE + CONTROL) mirroring the Pi's two outputs, so the whole thing is developable without hardware.

---

## 4. Render pipeline

```
generator (fractal) ──> layer composite ──> effect chain ──> PRESENT ──> screen
       ▲                      ▲                                   
   particles ─────────────────┘                                   
   ink ───────────────────────┘                                   
```

- **Generator** renders into an offscreen RGBA buffer.
- **Particles** and **ink** composite over it, gated by `g.fractal_level`, `p.*`, `d.*`.
- **Effect chain** is a list of slots, each enabled/bypassable.
- **PRESENT** is the only stage touching the physical framebuffer, and the only one that rotates.

Buffers ping-pong. Everything is sized to the *render* resolution, which the governor can change independently of the display.

### Pass fusion

`chain/compiler.cpp` — `groupIntoPasses()`. Effects are **pointwise** (operate on one texel) or **sampling** (read neighbours: blur, warp, kaleido, feedback). A pass holds at most one sampling effect, and it must be first; pointwise effects fuse onto it. So SATURATE + LEVELS + GRAIN become one full-screen pass, not three.

This is the single biggest performance lever in the engine and should be preserved by anything that adds effects.

---

## 5. Generators (kernels)

`src/core/kernels.cpp` — ~30, built via two named constructors:

- **`esc()` — Escape kernels.** Iterated orbits: `z ← f(z, c)` until escape. MANDELBROT, JULIA, MULTIBROT ZN+C, BURNING SHIP (+JULIA), TRICORN, CELTIC, BUFFALO, PERPENDICULAR, PHOENIX, MANOWAR, SPIDER, NEWTON Z3-1, NOVA Z3-1, SINE/COSINE/EXPONENTIAL JULIA, LAMBDA LOGISTIC, MAGNET 1, KIFS, KLEINIAN GROUP.
- **`fld()` — Field kernels.** Direct functions of position and time: DOMAIN WARP FBM, FBM VALUE NOISE, RIDGED MULTIFRACTAL, WORLEY F2-F1, VORONOI F1, METABALLS, SINE PLASMA, CURL NOISE.

Real mathematical names throughout — deliberate.

**One shader template, substituted.** `shader_source.cpp` holds a core template with `%%STEP%%`, `%%FIELD%%`, `%%STEPB%%`, `%%FIELDB%%`, `%%AUXINIT%%`, `%%DEFINES%%`. A kernel is a GLSL step function, not a shader. State is `vec4`: `.xy` = z, `.zw` = aux.

**Compilation is cached** on `kernel | kernelB : colorMode`. The colour mode is in the key because it decides which intrinsic features the inner loop computes, and those are `#define`s. Browsing back and forth must not recompile. Compilation is also **async** (`async_compiler.*`) so changing generator does not stall the frame loop.

**Morph interpolates the iteration itself**: `z ← mix(fA(z,c), fB(z,c), m)`. `canMorph()` gates which pairs are valid — same kind, compatible seeding. When morph is off or invalid, the B side is not compiled at all, because compiling it would double the inner loop for nothing.

**IN FLIGHT:** intrinsic per-kernel motion (`src/motion.h`, `tests/test_motion.cpp`) and bipolar SPEED with reverse. Escape kernels are pure functions of position and were previously frozen; only Field kernels animated. Scene time is *accumulated* (`sceneTime += dt * speed`), not `wallClock * speed`, so reverse rewinds smoothly and a speed change never jumps phase. **Preserve that property.**

---

## 6. The governor

`src/governor.{h,cpp}`. Defends the **60 fps goal**, explicitly *not* the display's refresh rate.

Degradation order: **detail (iterations) → particle count → resolution.**

**Two bugs lived here and both are pinned by `tests/test_governor.cpp`.** Do not reintroduce:
1. Targeting `refresh × 0.85` — invisible on a 120 Hz dev machine, fires every frame at the Pi's 60.
2. Trusting GLFW's reported 120 Hz on a ProMotion panel the compositor actually swaps at 60.

It throttles above `goal × 1.35` and restores below `goal × 1.10`. Hysteresis is deliberate; a single threshold oscillates.

---

## 7. Colour

8 modes, each reading a quantity the iteration *produces*: SMOOTH, TRAP (orbit trap), ANGLE (final angle), DERIV (derivative), STRIPE (stripe average), CURVE (curvature), PRISM, DUAL. Trap shapes: POINT, CROSS, CIRCLE, LINE.

Palettes live in a **dual LUT** — two palettes in a 2-row texture, mixed by a parameter, so you can morph between archetypes. `LINEAR` across t, `REPEAT` on s (the rotate control cycles forever), rows on exact texel centres so they never bleed.

**Hard-won:** particles carry the field's **actual RGB**, not a re-palettized luma. Re-mapping luma through the palette is a second colouring of an already-coloured image — a black interior came back as palette index 0 (bright red) and cohesive reconstruction became impossible. TINT is the opt-in that re-reads the palette.

---

## 8. Particles

`src/layers/particles.*`. State in ping-ponged **RGBA32F** — one texel *is* one particle. Draw is `GL_POINTS` + `texelFetch` by `gl_VertexID`: **no vertex buffer, nothing uploaded per frame.**

### The COHESION / HOME model

Each particle's **HOME** is its slot in a grid derived from its id — zero storage, never initialised. COHESION is a spring pulling it home. At cohesion 1 the pool tiles the screen and **reconstructs the image exactly**.

That reconstruction is a **protected property**. Measured: particles-only (`g.fractal_level=0`) mean brightness 95.04 vs the synth's 96.21, sharp interior boundary, no holes.

**COUNT is a density control, not a crop.** It was originally `idle = (id / total) > count`, which parked a contiguous run of ids and therefore deleted a *band of the picture* — at 0.5, half the frame went empty. Now HOME derives from an **alive grid** scaled by `sqrt(count)`, so whatever is alive always tiles the whole frame. The update and draw shaders must derive HOME **identically**; if they disagree, CARRY samples the field where the particle isn't being pulled and the reconstruction smears.

### Life and death

Three optional causes, all defaulting to 0: `p.die_interior` (SINK — dwelling where the field is black), `p.die_edge` (EDGE — 0 wraps, 1 dies, between is probabilistic), `p.die_exhaust` (BURN — speed spends life).

There is **no age channel** — a second render target is exactly what the state packing exists to avoid. So each cause is a **hazard**: a rate of dying while its condition holds, giving exponential dwell. The rate is `1-exp(-rate·dt)`, never `rate·dt` — the linear form asks for p>1 on a hitched frame and empties the pool in one step, and expected lifetime must be identical at 30/60/120 fps. That maths is in `layers/lifecycle.h`, GL-free and unit-tested by simulating survival curves at three framerates.

**Death is invisible at cohesion 1** structurally, not by tuning: it routes through the existing respawn, which respawns *at home* when cohesive, so the replacement lands where the dead one stood. Measured: all three causes at max differ from baseline by mean |Δ| 4.719; a baseline *re-run* differs by 4.721 (GRAIN's own noise).

Key parameters: `count size speed gravity swirl motion damping life brightness fieldColor cohesion carry paletteShift paletteMix deposit die_*`. **GRAVITY** = how hard the fractal pulls the swarm in (the structure of their reality). **MOTION** = how much they shoot around.

---

## 9. Ink (drawing)

`src/layers/ink.*`. Premultiplied canvas; brush colour sampled from the field. Particles can **deposit** onto it (`p.deposit`), closing a feedback loop.

**The alpha ratchet — read this before touching any accumulating buffer.** Bounding the RGB is not enough. Premultiplied OVER is `a' = a + dst·(1-a)`, which can only **climb**, so a canvas deposited into every frame reaches full opacity regardless of the amount, hides the generator, and leaves the swarm reading a flat field with nothing to steer it. Measured: `DEPOSIT 0.6` collapsed to uniform magenta in a few hundred frames.

Fix was one blend factor — `GL_SRC_ALPHA` instead of `GL_ONE` for the deposit pass — making it a lerp of all four channels, so the canvas converges *on* the amount rather than through it. White-pixel fraction 0.0000 at DEPOSIT 1 over 900 frames.

This is the same class of bug as FEEDBACK saturating to white via `max()`. `max()` survives only as the separate TRAILS effect, where ratcheting is the point.

---

## 10. Parameters, modulation, mapping

### ParamBank (`src/state.*`)

One flat store, **id-addressed, never index-addressed** — the bank's tail changes with the kernel and the chain, so an index means a different knob after any structural change. `find(id)` maps id → index. Each entry has id, label, group, min, max, default, curve (Linear / Exp / Bipolar), and a `slew` time.

**Three distinct readings per parameter. Conflating them is how a fader ends up fighting an LFO:**

| reading | meaning |
|---|---|
| `target(i)` | where control put it |
| `norm(i)` | the slewed current value |
| `normMod(i)` | that plus modulation |

Slew rate is `1 - exp(-dt/tau)`, **never** linear `dt/tau` — this engine drops the occasional 290 ms frame, where a linear coefficient of 2.9 overshoots to twice the target and rings. Below 1e-5 it snaps, so **a preset lands exactly where it was saved**. `slew = 0` means never interpolate. `setNormNow()` is for continuous gestures — dragging a fader must not glide.

`GLIDE` is the live rate; a preset recall runs at `GLIDE × 8` via a `GlideScope` at the call site. One knob, not two.

### The ONE mapping table (`src/control/mapping.*`)

Sources are **named strings**, so a new transport needs no change here: `sensor:tof1`, `cc:1:74`, `osc:/p/tilt`, `macro:0`, `lfo:0`. Many-to-one fan-out, per-destination `lo`/`hi` range, inversion by `lo > hi`, absolute/relative mode, loadable from a hot-reload-shaped text file (**not actually hot-reloaded yet** — loads once at startup).

**Add vs set is derived from the source kind, not a per-row flag** (`isModSource()`): `lfo:*` **ADDs** a bipolar offset via `addMod()`; everything else **SETs**. A flag would ship the broken option in both directions — an LFO that SET would pin every parameter it names, and a sensor that ADDed would *ratchet*, since ADD re-applies every frame. A macro is instructive: it sits with the LFOs but SETs, because it is a *knob* and has a position.

Two lifecycles, deliberately two calls:
- `apply(source, value, bank)` — event-driven, SETs, skips mod rows.
- `applyMod(source, bipolar, bank)` — every frame after `clearMod()`, ADDs, skips control rows.

For an ADD row, `lo`/`hi` is a **span**: `offset = (hi - lo) × shape × depth`. A modulator has no absolute position, so only the difference can mean anything.

### LFOs and macros (`src/mod.*`)

4 LFOs (shape/rate/depth/phase, free-running on **scene** time so SPEED affects them), 4 macros. A macro is a knob fed into the mapping table as `macro:N` — **no second mechanism was built**. Random-hold is a hash of the cycle number: no state, deterministic, testable.

**Learn gesture:** tap an LFO's TARGET button, then touch any parameter on any page. It **adds** a row; re-touching a bound knob removes that row and leaves the others. Escapes: tap again, `ESC`, or a 10 s timeout with a banner drawn on every page. Nothing drags while armed. An LFO may not target its own four knobs (its DEPTH slider sits inches from its TARGET button, and an LFO swinging its own depth through zero looks exactly like a broken one). The wire form (`/nv/lfo`) is **absolute** instead — a datagram states routing; a gesture edits it.

### Ownership: presets vs the rig

| source | owner | preset behaviour |
|---|---|---|
| `lfo:*` | **patch** | saved; a load replaces only these |
| everything else | **rig** | never saved, never touched |

A recall changes the sound, not the wiring to your hardware. An LFO learned on stage survives the next recall; loading a preset cannot cost you the pedal mapping you arrived with.

### Presets (`src/preset.*`)

Three rules: **address by id, never index**; **real units, not normalized** (a preset persists for a year and must survive a range being widened); **unknown keys skipped, missing keys keep defaults** (a preset from a newer build must load).

Order on load is load-bearing: selection and chain decide *which* knobs exist, so the bank is rebuilt before any value is applied.

---

## 11. HOME and the modes

`src/home/*`. HOME is a **view inside this binary** (`View::Home` vs `View::Synth`), not a separate program — because DRM master is exclusive and a separate menu would duplicate a GL context, font and panel renderer to draw four words, then hand the display back and forth.

Menu: **AUDIO · VIDEO · GAMES · MULTI-DIM-DSP**, titled NEPTR.

Launching an out-of-process mode is deferred to the top of the next frame, never done inside an input callback — dropping DRM master from an event handler tears the display out from under the frame being drawn.

**The DRM handoff is the fragile part, and it has bitten twice:**
- RetroArch ignores SIGTERM and gets SIGKILLed ~10 s later. HOME's reclaim window was 5 s, so HOME gave up, then drew into a CRTC it did not own — 26,729 journal lines and a black screen with no way back. Fixed: window 5 s → 30 s, plus `_exit(70)` on failure so `Restart=on-failure` restores HOME in 3 s rather than spinning.
- `games_console.py` suppresses `ui-release`/`ui-restore` because **HOME owns the handoff**. A standalone run must do HOME's half or RetroArch dies in `video_driver_init_internal()`.

**Residual risk:** if the engine crashes *after* taking DRM master, its restart loop and the fallback UI can contend until systemd's start-limit trips it to `failed`. Not observed.

---

## 12. Control plane

### The layers, and which is authoritative

```
hardware sensors ─┐
Teensy serial ────┼─> demiurge-io ──> /sensor <id:s> <norm:f>  ← THE bus (full resolution)
virtual panel ────┘         │
                            └──────> /cc <ch> <num> <val>      ← csound legacy, 7-bit
```

**`/sensor` is the control layer.** `/cc` is the *csound* vocabulary — 7-bit, and `--forward-control` is off by default so csound is not double-fed. Do not route new control through `/cc`; the user has been explicit that 7-bit is not acceptable.

### Engine ingress — UDP **8004** (`control/receiver.*`)

| address | args | meaning |
|---|---|---|
| `/sensor` | `<id:s> <norm:f>` | conditioned sensor, 0..1 |
| `/cc` | `<ch:i> <num:i> <val:i>` | legacy, always `/127` |
| `/p` | `<id:i> <val:f>` | normalized, id is an argument |
| `/nv/param` | `<id:s> <value:f 0..1>` | **normalized, id-addressed** |
| `/nv/preset` | `<n:i>` | |
| `/nv/mode` | `<n:i>` | |
| `/nv/kernel` | `<n:i>` | |
| `/nv/lfo` | `<slot:i> <id:s>` | absolute routing |

Unknown `/nv/` addresses are **dropped**, not handed to the mapping path — otherwise a stale mapping line could pick them up as `osc:/nv/...`.

`/nv/param` is normalized, never real units: a control surface has no business knowing zoom is exponential over four decades, and real units would silently change meaning the day a range is widened. (Presets go the *opposite* way, for the opposite reason — see §10.)

### Egress — the 10 Hz state broadcast

Sent to **the last peer heard from**, via `recvfrom`. That is the entire subscription mechanism; nothing is configured. Stops when the peer goes quiet.

Format: `/nv/sel <mode> <kernel>` **first** (a surface learns the shape before the values), then `/nv/state <id> <value>` per visible parameter. Split into ≤1200-byte datagrams so nothing fragments.

**It sends `target()`, not the live modulated value** — deliberately. Streaming the live value would make every fader on the phone twitch in time with the LFOs and jump around during a preset glide. Cost: a remote fader jumps to the destination at the start of a glide instead of riding it down.

**Not on the wire:** the bank's `group` field, parameter ranges, bipolar flags, kernel *names*, and mapping rows. Remote surfaces derive grouping from the id namespace. This is the main wire-format gap — see §15.

### demiurge-io decimation — the button latch

`publish()` decimates each source to 20 Hz. Correct for a curve, **wrong for an event**. The Teensy hides it by re-sending 100×/s, but the virtual panel sends **two datagrams per button**, so a tap under 50 ms has its release swallowed and, with no third datagram coming, **the button latches down forever**. On `jb` that quit the game the instant it opened.

Fixed with a `force` flag for discrete sources. Reproduced on hardware and verified fixed:
```
BEFORE  10 ms tap → [1.0]        LATCHED
BEFORE  60 ms tap → [1.0, 0.0]   clean
AFTER   10 ms tap → [1.0, 0.0]   clean
```
Regression guard: a continuous source pushed 50× over 250 ms still yields 5 frames.

---

## 13. User interfaces

### Device panel (`src/ui/panel.*`)

Paged: MAIN / COLOUR / SHAPE / SWARM / DRAW / MOD / MAP / FX / INPUT. Layout is computed from surface size every frame and flows into as many columns as fit — an 800×480 panel and a 1080p monitor both get a usable page with no hardcoded coordinates.

**Everything is drawn from ONE texture (the font atlas, whose last cell is solid) in ONE draw call** — bars, backgrounds and text share a vertex buffer. A control surface costing more than a fraction of a millisecond steals frames from the scene. Preserve this.

Theme (`ui/theme.h`), the H=296° system from the NEPTR python UI:
```
BG (28,0,30) · ACCENT (202,42,235) · TRACK_ACTIVE (220,186,222)
TRACK_DIM (92,58,94) · OFF_TEXT (155,114,158) · ALERT (255,84,120)
```

### Web UI — `http://demiurge.local:8080`

Served by `demiurge-web.service` (`demiurge_web.py --port 8080`) in the **DEMIURGE_OS** tree, not this one. Video section is `web/static/video.js` + `/api/video/state`, `/api/video/param`, `/api/video/select`.

- **VISUALS** — the whole bank as individual sliders, built from the 10 Hz broadcast. 67 parameters made playable by filter (matches id *and* label), a sticky jump-chip row per group, and fold (the group header *is* the control; folded, all 67 are a table of contents on one phone screen). Tracks stay 56 px.
- **Shortcuts** — seven curated pages (MAIN/COLOUR/SHAPE/SWARM/DRAW/MOD/FX).

The server keeps peer-ship by pinging `/nv/ping` once a second while a browser is reading. Poll uses a single-in-flight guard plus `cache: "no-store"` — a naive timer exhausts the browser's 6-connection budget (the app already holds 3 EventSources open) and then *every* request blocks, including parameter writes.

---

## 14. Building, running, testing

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j8
cd build && ctest              # 12 suites
./build/neptr-video
```

Pi deploy: `deploy/deploy-video.sh`; `neptr-video.service` is **enabled**, `neptr-ui` **disabled** (see §16).

**Env hooks — these are the test harness.** Anything untestable through them tends not to get tested:

```
NV_KERNEL=<n>  NV_KERNEL_B=<n>  NV_MORPH=<f>  NV_COLOR=<n>  NV_MODE=<n>
NV_SET="id=value,id=value"     NV_LFO="0=g.detail"     NV_MAPPINGS=<path>
NV_PRESETS=<dir>  NV_LOAD=<n>  NV_SAVE=<n>
NV_SHOT=<path>  NV_SHOT_FRAME=<n>  NV_SELFTEST  NV_PAGE=<n>  NV_MONITOR
NV_SCRIPT="10:tap:0.1,0.6;40:key:3+ctrl"   replays input into the real callbacks
NV_GEOM=WxH  NV_ROTATE=<deg>  NV_PANEL_CONNECTOR=<name>  NV_OSC_PORT=<n>
NV_POOL=<n>  NV_INLINE_COMPILE  NV_HOME  NV_PANEL  NV_HOME_INVERT_Y
```

**Verification standard used throughout this project:** claims come from *running the binary and measuring*, not from reading code. Frame captures with pixel-difference or brightness statistics; mutation-checking new tests (deliberately break the logic, confirm assertions fire); reproducing a bug **before** fixing it. Several real bugs here were found only by running, and at least one "fix" was proposed against a non-bug because the reading was stale.

---

## 15. Known gaps

**Wire format**
- No verb carries **kernel names** — remote surfaces show `#n` and step. *(Being added with the motion work.)*
- No verb reads or writes **mapping rows**; the MAP page is device-only. Recommended shape, already written into `deploy/README.md`: `/nv/map <source:s> <target:s> <lo:f> <hi:f>`, rows broadcast as `/nv/maprow`, plus `/nv/mapclear <source:s>`.
- No group names, ranges or bipolar flags on the wire — web derives them from id namespace and a hardcoded bipolar list.
- No preset **saving** from the web, no chain editing from the web.

**Persistence**
- **The mapping table is never written to disk.** This is why macro learn-by-touch is not shipped: a binding would survive a recall and still be lost on next start. One missing piece, not an open question.
- Mappings load once at startup, not hot-reloaded.

**Untested / unverified**
- **Touch on HOME** — hit-test is unit-tested, nobody has tapped the glass.
- Whether Galaga itself receives the fire button (`b1` doubles as the `ARCADE` launch token; input demonstrably reaches the supervisor).
- LFO depth modulating the picture *from the phone*.
- Preset slots against real preset files — **there are no preset files on disk yet**.
- The video web page in a real browser on the Pi (routes and APIs verified; DOM/layout not).
- Analog joystick nav (`cc:1:28`) is not on the telemetry bus — d-pad and face buttons cover HOME.

**Divergence**
- The **Mac `DEMIURGE_OS` tree lacks patches 1 and 2** that are live on the Pi. A naive deploy would revert them. Nothing is committed anywhere.
- `~/_______DEMIURGE/web/demiurge_web.py` on the Pi is behind `/opt` (pre-existing).

**In flight**
- Intrinsic motion + bipolar SPEED + continuity fix + kernel-names verb.

---

## 16. Operational notes (Pi)

Services: `neptr-video` (enabled, HOME at boot), `neptr-ui` (**disabled**, started on demand by `mode-audio.sh`), `demiurge`, `demiurge-io`, `demiurge-web`, `demiurge-touch-poll`.

**`live.conf`'s `ui` key is NOT an autostart flag.** It is read by `demiurge-ui-launch.sh`, which *is* `neptr-ui.service`'s ExecStart — so `ui = off` makes the unit exit 0 no matter who starts it, and AUDIO becomes a flicker. "No autostart, still start on demand" is **systemd enablement**, not that key. (`live.conf`'s documented `video =` key is read by nothing.)

`demiurge-ui-ensure` is the boot backstop and is HOME-aware: running engine → nothing; HOME enabled but down → push HOME once; only if HOME will not run does it fall back to `neptr-ui`. Fallback verified by moving the binary aside.

### Foot-guns that have cost real time

- **`pkill -f <pattern>` over ssh matches your own ssh command line** and kills your shell with no output. Use `pkill -x`. This has happened twice.
- `demiurge-io.service` had `ConditionPathExists=/dev/ttyACM0` — on a boardless rig the daemon never started at all.
- Unbounded `journalctl` on a power-marginal Pi contributed to a dropout. Always `-u <unit> -n <N> --no-pager`.
- The Pi's wifi has been the dominant source of lost time. Root causes found: a `HOME_SSID` enforcement mismatch tearing down working links every 20 s, and NetworkManager looping `asking for new secrets`. Journald is now persistent, which is why log spam matters (one line/second was ~86k lines/day).
- macOS has no `timeout`; zsh does not word-split `env $1`. Both silently no-op test runs.
- Multiple agents driving one Pi contend for DRM master. Serialize hardware work.

---

## 17. Invariants — break these and something subtle fails

1. Parameters are **id-addressed**; the bank's tail is dynamic.
2. `/nv/param` and `/nv/state` are **normalized**; presets are **real units**.
3. The state broadcast sends **target**, not the modulated value.
4. Scene time is **accumulated**, not `wallClock × speed`.
5. Exponential forms everywhere a rate meets a variable `dt` (`1-exp(-x)`), never linear — frames get dropped.
6. **Nothing that accumulates may ratchet.** Bounded RGB with monotonic alpha still destroys the image.
7. Particles carry the field's **actual RGB**; re-palettizing luma is a second colouring.
8. Update and draw shaders must derive particle **HOME identically**.
9. The panel is **one draw call from one atlas**.
10. **One mechanism per job.** Macros ride the mapping table; LFOs now do too. When two mechanisms appear, consolidate before shipping.
11. Only PRESENT rotates.
12. VAOs are per-context.
13. Death must be invisible at cohesion 1.
14. Unknown ids — parameters, mappings, preset keys — are **ignored or kept, never fatal**.
