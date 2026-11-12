# Cod Phase 1: Core Shell Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a working Cod terminal app — a Tauri window that spawns real
shell PTYs into draggable, resizable, fullscreenable panes on one freeform
canvas, with `Cmd+N` adding panes to the same window and a dark
purple/white theme. No dashboard, approval bar, or advisor yet (later
phases).

**Architecture:** Rust backend (Tauri) owns a `PtyManager` that spawns real
PTY processes via `portable-pty` and streams their output to the frontend
over Tauri events; thin `#[tauri::command]` wrappers expose spawn/write/
resize/close. Frontend (React + TypeScript) keeps all pane geometry (drag/
resize/focus/fullscreen) in a pure reducer (`layoutReducer`), rendered by a
`Canvas` component that hosts one `Pane` (xterm.js) per entry, wired to the
backend via a `usePty` hook.

**Tech Stack:** Tauri 2, Rust, `portable-pty`, React 18 + TypeScript, Vite,
`@xterm/xterm` + `@xterm/addon-fit`, Vitest (frontend tests), `cargo test`
(backend tests).

**Spec:** `docs/superpowers/specs/2026-08-12-cod-design.md`

## Global Constraints

- macOS only (matches existing Ghostty/watch-app setup) — no cross-platform
  code paths needed.
- New project lives entirely under `______2026NEW/cod/` — additive only,
  no existing files touched in this phase.
- `Cmd+N` always adds a pane to the current window; it must never open a new
  OS window.
- Theme is dark purple background, white text — the only theme, no theming
  system.
- No automated UI test suite for drag/resize/fullscreen interactions — those
  are verified manually per the spec's Testing section. Pure logic (pane
  layout reducer, PTY manager) is unit tested.
- Frontend uses `@xterm/xterm` (not the deprecated `xterm` package name).

---

### Task 1: Scaffold the Tauri + React project

**Files:**
- Create: `cod/` (via CLI scaffold, then modified below)

**Interfaces:**
- Produces: a running `cod` Tauri project skeleton that later tasks add
  files into. No code interfaces yet.

- [ ] **Step 1: Scaffold via the official Tauri CLI**

Run from `______2026NEW/`:

```bash
npm create tauri-app@latest cod -- --template react-ts --manager npm
```

When prompted, accept defaults (app name `cod`, window title `Cod`,
identifier e.g. `com.emgor.cod`).

- [ ] **Step 2: Install JS dependencies**

```bash
cd cod
npm install
npm install @xterm/xterm @xterm/addon-fit
npm install -D vitest
```

- [ ] **Step 3: Verify the scaffold runs**

```bash
npm run tauri dev
```

Expected: a native window opens showing the default Tauri+React starter
page. Close the window/stop the process once confirmed.

- [ ] **Step 4: Commit the scaffold**

```bash
git add cod
git commit -m "cod: scaffold Tauri + React project"
```

---

### Task 2: Rust PTY manager

**Files:**
- Modify: `cod/src-tauri/Cargo.toml`
- Create: `cod/src-tauri/src/pty.rs`
- Modify: `cod/src-tauri/src/main.rs` (add `mod pty;`)

**Interfaces:**
- Produces: `pty::PtyManager` with methods:
  - `PtyManager::new() -> PtyManager`
  - `spawn(&mut self, cmd: &str, args: &[&str], cols: u16, rows: u16) -> Result<String, String>` (returns pane id)
  - `take_receiver(&mut self, id: &str) -> Option<Receiver<String>>`
  - `write(&mut self, id: &str, data: &[u8]) -> Result<(), String>`
  - `resize(&self, id: &str, cols: u16, rows: u16) -> Result<(), String>`
  - `close(&mut self, id: &str) -> Result<(), String>`

- [ ] **Step 1: Add the `portable-pty` dependency**

Edit `cod/src-tauri/Cargo.toml`, add under `[dependencies]`:

```toml
portable-pty = "0.8"
```

- [ ] **Step 2: Write the failing test**

Create `cod/src-tauri/src/pty.rs` with just the test first:

