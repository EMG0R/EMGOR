# Cod Phase 1b: Canvas Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Redesign Cod's Canvas/Pane layer built in Phase 1: panes must never overlap (collision-push physics replaces z-index stacking), the canvas gains pan/zoom navigation with assignable location presets, the pane chrome gets a sleek/minimalist restyle, and the full layout persists across app restarts.

**Architecture:** `paneLayout.ts`'s reducer grows a collision-resolution pass (any action that moves/resizes/un-fullscreens a pane re-settles overlaps by pushing neighbors along the shortest axis) and a `viewport`/`presets` slice for pan/zoom/jump-to-preset. `Canvas.tsx` renders non-fullscreen panes inside a single CSS-transformed layer (pan/zoom) and fullscreen panes as untransformed siblings (so fullscreen always covers the real window regardless of zoom). Two new Rust commands persist/restore the serializable layout (pane geometry + viewport + presets) to the app's data directory; restored panes get fresh PTYs naturally because `Pane.tsx` already spawns a shell per mounted pane id.

**Tech Stack:** Same as Phase 1 — Tauri 2, Rust, React + TypeScript, Vitest.

**Spec:** `docs/superpowers/specs/2026-08-12-cod-design.md` (see the "Open questions / future ideas" section this phase promotes into real requirements: collision-push overlap model, pan/zoom, presets, persistence).

## Global Constraints

- Panes must never overlap, ever — this replaces Phase 1's freeform-with-z-index-stacking model entirely.
- The actively-dragged/resized pane always lands exactly where the user put it; other panes get pushed out of the way, never the reverse.
- Fullscreen panes must visually cover the entire real window regardless of the canvas's current pan/zoom — do not let a CSS `transform` ancestor break `position: fixed` fullscreen panes (see Task 3's note).
- Persistence restores pane *geometry* (position/size/fullscreen) and the viewport, not actual shell process state — reopening gives you the same window layout with fresh shells in each slot, not resumed shell sessions. This is a known, accepted limitation (see spec).
- No automated UI test suite for drag/resize/pan/zoom gestures — pure reducer logic (collision resolution, viewport math) gets full unit coverage; interactive verification is manual, same split as Phase 1's Task 8 (automatable parts by agent, hands-on checklist by the human).
- Frontend continues using `@xterm/xterm`/`@xterm/addon-fit` (unchanged from Phase 1, not touched by this plan).

---

### Task 1: Pane chrome polish

**Files:**
- Modify: `cod/src/components/Pane.tsx`
- Modify: `cod/src/components/Canvas.tsx`

**Interfaces:**
- Consumes: `PaneState` from `cod/src/layout/paneLayout.ts` (unchanged in this task).
- Produces: `PaneProps` gains a new required `isFocused: boolean` prop. `Canvas.tsx` passes `isFocused={pane.id === state.focusedId}` when rendering each `Pane`. No other component in this plan depends on this yet, but it establishes the prop for later visual work.

- [ ] **Step 1: Replace the full contents of `cod/src/components/Pane.tsx`**

```tsx
import { useEffect, useRef } from 'react';
import type { PaneState } from '../layout/paneLayout';
import { usePty } from '../pty/usePty';

interface PaneProps {
  pane: PaneState;
  isFocused: boolean;
  onFocus: (id: string) => void;
  onMove: (id: string, x: number, y: number) => void;
  onResize: (id: string, width: number, height: number) => void;
  onToggleFullscreen: (id: string) => void;
  onClose: (id: string) => void;
}

const paneButtonStyle: React.CSSProperties = {
  border: 'none',
  background: 'transparent',
  color: 'var(--cod-fg)',
  width: 20,
  height: 20,
  borderRadius: 5,
  cursor: 'pointer',
  fontSize: 12,
  lineHeight: '20px',
  padding: 0,
  opacity: 0.55,
  transition: 'opacity 120ms ease, background 120ms ease',
};

export function Pane({ pane, isFocused, onFocus, onMove, onResize, onToggleFullscreen, onClose }: PaneProps) {
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

  const positionStyle: React.CSSProperties = pane.fullscreen
    ? { position: 'fixed', inset: 0, zIndex: 1000 }
    : {
        position: 'absolute',
        left: pane.x,
        top: pane.y,
        width: pane.width,
        height: pane.height,
        zIndex: 1,
      };

  return (
    <div
      style={{
        ...positionStyle,
        background: 'var(--cod-bg-elevated)',
        borderRadius: pane.fullscreen ? 0 : 10,
        boxShadow: isFocused
          ? '0 0 0 1.5px var(--cod-accent), 0 8px 24px rgba(0,0,0,0.35)'
          : '0 4px 16px rgba(0,0,0,0.25)',
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
        transition: 'box-shadow 150ms ease',
      }}
      onPointerDown={() => onFocus(pane.id)}
    >
      <div
        onPointerDown={startDrag}
        style={{
          height: 26,
          flexShrink: 0,
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'flex-end',
          gap: 4,
          padding: '0 6px',
          cursor: pane.fullscreen ? 'default' : 'move',
          background: 'transparent',
        }}
      >
        <button
          onClick={() => onToggleFullscreen(pane.id)}
          style={paneButtonStyle}
          onMouseEnter={(e) => (e.currentTarget.style.opacity = '1')}
          onMouseLeave={(e) => (e.currentTarget.style.opacity = '0.55')}
        >
          {pane.fullscreen ? '⤢' : '⤡'}
        </button>
        <button
          onClick={() => onClose(pane.id)}
          style={paneButtonStyle}
          onMouseEnter={(e) => (e.currentTarget.style.opacity = '1')}
          onMouseLeave={(e) => (e.currentTarget.style.opacity = '0.55')}
        >
          ×
        </button>
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

- [ ] **Step 2: Update `cod/src/components/Canvas.tsx` to pass `isFocused`**

In the `<Pane .../>` JSX, add the new prop (keep every existing prop as-is):

```tsx
        <Pane
          key={pane.id}
          pane={pane}
          isFocused={pane.id === state.focusedId}
          onFocus={(id) => dispatch({ type: 'FOCUS_PANE', id })}
          onMove={(id, x, y) => dispatch({ type: 'MOVE_PANE', id, x, y })}
          onResize={(id, width, height) => dispatch({ type: 'RESIZE_PANE', id, width, height })}
          onToggleFullscreen={(id) => dispatch({ type: 'TOGGLE_FULLSCREEN', id })}
          onClose={(id) => dispatch({ type: 'CLOSE_PANE', id })}
        />
