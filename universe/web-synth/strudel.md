---
id: emgor.web-synth.strudel
launch: livecode.html
title: STRUDEL FORK
blurb: A fork of Strudel (TidalCycles patterns as JavaScript) — live-coded music in the tab
parent: emgor.web-synth
links:
  - { label: "Open the REPL", url: "livecode.html" }
  - { label: "strudel.cc", url: "https://strudel.cc" }
tags: [strudel, javascript, live-coding, web-audio, agpl]
updated: 2026-08-02
draft: false
---

# STRUDEL

**Language: Strudel (JavaScript).** [Strudel](https://strudel.cc) is a
JavaScript port of the [TidalCycles](https://tidalcycles.org) pattern language:
you describe music as patterns of events, and the runtime schedules them
against Web Audio in real time. Everything runs client-side — samples, synths,
effects, the works.

This site embeds the official `@strudel/repl` web component (pinned to `1.3.0`
from jsDelivr) directly in [livecode.html](livecode.html) — a real in-page
editor, not an iframe — loaded with three EMGOR starter patterns: dark techno,
glitch breaks, and an ambient drone. Edit anything, hit `ctrl+enter`, bend time.

Of the three web-synth languages, this is the *live-coded* one: the instrument
is the text buffer itself.

## License

Strudel is licensed under the **GNU AGPL-3.0**. This site uses the unmodified
published package from npm and links back to the source at
[codeberg.org/uzu/strudel](https://codeberg.org/uzu/strudel). This site itself is
also publicly available in the EMGOR site repo, which keeps everything
comfortably within the spirit and letter of the AGPL.

## Embed it yourself

Any page can host a live pattern with two tags. Pattern code goes in an HTML
comment inside the element:

```html
<script src="https://cdn.jsdelivr.net/npm/@strudel/repl@1.3.0/dist/index.js"></script>
<strudel-editor>
  <!--
setcps(0.5)
s("bd sd bd sd").bank("RolandTR909")
  -->
</strudel-editor>
```

To swap code programmatically, use the component's editor instance:

```js
const repl = document.querySelector('strudel-editor');
repl.editor.setCode('s("bd*4")'); // replace the buffer
repl.editor.evaluate();           // play
repl.editor.stop();               // stop
```