```rust
use portable_pty::{native_pty_system, CommandBuilder, PtySize};
use std::collections::HashMap;
use std::io::{Read, Write};
use std::sync::mpsc::{channel, Receiver};

pub struct PtyHandle {
    writer: Box<dyn Write + Send>,
    child: Box<dyn portable_pty::Child + Send + Sync>,
    master: Box<dyn portable_pty::MasterPty + Send>,
    receiver: Option<Receiver<String>>,
}

pub struct PtyManager {
    sessions: HashMap<String, PtyHandle>,
    next_id: u64,
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::time::{Duration, Instant};

    #[test]
    fn spawn_echoes_output() {
        let mut manager = PtyManager::new();
        let id = manager
            .spawn("echo", &["hello_cod"], 80, 24)
            .expect("spawn failed");
        let rx = manager.take_receiver(&id).expect("no receiver");

        let mut collected = String::new();
        let deadline = Instant::now() + Duration::from_secs(3);
        while Instant::now() < deadline {
            if let Ok(chunk) = rx.recv_timeout(Duration::from_millis(500)) {
                collected.push_str(&chunk);
                if collected.contains("hello_cod") {
                    break;
                }
            }
        }
        assert!(collected.contains("hello_cod"), "got: {collected}");
    }
}
```

- [ ] **Step 3: Run test to verify it fails to compile**

Run: `cd cod/src-tauri && cargo test pty::tests::spawn_echoes_output`
Expected: FAIL to compile — `PtyManager::new` and `spawn`/`take_receiver`
are not yet implemented.

- [ ] **Step 4: Implement `PtyManager`**

Add above the `#[cfg(test)]` block in `pty.rs`:

```rust
impl PtyManager {
    pub fn new() -> Self {
        Self {
            sessions: HashMap::new(),
            next_id: 0,
        }
    }

    pub fn spawn(&mut self, cmd: &str, args: &[&str], cols: u16, rows: u16) -> Result<String, String> {
        let pty_system = native_pty_system();
        let pair = pty_system
            .openpty(PtySize {
                rows,
                cols,
                pixel_width: 0,
                pixel_height: 0,
            })
            .map_err(|e| e.to_string())?;

        let mut builder = CommandBuilder::new(cmd);
        builder.args(args);

        let child = pair.slave.spawn_command(builder).map_err(|e| e.to_string())?;
        drop(pair.slave);

        let mut reader = pair.master.try_clone_reader().map_err(|e| e.to_string())?;
        let writer = pair.master.take_writer().map_err(|e| e.to_string())?;

        let (tx, rx) = channel();
        std::thread::spawn(move || {
            let mut buf = [0u8; 4096];
            loop {
                match reader.read(&mut buf) {
                    Ok(0) => break,
                    Ok(n) => {
                        let chunk = String::from_utf8_lossy(&buf[..n]).to_string();
                        if tx.send(chunk).is_err() {
                            break;
                        }
                    }
                    Err(_) => break,
                }
            }
        });

        self.next_id += 1;
        let id = format!("pane-{}", self.next_id);
        self.sessions.insert(
            id.clone(),
            PtyHandle {
                writer,
                child,
                master: pair.master,
                receiver: Some(rx),
            },
        );
        Ok(id)
    }

    pub fn take_receiver(&mut self, id: &str) -> Option<Receiver<String>> {
        self.sessions.get_mut(id).and_then(|h| h.receiver.take())
    }

    pub fn write(&mut self, id: &str, data: &[u8]) -> Result<(), String> {
        let handle = self.sessions.get_mut(id).ok_or("unknown pane id")?;
        handle.writer.write_all(data).map_err(|e| e.to_string())
    }

    pub fn resize(&self, id: &str, cols: u16, rows: u16) -> Result<(), String> {
        let handle = self.sessions.get(id).ok_or("unknown pane id")?;
        handle
            .master
            .resize(PtySize {
                rows,
                cols,
                pixel_width: 0,
                pixel_height: 0,
            })
            .map_err(|e| e.to_string())
    }

    pub fn close(&mut self, id: &str) -> Result<(), String> {
        let mut handle = self.sessions.remove(id).ok_or("unknown pane id")?;
        handle.child.kill().map_err(|e| e.to_string())
    }
}
```

