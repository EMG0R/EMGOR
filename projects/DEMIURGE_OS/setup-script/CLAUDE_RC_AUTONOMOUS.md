# Autonomous Claude Code (Remote Control) on a Pi — works every boot
> **2026-10-06: agent persistence now comes from the COD repo** (`cod-tmux.service` + `cod-agents.service` + `claude-persist`, installed by `setup-script/cod/install-cod.sh`). `claude-rc.service` below is the legacy mechanism, no longer shipped; pieces 1, 2, 5, 6 still apply.

Goal: every Pi (TWINK NEPTR, gorcave, Neural Grid, real NEPTR) brings up its own
Claude Code agent **autonomously and reliably** — reachable from the Claude Code
app / COD tank, with **zero interactive prompts** gating startup. Verified on
TWINK NEPTR (`gorpi7v2@demiurge.local`) 2026-10-05.

Bucket: **sync** (mechanism ships to every Pi). The *login credentials* are
**local/personal** — never ship `~/.claude/.credentials.json` or `~/.claude.json`.

## The pieces (all must be true)

1. **Claude installed** as an npm global under a per-user prefix:
   `~/.npm-global/bin` (so `npm update` needs no root). Put that + `~/.local/bin`
   on PATH.

2. **Logged in once, interactively** (cannot be automated, by design):
   `claude` → `/login`, finish the code/browser flow. This writes
   `~/.claude/.credentials.json` (has `oauthAccount`). Persists across reboots.

3. **`claude-persist`** (`~/.local/bin/claude-persist`): runs `claude --rc`
   inside a detached tmux session `claude`, in a `while true` loop so it survives
   SSH drops and restarts if it ever exits. `--rc` = interactive Remote Control
   (retries the link forever; the `claude remote-control` server mode gives up
   after ~10 min — do NOT use that one).

4. **`claude-rc.service`** (user unit, `~/.config/systemd/user/`): `Type=forking`,
   `ExecStart=%h/.local/bin/claude-persist start`, `WantedBy=default.target`,
   `enable`d.

5. **Linger ON** so the user service starts at boot without a login:
   `sudo loginctl enable-linger <user>` → `loginctl show-user <user>` shows `Linger=yes`.

6. **THE GATE THAT BITES — pre-accept all first-run prompts in `~/.claude.json`.**
   Without this, boot hangs forever at "Do you trust this folder?" and the agent
   never comes up. Set, for the cwd the agent runs in (claude-persist uses `$HOME`):
   ```python
   d["projects"]["<cwd>"]["hasTrustDialogAccepted"] = True
   d["projects"]["<cwd>"]["hasCompletedProjectOnboarding"] = True
   d["fullscreenUpsellSeenCount"] = 99   # suppress the "try fullscreen renderer?" upsell
   ```
   Also expect these already-true after one manual run:
   `hasCompletedOnboarding`, `hasSeenAutoDefaultNotice`.
   **Back up `~/.claude.json` before editing** (atomic replace; don't clobber a
   running claude's writes — do it when you can, it's picked up next launch/boot).

## Verify
```
loginctl show-user <user> | grep Linger          # Linger=yes
systemctl --user is-enabled claude-rc.service     # enabled
claude-persist status                             # running
```
Then reboot-test once if you can afford it: the tmux `claude` session must reach
the prompt (`/rc active`, auto mode) with NO trust/renderer prompt in the pane.

## Gotchas learned the hard way
- Clicking "Yes, I trust this folder" in the TUI does **not** reliably leave
  `hasTrustDialogAccepted: true` on disk for the next boot — set it in the file.
- `fullscreenUpsellSeenCount` is a soft nudge but it still gates the TUI until
  answered; bump it high so autonomous boot never sees it.
- `claude` lives under `~/.npm-global/bin`, which a non-interactive shell does
  not source — scripts must put it on PATH explicitly (see `demiurge-agent`).
