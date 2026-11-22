# Machine Lab OSC Contract — v1.3 devices (as read 2026-09-04)

Ground truth extracted by unpacking the embedded `patcher` JSON from each
`.amxd` in `_M4L/_machineLabM4L/DELIVERABLES/` (v1.3, current). Compared
against `DELIVERABLES april/` (v1.2) for the diff section at the bottom.

Method: `strings -n 4 FILE.amxd` to locate object text, and a byte-level
brace-balanced extraction of the JSON starting at the `{` before `"patcher"`
(handles embedded quoted strings correctly; `json.loads` on the result
parses cleanly for all seven files). Every device builds its outgoing
message the same way: `notein`/`stripnote` → `route <note-list>` (gates
which notes are wired to a real trigger) → `pack s i i` (address-string,
note, velocity) → `s LOCALITY___` → `udpsend <host> <port>`. All seven
`udpsend` objects read **`udpsend 192.168.0.103 8001`** verbatim in every
v1.3 file — the mini's Ethernet IP is current, not stale, in this
generation of devices (v1.2 hardcoded `192.168.0.15`, see diff section).

Wire format for every device: `/<address> <note:int> <vel:int>` — a single
OSC message, two int args, in that order (never a bundle). No device sends
floats, strings, or extra args on this path.

## Per-robot table