- [ ] **Step 5: Run test to verify it passes**

Run: `cd cod/src-tauri && cargo test pty::tests::spawn_echoes_output`
Expected: PASS

- [ ] **Step 6: Register the module**

In `cod/src-tauri/src/main.rs`, add near the top (before `fn main`):

```rust
mod pty;
```

- [ ] **Step 7: Commit**

```bash
git add cod/src-tauri/Cargo.toml cod/src-tauri/src/pty.rs cod/src-tauri/src/main.rs
git commit -m "cod: add PtyManager with spawn/write/resize/close"
```

---

### Task 3: Tauri commands exposing the PTY manager

**Files:**
- Modify: `cod/src-tauri/src/main.rs`

**Interfaces:**
- Consumes: `pty::PtyManager` from Task 2 (`new`, `spawn`, `take_receiver`,
  `write`, `resize`, `close`).
- Produces: Tauri commands invocable from the frontend as `spawn_pane`,
  `write_to_pane`, `resize_pane`, `close_pane`; and a `pty-output:{id}`
  window event per pane carrying `String` chunks.

- [ ] **Step 1: Replace `main.rs` contents**

Replace the full contents of `cod/src-tauri/src/main.rs` with:

```rust
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod pty;

use pty::PtyManager;
use std::sync::Mutex;
use tauri::{Emitter, Manager, State};

struct AppState {
    pty_manager: Mutex<PtyManager>,
}

#[tauri::command]
fn spawn_pane(cols: u16, rows: u16, app: tauri::AppHandle, state: State<AppState>) -> Result<String, String> {
    let mut manager = state.pty_manager.lock().unwrap();
    let id = manager.spawn("/bin/zsh", &["-l"], cols, rows)?;
    let rx = manager.take_receiver(&id).ok_or("missing receiver")?;

    let app_handle = app.clone();
    let event_name = format!("pty-output:{id}");
    std::thread::spawn(move || {
        while let Ok(chunk) = rx.recv() {
            let _ = app_handle.emit(&event_name, chunk);
        }
    });

    Ok(id)
}

#[tauri::command]
fn write_to_pane(id: String, data: String, state: State<AppState>) -> Result<(), String> {
    let mut manager = state.pty_manager.lock().unwrap();
    manager.write(&id, data.as_bytes())
}

#[tauri::command]
fn resize_pane(id: String, cols: u16, rows: u16, state: State<AppState>) -> Result<(), String> {
    let manager = state.pty_manager.lock().unwrap();
    manager.resize(&id, cols, rows)
}

#[tauri::command]
fn close_pane(id: String, state: State<AppState>) -> Result<(), String> {
    let mut manager = state.pty_manager.lock().unwrap();
    manager.close(&id)
}

fn main() {
    tauri::Builder::default()
        .manage(AppState {
            pty_manager: Mutex::new(PtyManager::new()),
        })
        .invoke_handler(tauri::generate_handler![
            spawn_pane,
            write_to_pane,
            resize_pane,
            close_pane
        ])
        .run(tauri::generate_context!())
        .expect("error while running cod");
}
```

- [ ] **Step 2: Verify it builds**

Run: `cd cod/src-tauri && cargo build`
Expected: builds with no errors. (`Manager` import may warn as unused —
that's fine, it's needed by some Tauri command signatures in later phases;
if `cargo build` errors on an unused-import lint escalated to `deny`, remove
the unused `Manager` import.)

- [ ] **Step 3: Commit**

```bash
git add cod/src-tauri/src/main.rs
git commit -m "cod: expose PTY manager as Tauri commands"
```

---

### Task 4: Pane layout reducer (pure logic)

**Files:**
- Create: `cod/src/layout/paneLayout.ts`
- Test: `cod/src/layout/paneLayout.test.ts`

**Interfaces:**
- Produces: `PaneState`, `LayoutState`, `LayoutAction` types,
  `initialLayoutState: LayoutState`, and
  `layoutReducer(state: LayoutState, action: LayoutAction): LayoutState`,
  consumed by the `Canvas` component in Task 5.

- [ ] **Step 1: Write the failing tests**

Create `cod/src/layout/paneLayout.test.ts`:

