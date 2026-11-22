#!/usr/bin/env bash
# Offline test: swap -> post-swap -> apply. Temp ROOT, stubbed systemctl/runuser; never writes /opt or /etc.
set -uo pipefail
SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"   # setup-script/..
T="$(mktemp -d)"; trap 'rm -rf "$T"' EXIT
export DEMIURGE_ROOT="$T/root" DEMIURGE_BIN_DIR="$T/bin" DEMIURGE_UNIT_DIR="$T/units" DEMIURGE_LINGER_DIR="$T/linger"
R="$DEMIURGE_ROOT"; mkdir -p "$R/versions" "$R/local" "$T/bin" "$T/units" "$T/stub" "$T/linger"
FAIL=0; ok() { if eval "$2"; then echo "ok   $1"; else echo "FAIL $1"; FAIL=1; fi; }
CALLS="$T/calls.log"; : > "$CALLS"
cat > "$T/stub/systemctl" <<S
#!/bin/sh
echo "systemctl \$*" >> "$CALLS"; [ "\$1" = is-enabled ] && exit 1; exit 0
S
cat > "$T/stub/runuser" <<S
#!/bin/sh
echo "runuser \$*" >> "$CALLS"; shift 3; [ "\$1" = -- ] && shift; exec "\$@"
S
cat > "$T/stub/nice" <<'S'
#!/bin/sh
shift 2; exec "$@"
S
chmod +x "$T/stub/"*; export PATH="$T/stub:$PATH" SYSTEMCTL="$T/stub/systemctl" RUNUSER="$T/stub/runuser"
export DEMIURGE_USER="$(id -un)"
mk() { # mk <hash> <post-swap-mode: real|fail> : fake version tree built from the real scripts
  local v="$R/versions/$1"; mkdir -p "$v/setup-script/updater"
  cp "$SRC/setup-script/updater/"* "$v/setup-script/updater/"
  printf '#!/bin/bash\necho applied-$HOME-%s >> "%s/apply.log"\n' "$1" "$T" > "$v/setup-script/apply-user.sh"
  [ "${2:-real}" = fail ] && printf '#!/bin/bash\nexit 7\n' > "$v/setup-script/updater/post-swap.sh"
}
mk v1; ln -s versions/v1 "$R/current"
SWAP="$SRC/setup-script/updater/demiurge-update-swap.sh"
# 1. swap to v2: post-swap runs, installs bin/units, writes marker
mk v2; ln -s versions/v2 "$R/next"; bash "$SWAP" >"$T/o1" 2>&1
ok "current -> v2" '[ "$(readlink "$R/current")" = versions/v2 ]'
ok "post-swap ran" 'grep -q "post-swap ok" "$T/o1"'
ok "scripts installed in BIN_DIR" '[ -x "$T/bin/demiurge-update-swap" ] && [ -x "$T/bin/demiurge-apply" ]'
ok "units installed in UNIT_DIR" '[ -f "$T/units/demiurge-apply.service" ] && [ -f "$T/units/demiurge-update-stage.timer" ]'
ok "apply-pending=v2" '[ "$(cat "$R/local/apply-pending")" = v2 ]'
ok "daemon-reload + enable called" 'grep -q "daemon-reload" "$CALLS" && grep -q "enable demiurge-apply" "$CALLS"'
ok "no audio unit touched" '! grep -Eq "restart|demiurge.service|csound|neptr" "$CALLS"'
# 2. self-update copies only changed files
touch -d '2001-01-01' "$T/bin/demiurge-update-stage"; : > "$CALLS"
echo "# changed" >> "$R/versions/v2/setup-script/updater/demiurge-update-revert.sh"
DEMIURGE_ROOT="$R" bash "$R/versions/v2/setup-script/updater/post-swap.sh" >"$T/o2" 2>&1
ok "only revert updated" '[ "$(grep -c "^\[post-swap\] updated" "$T/o2")" = 1 ] && grep -q "revert" "$T/o2"'
ok "unchanged file untouched" '[ "$(date -r "$T/bin/demiurge-update-stage" +%Y)" = 2001 ]'
# 3. apply runs apply-user once as user, clears marker
bash "$T/bin/demiurge-apply" >"$T/o3" 2>&1
ok "apply-user ran once" '[ "$(wc -l < "$T/apply.log")" = 1 ]'
ok "run via runuser as user w/ XDG_RUNTIME_DIR" 'grep -q "runuser -u $(id -un) -- env XDG_RUNTIME_DIR=/run/user/$(id -u)" "$CALLS"'
ok "marker cleared" '[ ! -e "$R/local/apply-pending" ]'
# 4. second boot / rerun: no-op
bash "$T/bin/demiurge-apply" >"$T/o4" 2>&1; bash "$SWAP" >"$T/o5" 2>&1
ok "second apply no-op" '[ "$(wc -l < "$T/apply.log")" = 1 ] && grep -q "nothing pending" "$T/o4"'
ok "second swap no-op" '[ "$(readlink "$R/current")" = versions/v2 ] && [ ! -e "$R/local/apply-pending" ]'
# 5. failing post-swap does not revert the swap
mk v3 fail; ln -s versions/v3 "$R/next"; bash "$SWAP" >"$T/o6" 2>&1; rc=$?
ok "swap kept when post-swap fails" '[ "$rc" = 0 ] && [ "$(readlink "$R/current")" = versions/v3 ] && grep -q "post-swap FAILED" "$T/o6"'
# 6. failed apply keeps marker
echo v3 > "$R/local/apply-pending"; printf '#!/bin/bash\nexit 1\n' > "$R/versions/v3/setup-script/apply-user.sh"
bash "$T/bin/demiurge-apply" >"$T/o7" 2>&1
ok "failed apply keeps marker" '[ -f "$R/local/apply-pending" ]'
[ "$FAIL" = 0 ] && echo "ALL PASS" || { echo "FAILURES"; exit 1; }