```

- [ ] **Step 3: Verify it builds**

Run: `cd cod && npx tsc --noEmit`
Expected: no type errors.

- [ ] **Step 4: Commit**

```bash
git add cod/src/components/Pane.tsx cod/src/components/Canvas.tsx
git commit -m "cod: restyle pane chrome — minimalist icon buttons, focus ring"
```

---

### Task 2: Collision-push physics (pure reducer logic)

**Files:**
- Modify: `cod/src/layout/paneLayout.ts`
- Modify: `cod/src/layout/paneLayout.test.ts`

**Interfaces:**
- Produces (breaking change from Phase 1): `PaneState` no longer has a `zIndex` field. `LayoutState` no longer has `nextZIndex`. A new exported helper `panesOverlap(a: PaneState, b: PaneState): boolean` is added for tests (and later tasks) to check the never-overlap invariant. `layoutReducer`'s action union and all other exports (`initialLayoutState`, action types `ADD_PANE`/`MOVE_PANE`/`RESIZE_PANE`/`FOCUS_PANE`/`TOGGLE_FULLSCREEN`/`CLOSE_PANE`) keep their existing shapes. Every reducer branch spreads `...state` so later tasks (Task 3/4 add `viewport`/`presets` to `LayoutState`) don't need to touch these branches again.
- Note: `Pane.tsx`/`Canvas.tsx` do NOT need changes for this task — Task 1 already hardcoded `zIndex: 1` / `zIndex: 1000` inline instead of reading `pane.zIndex`, so removing the field from `PaneState` doesn't break the build.

- [ ] **Step 1: Write the failing tests**

Replace the full contents of `cod/src/layout/paneLayout.test.ts`:

```typescript
import { describe, expect, it } from 'vitest';
import {
  initialLayoutState,
  layoutReducer,
  panesOverlap,
  type LayoutState,
} from './paneLayout';

function anyOverlap(state: LayoutState): boolean {
  const { panes } = state;
  for (let i = 0; i < panes.length; i++) {
    for (let j = i + 1; j < panes.length; j++) {
      if (panesOverlap(panes[i], panes[j])) return true;
    }
  }
  return false;
}