```typescript
import { describe, expect, it } from 'vitest';
import {
  initialLayoutState,
  layoutReducer,
  type LayoutState,
} from './paneLayout';

describe('layoutReducer', () => {
  it('adds a pane with default geometry and focuses it', () => {
    const state = layoutReducer(initialLayoutState, { type: 'ADD_PANE', id: 'p1' });
    expect(state.panes).toHaveLength(1);
    expect(state.panes[0]).toMatchObject({ id: 'p1', fullscreen: false });
    expect(state.focusedId).toBe('p1');
  });

  it('cascades position for successive panes', () => {
    let state = layoutReducer(initialLayoutState, { type: 'ADD_PANE', id: 'p1' });
    state = layoutReducer(state, { type: 'ADD_PANE', id: 'p2' });
    const [p1, p2] = state.panes;
    expect(p2.x).toBeGreaterThan(p1.x);
    expect(p2.y).toBeGreaterThan(p1.y);
  });

  it('moves the matching pane only', () => {
    let state = layoutReducer(initialLayoutState, { type: 'ADD_PANE', id: 'p1' });
    state = layoutReducer(state, { type: 'ADD_PANE', id: 'p2' });
    state = layoutReducer(state, { type: 'MOVE_PANE', id: 'p1', x: 500, y: 500 });
    const p1 = state.panes.find((p) => p.id === 'p1')!;
    const p2 = state.panes.find((p) => p.id === 'p2')!;
    expect(p1.x).toBe(500);
    expect(p1.y).toBe(500);
    expect(p2.x).not.toBe(500);
  });

  it('ignores move for a fullscreen pane', () => {
    let state = layoutReducer(initialLayoutState, { type: 'ADD_PANE', id: 'p1' });
    state = layoutReducer(state, { type: 'TOGGLE_FULLSCREEN', id: 'p1' });
    const before = state.panes[0];
    state = layoutReducer(state, { type: 'MOVE_PANE', id: 'p1', x: 999, y: 999 });
    expect(state.panes[0].x).toBe(before.x);
    expect(state.panes[0].y).toBe(before.y);
  });

  it('enforces a minimum size on resize', () => {
    let state = layoutReducer(initialLayoutState, { type: 'ADD_PANE', id: 'p1' });
    state = layoutReducer(state, { type: 'RESIZE_PANE', id: 'p1', width: 10, height: 10 });
    expect(state.panes[0].width).toBeGreaterThanOrEqual(200);
    expect(state.panes[0].height).toBeGreaterThanOrEqual(120);
  });

  it('raises z-index and focus on FOCUS_PANE', () => {
    let state: LayoutState = layoutReducer(initialLayoutState, { type: 'ADD_PANE', id: 'p1' });
    state = layoutReducer(state, { type: 'ADD_PANE', id: 'p2' });
    const p1Before = state.panes.find((p) => p.id === 'p1')!;
    state = layoutReducer(state, { type: 'FOCUS_PANE', id: 'p1' });
    const p1After = state.panes.find((p) => p.id === 'p1')!;
    expect(p1After.zIndex).toBeGreaterThan(p1Before.zIndex);
    expect(state.focusedId).toBe('p1');
  });

  it('toggles fullscreen and focuses the pane', () => {
    let state = layoutReducer(initialLayoutState, { type: 'ADD_PANE', id: 'p1' });
    state = layoutReducer(state, { type: 'TOGGLE_FULLSCREEN', id: 'p1' });
    expect(state.panes[0].fullscreen).toBe(true);
    state = layoutReducer(state, { type: 'TOGGLE_FULLSCREEN', id: 'p1' });
    expect(state.panes[0].fullscreen).toBe(false);
  });

  it('reassigns focus to the topmost remaining pane on close, or null if none remain', () => {
    let state = layoutReducer(initialLayoutState, { type: 'ADD_PANE', id: 'p1' });
    state = layoutReducer(state, { type: 'ADD_PANE', id: 'p2' });
    state = layoutReducer(state, { type: 'CLOSE_PANE', id: 'p2' });
    expect(state.panes).toHaveLength(1);
    expect(state.focusedId).toBe('p1');

    state = layoutReducer(state, { type: 'CLOSE_PANE', id: 'p1' });
    expect(state.panes).toHaveLength(0);
    expect(state.focusedId).toBeNull();
  });
});
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cd cod && npx vitest run src/layout/paneLayout.test.ts`
Expected: FAIL — `./paneLayout` module does not exist yet.

