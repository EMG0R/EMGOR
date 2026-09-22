# Papers: Accuracy Pass and Season Restructure

**Date:** 2026-09-21
**Author:** Emory Smith (with Claude)
**Status:** Approved for implementation

---

## Problem

The papers section of the site was written 2026-08-02/03 from a planning
document last edited 2026-07-02. Since then the underlying work moved
substantially, and the site now states things that are false:

- **Digital Luthier** is described as a CalArts thesis rendered in NIME
  format with seven hardware case studies. It is actually a Process Report
  submitted to ROTURA — *Revista de Comunicação, Cultura e Artes* — under
  double-blind review, co-authored with Ajay Kapur, with exactly two case
  studies: NEPTR and BOUBA.
- **BOUBA** is described as "design stage, no repository yet," running a
  Teensy 4.0 under ChucK with one internal speaker, a battery, an aux out,
  and a web mirror. BOUBA is built: quadraphonic, roughly $80 USD to
  reproduce, programmed with the Teensy Audio Library, four speakers from
  two PCM5102 DACs amplified by four LM386s, powered from a 9 V jack, and
  in consistent use for two months.
- **NeuralGrid** is described with a legacy architecture ("Phases 1–4
  verified," a 4,561-parameter Interface-VAE). The project's own handoff
  marks that planning as superseded; the current direction is a symbolic
  bol-vocabulary layer plus a seven-axis translator.
- **Unified Streaming** is described as "design stage, no prototype
  repository." OMNIPLEX exists as a Rust workspace of eight crates plus a
  driver and packaging tree.
- **NAM × Csound** is summarised with a framing the submitted paper does not
  use ("three non-obvious bugs"), a loudness spread of 29 dB that appears
  nowhere in the submission, and a "10 calibrated profiles converging"
  status. The submitted paper's contribution is the NAMProcess opcode plus
  live dry-referenced volume compensation, measured over 25 captures.
- **NEPTR** is described as having 19 menus. The code asserts 20.
- Every node carries `updated: 2026-07-28`.

Separately, the section has no structure: eight papers sit in one flat list
with no indication of which are this year's work and which are queued.

## Goals

1. Every factual claim on every papers surface matches the current state of
   the underlying work, verified against source where source exists.
2. The section is divided into two sub-planets: work shipping this academic
   season, and work deferred to 2027 and beyond.
3. The masters thesis exists as a node, positioned as the work that binds
   the season's three papers. It is named last, after the rest is in place.
4. The NAM opcode's source and resources are downloadable from the site.

## Non-goals

- Rewriting the ROTURA manuscript itself. The site reflects it; it is not
  the source of truth for it.
- Changing the visual design of the papers pages or the NIME draft styling.
- Resurrecting anything from `neuralGrid/legacy-planning/`.
- Building OMNIPLEX into DemiurgeOS. That is functional work happening
  elsewhere; here it is only described.

---

## Structure

`emgor.papers` gains two children. Every existing paper node re-parents to
one of them.

### `emgor.papers.season-2026-27` — "2026–2027 SEASON"

| Paper | Venue | Status | Co-authors |
|---|---|---|---|
| Workflow of a Modern Digital Luthier | ROTURA (Process Report) | In submission, double-blind | Emory Smith, Ajay Kapur |
| DemiurgeOS | Linux Audio Conference | Running; paper drafting | Emory Smith, Ilai Gilbert, Ajay Kapur |
| Performance System of a Modern Digital Luthier | NIME / ICMC | New node; instrument running | Emory Smith |
| Masters Thesis (untitled) | CalArts | In development | Emory Smith, Ajay Kapur |

### `emgor.papers.future` — "2027+"

| Paper | Venue | Status | Co-authors |
|---|---|---|---|
| NAM × Csound | ICSC (deferred to 2027) | Paper complete; PDF + source downloadable | Emory Smith |
| NeuralGrid | NIME | Active; first encoder working on hardware | Emory Smith, Ajay Kapur, Jake Cheng |
| BOUBA | AMTA / music therapy venues | Object built; paper not started | Emory Smith |
| OMNIPLEX | NIME / CHI / ACM MM / IEEE RTSS | Rust workspace exists; folding into DemiurgeOS functionally | Emory Smith |
| Open-Pedal | NIME / TEI / CHI / ICMC | Concept | Emory Smith |

Speculative co-authors from the planning document (bare first names followed
by `?`) are dropped rather than published as guesses. Where a name was
confirmed in this session it is used in full.

---

## Per-paper content decisions

### Workflow of a Modern Digital Luthier

Rebuilt from `luthier_paper_v2/rotura_submission.md`. The NIME-format draft
page carries the real structure: Approach (7 subsections), Case Study NEPTR
(purpose, programming, hardware, fabrication, evaluation), Case Study BOUBA
(same five), Conclusions, and the bibliography.

Published in full at the user's explicit direction, with the double-blind
risk raised and accepted. Author identification stays on the site because
the site is already the author's; the manuscript itself remains blind.

**The NEPTR duplication is deliberate and must be stated.** The ROTURA
manuscript documents an earlier NEPTR — Pi 5 with a Teensy 4.0 for audio I/O
and a Teensy 4.1 for controls, ChucK synthesis with a Python GUI over OSC.
The current instrument is Csound on DEMIURGE_OS. Both the Luthier page and
the NEPTR paper page carry a line making the relationship explicit, so a
reader who notices the discrepancy finds it already accounted for.

### DemiurgeOS

Refreshed from the current `DEMIURGE_OS/README.md`. Facts to carry: the
PipeWire virtual layer with `demiurge-sink`, the ALSA-seq MIDI merge, the
`demiurge-clock` C++ daemon at 24 PPQN with BPM on ch16 CC119 and optional
Ableton Link, hot-reloaded `live.conf` with graceful-failure re-routing,
automatic multi-interface aggregation, remote Teensy flashing over SSH, and
the M8 performance pack (quantum 128 at 48 kHz, `isolcpus=3` paired with
`CPUAffinity=3`, safe 2800 MHz overclock, thermal watchdog).

Adds a line noting OMNIPLEX is being folded in as the transport layer.

### Performance System of a Modern Digital Luthier (new)

The NEPTR paper. Verified numbers only:

- 20 menus (`MENU_COUNT = 20`, asserted against the `MENUS` table):
  PRESETS, VOLUME, SPECTRAL, PITCH, FILTERING, MOD, DRIVE, PREAMP, GRANULAR,
  DIGIVERB, DELAY, BASS, SYNTH, AIR, LOOPER, CHASE BLISS, GLITCH, NAM,
  IR VERB, UTILITY.
- 63 effect `.orc` files under `neptr/csound/effects/`.
- Csound on a Raspberry Pi 5 over raw ALSA, 48 kHz, 128-sample period.

The `~7 ms` round trip and `~70 effects` figures from the old draft are
replaced: 63 is counted, and latency is stated as the block granularity
(2.7 ms at 48 kHz with ksmps 128) plus interface conversion, rather than a
round-trip number nothing in the repo measures.

`docs/neptr_full_reference.md` says 17 menus and is stale; code wins.

### Masters Thesis

Created as a node with no title. Body states what it binds — DemiurgeOS,
the Luthier methodology, and the NEPTR performance system — and that it is
a further-meta treatment of the digital lutherie argument. Named in a
follow-up pass once the three papers are on the page.

### NAM × Csound

Rewritten from `latex-icsc/nam-csound-icsc.tex`. Corrections:

- Contribution is the `NAMProcess` opcode plus live dry-referenced volume
  compensation, not a bug catalogue. The `thread = 5` scheduling finding and
  the −4.7 dB shared-instance crosstalk stay as implementation notes.
- 25-capture test library, not 10 profiles.
- Loudness spread: 17.3 dB seed-only (σ 4.3 dB) collapsing to 3.6 dB
  (σ 0.74 dB) on saw material, all but one capture within ±2 dB. On guitar
  material, 3.1 dB (σ 0.83 dB) against 9.9 dB (σ 2.60 dB) for the best
  static probe. The universe node currently conflates these two sets; they
  are reported separately.
- The 29 dB figure is removed entirely.
- Latency: no algorithmic latency; block granularity only, 2.7 ms at 48 kHz.
- CPU: ~15% of the audio core, stated as the informal observation it is.
- Status: complete and submitted; venue deferred to 2027.

**Downloads** copied into `universe/papers/files/nam-opcode/`:
`csound_nam_opcode.cpp`, `nam_loader.cpp`, `nam_loader.h`, `di_probe.h`,
`kweight.h`, `CMakeLists.txt`, `build-on-pi.sh`,
`test_nam_passthrough.csd`, and the benchmark tooling (`nam_bench.csd`,
`analyze.py`). Wired into the node's `downloads:` frontmatter so the galaxy
build validates their presence.

### NeuralGrid

Keeps the name NeuralGrid. Septagraph is the name of the seven-axis
coordinate layer inside it, not of the paper.

Two-layer architecture:

1. **Symbolic layer.** Sensor input is smoothed and interpolated into a
   coherent output format defined by the dimensional space the sensors
   represent — first encoder working on hardware. A model interprets gesture
   through a symbolic vocabulary borrowed from tabla *bols*, holds a cycle
   (*theka*/*taal*), and applies generative variation through operations
   that tabla already formalises as geometric transforms over bol sequences
   (*tihai*, *kaida*). The vocabulary is speech-synthesisable, with room to
   extend into English symbolism.
2. **Translator layer.** Maps symbolic output onto any synthesis engine
   through seven perceptual axes: frequency, loudness, harmonicity, rate,
   scale, space, time. Hardcoded regions of translator behaviour define
   specific *instruments*; loosening them lets the instrument be
   generatively shaped by the translator's *deaf* reactions to the player —
   it never hears what it renders.

The borrowing from a living tradition is named as a borrowing, with the
tabla scholarship cited rather than treated as found abstraction. Ajay
Kapur's ETabla, Electronic Dholak, and MahaDeviBot work is the obvious
citation base and he is a co-author.

Nothing from `legacy-planning/` is carried forward.

### BOUBA

Rewritten as built, from the ROTURA case study: quadraphonic sound object,
~$80 USD to reproduce, Teensy Audio Library, four speakers from two PCM5102
DACs amplified by four LM386s, 9 V jack stepped down per component, onboard
controls selecting and interpolating between generative presets and
adjusting volume, probability, and pitch. O'Modhrain evaluation: playability
lower by design, learnability high, sonic range deliberately constrained,
robustness high over two months.

The research baseline keeps its four pillars and its citations, which the
ROTURA manuscript states with effect sizes.

The *paper* remains unwritten and 2027+; the *object* is done. The node must
not blur the two.

### OMNIPLEX

Renamed from Unified Streaming. Described as an existing Rust workspace:
crates `omniplex-wire`, `-transport`, `-mux`, `-session`, `-discovery`,
`-clock`, `-metrics`, `-cli`, plus `driver/` and `packaging/`. The abstract
and design framing from the planning document stay; "no prototype repository
has been started" is removed as false.

Notes that the transport is being folded into DemiurgeOS functionally, and
that the standalone paper is deferred.

### Open-Pedal

The only node with nothing new to verify. Claims stay; "a search of the
EMGOR working archive found no dedicated repository yet" is softened to a
plain statement that it is at concept stage, consistent with how the other
deferred nodes read.

---

## Surfaces

### `universe/papers/`

- New `season-2026-27/_index.md` and `future/_index.md`.
- Leaf nodes move into the matching directory, `parent` re-pointed, `id`
  updated to match the new path. `route` follows.
- `neuralgrid.md` keeps its name. `unified-streaming.md` → `omniplex.md`.
- New `neptr-performance-system.md`, new `thesis.md`.
- `nam-csound.md` moves to `future/`, gains the opcode downloads.
- Every touched node's `updated:` set to `2026-09-21`.
- `_index.md` for `emgor.papers` rewritten: blurb stops saying "one paper
  shipped, seven in orbit."

Node `id`s change, which changes routes. Any in-repo link to an old
`/papers/<slug>` route is updated in the same pass.

### `papers/nime/`

- `index.html` splits into the two seasons.
- `digital-luthier.html` rebuilt from the ROTURA manuscript.
- `neuralgrid.html` rewritten to the two-layer architecture.
- `unified-streaming.html` → `omniplex.html`.
- `bouba.html` rewritten as built.
- `demiurgeos.html` refreshed.
- New `nam-csound.html` — a real page with the download list, replacing the
  bare PDF link in the index.
- New `neptr-performance-system.html`, new `thesis.html`.
- `neptr.html` — retitled to the new paper, or kept as the Phase 4 draft and
  superseded by the new page. Decided during implementation; whichever way,
  only one NEPTR paper page exists at the end.

### `papers.html`

Restructured into two sections with the season headings. Every abstract,
summary, venue, and co-author line replaced with the corrected text. The
`Deadline — July 11 (ICSC 2026)` line is removed; deadlines that have passed
are not shown as deadlines.

### `galaxy.json`

Not hand-edited. Regenerated with `node tools/build-galaxy.mjs`, which
validates ids, parents, reachability, and that every `downloads:` path
exists on disk. `node tools/build-galaxy.mjs --check` runs first.

---

## Verification

1. `node tools/build-galaxy.mjs --check` passes — no duplicate ids, no
   dangling parents, no unreachable nodes, no missing download files.
2. `node tools/build-galaxy.mjs` writes `galaxy.json`; the two sub-planets
   and all nine leaves appear with correct parents.
3. Every internal link in `papers.html` and `papers/nime/*.html` resolves to
   a file that exists — checked by script, not by eye.
4. The NAM opcode downloads resolve and are non-empty.
5. `grep` for the retired figures across all papers surfaces returns
   nothing: `29 dB`, `19 menus`, `~70 effects`, `10 calibrated`,
   `no dedicated .* repository`, `2026-07-28`.
6. Open `index.html` and the papers pages in Chrome; confirm the galaxy
   renders the new structure and no node is orphaned.

## Risks

- **Route changes break inbound links.** Old `/papers/<slug>` routes change
  for every node that moves. Mitigated by updating in-repo links; external
  links are accepted as broken, as the papers page is `noindex` and
  unlisted.
- **Double-blind exposure.** Publishing the ROTURA content while under
  review is a real risk to the submission. Raised and explicitly accepted by
  the author.
- **The thesis node ships untitled**, which is visible on the site until the
  naming pass. Acceptable; it is marked in development.
