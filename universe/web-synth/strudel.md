---
id: emgor.web-synth.strudel
title: STRUDEL
blurb: TidalCycles patterns as JavaScript — live-coded music in the tab
parent: emgor.web-synth
links:
  - { label: "Open the REPL", url: "livecode.html" }
  - { label: "strudel.cc", url: "https://strudel.cc" }
tags: [strudel, javascript, live-coding, web-audio]
updated: 2026-08-02
draft: false
---

# STRUDEL

**Language: Strudel (JavaScript).** [Strudel](https://strudel.cc) is a
JavaScript port of the [TidalCycles](https://tidalcycles.org) pattern language:
you describe music as patterns of events, and the runtime schedules them
against Web Audio in real time. Everything runs client-side — samples, synths,
effects, the works.

This site embeds the official `@strudel/repl` web component directly in
[livecode.html](livecode.html) — a real in-page editor, not an iframe — loaded
with three EMGOR starter patterns: dark techno, glitch breaks, and an ambient
drone. Edit anything, hit `ctrl+enter`, bend time.

Of the three web-synth languages, this is the *live-coded* one: the instrument
is the text buffer itself. For the full REPL experience (and the license/embed
details), see the [LIVE CODE](#/live-code) planet.