- [ ] **Step 3: Implement the reducer**

Create `cod/src/layout/paneLayout.ts`:

```typescript
export interface PaneState {
  id: string;
  x: number;
  y: number;
  width: number;
  height: number;
  zIndex: number;
  fullscreen: boolean;
}

export interface LayoutState {
  panes: PaneState[];
  nextZIndex: number;
  focusedId: string | null;
}

export type LayoutAction =
  | { type: 'ADD_PANE'; id: string }
  | { type: 'MOVE_PANE'; id: string; x: number; y: number }
  | { type: 'RESIZE_PANE'; id: string; width: number; height: number }
  | { type: 'FOCUS_PANE'; id: string }
  | { type: 'TOGGLE_FULLSCREEN'; id: string }
  | { type: 'CLOSE_PANE'; id: string };

export const initialLayoutState: LayoutState = {
  panes: [],
  nextZIndex: 1,
  focusedId: null,
};

const DEFAULT_WIDTH = 640;
const DEFAULT_HEIGHT = 400;
const MIN_WIDTH = 200;
const MIN_HEIGHT = 120;
const CASCADE_OFFSET = 32;
const CASCADE_WRAP = 8;

export function layoutReducer(state: LayoutState, action: LayoutAction): LayoutState {
  switch (action.type) {
    case 'ADD_PANE': {
      const offset = (state.panes.length % CASCADE_WRAP) * CASCADE_OFFSET;
      const pane: PaneState = {
        id: action.id,
        x: 40 + offset,
        y: 40 + offset,
        width: DEFAULT_WIDTH,
        height: DEFAULT_HEIGHT,
        zIndex: state.nextZIndex,
        fullscreen: false,
      };
      return {
        panes: [...state.panes, pane],
        nextZIndex: state.nextZIndex + 1,
        focusedId: action.id,
      };
    }

    case 'MOVE_PANE': {
      return {
        ...state,
        panes: state.panes.map((p) =>
          p.id === action.id && !p.fullscreen ? { ...p, x: action.x, y: action.y } : p,
        ),
      };
    }

    case 'RESIZE_PANE': {
      return {
        ...state,
        panes: state.panes.map((p) =>
          p.id === action.id && !p.fullscreen
            ? {
                ...p,
                width: Math.max(MIN_WIDTH, action.width),
                height: Math.max(MIN_HEIGHT, action.height),
              }
            : p,
        ),
      };
    }

    case 'FOCUS_PANE': {
      if (state.focusedId === action.id) return state;
      return {
        panes: state.panes.map((p) =>
          p.id === action.id ? { ...p, zIndex: state.nextZIndex } : p,
        ),
        nextZIndex: state.nextZIndex + 1,
        focusedId: action.id,
      };
    }

    case 'TOGGLE_FULLSCREEN': {
      return {
        panes: state.panes.map((p) =>
          p.id === action.id
            ? { ...p, fullscreen: !p.fullscreen, zIndex: state.nextZIndex }
            : p,
        ),
        nextZIndex: state.nextZIndex + 1,
        focusedId: action.id,
      };
    }

    case 'CLOSE_PANE': {
      const remaining = state.panes.filter((p) => p.id !== action.id);
      let focusedId = state.focusedId;
      if (state.focusedId === action.id) {
        focusedId =
          remaining.length > 0
            ? remaining.reduce((a, b) => (a.zIndex > b.zIndex ? a : b)).id
            : null;
      }
      return { ...state, panes: remaining, focusedId };
    }

    default:
      return state;
  }
}
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cd cod && npx vitest run src/layout/paneLayout.test.ts`
Expected: PASS (all 8 tests)

- [ ] **Step 5: Commit**

```bash
git add cod/src/layout/paneLayout.ts cod/src/layout/paneLayout.test.ts
git commit -m "cod: add pure pane layout reducer with tests"
```