describe('layoutReducer', () => {
  it('adds a pane with default geometry and focuses it', () => {
    const state = layoutReducer(initialLayoutState, { type: 'ADD_PANE', id: 'p1' });
    expect(state.panes).toHaveLength(1);
    expect(state.panes[0]).toMatchObject({ id: 'p1', fullscreen: false });
    expect(state.focusedId).toBe('p1');
  });

  it('never lets two panes overlap after adding several', () => {
    let state = initialLayoutState;
    for (const id of ['p1', 'p2', 'p3', 'p4']) {
      state = layoutReducer(state, { type: 'ADD_PANE', id });
    }
    expect(anyOverlap(state)).toBe(false);
  });

  it('moves the dragged pane to exactly the requested position', () => {
    let state = layoutReducer(initialLayoutState, { type: 'ADD_PANE', id: 'p1' });
    state = layoutReducer(state, { type: 'MOVE_PANE', id: 'p1', x: 500, y: 500 });
    const p1 = state.panes.find((p) => p.id === 'p1')!;
    expect(p1.x).toBe(500);
    expect(p1.y).toBe(500);
  });

  it('pushes a pane out of the way when dragged into it, with no overlap remaining', () => {
    let state = layoutReducer(initialLayoutState, { type: 'ADD_PANE', id: 'p1' });
    state = layoutReducer(state, { type: 'ADD_PANE', id: 'p2' });
    state = layoutReducer(state, { type: 'MOVE_PANE', id: 'p1', x: 0, y: 0 });
    const p1 = state.panes.find((p) => p.id === 'p1')!;
    state = layoutReducer(state, { type: 'MOVE_PANE', id: 'p2', x: p1.x, y: p1.y });
    const draggedP2 = state.panes.find((p) => p.id === 'p2')!;
    expect(draggedP2.x).toBe(p1.x);
    expect(draggedP2.y).toBe(p1.y);
    expect(anyOverlap(state)).toBe(false);
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

  it('pushes a neighbor away when resizing into it, with no overlap remaining', () => {
    let state = layoutReducer(initialLayoutState, { type: 'ADD_PANE', id: 'p1' });
    state = layoutReducer(state, { type: 'ADD_PANE', id: 'p2' });
    state = layoutReducer(state, { type: 'RESIZE_PANE', id: 'p1', width: 2000, height: 2000 });
    expect(anyOverlap(state)).toBe(false);
    const p1 = state.panes.find((p) => p.id === 'p1')!;
    expect(p1.width).toBe(2000);
    expect(p1.height).toBe(2000);
  });

  it('excludes a fullscreen pane from overlap checks, and resolves overlap when it exits fullscreen', () => {
    let state = layoutReducer(initialLayoutState, { type: 'ADD_PANE', id: 'p1' });
    state = layoutReducer(state, { type: 'ADD_PANE', id: 'p2' });
    state = layoutReducer(state, { type: 'TOGGLE_FULLSCREEN', id: 'p1' });
    const p1Full = state.panes.find((p) => p.id === 'p1')!;
    expect(p1Full.fullscreen).toBe(true);
    state = layoutReducer(state, { type: 'MOVE_PANE', id: 'p2', x: p1Full.x, y: p1Full.y });
    const p2 = state.panes.find((p) => p.id === 'p2')!;
    expect(p2.x).toBe(p1Full.x);
    state = layoutReducer(state, { type: 'TOGGLE_FULLSCREEN', id: 'p1' });
    expect(anyOverlap(state)).toBe(false);
  });

  it('reassigns focus to a remaining pane on close, or null if none remain', () => {
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
Expected: FAIL — `panesOverlap` doesn't exist yet, and the current implementation still has `zIndex`.

- [ ] **Step 3: Replace the full contents of `cod/src/layout/paneLayout.ts`**

```typescript
export interface PaneState {
  id: string;
  x: number;
  y: number;
  width: number;
  height: number;
  fullscreen: boolean;
}

export interface LayoutState {
  panes: PaneState[];
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
  focusedId: null,
};

const DEFAULT_WIDTH = 640;
const DEFAULT_HEIGHT = 400;
const MIN_WIDTH = 200;
const MIN_HEIGHT = 120;
const CASCADE_OFFSET = 32;
const CASCADE_WRAP = 8;
const COLLISION_PASSES = 8;

export function panesOverlap(a: PaneState, b: PaneState): boolean {
  if (a.fullscreen || b.fullscreen) return false;
  const overlapX = Math.min(a.x + a.width, b.x + b.width) - Math.max(a.x, b.x);
  const overlapY = Math.min(a.y + a.height, b.y + b.height) - Math.max(a.y, b.y);
  return overlapX > 0 && overlapY > 0;
}

function overlapAmount(a: PaneState, b: PaneState): { dx: number; dy: number } | null {
  const overlapX = Math.min(a.x + a.width, b.x + b.width) - Math.max(a.x, b.x);
  const overlapY = Math.min(a.y + a.height, b.y + b.height) - Math.max(a.y, b.y);
  if (overlapX <= 0 || overlapY <= 0) return null;
  return { dx: overlapX, dy: overlapY };
}

function pushAway(anchor: PaneState, moving: PaneState, amount: { dx: number; dy: number }): PaneState {
  const anchorCenterX = anchor.x + anchor.width / 2;
  const anchorCenterY = anchor.y + anchor.height / 2;
  const movingCenterX = moving.x + moving.width / 2;
  const movingCenterY = moving.y + moving.height / 2;
  if (amount.dx < amount.dy) {
    const dir = movingCenterX >= anchorCenterX ? 1 : -1;
    return { ...moving, x: moving.x + dir * amount.dx };
  }
  const dir = movingCenterY >= anchorCenterY ? 1 : -1;
  return { ...moving, y: moving.y + dir * amount.dy };
}

function resolveCollisions(panes: PaneState[], anchorId: string): PaneState[] {
  const result = panes.map((p) => ({ ...p }));
  const settled = new Set<string>([anchorId]);
  const maxPasses = Math.max(COLLISION_PASSES, result.length);
  for (let pass = 0; pass < maxPasses; pass++) {
    let anyPush = false;
    for (let i = 0; i < result.length; i++) {
      for (let j = i + 1; j < result.length; j++) {
        const a = result[i];
        const b = result[j];
        if (a.fullscreen || b.fullscreen) continue;
        const amount = overlapAmount(a, b);
        if (!amount) continue;
        anyPush = true;
        const aSettled = settled.has(a.id);
        const bSettled = settled.has(b.id);
        if (aSettled && !bSettled) {
          result[j] = pushAway(a, b, amount);
          settled.add(b.id);
        } else if (bSettled && !aSettled) {
          result[i] = pushAway(b, a, amount);
          settled.add(a.id);
        } else if (a.id < b.id) {
          result[j] = pushAway(a, b, amount);
          settled.add(a.id);
          settled.add(b.id);
        } else {
          result[i] = pushAway(b, a, amount);
          settled.add(a.id);
          settled.add(b.id);
        }
      }
    }
    if (!anyPush) break;
  }
  return result;
}

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
        fullscreen: false,
      };
      const panes = resolveCollisions([...state.panes, pane], action.id);
      return { ...state, panes, focusedId: action.id };
    }

    case 'MOVE_PANE': {
      const target = state.panes.find((p) => p.id === action.id);
      if (!target || target.fullscreen) return state;
      const moved = state.panes.map((p) =>
        p.id === action.id ? { ...p, x: action.x, y: action.y } : p,
      );
      return { ...state, panes: resolveCollisions(moved, action.id) };
    }

    case 'RESIZE_PANE': {
      const target = state.panes.find((p) => p.id === action.id);
      if (!target || target.fullscreen) return state;
      const resized = state.panes.map((p) =>
        p.id === action.id
          ? {
              ...p,
              width: Math.max(MIN_WIDTH, action.width),
              height: Math.max(MIN_HEIGHT, action.height),
            }
          : p,
      );
      return { ...state, panes: resolveCollisions(resized, action.id) };
    }

    case 'FOCUS_PANE': {
      if (state.focusedId === action.id) return state;
      return { ...state, focusedId: action.id };
    }

    case 'TOGGLE_FULLSCREEN': {
      const target = state.panes.find((p) => p.id === action.id);
      if (!target) return state;
      const toggled = state.panes.map((p) =>
        p.id === action.id ? { ...p, fullscreen: !p.fullscreen } : p,
      );
      const panes = target.fullscreen ? resolveCollisions(toggled, action.id) : toggled;
      return { ...state, panes, focusedId: action.id };
    }

    case 'CLOSE_PANE': {
      const remaining = state.panes.filter((p) => p.id !== action.id);
      let focusedId = state.focusedId;
      if (state.focusedId === action.id) {
        focusedId = remaining.length > 0 ? remaining[remaining.length - 1].id : null;
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
Expected: PASS (all 9 tests)

- [ ] **Step 5: Verify the whole frontend still type-checks**

Run: `cd cod && npx tsc --noEmit`
Expected: no errors (confirms `Pane.tsx`/`Canvas.tsx` from Task 1 don't reference the removed `zIndex`/`nextZIndex` fields).

- [ ] **Step 6: Commit**

```bash
git add cod/src/layout/paneLayout.ts cod/src/layout/paneLayout.test.ts
git commit -m "cod: replace z-index stacking with collision-push physics"
```

---

### Task 3: Viewport pan/zoom

**Files:**
- Modify: `cod/src/layout/paneLayout.ts`
- Modify: `cod/src/layout/paneLayout.test.ts`
- Modify: `cod/src/components/Canvas.tsx`

**Interfaces:**
- Produces: new exported `Viewport` interface (`{ x: number; y: number; scale: number }`), `LayoutState.viewport: Viewport`, two new `LayoutAction` variants `PAN` and `ZOOM_AT`. Task 4 (presets) and Task 5 (persistence) both read/write `state.viewport`.
- Consumes: `layoutReducer`/`initialLayoutState`/`LayoutState` from Task 2 (this task extends, not replaces, that file).

**Important implementation note:** fullscreen panes use `position: fixed`, which is normally relative to the browser window — but a CSS `transform` on an ancestor element makes that ancestor the containing block for `position: fixed` descendants, breaking "fullscreen covers the whole window." So `Canvas.tsx` must render fullscreen panes as **siblings outside** the pan/zoom-transformed layer, not inside it. Non-fullscreen panes render inside the transformed layer.

- [ ] **Step 1: Write the failing tests**

Add to the end of the `describe('layoutReducer', ...)` block in `cod/src/layout/paneLayout.test.ts` (keep every existing test):

```typescript
  it('pans the viewport by the given delta', () => {
    const state = layoutReducer(initialLayoutState, { type: 'PAN', dx: 50, dy: -20 });
    expect(state.viewport.x).toBe(-50);
    expect(state.viewport.y).toBe(20);
  });

  it('clamps zoom to the min/max scale', () => {
    let state = initialLayoutState;
    for (let i = 0; i < 20; i++) {
      state = layoutReducer(state, { type: 'ZOOM_AT', screenX: 100, screenY: 100, deltaScale: 1 });
    }
    expect(state.viewport.scale).toBeLessThanOrEqual(2);

    state = initialLayoutState;
    for (let i = 0; i < 20; i++) {
      state = layoutReducer(state, { type: 'ZOOM_AT', screenX: 0, screenY: 0, deltaScale: -0.5 });
    }
    expect(state.viewport.scale).toBeGreaterThanOrEqual(0.25);
  });

  it('keeps the point under the cursor fixed while zooming', () => {
    const zoomed = layoutReducer(initialLayoutState, {
      type: 'ZOOM_AT',
      screenX: 200,
      screenY: 150,
      deltaScale: 1,
    });
    const worldXAfter = (200 - zoomed.viewport.x) / zoomed.viewport.scale;
    const worldYAfter = (150 - zoomed.viewport.y) / zoomed.viewport.scale;
    expect(worldXAfter).toBeCloseTo(200, 5);
    expect(worldYAfter).toBeCloseTo(150, 5);
  });
```

Also update the import line to include `Viewport` isn't needed in the test file (not used directly), so no import change needed there.

- [ ] **Step 2: Run tests to verify they fail**

Run: `cd cod && npx vitest run src/layout/paneLayout.test.ts`
Expected: FAIL — `PAN`/`ZOOM_AT` action types and `state.viewport` don't exist yet.

- [ ] **Step 3: Extend `cod/src/layout/paneLayout.ts`**

Add this new exported interface near the top, right after `PaneState`:

```typescript
export interface Viewport {
  x: number;
  y: number;
  scale: number;
}
```

Change `LayoutState` to:

```typescript
export interface LayoutState {
  panes: PaneState[];
  focusedId: string | null;
  viewport: Viewport;
}
```

Add two variants to the `LayoutAction` union (keep all six existing variants):

```typescript
  | { type: 'PAN'; dx: number; dy: number }
  | { type: 'ZOOM_AT'; screenX: number; screenY: number; deltaScale: number }
```

Change `initialLayoutState` to:

```typescript
export const initialLayoutState: LayoutState = {
  panes: [],
  focusedId: null,
  viewport: { x: 0, y: 0, scale: 1 },
};
```

Add these two constants near the other constants:

```typescript
const MIN_SCALE = 0.25;
const MAX_SCALE = 2;
```

Add these two cases inside the `layoutReducer` switch (anywhere among the existing cases, before `default`):

```typescript
    case 'PAN': {
      return {
        ...state,
        viewport: { ...state.viewport, x: state.viewport.x - action.dx, y: state.viewport.y - action.dy },
      };
    }

    case 'ZOOM_AT': {
      const { viewport } = state;
      const newScale = Math.min(MAX_SCALE, Math.max(MIN_SCALE, viewport.scale * (1 + action.deltaScale)));
      if (newScale === viewport.scale) return state;
      const worldX = (action.screenX - viewport.x) / viewport.scale;
      const worldY = (action.screenY - viewport.y) / viewport.scale;
      const newX = action.screenX - worldX * newScale;
      const newY = action.screenY - worldY * newScale;
      return { ...state, viewport: { x: newX, y: newY, scale: newScale } };
    }
```

(All other cases already spread `...state`, from Task 2, so `viewport` carries through them automatically — do not need to touch them.)

- [ ] **Step 4: Run tests to verify they pass**

Run: `cd cod && npx vitest run src/layout/paneLayout.test.ts`
Expected: PASS (all 12 tests)

- [ ] **Step 5: Update `Canvas.tsx` to render the transformed layer and handle wheel input**

Replace the full contents of `cod/src/components/Canvas.tsx`:

```tsx
import { useEffect, useReducer, useRef } from 'react';
import { initialLayoutState, layoutReducer } from '../layout/paneLayout';
import { Pane } from './Pane';

let paneCounter = 0;

export function Canvas() {
  const [state, dispatch] = useReducer(layoutReducer, initialLayoutState);
  const containerRef = useRef<HTMLDivElement>(null);

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

  useEffect(() => {
    const el = containerRef.current;
    if (!el) return;
    function onWheel(e: WheelEvent) {
      e.preventDefault();
      const rect = el!.getBoundingClientRect();
      if (e.ctrlKey) {
        dispatch({
          type: 'ZOOM_AT',
          screenX: e.clientX - rect.left,
          screenY: e.clientY - rect.top,
          deltaScale: -e.deltaY * 0.01,
        });
      } else {
        dispatch({ type: 'PAN', dx: e.deltaX, dy: e.deltaY });
      }
    }
    el.addEventListener('wheel', onWheel, { passive: false });
    return () => el.removeEventListener('wheel', onWheel);
  }, []);

  const normalPanes = state.panes.filter((p) => !p.fullscreen);
  const fullscreenPanes = state.panes.filter((p) => p.fullscreen);

  function renderPane(pane: (typeof state.panes)[number]) {
    return (
      <Pane
        key={pane.id}
        pane={pane}
        isFocused={pane.id === state.focusedId}
        onFocus={(id) => dispatch({ type: 'FOCUS_PANE', id })}
        onMove={(id, x, y) => dispatch({ type: 'MOVE_PANE', id, x, y })}
        onResize={(id, width, height) => dispatch({ type: 'RESIZE_PANE', id, width, height })}
        onToggleFullscreen={(id) => dispatch({ type: 'TOGGLE_FULLSCREEN', id })}
        onClose={(id) => dispatch({ type: 'CLOSE_PANE', id })}
      />
    );
  }

  return (
    <div
      ref={containerRef}
      style={{ position: 'relative', width: '100%', height: '100%', overflow: 'hidden' }}
    >
      <div
        style={{
          position: 'absolute',
          inset: 0,
          transform: `translate(${state.viewport.x}px, ${state.viewport.y}px) scale(${state.viewport.scale})`,
          transformOrigin: '0 0',
          transition: 'transform 150ms ease',
        }}
      >
        {normalPanes.map(renderPane)}
      </div>
      {fullscreenPanes.map(renderPane)}
    </div>
  );
}
```

- [ ] **Step 6: Verify it builds**

Run: `cd cod && npx tsc --noEmit`
Expected: no type errors.

- [ ] **Step 7: Commit**

```bash
git add cod/src/layout/paneLayout.ts cod/src/layout/paneLayout.test.ts cod/src/components/Canvas.tsx
git commit -m "cod: add pan/zoom viewport with cursor-anchored zoom"
```

---

### Task 4: Assignable viewport presets

**Files:**
- Modify: `cod/src/layout/paneLayout.ts`
- Modify: `cod/src/layout/paneLayout.test.ts`
- Modify: `cod/src/components/Canvas.tsx`

**Interfaces:**
- Consumes: `Viewport`, `LayoutState.viewport` from Task 3.
- Produces: `LayoutState.presets: Record<string, Viewport>`, two new `LayoutAction` variants `SAVE_PRESET` and `JUMP_TO_PRESET`. Task 5 (persistence) reads/writes `state.presets`.

- [ ] **Step 1: Write the failing tests**

Add to the end of the `describe('layoutReducer', ...)` block in `cod/src/layout/paneLayout.test.ts`:

```typescript
  it('saves the current viewport as a preset and jumps back to it', () => {
    let state = layoutReducer(initialLayoutState, { type: 'PAN', dx: 100, dy: 50 });
    state = layoutReducer(state, { type: 'SAVE_PRESET', slot: '1' });
    state = layoutReducer(state, { type: 'PAN', dx: -500, dy: -500 });
    expect(state.viewport.x).not.toBe(-100);
    state = layoutReducer(state, { type: 'JUMP_TO_PRESET', slot: '1' });
    expect(state.viewport).toEqual({ x: -100, y: -50, scale: 1 });
  });

  it('ignores jumping to an unset preset slot', () => {
    const before = layoutReducer(initialLayoutState, { type: 'PAN', dx: 10, dy: 10 });
    const after = layoutReducer(before, { type: 'JUMP_TO_PRESET', slot: '9' });
    expect(after.viewport).toEqual(before.viewport);
  });
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cd cod && npx vitest run src/layout/paneLayout.test.ts`
Expected: FAIL — `SAVE_PRESET`/`JUMP_TO_PRESET` don't exist yet.

- [ ] **Step 3: Extend `cod/src/layout/paneLayout.ts`**

Change `LayoutState` to add `presets`:

```typescript
export interface LayoutState {
  panes: PaneState[];
  focusedId: string | null;
  viewport: Viewport;
  presets: Record<string, Viewport>;
}
```

Add to `initialLayoutState`:

```typescript
export const initialLayoutState: LayoutState = {
  panes: [],
  focusedId: null,
  viewport: { x: 0, y: 0, scale: 1 },
  presets: {},
};
```

Add two variants to `LayoutAction`:

```typescript
  | { type: 'SAVE_PRESET'; slot: string }
  | { type: 'JUMP_TO_PRESET'; slot: string }
```

Add two cases to the `layoutReducer` switch:

```typescript
    case 'SAVE_PRESET': {
      return { ...state, presets: { ...state.presets, [action.slot]: { ...state.viewport } } };
    }

    case 'JUMP_TO_PRESET': {
      const preset = state.presets[action.slot];
      if (!preset) return state;
      return { ...state, viewport: { ...preset } };
    }
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cd cod && npx vitest run src/layout/paneLayout.test.ts`
Expected: PASS (all 14 tests)

- [ ] **Step 5: Add keyboard shortcuts in `Canvas.tsx`**

In `cod/src/components/Canvas.tsx`, find the existing `onKeyDown` handler (the one checking `e.metaKey && e.key.toLowerCase() === 'n'`) and add these checks at the top of the function, before the `Cmd+N` check:

```typescript
      if (e.metaKey && e.shiftKey && /^[1-9]$/.test(e.key)) {
        e.preventDefault();
        dispatch({ type: 'SAVE_PRESET', slot: e.key });
        return;
      }
      if (e.metaKey && !e.shiftKey && /^[1-9]$/.test(e.key)) {
        e.preventDefault();
        dispatch({ type: 'JUMP_TO_PRESET', slot: e.key });
        return;
      }
```

The full handler should now check preset-save, then preset-jump, then `Cmd+N`, in that order (order matters: `Cmd+Shift+1` must not also match a looser `Cmd+1` check).

- [ ] **Step 6: Verify it builds**

Run: `cd cod && npx tsc --noEmit`
Expected: no type errors.

- [ ] **Step 7: Commit**

```bash
git add cod/src/layout/paneLayout.ts cod/src/layout/paneLayout.test.ts cod/src/components/Canvas.tsx
git commit -m "cod: add Cmd+Shift+1-9 save / Cmd+1-9 jump viewport presets"
```

---

### Task 5: Layout persistence across restarts

**Files:**
- Modify: `cod/src-tauri/src/lib.rs`
- Modify: `cod/src/components/Canvas.tsx`

**Interfaces:**
- Produces: two new Tauri commands, `save_layout(json: String) -> Result<(), String>` and `load_layout() -> Result<Option<String>, String>`, registered in `tauri::generate_handler![...]` alongside the existing PTY commands and `greet`.
- Consumes: `LayoutState`'s `panes`/`viewport`/`presets` fields (from Tasks 2-4) — the persisted JSON shape is exactly `{ panes, viewport, presets }`.

- [ ] **Step 1: Add the Rust commands to `cod/src-tauri/src/lib.rs`**

Add these two functions near the other `#[tauri::command]` functions (any order relative to them is fine):

```rust
#[tauri::command]
fn save_layout(json: String, app: tauri::AppHandle) -> Result<(), String> {
    let dir = app.path().app_data_dir().map_err(|e| e.to_string())?;
    std::fs::create_dir_all(&dir).map_err(|e| e.to_string())?;
    std::fs::write(dir.join("layout.json"), json).map_err(|e| e.to_string())
}

#[tauri::command]
fn load_layout(app: tauri::AppHandle) -> Result<Option<String>, String> {
    let dir = app.path().app_data_dir().map_err(|e| e.to_string())?;
    let path = dir.join("layout.json");
    if !path.exists() {
        return Ok(None);
    }
    std::fs::read_to_string(path).map(Some).map_err(|e| e.to_string())
}
```

Add `save_layout, load_layout` to the `tauri::generate_handler![...]` list (alongside `greet`, `spawn_pane`, `write_to_pane`, `resize_pane`, `close_pane`).

If `app.path()` requires the `tauri::Manager` trait to be in scope and it isn't already imported, add `use tauri::Manager;` to the top of the file (check the existing imports first — Task 3 of the core-shell plan already discussed this API and may have already imported it, or removed it as unused; only add it if `cargo build` reports `path` isn't a method on `AppHandle`).

- [ ] **Step 2: Verify the Rust side builds**

Run: `cd cod/src-tauri && cargo build`
Expected: no errors.

- [ ] **Step 3: Wire persistence into `Canvas.tsx`**

Replace the two existing `useEffect` calls that currently handle `Cmd+N`/preset-shortcuts and "add first pane if empty" — keep the `Cmd+N`/preset-shortcut effect exactly as Task 4 left it, but replace the "if empty on mount, add first pane" effect with restore-or-default logic, and add a new debounced-autosave effect. The relevant part of `Canvas.tsx` becomes:

```tsx
import { useEffect, useReducer, useRef } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { initialLayoutState, layoutReducer, type PaneState, type Viewport } from '../layout/paneLayout';
import { Pane } from './Pane';

let paneCounter = 0;

interface PersistedLayout {
  panes: PaneState[];
  viewport: Viewport;
  presets: Record<string, Viewport>;
}

export function Canvas() {
  const [state, dispatch] = useReducer(layoutReducer, initialLayoutState);
  const containerRef = useRef<HTMLDivElement>(null);

  // ... keep the existing Cmd+N / SAVE_PRESET / JUMP_TO_PRESET keydown effect exactly as-is ...

  useEffect(() => {
    let cancelled = false;
    (async () => {
      const raw = await invoke<string | null>('load_layout');
      if (cancelled) return;
      if (raw) {
        try {
          const parsed = JSON.parse(raw) as PersistedLayout;
          if (parsed.panes.length > 0) {
            dispatch({
              type: 'RESTORE_LAYOUT',
              panes: parsed.panes,
              viewport: parsed.viewport,
              presets: parsed.presets,
            });
            const maxSuffix = parsed.panes.reduce((max, p) => {
              const match = /^frontend-pane-(\d+)$/.exec(p.id);
              return match ? Math.max(max, Number(match[1])) : max;
            }, 0);
            paneCounter = maxSuffix;
            return;
          }
        } catch {
          // corrupt persisted layout — fall through to default first-pane behavior
        }
      }
      paneCounter += 1;
      dispatch({ type: 'ADD_PANE', id: `frontend-pane-${paneCounter}` });
    })();
    return () => {
      cancelled = true;
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  useEffect(() => {
    const timer = setTimeout(() => {
      void invoke('save_layout', {
        json: JSON.stringify({ panes: state.panes, viewport: state.viewport, presets: state.presets }),
      });
    }, 400);
    return () => clearTimeout(timer);
  }, [state]);

  // ... keep the existing wheel-handling effect, renderPane, and JSX return exactly as Task 3/4 left them ...
}
```

Delete the old `useEffect(() => { if (state.panes.length === 0) { ... } }, [])` block entirely — the restore-or-default effect above replaces it.

- [ ] **Step 4: Add the `RESTORE_LAYOUT` action to the reducer**

In `cod/src/layout/paneLayout.ts`, add one more variant to `LayoutAction`:

```typescript
  | { type: 'RESTORE_LAYOUT'; panes: PaneState[]; viewport: Viewport; presets: Record<string, Viewport> }
```

Add one more case to `layoutReducer`:

```typescript
    case 'RESTORE_LAYOUT': {
      return {
        panes: action.panes,
        focusedId: action.panes[0]?.id ?? null,
        viewport: action.viewport,
        presets: action.presets,
      };
    }
```

- [ ] **Step 5: Add a test for `RESTORE_LAYOUT`**

Add to `cod/src/layout/paneLayout.test.ts`:

```typescript
  it('restores a full layout from persisted state', () => {
    const restored = layoutReducer(initialLayoutState, {
      type: 'RESTORE_LAYOUT',
      panes: [{ id: 'p1', x: 10, y: 10, width: 300, height: 200, fullscreen: false }],
      viewport: { x: 5, y: 5, scale: 1.5 },
      presets: { '1': { x: 0, y: 0, scale: 1 } },
    });
    expect(restored.panes).toHaveLength(1);
    expect(restored.focusedId).toBe('p1');
    expect(restored.viewport).toEqual({ x: 5, y: 5, scale: 1.5 });
    expect(restored.presets['1']).toEqual({ x: 0, y: 0, scale: 1 });
  });
```

- [ ] **Step 6: Run all frontend tests, verify build**

Run:
```
cd cod && npx vitest run src/layout/paneLayout.test.ts
npx tsc --noEmit
```
Expected: all tests pass (15 total), no type errors.

- [ ] **Step 7: Commit**

```bash
git add cod/src-tauri/src/lib.rs cod/src/layout/paneLayout.ts cod/src/layout/paneLayout.test.ts cod/src/components/Canvas.tsx
git commit -m "cod: persist and restore pane layout, viewport, and presets across restarts"
```

---

### Task 6: End-to-end verification

**Files:** none (verification only)

**Interfaces:** none — confirms Tasks 1-5 work together.

- [ ] **Step 1: Run the full test suites**

```
cd cod/src-tauri && cargo test
cd .. && npx vitest run
```
Expected: all Rust and Vitest tests pass (1 Rust test from Phase 1, 15 Vitest tests from this plan).

- [ ] **Step 2: Confirm the app builds and launches cleanly**

Run `npm run tauri dev` from `cod/`, confirm the Vite server, Cargo build, and Tauri window-ready output all appear with no errors, then stop the process. (Same automated-only verification bar as Phase 1's Task 8 — driving the actual window is a human step, see Step 3.)

- [ ] **Step 3: Interactive checklist (for the human partner, not an agent)**

Run the app and confirm:
- Dragging one pane into another pushes the other pane aside — they never overlap.
- Resizing a pane so it would overlap a neighbor pushes the neighbor aside instead.
- Fullscreening a pane covers the entire window even after panning/zooming the canvas.
- Two-finger trackpad scroll pans the canvas; pinch-to-zoom (trackpad) zooms toward the cursor.
- `Cmd+Shift+1` saves the current view; panning away and pressing `Cmd+1` animates back to it.
- Pane buttons look like flat minimalist icons, not native OS buttons; the focused pane shows a subtle accent-colored ring.
- Quit the app (or stop `tauri dev`) and relaunch: the same panes reappear in the same positions/sizes, and the last-used viewport/presets are intact (shells themselves are fresh, per the spec's documented limitation).

- [ ] **Step 4: Commit the phase completion marker (only after the human confirms Step 3)**

```bash
cd /Users/emgor/Documents/_EMGOR_SYNTH/______2026NEW
git add cod
git commit -m "cod: Phase 1b canvas redesign complete and manually verified"
```
