# Cod — AI-centered terminal shell

Date: 2026-08-12
Status: approved, ready for implementation planning

## Summary

Cod is a native macOS terminal application purpose-built around Claude
Code. It replaces Ghostty as the daily driver: one window, `Cmd+N` spawns new
terminal panes inside it (never new OS windows), panes are freely
draggable/resizable/layerable and can be fullscreened individually or the
whole app can go true-fullscreen. A persistent top bar shows live Claude
token/usage info, a side utility panel surfaces usage detail, live subagent
trees, and Sonnet-generated workflow suggestions, and a bottom strip flashes
numbered hotkeys for pending tool-permission approvals — integrated with the
existing `claude-watch-approve` smartwatch approval system rather than
replacing it.

Theme: dark purple background, white text. Locked in as the only theme for
v1.

## Goals

- A real terminal (runs shell, vim, `claude` CLI, anything) with a freeform
  window-manager-like pane layout instead of OS-level windows or rigid tiling.
- Always-visible Claude usage info (tokens, cost, context %) blending global
  (all panes today) and focused-pane detail.
- Live visualization of Claude Code subagents as they run, not just nested
  transcript text.
- Fast keyboard approval of Claude Code permission prompts from inside
  Cod, racing against the existing watch-tap flow — first response wins.
- A Sonnet-powered advisor that (a) auto-approves genuinely low-risk tool
  calls so you're not interrupted for `ls`-tier actions, and (b) posts
  proactive workflow-optimization suggestions.
- Fail-safe: if any Cod-specific piece (broker, advisor) is down, the
  existing hook behavior (watch → terminal prompt) is unaffected. Cod is
  additive to `claude-watch-approve`, never a hard dependency of it.

## Non-goals (v1)

- Cross-platform support (macOS only, matches current Ghostty/watch-app setup).
- Automated UI test suite — manual verification per phase.
- Multiple themes / theming system.
- Auto-spawning a dedicated pane per subagent (subagents are visualized in a
  tree panel, not as their own panes).
- Rewriting or replacing the Swift companion app / watch firmware — Cod
  treats the existing bridge (127.0.0.1:8765) as a peer it races against, not
  something it owns.

## Architecture

**Stack**: Tauri (Rust backend + web-view frontend, TypeScript/React).
New project folder: `______2026NEW/cod`, committed as its own additive
project per the repo's no-delete/additive-only convention.

### Components

1. **PTY layer (Rust)** — spawns real PTY processes (login shell) per pane,
   streamed to the frontend over Tauri IPC.
2. **Canvas UI (frontend)** — xterm.js per pane, rendered on a freeform
   surface: drag, resize, layer (z-index/focus), per-pane fullscreen zoom,
   and true app-level fullscreen. `Cmd+N` adds a pane to the current window;
   it never opens a new OS window.
3. **Top usage bar** — single unified strip: global rollup (today's total
   tokens/cost across all panes) plus focused-pane detail (session tokens,
   context % used, model). Sourced by tailing `~/.claude/projects/*/*.jsonl`
   transcript files live (the same technique tools like `ccusage` use — no
   official usage API exists).
4. **Utility panel** (collapsible side dock) — tabbed:
   - **Usage** — detailed breakdown/history beyond the top bar's glance view.
   - **Approvals** — full pending/historical approval queue.
   - **Advisor** — Sonnet's running feed of workflow-optimization
     suggestions.
5. **Subagent tree panel** — contextual to the focused pane. Live tree
   (name, task, status: running/done, tokens) parsed from that pane's
   transcript tail. Click a node to jump to its output in the pane.
6. **Bottom approval strip** — thin, flashes on new pending requests, shows
   numbered hotkey slots (1, 2, 3, ...); pressing the number key approves
   that request from any pane.

### Approval flow (Approval Broker)

A new local service (WebSocket + HTTP) lives inside Cod's Rust backend:
the **Approval Broker**.

1. `watch_approve_hook.py` (PreToolUse) keeps its existing checks unchanged
   and in order: tool not in `route_tools` → allow; `auto_allow` prefix match
   → allow; session-scope cache → allow; always-scope cache → allow.
2. If still undecided, the hook calls the **Cod broker** (new step,
   replacing the direct-to-watch-bridge call).
3. Broker calls the **Sonnet advisor** to classify risk on the tool call
   payload. Genuinely low-risk, non-destructive calls → auto-allow, logged
   silently to the Advisor tab ("auto-approved: `ls -la`, low risk"). Anything
   uncertain, destructive, or matching existing sensitive-action heuristics
   (force-push, `rm -rf`, prod/shared-system actions, secrets, chained/
   redirected commands — same spirit as the current `auto_allow` prefix-only
   exclusion) → escalate, never auto-approved.