---

### Task 5: Dark purple/white theme

**Files:**
- Create: `cod/src/theme.css`
- Modify: `cod/src/main.tsx` (import the stylesheet)

**Interfaces:**
- Produces: CSS custom properties `--cod-bg`, `--cod-bg-elevated`,
  `--cod-fg`, `--cod-accent` on `:root`, consumed by `Canvas.tsx`
  and `Pane.tsx` in Task 6.

- [ ] **Step 1: Create the theme stylesheet**

Create `cod/src/theme.css`:

```css
:root {
  --cod-bg: #1a0f2e;
  --cod-bg-elevated: #241640;
  --cod-fg: #ffffff;
  --cod-accent: #7c4dff;
  --cod-border: #3a2a5c;
}

* {
  box-sizing: border-box;
}

html,
body,
#root {
  margin: 0;
  padding: 0;
  width: 100%;
  height: 100%;
  background: var(--cod-bg);
  color: var(--cod-fg);
  font-family: -apple-system, 'SF Pro Text', system-ui, sans-serif;
  overflow: hidden;
}
```

- [ ] **Step 2: Import it in `main.tsx`**

In `cod/src/main.tsx`, add near the top with the other imports:

```typescript
import './theme.css';
```

- [ ] **Step 3: Verify manually**

Run `npm run tauri dev` from `cod/`. Expected: window background is
dark purple, no white flash on load.

- [ ] **Step 4: Commit**

```bash
git add cod/src/theme.css cod/src/main.tsx
git commit -m "cod: add dark purple/white theme"
```

---

### Task 6: `usePty` hook — frontend/backend bridge

**Files:**
- Create: `cod/src/pty/usePty.ts`

**Interfaces:**
- Consumes: Tauri commands `spawn_pane`, `write_to_pane`, `resize_pane`,
  `close_pane` and event `pty-output:{id}` from Task 3.
- Produces: `usePty(paneId: string): { attach: (el: HTMLElement) => () => void }`
  — `attach` mounts an xterm.js terminal into `el`, spawns the backing PTY,
  wires input/output, and returns a cleanup function that closes the PTY and
  disposes the terminal. Consumed by `Pane.tsx` in Task 7.

- [ ] **Step 1: Implement the hook**

Create `cod/src/pty/usePty.ts`:

```typescript
import { invoke } from '@tauri-apps/api/core';
import { listen, type UnlistenFn } from '@tauri-apps/api/event';
import { Terminal } from '@xterm/xterm';
import { FitAddon } from '@xterm/addon-fit';

export function usePty(paneId: string) {
  async function attach(el: HTMLElement): Promise<() => void> {
    const term = new Terminal({
      theme: {
        background: '#1a0f2e',
        foreground: '#ffffff',
      },
      fontFamily: 'Menlo, monospace',
      fontSize: 13,
    });
    const fit = new FitAddon();
    term.loadAddon(fit);
    term.open(el);
    fit.fit();

    const backendId: string = await invoke('spawn_pane', {
      cols: term.cols,
      rows: term.rows,
    });

    let unlisten: UnlistenFn | null = await listen<string>(
      `pty-output:${backendId}`,
      (event) => {
        term.write(event.payload);
      },
    );

    const inputDisposable = term.onData((data) => {
      void invoke('write_to_pane', { id: backendId, data });
    });

    const resizeDisposable = term.onResize(({ cols, rows }) => {
      void invoke('resize_pane', { id: backendId, cols, rows });
    });

    return () => {
      inputDisposable.dispose();
      resizeDisposable.dispose();
      if (unlisten) unlisten();
      void invoke('close_pane', { id: backendId });
      term.dispose();
    };
  }

  return { attach };
}
```

- [ ] **Step 2: Verify it builds**

