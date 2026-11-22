# GORMAPPER — projection mapping app (v1 spec)

Date: 2026-08-28 · Status: approved in chat, building ASAP

## What
Lightweight, terminal-friendly projection mapping app for the room projector.
No TouchDesigner. Single web app + tiny dependency-free Node server.
Lives at `______2026NEW/gormapper/` (additive only).

## Architecture
- `server.js` — Node built-ins only. Serves static files, GET/POST scene JSON
  (`scenes/*.json`), POST media uploads to `media/`. Port 8711.
- `index.html` — one page, two modes:
  - **Output** (`/output`) — fullscreen on the projector, renders layers, no UI.
  - **Editor** (`/editor`) — on the Mac screen: live preview + drawing/editing UI.
  - Synced live via BroadcastChannel (same origin, same Chrome profile).

## Layers (rendered in order, WebGL2)
1. **Screen quad** — `getDisplayMedia()` capture of a BetterDisplay virtual
   display, textured with a 4-corner homography warp. Acts like a real monitor.
   Capture is initiated in the output window (hotkey `C`, needs user gesture).
2. **Animation zones** — arbitrary polygons (click to place points, drag to
   redraw, insert point on edge click, alt-click to delete point). Content
   clipped to polygon:
   - Shader presets: aurora, lava, rain, fireflies, scanlines, gradient wash
   - Video/image loops (uploaded to `media/`, persisted)
   - Solid color / slow color wash
   Per-zone params: speed, hue/color, brightness, opacity.
3. **Masks** — black polygons that darken whatever is under them.

## Editing model
- `E` toggles edit mode (outlines + handles in both windows), `S` saves scene,
  `F` fullscreen. Scenes are plain JSON — git-trackable, terminal-editable.
- Scene switching via `GET /api/scene/<name>/activate` (future smartGOR hook).

## Vibecoded patterns (hot reload) — added to v1
- `patterns/` directory: each pattern is a standalone `.glsl` fragment-shader
  file with a small metadata header comment (name, params it exposes).
- Built-in presets (aurora, lava, rain, …) live there too as normal files —
  editable like any other pattern.
- Server watches `patterns/` (fs.watch) and pushes changes to clients via SSE
  (`/api/events`). Clients recompile the shader in place; every zone using it
  updates live. No server restart, no page reload. Compile errors show as a
  non-fatal overlay in the editor (zone keeps last working shader).
- New pattern files appear in the editor's content dropdown automatically.

## Everything is an object
- The "main screen" is not special: any polygon/quad object can be given
  content type **screen** (the getDisplayMedia capture). So the main monitor
  rectangle is just an object whose content is "screen".

## Scenes
- Multiple named scenes (e.g. `default`, `movie`), each a full layout+content
  JSON. Quick switcher in editor + `GET /api/scene/<name>/activate` so scenes
  can be switched from the terminal or smartGOR.

## v2 (not now)
Hydra live-coding zones, bezier edges, multi-projector.
