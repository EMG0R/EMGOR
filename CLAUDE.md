# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

EMGOR.ONLINE — a musician's portfolio and interactive web synthesizer platform. Pure static site (no build tools, no package manager, no framework). All files served as-is.

## Architecture

- **Vanilla HTML/CSS/JS** with no dependencies except CDN-loaded libraries
- **RNBO** (Cycling 74's Max/MSP web export) powers the synthesizer engine — patch definition lives in `export/patch.export.json`
- **Matter.js** (CDN) drives the interactive starfield physics on the landing page
- **WebGL** shader provides the animated lava background
- **Web Audio API** for audio context management and MIDI scheduling

## Key Pages

- `index.html` — landing page with physics starfield + lava background
- `webSynthHub.html` — hub linking to synth apps (NEPTR, BMO, CIESEN)
- `NEPTR.html` — primary synth interface: piano keyboard, XY grid, sliders, RNBO engine
- `music.html` — portfolio with video showcases and audio player

## Key JS Modules

- `js/app.js` — RNBO device setup, MIDI note playback, grid/slider controls, parameter mapping
- `js/physics.js` — Matter.js starfield with obstacle avoidance (45 stars)
- `js/lava.js` — WebGL Perlin noise background shader
- `js/music.js` — Audio player controls
- `js/guardrails.js` — Error handling

## Development

No build step. To develop, serve the directory with any static HTTP server:

```
npx serve .
# or
python3 -m http.server
```

RNBO runtime is loaded from CDN (`https://c74-public.nyc3.digitaloceanspaces.com/rnbo/`). The synth patch parameters (BPM, offset, oddsTRIG, etc.) are defined in `export/patch.export.json` and mapped to UI controls in `app.js`.

## Styling

CSS variables for theming are defined in `style/style.css` using a purple/blue/green dark palette. Glass morphism aesthetic throughout.