| Robot | OSC address (device object, `udpsend` line) | Target host:port (in file → corrected live) | Route-gated notes (the only notes that actually produce a message) | Documented full note range (comment box) | Arg order / types | Velocity as sent | Beater/element naming found | Max re-strike rate | Front-door handler |
|---|---|---|---|---|---|---|---|---|---|
| **BreakBot** | `/breakBot` | `192.168.0.103:8001` (matches live) | `0, 1, 5, 7, 11, 12` (6) — `route 0 1 5 7 11 12`, named `zero one five seven eleven twelve` | `(C-2)–(C0) / 0–12` | `i note, i vel` via `pack s i i` | `scale 0 127 65 127` floor+`VELO min` comment ⇒ wire vel ∈ **[65,127]**, not 0/1–127 | none beyond note number (no `live.menu`/named beaters found) | unknown — measure | **live**: MIDI port 1 (project memory, not re-verified this session — mini unreachable) |
| **GanaPati** | `/ganaPati` | `192.168.0.103:8001` (matches live) | `1, 2, 3, 6, 7, 8, 11, 12, 13, 14` (10) — `route 1 2 3 6 7 8 11 12 13 14` | `(F#-2)–(D0) / 6–14` (note: comment range 6–14 doesn't cover route notes 1–3; unresolved, see open questions) | `i note, i vel` | `scale 0 127 65 127` + `VELO min` ⇒ wire vel ∈ **[65,127]** | a legend text box reads `1 1A / 2 1B / 3 2A / 7 2B / 8 3A / 10 3B / 12 4A / 13 4B / 14 5A` — looks like note→beater-position labels (5 beaters, A/B strike zones) but note 10 isn't in the route list and note 6/11 aren't in the legend, so the mapping is **partial/inconsistent**; cite verbatim, don't trust as complete | unknown — measure (mechanical note: **thermally shuts down if driven too fast**, per project memory — not in the device file itself, **INFERRED** from prior verified session) | **live**: MIDI port 0 (project memory) |
| **Tammy** | `/tammy` | `192.168.0.103:8001` (matches live) | `1, 2, 3, 4, 5, 6, 7, 8, 11, 12, 13, 25` (12) — `route 1 2 3 4 5 6 7 8 11 12 13 25`, named `one two three fo fa si se ei lev twe thir twefi` (abbreviated number words, not instrument names) | `(C#-1)–(C#1) / 1–25` | `i note, i vel` | `scale 0 127 65 127` + `VELO min` ⇒ wire vel ∈ **[65,127]** | none (abbreviated ordinals only) | unknown — measure | **live**: MIDI idx 3, re-enabled (project memory) |
| **Lydia** | `/trimpbeat` (device is titled LYDIA but the wire address is `trimpbeat`) | `192.168.0.103:8001` (matches live) | `62, 63, 64, 65, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76` (14; 66 is skipped) — `route 62 63 64 65 67 68 69 70 71 72 73 74 75 76`, named `sitw sith sifw sifa sise siei sini sev seo setw sethr sefo sefi sesi` (syllable code, not instrument names) | `(D3)–(E4) / 62–76` | `i note, i vel` | `scale 0 127 65 127` + `VELO min` ⇒ wire vel ∈ **[65,127]** | none | unknown — measure | **live**: OSC forward to `localhost:50000` (project memory) |
| **Marimbot** | `/marimba` | `192.168.0.103:8001` (matches live) | 32 notes: `45,47,48,50,52,53,54,55,57,59,60,62,64,65,66,67,69,71,72,73,74,77,79,81,83,84,86,89,90,92,94,93` — `route <32 notes>`, named with syllable codes (`fofa fose foei fifi …`), not instrument names | `(A2)–(Ab6) / 45–62` (comment's trailing numeric range looks truncated/stale vs. the actual 45–94 the route covers — see open questions) | `i note, i vel` | `scale 0 127 65 127` + `VELO min` ⇒ wire vel ∈ **[65,127]** | none (pitched bars; note number is the real information — used for `mu[freq]` via `k = log(hz/20)/log(1000)`, `hz` from standard MIDI note→Hz, `hz = 440·2^((note−69)/12)`) | unknown — measure | **live**: OSC forward to `localhost:50000` (project memory) |
| **Modulettes** | `/modulettes` | `192.168.0.103:8001` (matches live) | `0,1,2,3,4,5,6,7,8,9,10` (11) — `route 0 1 2 3 4 5 6 7 8 9 10`, named `one two three four five six seven eight nine ten eleven`. Note: the sample message in `MACH_ORCH_CLASS.maxpat` uses `/modulettes 21 127` and the comment documents `0–63`, i.e. the device's own note range and test traffic go well past what its `route` gates — the wider range likely feeds a different (generative-UI) path; treated `route` as the real trigger set here (see open questions) | `(C-1)–(Eb4) / 0–63` | `i note, i vel` | **no `65 127` floor found** — `scale` objects present are `0 127 0.5 1.`, `0. 1. 0.4 1.`, `1. 4. 0. 1.`, `5. 8. 0.4 1.` (none matching the VELO-min pattern) ⇒ wire vel ∈ **[0,127]**, unfloored, as read | none | unknown — measure | **commented out** (project memory: only reachable via direct MIDI notes 0–15 to `Teensy MIDI`, velocity = strike-ms — this alternate path is **INFERRED**, sourced from prior verified session memory, not from this session's file read, since the front-door script lives only on the unreachable mini) |
| **Rattletron** | `/rattleTron` | `192.168.0.103:8001` (matches live) | `0,1,2,3,4,5,6,7,8,9,10` (11) — identical structure to Modulettes (`route 0 1 2 3 4 5 6 7 8 9 10`) | `(C-1)–(Eb4) / 0–63` | `i note, i vel` | same as Modulettes — no floor found ⇒ **[0,127]** | none | unknown — measure | **commented out** (project memory, same caveat as Modulettes — **INFERRED**) |

Every value above with a file+object citation was read directly from the
`.amxd` JSON this session. Front-door routing (MIDI port indices, the
`localhost:50000` OSC forward, and the commented-out Modulettes/RattleTron
handlers) lives in a server script on the Mac mini
(`~/git/machineLabCode/...`), which was **not reachable this session**
(`ssh mtiid@192.168.0.103` timed out — see below) — those cells are carried
over from project memory and are marked **INFERRED**, not re-verified
against the live front-door source this session.

## Notes on `pack s i i` / velocity floor

Five of the seven devices (BreakBot, GanaPati, Tammy, Lydia, Marimbot) share
an identical velocity-conditioning chain: a `scale 0 127 65 127` object
sitting right under a `VELO min` comment, immediately before the value
reaches `pack s i i`. That means **the wire velocity these devices actually
send is bounded to 65–127**, not the full 1–127 (or 0–127) MIDI range —
even a velocity-0 hit from Ableton's generative mode gets floored to 65 so
the physical striker always moves with enough force to sound. Modulettes
and RattleTron do **not** have this object anywhere in their chains (their
`scale` objects use different ranges entirely, none matching `65 127`), so
their wire velocity is the plain **0–127** MIDI value, consistent with
project memory's note that Modulettes' hardware-side velocity is
repurposed as strike duration in milliseconds rather than force.

## Mini reachability this session

`ssh mtiid@192.168.0.103` (password `blackberry`) **timed out** (`Operation
timed out` on port 22) — the mini was not reachable from this network this
session, as project memory predicted. No `cat` of
`~/git/machineLabCode/updatedTriggerList.txt` or
`~/Desktop/GOR_STYLE/docs/INSTRUMENT_COMMS.md` was possible; nothing on the
mini was read, and nothing was written or restarted there (this task never
attempted anything beyond the one read-only reachability check).

## `lab_map.json` construction notes

- One catalog point per **route-gated** note (the notes with a real wired
  trigger), not every note in the documented comment-box range — the wider
  ranges (e.g. Modulettes' `0–63`, Marimbot's implied `45–94`) appear to
  serve other UI/generative paths inside the M4L device that never reach
  `pack s i i` / `udpsend`. 96 points total (BreakBot 6, GanaPati 10, Tammy
  12, Lydia 14, Marimbot 32, Modulettes 11, RattleTron 11); Modulettes and
  RattleTron are `"enabled": false` (front door commented out) but present
  so the catalog stays complete once that path is restored.
- `element` names are `beater_<note>` (percussion) or `bar_<note>`
  (Marimbot) — no device exposes real instrument-part names per note
  (GanaPati's partial `1A/1B/2A/…` legend is the one exception, and it's
  internally inconsistent with its own `route` list, so it was **not**
  used to name `element` fields; see open question #2).
- `mu[freq]` (index 0): computed from actual pitch only for Marimbot
  (`k = log(hz/20)/log(1000)`, `hz = 440·2^((note−69)/12)`), since it's the
  only instrument whose note number is a real, standard-tuned MIDI pitch.
  All other instruments are **estimated** by family (drum: low-ish freq
  ~0.22–0.4, harm ~0.15–0.3, scale ~0.55–0.6, rate at/below the 0.05 dead
  zone i.e. "static"; shaker/rattle for Modulettes/RattleTron: harm 0.1,
  rate 0.6) with a small linear spread across each device's own note index
  so its points aren't all identical, plus nonzero sigma on every axis that
  was estimated rather than read.
- `az`/`spread`/`dist`: **no room-layout document was found** describing
  where each robot physically sits (checked `MACH_ORCH_CLASS.maxpat`, the
  website `machine-lab/*.md` pages, and the `_M4L` folder generally) — every
  point uses `az=0.5, σ=0.3` per the task's fallback instruction, plus
  `spread=0.05, σ=0` (robots are point sources) and `dist=0.4, σ=0.2`
  (unknown room position, same value/uncertainty for every robot).
- `max_rate_hz`: **unknown for every robot** except GanaPati, where project
  memory's thermal-shutdown note justifies estimating a lower ceiling
  (6 Hz) than the rest (8–12 Hz); all are `"source": "estimated"`,
  Phase 4 measures the real numbers.
- Every point has `"source": "estimated"` per the task spec; nothing here
  was measured against the physical lab.

## v1.2 → v1.3 differences

- **IP fix**: every v1.3 device's `udpsend` now reads
  `192.168.0.103 8001`; every v1.2 device (`BREAKBOT_v1.2`, `GANA_PATI_v1.2`,
  `LYDIA_v1.2`, `TAMMY_v1.2`, `MARIMBOT_v1.2.2`, `MODULETTES_v1.2.2`) reads
  the stale `192.168.0.15 8001`. Confirmed by `strings`-grepping every
  `udpsend` line in both `DELIVERABLES/` and `DELIVERABLES april/`.
- **New device**: `RATTLETRON_v1.3.amxd` has no v1.2 counterpart — it first
  appears in the v1.3 drop. Its patcher is structurally near-identical to
  Modulettes' (same object graph, same `route 0…10` gate, same missing
  velocity floor), just re-addressed to `/rattleTron` and re-labelled.
  Whether it's a renamed/forked Modulettes or a genuinely separate
  physical unit is unknown from the file alone.
- Beyond the IP and the new RattleTron device, the v1.2 and v1.3 message
  grammars (`pack s i i`, `route <notes>`, velocity floor objects, comment
  boxes) are unchanged for every device that exists in both drops — this
  was a network-fix + one-new-device release, not a protocol change.

## Open questions for the user

1. **Front-door routing not re-verified this session.** MIDI port indices
   for BreakBot/GanaPati/Tammy, the `localhost:50000` OSC-forward targets
   for Marimbot/Lydia, and the "Modulettes/RattleTron commented out"
   status all come from project memory, not a fresh read of the mini's
   front-door script (unreachable). Worth a session where the mini is up
   to confirm these haven't drifted, especially since RattleTron is new
   and may need its own front-door case added (or may already share
   Modulettes' commented-out one).
2. **GanaPati's `1A/1B/2A/2B/3A/3B/4A/4B/5A` legend is inconsistent** with
   its own `route 1 2 3 6 7 8 11 12 13 14` list (legend references note 10,
   which isn't in the route; route's notes 6 and 11 aren't in the legend).
   Is this legend stale (from an earlier beater layout), or does it
   describe a real 5-beater/A-B-strike-zone physical design that the
   current `route` list only partially reflects? If real, `lab_map.json`'s
   GanaPati `element` names should be redone using it.
3. **Comment-box note ranges vs. `route` lists disagree** for GanaPati
   (`6–14` comment vs. `1–14` route), Marimbot (`45–62` comment vs. what
   the 32-note route implies is really `45–94`), and Modulettes/RattleTron
   (`0–63` comment / `/modulettes 21 127` test traffic vs. `0–10` route).
   This manifest trusts `route` as "the notes that actually produce a
   wire message" — is that the right read, or do the wider comment-box
   ranges reach the robot through some other path this survey missed?
4. **No mechanical re-strike-rate numbers exist anywhere read this
   session** (not in the `.amxd`s, not in `MACH_ORCH_CLASS.maxpat`, not in
   the website docs). Every `max_rate_hz` in `lab_map.json` is a guess;
   Phase 4 needs real measurement, especially for GanaPati given its
   thermal-shutdown history.
5. **No room-layout document exists** giving each robot's physical
   position — every `az` in the manifest is `0.5 ± 0.3` (i.e. "don't
   know"). If a layout exists somewhere outside `_M4L` (e.g. in
   `smartGOR`/GORMAPPER's room model), pointing this task at it would let
   a v1 manifest give real azimuths.

## Trigger list cross-check (2026-09-08)

Cross-checked against the mini's `updatedTriggerList.txt` (verified
2026-09-08, per project memory — the mini itself was not reached this
session; the list content was supplied directly, not re-read from the
mini). `lab_map.json` was updated accordingly:

- **BreakBot** — 'X' (broken): notes **0, 7**. Working: **1** (Kick 1.2),
  **5** (Snare brush), **6** (Ride 1.1), **8** (Ride 1.3), **11** (Crash
  1.1), **12** (Crash 1.2). `lab_map.json`'s BreakBot route only has
  points for notes 0, 1, 5, 7, 11, 12 — notes 0 and 7 map directly to
  existing points (`point_id` 0 and 3), which are now `"enabled": false`
  (points kept, not removed). Notes 1, 5, 11, 12 map to existing points
  (`point_id` 1, 2, 4, 5), which stay enabled and now carry the trigger
  list's `element` names (Kick 1.2 / Snare brush / Crash 1.1 / Crash
  1.2). Notes 6 and 8 from the "working" list have **no corresponding
  point** in `lab_map.json` (BreakBot's `route` never gated those notes
  per this file's own survey above) — nothing to enable/disable for them;
  flagged here for whoever revisits the BreakBot `route` object on the
  mini.
- **GanaPati** — 'X' (broken): notes **0, 4, 5**. Working: **1** (Drum 4),
  **2** (Drum 3.1), **3** (Drum 3.2), **6** (Drum 1.1), **7** (Drum 1.2),
  **8** (Drum 1.3). All six working notes map directly onto existing
  enabled points (`point_id` 6–11), which now carry those `element`
  names. Notes 0, 4, 5 marked broken have no corresponding point in
  `lab_map.json` either (GanaPati's `route` per this file's survey is `1
  2 3 6 7 8 11 12 13 14`, never 0/4/5) — nothing to disable there.
  GanaPati notes **11–14** (`point_id` 12–15) are **not present at all**
  in `updatedTriggerList.txt` — kept in the catalog but marked
  `"enabled": false` with `"note": "unverified — not in
  updatedTriggerList"`, pending a future check against the mini.
- **Tammy** — front door currently routes to **MIDI port 3** (per project
  memory, matches this file's per-robot table above). No trigger-list
  X/working data was supplied for Tammy this pass; its `lab_map.json`
  points are unchanged.
- **Marimbot + Lydia (`/trimpbeat`)** — front door forwards both to
  **OSC `localhost:50000`**, not straight to the physical device (per
  project memory, matches this file's per-robot table above). No
  trigger-list data was supplied for either this pass; unchanged.
- **RattleTron** — the front door sends its messages via `messageSend` on
  a `MidiOut` object whose `init` call is **commented out** in the mini's
  server script, i.e. an uninitialised MIDI output — so RattleTron is
  effectively dead at the front door today, independent of anything in
  this file's own per-`.amxd` survey (which already had RattleTron's
  Max patch reachable but its "commented out" front-door status
  inferred, never confirmed). All of RattleTron's `lab_map.json` points
  were already `"enabled": false` (Modulettes/RattleTron were disabled
  from the original manifest build, see the "`lab_map.json` construction
  notes" section above); reconfirmed and left as-is.

None of this changes `load_lab_map`'s behaviour or contract — disabled
points are still kept in the raw dict (so `direct_strike`/`lab_osc` can
still look up their OSC template) and excluded only from
`Manifest.points` / catalog selection / the Direct-mode default 4-row
instrument pick, exactly as before.

## GOR Python server contract (2026-09-08)

The Machine Lab now has a **GOR Python server** built and reachable on the
mini (`192.168.0.103:8001`) as an alternative front door to the existing
ChucK/Max front door surveyed above. It speaks the *same wire format* as
everything documented in this file — `/<address> <note:int32>
<vel:int32>`, UDP, no bundles — so `lab_osc`/`load_lab_map` needed no
protocol change, only tighter per-instrument note/velocity clamps and a
liveness check. `lab_map.json` was updated to the exact clamps below
(grid-side `max_rate_hz` set to each actuator's server-side floor, so the
grid never sends faster than the server will accept, and never assumes a
note/velocity the server would silently drop).

**Liveness**: send `/ping` (no args) to `192.168.0.103:8001` → the server
replies `/pong gor_server <uptime:int32>` to the sender's own ip:port (not
to a fixed listener port — the socket that sent `/ping` must also be the
one that receives, or open one socket for both). `septagraph.renderers.lab.LabRenderer`
implements this as `start_liveness()`: sends `/ping` every 2 s on the same
UDP socket used to strike the lab, parses `/pong gor_server <int>` replies,
and exposes `lab_reachable` (true iff a `/pong` arrived within the last
5 s), `lab_uptime` (the int from the last `/pong`), and `last_pong_age_s`.
**Liveness is always on** in the real app (`septagraph.pi.app` and
`python -m septagraph.renderers.lab`'s own CLI both call
`start_liveness()` unconditionally after constructing the renderer) — there
is no chuck/python mode switch; the grid sends the same OSC regardless of
which front door is listening on the mini. `/api/state` exposes
`lab_reachable` / `lab_uptime` / `last_pong_age_s`, and the web UI's header
shows a "LAB" pill: green when reachable, red otherwise.

| Instrument | OSC address | Notes | Velocity clamp | Per-actuator min interval | `max_rate_hz` used in `lab_map.json` |
|---|---|---|---|---|---|
| BreakBot | `/breakBot` | 0–12 | 65–127 | 60 ms | 16.7 |
| GanaPati | `/ganaPati` | 1–14 | 65–127 | **140 ms** (thermal protection) | 7.1 |
| Tammy | `/tammy` | 0–25 | 1–127 | 50 ms | 20 |
| Modulettes | `/modulettes` | 0–15 | 0–127 (= strike ms) | 50 ms | 20 |
| RattleTron | `/rattleTron` | 0–10 | 0–127 | 50 ms | 20 |
| Marimbot | `/marimba` | **must be in the 36-note scale below — other notes are DROPPED by the server** | 0–127 | 50 ms | 20 |
| Lydia | `/trimpbeat` | 60–79 | not specified by the server contract; `lab_map.json` keeps the existing 65–127 clamp | 50 ms | 20 |

Marimba scale (36 notes; any note outside this set is silently dropped by
the server, so `lab_map.json` marks the three prior marimbot points that
fall outside it — 73, 92, 94 — `"enabled": false` with
`"note": "not in server scl — dropped by server"` rather than deleting
them):

```
45 47 48 50 52 53 54 55 57 59 60 62 64 65 66 67 69 71 72 74 76 77 78 79
81 83 84 86 88 89 90 91 93 95 96 97
```

**Aliases**: `/trimpspin` (60–79) and `/stringthing` (0–4) also exist on
the server's own instrument list, but the grid never sends either — no
`lab_map.json` points reference them, and none should be added; they are
documented here only so a future reader doesn't mistake their absence for
an oversight.

**Rate-limit semantics**: the server itself drops any second hit to the
*same actuator* (`point_id` in `lab_map.json` terms) that arrives sooner
than its listed interval — it does not queue or delay the extra hit, it
silently discards it. `lab_map.json`'s per-point `max_rate_hz` is set to
exactly that floor (`1000 / interval_ms`, e.g. GanaPati's 140 ms → 7.1 Hz)
so `LabRenderer._rate_ok` / `direct_strike`'s own rate limiter never sends
a hit the server would have dropped anyway, and never holds a point to a
rate slower than the server actually allows.

**Broken beaters** (physical, independent of which server is listening —
per the mini's `updatedTriggerList.txt`, already reflected above in the
"Trigger list cross-check" section): BreakBot notes 0 and 7, GanaPati
notes 0, 4, 5 stay `"enabled": false` regardless of server.

**No mode switch**: earlier drafts of this integration considered a
`--lab-server chuck|python` switch to change which instruments/notes the
grid would emit (e.g. leaving Tammy/Modulettes/RattleTron disabled when
only the ChucK front door was live). That approach was dropped — the grid
now always enables every instrument the lab can route (subject only to
each point's own `"enabled"`, i.e. the physically-broken beaters and the
three out-of-scale marimbot notes above), and always runs `/ping`
liveness. Whichever front door is actually listening on the mini decides
whether a given OSC message reaches a robot; the grid's job is just to
never send something outside the documented clamps.

## Re-survey 2026-09-08 (union)

Re-parsed `BREAKBOT_v1.3.amxd`, `GANA_PATI_v1.3.amxd`, `TAMMY_v1.3.amxd`,
`LYDIA_v1.3.amxd`, `MARIMBOT_v1.3.amxd` directly from the structured
`patcher` JSON (boxes + patchlines walked programmatically, not `strings`)
to build the UNION of every note number each device can actually emit,
instead of trusting `route`/`sel` alone. Modulettes/RattleTron were left
untouched (out of scope).

**Notein → pack passthrough check**: for all five devices, `notein` feeds
`stripnote` which feeds the `route <notes>` object (word-named outlets) →
per-note `message` boxes (`zero`/`one`/…) → `pack s i i`. No device has an
unfiltered `notein`/`stripnote` wired straight into `pack`/`prepend` —
every path to `pack s i i` was traced and originates from a `route`-gated
message box. **No 0–127 passthrough exists on any of the five devices**;
the "practical set" below is the full union actually found, not a
fallback from an unfiltered path.

**Other sources checked and found empty**: no `umenu`/`live.menu`/
`live.tab` objects exist in any of the five patchers (only two
`live.dial`s per device, both generative controls — `chance` 0–100 and
`subd` 0–7, unrelated to note selection). No drum-rack-style note→sample
map exists in the patcher JSON. `_SAMPLES.zip` sample filenames
(`bb1..bb6`, `gana1..gana10`, `tam1..tam12`, `lydia1..lydia14`,
`marbo1..marbo32`) are sequentially numbered per-device, **not** tagged
with MIDI note numbers, and each device's sample count matches its
`route` count exactly — sample names confirm the `route` set but add no
new note numbers (no `sample-name` tags below).

The one real new source across all five is each device's own **comment
box note-range** (`(C-2)-(C0) / 0-12` etc.), which for BreakBot/GanaPati/
Tammy/Lydia turned out to name a *wider* contiguous range than `route`
gates — the union below fills every integer in that comment range in
alongside the `route` set. Marimbot is the exception: its comment
(`45-62`) is narrower than what `route` already covers (`45-94`, 32
notes) — already flagged as stale/truncated in the original survey — so
it adds nothing; `route` stays Marimbot's full practical set unchanged,
consistent with its sample count (32 samples, 32 route notes, exactly
32 points, no gaps filled from the comment).

### BreakBot (6 → 13 points)

| note | source tags | old status | new status |
|---|---|---|---|
| 0 | route | enabled=false (broken, updatedTriggerList) | enabled=false (unchanged) |
| 1 | route | enabled=true (Kick 1.2) | enabled=true (unchanged) |
| 2 | comment | absent | **new**, enabled=true, `beater_2` |
| 3 | comment | absent | **new**, enabled=true, `beater_3` |
| 4 | comment | absent | **new**, enabled=true, `beater_4` |
| 5 | route | enabled=true (Snare brush) | enabled=true (unchanged) |
| 6 | comment | absent | **new**, enabled=true, `Ride 1.1` (trigger-list name) |
| 7 | route | enabled=false (broken, updatedTriggerList) | enabled=false (unchanged) |
| 8 | comment | absent | **new**, enabled=true, `Ride 1.3` (trigger-list name) |
| 9 | comment | absent | **new**, enabled=true, `beater_9` |
| 10 | comment | absent | **new**, enabled=true, `beater_10` |
| 11 | route | enabled=true (Crash 1.1) | enabled=true (unchanged) |
| 12 | route | enabled=true (Crash 1.2) | enabled=true (unchanged) |

Notes 6 and 8, previously flagged in the trigger-list cross-check as
"working but no corresponding point," now have points and carry their
trigger-list names (`Ride 1.1`, `Ride 1.3`). Notes added: **+7** (comment
range `0-12` fills the gaps `route` skipped). Broken notes 0 and 7 stay
`enabled: false` as required.

### GanaPati (10 → 12 points)

| note | source tags | old status | new status |
|---|---|---|---|
| 1 | route | enabled=true (Drum 4) | enabled=true (unchanged) |
| 2 | route | enabled=true (Drum 3.1) | enabled=true (unchanged) |
| 3 | route | enabled=true (Drum 3.2) | enabled=true (unchanged) |
| 6 | route | enabled=true (Drum 1.1) | enabled=true (unchanged) |
| 7 | route | enabled=true (Drum 1.2) | enabled=true (unchanged) |
| 8 | route | enabled=true (Drum 1.3) | enabled=true (unchanged) |
| 9 | comment | absent | **new**, enabled=true, `beater_9` |
| 10 | comment, legend | absent | **new**, enabled=true, `beater_10` (legend calls it "3B" but per open question #2 the legend is not trusted for naming, so it keeps the fallback name) |
| 11 | route | enabled=false (unverified, not in updatedTriggerList) | enabled=false (unchanged) |
| 12 | route | enabled=false (unverified, not in updatedTriggerList) | enabled=false (unchanged) |
| 13 | route | enabled=false (unverified, not in updatedTriggerList) | enabled=false (unchanged) |
| 14 | route | enabled=false (unverified, not in updatedTriggerList) | enabled=false (unchanged) |

Notes added: **+2** (comment range `6-14` fills notes 9 and 10 that
`route` skipped; note 10 also appears in the beater legend). Broken
notes 0, 4, 5 were never in `route`, the comment range, or the legend —
they still have no corresponding point, so there is nothing to
disable; this satisfies the "must stay disabled" requirement vacuously,
same as the original survey.

### Tammy (12 → 25 points)

| note | source tags | old status | new status |
|---|---|---|---|
| 1–8, 11–13, 25 | route | enabled=true | enabled=true (unchanged, 12 pts) |
| 9, 10, 14–24 | comment | absent | **new**, enabled=true, `beater_<n>` (13 pts) |

No trigger-list working/broken data was ever supplied for Tammy, so none
of its notes carry special names or disabled status. Notes added:
**+13** — the comment range `1-25` (also confirmed by a second, separate
`1-25` comment box in the patcher) fills every gap `route` left between
1 and 25.

### Lydia (14 → 15 points)

| note | source tags | old status | new status |
|---|---|---|---|
| 62–65, 67–76 | route | enabled=true | enabled=true (unchanged, 14 pts) |
| 66 | comment | absent | **new**, enabled=true, `beater_66` |

Note added: **+1** — the comment range `62-76` includes note 66, the one
gap `route` skips (`route` reads `62 63 64 65 67 68 69 70 71 72 73 74 75
76`, deliberately omitting 66). No trigger-list data was supplied for
Lydia.

### Marimbot (32 → 32 points, unchanged)

No new sources found: no menu/umenu, no notein→pack passthrough, sample
count (32) matches route count (32) exactly, and the comment's `45-62`
range is a strict subset of what `route` already covers (`45-94`
non-contiguous). `route` remains Marimbot's complete practical set;
no points added or removed.

### `mu[0]` recomputation for expanded devices

For BreakBot, GanaPati, Tammy, and Lydia — the four devices that gained
points — `mu[0]` was **recomputed for every point in that device's
block** (existing and new) using the same per-instrument `base`/`span`
constants the original survey established (verified by back-solving the
existing file: `mu[0] = base + idx * (span / (N-1))`, `idx` = the note's
rank in the device's own sorted note list):

| instrument | base | span | old N | new N |
|---|---|---|---|---|
| breakBot | 0.155 | 0.25 | 6 | 13 |
| ganaPati | 0.195 | 0.25 | 10 | 12 |
| tammy | 0.12 | 0.20 | 12 | 25 |
| lydia | 0.30 | 0.20 | 14 | 15 |

The endpoints (`mu[0]` of the lowest and highest note) are unchanged;
only the spacing between them shrinks to fit the extra points, which
also nudges the `mu[0]` of pre-existing points slightly (e.g. BreakBot
note 12 stays exactly `0.405`, but note 5's `mu[0]` moves from `0.255`
to `0.2592` since it's now index 5 of 13 instead of index 2 of 6). No
other field on any pre-existing point was touched — `point_id`,
`osc.address`, `osc.args` (note number), `element` name, `enabled`,
`vel_range`, `mu[1..7]`, `sigma`, `max_rate_hz`, `vel_min`/`vel_max`, and
`source` are all identical to before this pass. Marimbot's `mu[0]`
(real-pitch-derived, not index-derived) is untouched since it gained no
points.

### Summary

| robot | old → new count | main new sources |
|---|---|---|
| BreakBot | 6 → 13 | +comment (range `0-12`), trigger-list names recovered for 2 |
| GanaPati | 10 → 12 | +comment (range `6-14`), +legend (note 10) |
| Tammy | 12 → 25 | +comment (range `1-25`) |
| Lydia | 14 → 15 | +comment (range `62-76`, fills gap note 66) |
| Marimbot | 32 → 32 | no new sources; comment range is a stale subset of `route` |

`lab_map.json` total points: 96 → 119 (23 new points, ids 96–118,
continuing from the prior max of 95). Validated with
`python3 -m json.tool lab_map.json` after the edit. Modulettes and
RattleTron sections/points were not touched.

## 2026-09-10 — modulettes 16–63 added (64-pad MODULETTES layout)

modulettes 16–63 added at the user's request (64-pad layout); the GOR
server contract lists 0–15 — notes above 15 may be dropped server-side
until confirmed.
