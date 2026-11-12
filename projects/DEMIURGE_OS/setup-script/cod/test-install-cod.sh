#!/usr/bin/env bash
# Offline test for install-cod.sh: temp HOME, stubbed systemctl/loginctl/tailscale/sudo.
# Usage: COD_TEST_REMOTE=<path-or-url of a cod checkout/remote> bash test-install-cod.sh
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REMOTE="${COD_TEST_REMOTE:-https://github.com/EMG0R/cod.git}"
case "$REMOTE" in /*) REMOTE="file://$REMOTE";; esac
T="$(mktemp -d)"; trap 'rm -rf "$T"' EXIT
export HOME="$T/home"; mkdir -p "$HOME" "$T/bin"
export COD_ROOT="$T/codroot" COD_REMOTE="$REMOTE" SYSTEMCTL="$T/bin/systemctl" LOGINCTL="$T/bin/loginctl"
export PATH="$T/bin:$PATH"; CALLS="$T/calls"; : > "$CALLS"; export CALLS T
cat > "$T/bin/systemctl" <<'S'
#!/bin/bash
echo "systemctl $*" >> "$CALLS"
case "$*" in *is-enabled*) u="${@: -1}"; [ -e "$T/enabled.$u" ];; *" enable "*) echo "$*" | tr ' ' '\n' | grep '\.\(service\|timer\)$' | while read u; do touch "$T/enabled.$u"; done;; esac
S
cat > "$T/bin/loginctl" <<'S'
#!/bin/bash
echo "Linger=yes"
S
for n in tailscale sudo curl; do printf '#!/bin/bash\necho "%s $*" >> "$CALLS"\nexit 1\n' $n > "$T/bin/$n"; done
chmod +x "$T"/bin/*
ok() { echo "ok   $*"; }; bad() { echo "FAIL $*"; exit 1; }
snap() { (cd "$T" && find home codroot -not -path '*/versions/*/*' | sort; md5sum $(find home -type f | sort) 2>/dev/null) | md5sum; }

bash "$HERE/install-cod.sh" >"$T/run1.log" 2>&1 || { cat "$T/run1.log"; bad "run 1 exited nonzero"; }
[ -L "$COD_ROOT/current" ] && [ -f "$COD_ROOT/current/aquarium/cod" ] && ok "cod checkout present ($(basename "$(readlink "$COD_ROOT/current")"))" || { cat "$T/run1.log"; bad "no checkout"; }
[ -L "$HOME/cod" ] && ok "~/cod -> checkout"
for f in .local/bin/claude-persist .local/bin/cod .config/systemd/user/cod-tmux.service .config/systemd/user/cod-agents.service \
         .config/systemd/user/cod-hub.service .config/systemd/user/cod-bus.service .config/systemd/user/cod-bus.service.d/10-dormant-guard.conf \
         .config/systemd/user/cod-update.timer; do [ -f "$HOME/$f" ] || bad "missing $f"; done; ok "units + client + claude-persist written"
grep -q ExecStop "$HOME/.config/systemd/user/cod-agents.service" "$HOME/.config/systemd/user/cod-tmux.service" 2>/dev/null \
  && grep -v '^#' "$HOME/.config/systemd/user/cod-agents.service" | grep -q '^ExecStop' && bad "ExecStop in agent unit"; ok "no ExecStop in agent units"
grep -E 'enable' "$CALLS" | grep -q 'cod-bus' && bad "cod-bus enabled"; ok "cod-bus not enabled"
grep -E 'start|restart|--now' "$CALLS" && bad "something started" ; ok "no unit started/restarted"
grep -E 'enable' "$CALLS" | grep -E 'cod-(agents|tmux|hub)' && bad "agent/hub enabled with no registered agents"; ok "agents/hub dormant (no registered agents)"
grep -q 'enable cod-update.timer' "$CALLS" && ok "cod-update.timer enabled"
[ ! -e "$HOME/.cod-bus.conf" ] && ok "no ~/.cod-bus.conf created"
grep -E '^(tailscale|sudo|curl)' "$CALLS" && bad "tailscale/sudo/curl called"; ok "no tailscale up / install"

S1="$(snap)"; : > "$CALLS"
bash "$HERE/install-cod.sh" >"$T/run2.log" 2>&1 || { cat "$T/run2.log"; bad "run 2 failed"; }
[ "$S1" = "$(snap)" ] && ok "re-run: tree unchanged" || bad "re-run changed files"
grep -v 'is-enabled' "$CALLS" | grep -q . && { cat "$CALLS"; bad "re-run made state-changing systemctl calls"; } || ok "re-run: no state-changing systemctl calls (only is-enabled queries)"

printf 'COD_BUS_NODE=x\nCOD_BUS_URL=http://h:1\nCOD_BUS_TOKEN=SECRET\n' > "$HOME/.cod-bus.conf"; chmod 600 "$HOME/.cod-bus.conf"
mkdir -p "$HOME/.claude-persist"; echo abc > "$HOME/.claude-persist/agentA.sid"; echo "keep" > "$HOME/.claude-persist/agentA.cwd"
H1="$(md5sum "$HOME/.cod-bus.conf" "$HOME/.claude-persist/"* | md5sum)"; : > "$CALLS"
bash "$HERE/install-cod.sh" >"$T/run3.log" 2>&1 || { cat "$T/run3.log"; bad "run 3 failed"; }
[ "$H1" = "$(md5sum "$HOME/.cod-bus.conf" "$HOME/.claude-persist/"* | md5sum)" ] && ok "existing ~/.cod-bus.conf + agent registry untouched" || bad "clobbered"
grep -q 'enable cod-bus.service' "$CALLS" && grep -q 'enable cod-agents.service' "$CALLS" && ok "existing member: cod-bus + agents enabled (not started)" || bad "member not preserved"
grep -E 'start|restart|--now' "$CALLS" && bad "started on member re-run"
echo "ALL PASS"