4. Escalated requests are broadcast to both display surfaces simultaneously:
   the existing watch bridge (127.0.0.1:8765, unchanged behavior) and
   Cod's bottom flash-bar + Approvals tab. First response wins (watch tap
   or Cod hotkey); the other surface's pending request is cancelled.
5. **Fail-safe**: if the Cod broker is unreachable, the hook falls
   straight through to the existing terminal `ask` prompt exactly as it does
   today when the watch bridge is down. Never blocks, never silently denies.

The advisor separately tails transcripts to post proactive suggestions
("you've re-run this command 5 times, want a script/alias?") to the Advisor
tab — independent of the approval path, purely observational. Because the
advisor has visibility into every pane's transcript at once (not just one),
its suggestions are cross-pane, not per-pane: it can notice structural
workflow patterns across the whole window — duplicate work happening in two
panes, one pane's edits colliding with another's, a pattern in how you
sequence tasks across sessions — not just repetition within a single
session.

### Refinement: direct PTY approval injection

In addition to the broker path above (which races against the watch bridge
and is needed when you're away from the screen), Cod's own panes give
the advisor a second, more direct channel for the common case where you're
looking at the screen: since Cod already owns raw PTY read/write access
per pane (the `PtyManager`'s `write`/output-stream from the core shell), the
advisor can watch a pane's live output for Claude Code's terminal permission
prompt text, classify risk the same way as the broker path, and — if
low-risk — directly write the approval keystroke (e.g. `1\n`) into that
pane's PTY via the existing `write_to_pane` command. No hook-file change or
broker round-trip needed for this channel; it only works for prompts
appearing in a pane Cod itself is running. Still auto-allow only
genuinely low-risk calls, same heuristics as the broker path, and still log
the action to the Advisor tab. Falls under the same fail-safe principle:
if the advisor is uncertain or the pattern match on the prompt text is
ambiguous, do nothing and let the normal terminal prompt sit — never guess
at an approval.

## Data flow summary

```
Claude Code (PreToolUse)
  -> watch_approve_hook.py (existing rule checks)
    -> [undecided] Cod Approval Broker
      -> Sonnet advisor risk check
        -> [low risk] auto-allow, log to Advisor tab
        -> [escalate] broadcast to watch bridge AND Cod bottom bar
          -> first response wins -> allow/deny back to hook
      -> [broker unreachable] fall through to terminal `ask` prompt
```

```
Transcript files (~/.claude/projects/*/*.jsonl)
  -> tailed live by Cod
    -> Top usage bar (global + focused-pane stats)
    -> Utility panel Usage tab (detail/history)
    -> Subagent tree panel (per focused pane)
    -> Sonnet advisor (workflow suggestion feed)
```

## Error handling

- Broker down → hook fail-safes to terminal prompt (existing behavior,
  unmodified).
- Watch bridge down → existing hook behavior unchanged; Cod bottom bar
  still works standalone.
- Transcript file unreadable/rotated mid-tail → usage bar/subagent
  panel show last-known values, don't crash the pane.
- PTY process exit → pane shows exited state, stays on canvas until
  manually closed (not auto-removed, avoids losing scrollback/position).

## Build phases

1. **Core shell** — Tauri skeleton, PTY spawn, xterm.js panes, freeform
   drag/resize/fullscreen canvas, `Cmd+N`, dark purple/white theme. Usable
   terminal replacement, no dashboard yet.
2. **Usage bar + Utility panel (Usage tab)** — transcript tailing, live
   token/cost/context% display.
3. **Subagent tree panel** — transcript-derived live tree per pane.
4. **Approval broker + bottom flash bar + Approvals tab** — extends
   `watch_approve_hook.py`, races watch vs. Cod, preserves fail-safe.
5. **Sonnet advisor** — risk classification for auto-approve, plus proactive
   suggestions in the Advisor tab.

Each phase is independently usable/testable before moving to the next.

## Testing

- Rust unit tests for PTY handling and broker decision logic (rule
  precedence, race resolution, fail-safe fallback).
- Manual verification per phase — run the app, drive the feature, confirm
  behavior. No automated UI test suite for v1 (GUI app, out of scope per
  Non-goals).

## Open questions / future ideas

- Persisting pane layouts across app restarts (save/restore canvas state).
- Advisor auto-approve rules becoming user-editable (beyond the built-in
  risk heuristics), similar to today's `auto_allow` config.
- Approval strip supporting "always" scope from Cod itself, not just via
  the watch.
- Exporting the Advisor tab's suggestion history somewhere durable (currently
  ephemeral to the running session).
- Drag-a-folder-onto-icon launch: dropping a folder on Cod's dock icon
  while the app is closed should launch it with a pane already `cd`'d into
  that folder (macOS "open with a folder" file-association handling).
- Recent-folders `cd` autocomplete: track recently-visited directories per
  pane/session and show them as suggestions when the user starts typing
  `cd` — needs a design decision on how Cod intercepts/augments shell
  input without breaking normal terminal passthrough (out of scope for the
  Phase 1 core shell; revisit once panes are real PTYs users are actually
  driving).
- Activity-driven pane sizing: panes react visually to their own session's
  state, detected from the same transcript-tailing pipe as the usage bar
  and subagent tree (Phase 2/3). Three signals: (1) the session is asking a
  genuine open-ended question (not a yes/no approval prompt) — after the
  user stops typing for a few seconds, the pane smoothly grows to take over
  the main view; (2) the session is actively working — the pane shrinks
  slightly to de-emphasize it; (3) the session just finished — the pane's
  border flashes for a few seconds, fades back to the normal focus color,
  and settles slightly larger than its working size to signal completion.
  All transitions animated (CSS transition/keyframes), never instant.
  Needs: a prompt-vs-approval classifier on terminal output (distinguish
  "real question" from a yes/no permission prompt — related to the
  Advisor's own risk classification work in Phase 5), an idle-typing timer,
  and reducer actions for programmatic (non-drag) resize with distinct
  "grow for attention" vs "shrink while working" semantics layered on top
  of the collision-push geometry from Phase 1b.
- Animated Cod presence: the Cod sprite (locked pixel-art design — see
  design artifact; species and mascot are both just "Cod," not a separate
  "Codling" name) animates itself moving fast between panes, e.g. darting
  to a pane right before it injects an approval keystroke or posts a
  suggestion about that pane's activity — gives Cod a visible "presence"
  rather than being purely a log feed in the Advisor tab. Leaves a subtle
  bubble trail behind it while moving (motion trail, distinct from the
  separate ambient background bubble accent already designed for the
  underwater theme). Cosmetic/UX layer on top of the master-agent
  functionality; design once Cod's actual event stream exists.
- UI/theme picker: v1 deliberately locks in one dark-purple/white theme
  (see Non-goals). A future version could let the user pick between a
  small set of curated layouts/palettes instead of hardcoding one — would
  need to replace the single `theme.css` with a small theme-switching
  mechanism. Deferred; revisit only if the single-theme v1 feels limiting
  in practice.
- Subagent tree as navigable panes: when a Claude Code session dispatches a
  subagent (Task tool), it should visually become its own pane in Cod —
  sometimes nested (a tree, when subagents dispatch further subagents),
  sometimes standalone. Confirmed via research: Claude Code is closed-source
  and not forkable (hooks/settings/MCP/Agent SDK are the only integration
  surface) — no fork needed or possible. Real architecture: subagent
  transcripts are real files at `~/.claude/projects/{project}/{sessionId}/
  subagents/agent-{agentId}.jsonl` (this is what Claude Code's own built-in
  subagent panel is built from). Cod watches that directory for new files
  (= subagent spawned → new pane node) and tails each file (= that pane's
  live output), with `SubagentStart`/`SubagentStop` hooks as fast push
  signals on top of the file watch. Tree reconstruction uses `agent_id`/
  `agent_type` from hook payloads; exact parent-link field inside the
  transcript JSON itself needs empirical confirmation (spawn a nested
  subagent, inspect the resulting file) — not documented by name. One real
  constraint: a subagent pane can show live output but isn't independently
  interactive the way a terminal pane is — you can't pipe raw keystrokes
  into a specific running subagent, only observe it (interacting with a
  subagent goes through the CLI's own subagent panel/SendMessage
  mechanism, not a raw PTY). This is the natural extension of the subagent
  tree panel already planned (Phase 3) — panes per node instead of a
  tree-list view. Sequenced with the other new-pane-type work (file
  manager, Coop editor) since it also needs a new pane type in
  PaneState/Canvas.tsx.
- Pluggable agent backends: every pane currently hardcodes `exec claude`
  (using the CLI's plan-based auth, not raw API credits — already the
  default behavior, nothing to change there). A future version could let
  a new pane choose its backend — Claude Code, a local model via some
  other CLI, another agent framework — instead of always launching
  `claude`. Needs a per-pane "backend" concept in `spawn_pane`/`PaneState`
  and a way to pick it when creating a pane. Deferred; the terminal-panel
  architecture (PTY spawns an arbitrary command) already supports this in
  principle, it just needs the UI/plumbing to choose.
