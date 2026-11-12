# Cod full-stack overhaul — design spec (2026-08-16)

Requirements source: `cod/docs/2026-08-16-full-stack-cod-requirements.md` (both rounds). This spec is the approved design. Five workstreams, built in parallel by Sonnet subagents, integrated in order A → B → C, with D and E independent.

## Workstream A — Interaction physics ("ghost until clicked")

**Model.** Every pane has exactly two states:

- **Unfocused (ghost)**: content is `pointer-events: none` permanently — the canvas receives ALL wheel/pinch/pan/drag gestures over the pane, no exceptions. The pane remains **fully visible, never blurred or dimmed**. It keeps one hit-surface: pointerdown anywhere on it = grab-anywhere drag; a click (down+up without significant movement) = focus it.
- **Focused**: full content interactivity (terminal keyboard/scroll, IDE editing, etc.). Move by grabbing the **top bar only**. Deselect by **clicking outside the pane** — never Escape (Claude Code owns Escape).

**Stonewall collisions.** Dragged panes NEVER displace other panes. Other panes are rigid barriers: the drag stops flush at their edge; if the pointer travels far enough past, the dragged pane jumps to the far side (keep the existing barrier-jump implementation, verify it against ghost-state changes).

**Current-size correctness.** All drag/collision/jump/grab math reads the pane's **live width/height from the layout store** (post-resize), never creation-time dimensions. Audit `paneLayout.ts` + `Canvas.tsx` for any cached/original-size usage; add regression tests that resize then drag.

**Edge resizing.** All four edges + corners of a focused pane are drag-to-resize handles (~6px hot zone, proper cursors). Resize obeys min sizes and the usage-pane 320px height cap. Resizing updates the same live layout store (feeding the size-correctness rule).

**Pane title.** A `/name`d pane shows its name **bold, top-center** of the header bar.

**Testing.** Synthetic pointer-event suites (vitest + jsdom) for: ghost pane never captures wheel/scroll; canvas zoom over a terminal works; grab-anywhere on unfocused; barrier stop + far-side jump; resized-then-dragged uses new size; click-out deselects; Escape does NOT deselect. Plus a standalone minimal HTML repro to verify in real WKWebView.

## Workstream B — Cod control plane: the md-file bus + skill overhaul

Cod is **text-only**. No visual tools. Two files in `<app_data_dir>/cod-bus/` are the entire interface:

### `cod-out.md` (cod → app, command channel)
Cod appends commands, one per line, simple grep-able DSL:

```
SPAWN type:terminal cwd:/path name:builder
RUN_CLAUDE pane:builder model:sonnet prompt:"do X; when done reply exactly DONE: <summary>"
TYPE pane:builder text:"continue with step 2"
KEY pane:builder key:Enter        # also Up/Down/1..9/Tab/Ctrl-C etc.
MOVE pane:builder x:420 y:80
RESIZE pane:builder w:900 h:560
FOCUS pane:builder | CLOSE pane:x | NAME pane:x name:y
APPROVE pane:builder option:1
ASK_USER text:"builder asks: which DB should it use?"
SET_DEFAULT model:sonnet
```

The app watches the file (notify crate, debounced), **atomically reads + truncates**, executes every command, and writes per-command acks (seq number, ok/error) into the STATUS section of `cod-in.md`. Unknown/malformed lines get an error ack, never a crash. Pane refs accept name or id.

### `cod-in.md` (app → cod, state digest)
**Overwritten** (never appended) on every meaningful change, engineered for token efficiency. Sections:

1. `## PANES` — one line per pane (every type incl. files/IDE/usage/dashboard): `id | name | type | x,y | w×h | focused? | agent-status`.
2. `## ACKS` — results of recent cod-out commands (seq, ok/err, detail), pruned aggressively.
3. `## EVENTS` — ONLY three event kinds, each tagged by pane:
   - `RESPONSE` — an agent's final response tail (cod instructs agents to end with a sentinel like `DONE: …` so the app can extract just that).
   - `APPROVAL` — a Claude Code permission prompt, verbatim (tool, command, options).
   - `QUESTION` — a question directed at the user.
   Streaming output is never included. Old events pruned after ack.

### Cod's loop and policy (skill rewrite: `cod/skills/cod-self-awareness/SKILL.md` → operator playbook)
- Loop: read `cod-in.md` → decide → write `cod-out.md`. Backend keeps `ask_cod` but the prompt now embeds the current `cod-in.md` digest.
- **Approvals**: cod is the approval layer. NEVER enable skip-permissions on spawned claudes. On every `APPROVAL` event: instantly `APPROVE` unless it's a true security threat (credential exfiltration, destructive rm/force-push, network exfil, touching files outside the working folder, anything violating the _EMGOR_SYNTH no-delete rule) → then `ASK_USER` and wait; user approves directly. The old Haiku approval-broker path is superseded by this (keep code, disable as default).
- **Questions**: cod NEVER answers a question meant for the user — always forwards via `ASK_USER`.
- **Models**: default fan-out model is a setting (`SET_DEFAULT model:` / config file), user-preferred initial value `sonnet`, overridable per `RUN_CLAUDE`. Never hardcoded.
- **Fan-out protocol** documented in the skill: spawn pane → run claude → instruct exact-format sentinel responses → poll `RESPONSE` events → extract.

**Testing.** Rust integration tests: write commands to cod-out → assert executed against real AppState + real PTY (bytes arrive) + cod-in regenerated correctly; malformed-command fuzz; approval-classification prompt tests; truncate-race test (write during read).

## Workstream C — Fleet & agent visualization (after B merges)
- `RUN_CLAUDE`-spawned panes are full interactive Claude Code terminals — user can click in and type directly (guaranteed interactive fan-out path).
- Stock in-process Task subagents: Claude Code hooks (SubagentStart/Stop, tool events) notify the app → auto-spawn **watch-only viewer panes** live-tailing the subagent's sidechain transcript JSONL, auto-arranged near the parent pane, auto-labeled. (Direct interaction with these is impossible in stock CC — documented; the DIY claudecode workstream inherits this as a hard requirement.)
- DIY claudecode: pinned. Future spec must guarantee: children as real PTY processes (fully interactive), compatibility with community skills (markdown dirs), hooks (settings.json), MCP.

## Workstream D — Claude Dashboard pane
- Hidden Tauri webview logged into claude.ai (one-time login inside the app; session persists in app storage — no Chrome, no API tokens, autonomous).
- Scraper polls it for: current balance, auto-reload state, monthly spending, extra-usage consumption/limits. Native tiles render everything alongside existing session/weekly usage data. Actions: toggle auto-reload, extend extra usage — executed inside the hidden webview.
- Staleness self-check: if selectors fail, show "data stale — scraper needs update", never silently-wrong numbers. Claude-only; other vendors future.

## Workstream E — Competitive research (read-only)
Teardown of Neovim, tmux, Zellij, Warp, Ghostty + agentic-tool landscape → concrete steal-list ranked by impact: modal/leader-key pane navigation, jump-to-pane, command palette, session persistence, viral hooks (fleet screenshot, session timelapse export). Output: `cod/docs/2026-08-16-competitive-research.md`. Results may be pushed to in-flight agents.

## Integration & test gate
Merge order A → B → C; D, E independent. Gate before user testing: `npx tsc --noEmit`, `npx vitest run`, `cargo build && cargo test`, headless PTY integration tests, then fresh `.app` deploy (build → pkill -x cod → replace /Applications/cod.app → open → verify NEW pid) and a specific user test checklist. Trust user reports over static review (HANDOFF.md lesson).
