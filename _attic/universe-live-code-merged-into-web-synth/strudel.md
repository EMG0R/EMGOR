---
id: emgor.live-code.strudel
title: Strudel REPL
blurb: TidalCycles patterns in the browser, AGPL and proud
parent: emgor.live-code
links:
  - { label: "Open the REPL", url: "livecode.html" }
  - { label: "strudel.cc", url: "https://strudel.cc" }
  - { label: "Source (Codeberg)", url: "https://codeberg.org/uzu/strudel" }
tags: [live-coding, strudel, web-audio, agpl]
updated: 2026-07-28
draft: false
---

# Strudel REPL

[Strudel](https://strudel.cc) is a JavaScript port of the
[TidalCycles](https://tidalcycles.org) pattern language: you describe music as
patterns of events, and the runtime schedules them against Web Audio in real time.
Everything runs client-side in the browser — samples, synths, effects, the works.

This site embeds the official `@strudel/repl` web component (pinned to `1.3.0`
from jsDelivr) directly in [livecode.html](livecode.html) — a real in-page editor,
not an iframe. It ships with three EMGOR starter patterns (dark techno, glitch
breaks, ambient drone) switchable from the page.

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