Run: `cd cod && npx tsc --noEmit`
Expected: no type errors. (`paneId` is intentionally unused by the current
implementation — it's kept in the signature for the multi-pane wiring done
in Task 7's `Pane` component, which passes its own id in.)

- [ ] **Step 3: Commit**

```bash
git add cod/src/pty/usePty.ts
git commit -m "cod: add usePty hook bridging xterm.js to backend PTYs"
```

---

### Task 7: `Pane` and `Canvas` components — draggable/resizable/fullscreen UI

**Files:**
- Create: `cod/src/components/Pane.tsx`
- Create: `cod/src/components/Canvas.tsx`
- Modify: `cod/src/App.tsx`

**Interfaces:**
- Consumes: `layoutReducer`, `initialLayoutState`, `PaneState` from Task 4;
  `usePty` from Task 6.
- Produces: `Canvas` — the top-level component rendered by `App.tsx`; owns
  the `layoutReducer` state, listens for `Cmd+N` to dispatch `ADD_PANE`, and
  renders one `Pane` per `PaneState`.

- [ ] **Step 1: Implement `Pane.tsx`**

Create `cod/src/components/Pane.tsx`:

```tsx
import { useEffect, useRef } from 'react';
import type { PaneState } from '../layout/paneLayout';
import { usePty } from '../pty/usePty';

interface PaneProps {
  pane: PaneState;
  onFocus: (id: string) => void;
  onMove: (id: string, x: number, y: number) => void;
  onResize: (id: string, width: number, height: number) => void;
  onToggleFullscreen: (id: string) => void;
  onClose: (id: string) => void;
}

export function Pane({ pane, onFocus, onMove, onResize, onToggleFullscreen, onClose }: PaneProps) {
  const terminalRef = useRef<HTMLDivElement>(null);
  const { attach } = usePty(pane.id);

  useEffect(() => {
    if (!terminalRef.current) return;
    let cleanup: (() => void) | undefined;
    let cancelled = false;
    void attach(terminalRef.current).then((fn) => {
      if (cancelled) fn();
      else cleanup = fn;
    });
    return () => {
      cancelled = true;
      cleanup?.();
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  function startDrag(e: React.PointerEvent) {
    if (pane.fullscreen) return;
    onFocus(pane.id);
    const startX = e.clientX - pane.x;
    const startY = e.clientY - pane.y;

    function onPointerMove(ev: PointerEvent) {
      onMove(pane.id, ev.clientX - startX, ev.clientY - startY);
    }
    function onPointerUp() {
      window.removeEventListener('pointermove', onPointerMove);
      window.removeEventListener('pointerup', onPointerUp);
    }
    window.addEventListener('pointermove', onPointerMove);
    window.addEventListener('pointerup', onPointerUp);
  }

  function startResize(e: React.PointerEvent) {
    e.stopPropagation();
    if (pane.fullscreen) return;
    onFocus(pane.id);
    const startWidth = pane.width;
    const startHeight = pane.height;
    const startX = e.clientX;
    const startY = e.clientY;

    function onPointerMove(ev: PointerEvent) {
      onResize(pane.id, startWidth + (ev.clientX - startX), startHeight + (ev.clientY - startY));
    }
    function onPointerUp() {
      window.removeEventListener('pointermove', onPointerMove);
      window.removeEventListener('pointerup', onPointerUp);
    }
    window.addEventListener('pointermove', onPointerMove);
    window.addEventListener('pointerup', onPointerUp);
  }

  const style: React.CSSProperties = pane.fullscreen
    ? { position: 'absolute', inset: 0, zIndex: pane.zIndex }
    : {
        position: 'absolute',
        left: pane.x,
        top: pane.y,
        width: pane.width,
        height: pane.height,
        zIndex: pane.zIndex,
      };

  return (
    <div
      style={{
        ...style,
        background: 'var(--cod-bg-elevated)',
        border: '1px solid var(--cod-border)',
        borderRadius: pane.fullscreen ? 0 : 8,
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
      }}
      onPointerDown={() => onFocus(pane.id)}
    >
      <div
        onPointerDown={startDrag}
        style={{
          height: 28,
          flexShrink: 0,
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'flex-end',
          gap: 8,
          padding: '0 8px',
          cursor: pane.fullscreen ? 'default' : 'move',
          background: 'var(--cod-border)',
        }}
      >
        <button onClick={() => onToggleFullscreen(pane.id)}>
          {pane.fullscreen ? '⤢' : '⤡'}
        </button>
        <button onClick={() => onClose(pane.id)}>×</button>
      </div>
      <div ref={terminalRef} style={{ flex: 1, minHeight: 0 }} />
      {!pane.fullscreen && (
        <div
          onPointerDown={startResize}
          style={{
            position: 'absolute',
            right: 0,
            bottom: 0,
            width: 14,
            height: 14,
            cursor: 'nwse-resize',
          }}
        />
      )}
    </div>
  );
}
```

- [ ] **Step 2: Implement `Canvas.tsx`**

Create `cod/src/components/Canvas.tsx`:

```tsx
import { useEffect, useReducer } from 'react';
import { initialLayoutState, layoutReducer } from '../layout/paneLayout';
import { Pane } from './Pane';

let paneCounter = 0;

export function Canvas() {
  const [state, dispatch] = useReducer(layoutReducer, initialLayoutState);

  useEffect(() => {
    function onKeyDown(e: KeyboardEvent) {
      if (e.metaKey && e.key.toLowerCase() === 'n') {
        e.preventDefault();
        paneCounter += 1;
        dispatch({ type: 'ADD_PANE', id: `frontend-pane-${paneCounter}` });
      }
    }
    window.addEventListener('keydown', onKeyDown);
    return () => window.removeEventListener('keydown', onKeyDown);
  }, []);

  useEffect(() => {
    if (state.panes.length === 0) {
      paneCounter += 1;
      dispatch({ type: 'ADD_PANE', id: `frontend-pane-${paneCounter}` });
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  return (
    <div style={{ position: 'relative', width: '100%', height: '100%' }}>
      {state.panes.map((pane) => (
        <Pane
          key={pane.id}
          pane={pane}
          onFocus={(id) => dispatch({ type: 'FOCUS_PANE', id })}
          onMove={(id, x, y) => dispatch({ type: 'MOVE_PANE', id, x, y })}
          onResize={(id, width, height) => dispatch({ type: 'RESIZE_PANE', id, width, height })}
          onToggleFullscreen={(id) => dispatch({ type: 'TOGGLE_FULLSCREEN', id })}
          onClose={(id) => dispatch({ type: 'CLOSE_PANE', id })}
        />
      ))}
    </div>
  );
}
```

- [ ] **Step 3: Wire it into `App.tsx`**

Replace the full contents of `cod/src/App.tsx` with:

```tsx
import { Canvas } from './components/Canvas';

function App() {
  return <Canvas />;
}

export default App;
```

- [ ] **Step 4: Commit**

```bash
git add cod/src/components/Pane.tsx cod/src/components/Canvas.tsx cod/src/App.tsx
git commit -m "cod: add draggable/resizable/fullscreen Canvas and Pane components"
```

---

### Task 8: End-to-end manual verification

**Files:** none (verification only)

**Interfaces:** none — this task confirms Tasks 1–7 work together.

- [ ] **Step 1: Run the full test suites**

```bash
cd cod/src-tauri && cargo test
cd .. && npx vitest run
```

Expected: all Rust and Vitest tests pass.

- [ ] **Step 2: Launch the app**

```bash
cd cod && npm run tauri dev
```

- [ ] **Step 3: Verify the checklist**

Confirm each, in the running app:
- A terminal pane appears on launch, running your real login shell (test
  with e.g. `whoami`, `pwd`).
- `Cmd+N` adds a new pane into the *same window* (no new OS window), each
  running its own independent shell.
- Dragging a pane's title bar moves it; dragging the bottom-right corner
  resizes it (minimum size enforced, doesn't shrink to zero).
- Clicking a pane brings it to the front (z-order) over overlapping panes.
- The `⤡` button fullscreens a pane to fill the window; `⤢` restores it to
  its previous position/size.
- Background is dark purple, text is white, no theme flash on launch.
- Closing a pane (`×`) terminates its shell process (verify with
  `ps aux | grep zsh` before/after, or by running a long-lived command like
  `sleep 100 &` in the pane and confirming the process exits after close).

- [ ] **Step 4: Commit the phase completion marker**

```bash
cd /Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW
git add cod
git commit -m "cod: Phase 1 core shell complete and manually verified"
```
