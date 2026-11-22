#!/bin/bash
# deploy-neptr-pool.sh — put NEPTR's non-encoder controls on the Demiurge parameter pool and
# install the USS instrument slot. Run ON the rig (twink / NEPTR) as the rig user (uses sudo).
# Runbook: docs/NEPTR_POOL_BENCH.md.   Undo: setup-script/rollback-neptr-pool.sh
#
#   bash setup-script/deploy-neptr-pool.sh                 DRY RUN of `stage` (default): prints, changes NOTHING
#   bash setup-script/deploy-neptr-pool.sh plan            read-only preflight report
#   APPLY=yes bash setup-script/deploy-neptr-pool.sh stage
#   APPLY=yes CONFIRM_LIVE_RESTART=yes bash setup-script/deploy-neptr-pool.sh io
#   APPLY=yes CONFIRM_LIVE_RESTART=yes bash setup-script/deploy-neptr-pool.sh engine
#
# PHASES (each independently shippable and reversible, in this order)
#   plan    read-only: what is running, which ports/serial owners, what would change.
#   stage   FILES ONLY. Backs up, then installs: demiurge-io (pool + arcade + pedal source),
#           resources/ hooks, the demiurge-io unit (daemon-reload only), the USS wrappers
#           (take effect on the NEXT stage spawn), `demiurge-uss`, USS instruments into
#           ~/demiurge/uss, the NEPTR default patch map(s), and the PEDAL_VIA_POOL guard in
#           NEPTR's midi_handler.orc (no-op unless the macro is defined). Restarts NOTHING.
#   io      restarts ONLY demiurge-io (control plane; csound/launcher/neptr-ui untouched). Stops
#           neptr-bridge first if it is active (one owner of the Teensy serial port).
#           Needs CONFIRM_LIVE_RESTART=yes. Result: arcade = tap tempo through the pool, pedal
#           delivered by the pool (engine funnel still on = same value twice, idempotent),
#           sensors/buttons are pool sources, USS slot usable once phase `engine` is done.
#   engine  installs the new demiurge-launcher (pool launch contract: the USS stage needs its
#           parameter hook + port) and sets --omacro:PEDAL_VIA_POOL=1 in live.conf csound_extra
#           (engine funnel OFF: ONE mechanism for the pedal), then restarts demiurge.service.
#           THIS RESTARTS CSOUND (audio gap of a few seconds). Needs CONFIRM_LIVE_RESTART=yes.
#           After it, `demiurge-uss <name>` swaps instruments with NO restart of anything.
#
# BACKUPS: ~/demiurge/backups/neptr-pool/<stamp>/ (pointer LATEST); phases run later in the same
# session append to it. Rollback closes it (ROLLED_BACK) so the next deploy starts clean.
# SAFETY: dry-run unless APPLY=yes. Any restart needs CONFIRM_LIVE_RESTART=yes on top. Every
# file it replaces is copied to $BACKUP_ROOT/<stamp>/ first and listed in MANIFEST; nothing is
# deleted that was not created here. Never touches audio devices, JACK/PipeWire, the Bela, or
# encoders/MIDI routing. `engine` refuses to run if the launcher build is missing.
set -euo pipefail

PHASE="${1:-stage}"
APPLY="${APPLY:-no}"
CONFIRM="${CONFIRM_LIVE_RESTART:-no}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="${REPO:-$(cd "$HERE/.." && pwd)}"
RIG_USER="${RIG_USER:-$(id -un)}"
DEM="${DEMIURGE_HOME:-$HOME/demiurge}"
NEPTR="${NEPTR_TREE:-$HOME/NEPTR_phase4}"
OPT="${OPT:-/opt/demiurge}"
BACKUP_ROOT="${BACKUP_ROOT:-$DEM/backups/neptr-pool}"
STAMP="$(date +%Y%m%d-%H%M%S)"
BK="$BACKUP_ROOT/$STAMP"
LAUNCHER_SRC="${LAUNCHER_SRC:-$REPO/src/demiurge-launcher-rs/target/release/demiurge-launcher}"
SUDO=""; [ "$(id -u)" -eq 0 ] || SUDO="sudo"
# SANDBOX=1 (tests only): no sudo, no systemctl mutations; point OPT/DEMIURGE_HOME/NEPTR_TREE/
# SYSTEMD_DIR/LOCAL_BIN/BACKUP_ROOT at a scratch dir to rehearse `stage` and the rollback safely.
SANDBOX="${SANDBOX:-0}"; [ "$SANDBOX" = 1 ] && SUDO=""
SYSTEMD_DIR="${SYSTEMD_DIR:-/etc/systemd/system}"
LOCAL_BIN="${LOCAL_BIN:-/usr/local/bin}"
IO_ARGS="--osc-in --midi --midi-port Through --midi-neptr"

say()  { printf '%s\n' "$*"; }
die()  { printf 'REFUSING: %s\n' "$*" >&2; exit 1; }
act()  { say "+ $*"; if [ "$APPLY" = yes ]; then "$@"; fi; }
sact() { say "+ $SUDO $*"; if [ "$APPLY" = yes ]; then $SUDO "$@"; fi; }
sysd() { # systemctl mutations: skipped in a sandbox
  say "+ $SUDO systemctl $*"
  if [ "$APPLY" = yes ] && [ "$SANDBOX" != 1 ]; then $SUDO systemctl "$@"; fi
}

need_confirm() {
  [ "$APPLY" = yes ] || return 0
  [ "$CONFIRM" = yes ] || die "phase '$PHASE' restarts live services. Re-run with CONFIRM_LIVE_RESTART=yes (and not mid-performance)."
}

# ---- backup (first thing any mutating phase does) ---------------------------------------------
BK_READY=0
# One backup per deploy SESSION: phases run in separate invocations (stage, then io, then engine)
# and all append to the same LATEST backup until a rollback closes it. The FIRST copy of a path
# is the true original, so a later phase never overwrites it.
bk_init() {            # phase-name
  [ "$BK_READY" = 1 ] && { [ "$APPLY" = yes ] && echo "PHASE $1" >> "$BK/MANIFEST"; return 0; }
  BK_READY=1
  [ "$APPLY" = yes ] || { say "+ (would back up into $BACKUP_ROOT/LATEST, or a new $BK)"; return 0; }
  mkdir -p "$BACKUP_ROOT"
  if [ -L "$BACKUP_ROOT/LATEST" ] && [ -f "$BACKUP_ROOT/LATEST/MANIFEST" ] && [ ! -e "$BACKUP_ROOT/LATEST/ROLLED_BACK" ]; then
    BK="$(cd "$BACKUP_ROOT/LATEST" && pwd -P)"
    say "backup dir (continuing this session): $BK"
  else
    mkdir -p "$BK/files"
    : > "$BK/MANIFEST"
    {
      echo "STAMP $STAMP"
      echo "STATE io_active=$(systemctl is-active demiurge-io 2>/dev/null || true)"
      echo "STATE bridge_active=$(systemctl is-active neptr-bridge 2>/dev/null || true)"
      echo "STATE engine_active=$(systemctl is-active demiurge 2>/dev/null || true)"
    } >> "$BK/MANIFEST"
    ln -sfn "$BK" "$BACKUP_ROOT/LATEST"
    say "backup dir: $BK   (pointer: $BACKUP_ROOT/LATEST)"
  fi
  echo "PHASE $1" >> "$BK/MANIFEST"
}
bk_item() {            # path  -> copy aside (or record as NEW so rollback can remove it)
  local p="$1"
  if [ "$APPLY" = yes ] && { grep -qxF "BACKUP $p" "$BK/MANIFEST" || grep -qxF "NEW $p" "$BK/MANIFEST"; }; then
    say "= already protected by this session's backup: $p"; return 0
  fi
  if [ -e "$p" ] || [ -L "$p" ]; then
    say "+ back up $p"
    [ "$APPLY" = yes ] || return 0
    mkdir -p "$BK/files$(dirname "$p")"
    $SUDO cp -a "$p" "$BK/files$p"
    echo "BACKUP $p" >> "$BK/MANIFEST"
  else
    say "+ (new, nothing to back up) $p"
    [ "$APPLY" = yes ] && echo "NEW $p" >> "$BK/MANIFEST"
  fi
  return 0
}

# ---- preflight (read-only, always) --------------------------------------------------------------
preflight() {
  say "== preflight (read-only) =="
  [ -d "$REPO/src/demiurge-io/demiurge_io" ] || die "repo not found at $REPO"
  say "repo:        $REPO  ($(git -C "$REPO" rev-parse --short HEAD 2>/dev/null || echo 'no git'))"
  say "rig user:    $RIG_USER   demiurge home: $DEM   neptr tree: $NEPTR"
  say "services:    demiurge=$(systemctl is-active demiurge 2>/dev/null || true)  demiurge-io=$(systemctl is-active demiurge-io 2>/dev/null || true)  neptr-bridge=$(systemctl is-active neptr-bridge 2>/dev/null || true)  neptr-ui=$(systemctl is-active neptr-ui 2>/dev/null || true)"
  say "UDP 9100:    $(ss -ulnp 2>/dev/null | awk '$4 ~ /127.0.0.1:9100$/ {print $NF}' | head -1) (must be demiurge-clock: it owns /transport/tap)"
  if ss -ulnp 2>/dev/null | awk '{print $4}' | grep -q '127.0.0.1:9102$'; then
    say "UDP 9102:    IN USE (expected only if the new demiurge-io is already running)"
  else
    say "UDP 9102:    free (pool ingress will bind here)"
  fi
  local ownr=""; ownr="$(ls /dev/serial/by-id 2>/dev/null | grep -i teensy | head -1 || true)"
  say "body serial: ${ownr:-NOT attached (pedal + USS still work; the arcade/sensors need the body)}"
  grep -q 'DEMIURGE_PARAM_PORT' "$OPT/bin/demiurge-launcher" 2>/dev/null \
    && say "launcher:    already has the pool launch contract" \
    || say "launcher:    OLD (no pool launch contract) -> phase 'engine' installs the new one"
  grep -Eq '^[[:space:]]*sync_layer[[:space:]]*=[[:space:]]*(off|false|no|0)' "$DEM/live.conf" 2>/dev/null \
    && say "WARNING:     live.conf has sync_layer = off: the USS slot cannot work in bypass" || true
  say "active patch: $(sed -n 's/^# set:[[:space:]]*//p' "$DEM/live.conf" 2>/dev/null | head -1)"
  say "csound_extra: $(sed -n 's/^csound_extra[[:space:]]*=[[:space:]]*//p' "$DEM/live.conf" 2>/dev/null | head -1)"
}

# ---- stage ------------------------------------------------------------------------------------
# sha256 of midi_handler.orc BEFORE the PEDAL_VIA_POOL guard (= the live file on 2026-10-07).
orc_sha_pre="d42e2e52496582aadd179496a33fdaa85f29edaa4ab4b73fe2ee6b58434133cb"

phase_stage() {
  preflight
  say; say "== stage: files only, restarts NOTHING =="
  # sanity gates: syntax before anything is installed
  bash -n "$REPO/src/wrappers/demiurge-run-csound" "$REPO/src/wrappers/demiurge-run-chuck" || die "wrapper syntax"
  python3 -I -c "import ast,sys; ast.parse(open(sys.argv[1]).read())" "$REPO/src/wrappers/demiurge-uss" || die "demiurge-uss syntax"
  python3 -I -m compileall -q "$REPO/src/demiurge-io/demiurge_io" >/dev/null || die "demiurge-io syntax"
  find "$REPO/src/demiurge-io" -name __pycache__ -prune -exec rm -rf {} + 2>/dev/null || true
  bk_init stage

  # 1. demiurge-io package (+ tests)
  bk_item "$OPT/src/demiurge-io"
  sact install -d "$OPT/src"
  sact rsync -a --delete --exclude __pycache__ "$REPO/src/demiurge-io/" "$OPT/src/demiurge-io/"

  # 2. resources/: pool hooks per language 
  bk_item "$OPT/resources"
  sact rsync -a --delete --exclude __pycache__ "$REPO/resources/" "$OPT/resources/"

  # 3. unit: rendered for this user. daemon-reload reads it; it restarts nothing.
  bk_item "$SYSTEMD_DIR/demiurge-io.service"
  if [ "$APPLY" = yes ]; then
    $SUDO install -d "$SYSTEMD_DIR"
    sed "s/@RIG_USER@/$RIG_USER/" "$REPO/src/demiurge-io/demiurge-io.service" | $SUDO tee "$SYSTEMD_DIR/demiurge-io.service" >/dev/null
    sysd daemon-reload
  else
    say "+ render demiurge-io.service (User=$RIG_USER, ExecStart: -m demiurge_io $IO_ARGS) -> $SYSTEMD_DIR/, then daemon-reload"
  fi

  # 4. wrappers: USS branches (uss-* only) in run-chuck/run-csound, and the slot tool
  sact install -d "$OPT/bin"
  for w in demiurge-run-chuck demiurge-run-csound demiurge-uss; do
    bk_item "$OPT/bin/$w"
    sact install -m0755 "$REPO/src/wrappers/$w" "$OPT/bin/$w"
  done
  bk_item "$LOCAL_BIN/demiurge-uss"
  sact install -d "$LOCAL_BIN"
  sact install -m0755 "$REPO/src/wrappers/demiurge-uss" "$LOCAL_BIN/demiurge-uss"

  # 5. USS instruments -> ~/demiurge/uss/instruments (copy; the slot tool symlinks from here)
  bk_item "$DEM/uss"
  act install -d "$DEM/uss"
  act rsync -a --exclude __pycache__ "$REPO/uss/instruments" "$DEM/uss/"

  # 6. NEPTR default patch map for the active patch + every NEPTR patch conf (never overwrite)
  local patches=() p
  p="$(sed -n 's/^# set:[[:space:]]*//p' "$DEM/live.conf" 2>/dev/null | head -1)"; [ -n "$p" ] && patches+=("$p")
  while IFS= read -r f; do patches+=("$(basename "$f" .conf)"); done < <(grep -lE 'neptrPhase4\.csd' "$DEM"/patches/*.conf "$DEM"/patches/*/*.conf "$DEM"/sets/*.conf 2>/dev/null || true)
  act install -d "$DEM/patches"
  for p in $(printf '%s\n' "${patches[@]}" | sort -u); do
    local dst="$DEM/patches/$p.map.json"
    [ -d "$DEM/patches/$p" ] && dst="$DEM/patches/$p/$p.map.json"
    if [ -e "$dst" ]; then say "= keep existing $dst (never overwritten)"; continue; fi
    bk_item "$dst"
    act install -m0644 "$REPO/neptr/pool/neptr_default.map.json" "$dst"
  done

  # 7. NEPTR midi_handler.orc: the PEDAL_VIA_POOL guard. Only if the live file is exactly the
  #    version the guard was written against (anything else = somebody edited it: stop).
  local live_orc="$NEPTR/csound/midi_handler.orc" new_orc="$REPO/neptr/csound/midi_handler.orc"
  if [ -f "$live_orc" ]; then
    if grep -q 'PEDAL_VIA_POOL' "$live_orc"; then say "= midi_handler.orc already has the guard"
    elif [ "$(sha256sum "$live_orc" | cut -d' ' -f1)" = "$orc_sha_pre" ]; then
      bk_item "$live_orc"; act install -m0644 "$new_orc" "$live_orc"
      say "  (csound reads the file at start: the running engine is unaffected until phase 'engine')"
    else
      say "!! $live_orc differs from the repo version the guard was written against: NOT touched."
      say "   Apply the 6-line guard by hand (see neptr/csound/midi_handler.orc, search PEDAL_VIA_POOL) before phase 'engine'."
    fi
  else say "!! $live_orc not found: skip (phase 'engine' will refuse without it)"; fi

  say; say "stage done$([ "$APPLY" = yes ] || echo ' (DRY RUN: nothing changed; re-run with APPLY=yes)')."
  [ "$APPLY" = yes ] && say "rollback: bash $REPO/setup-script/rollback-neptr-pool.sh   (backup $BK)"
  return 0
}

# ---- io ---------------------------------------------------------------------------------------
phase_io() {
  preflight; say; say "== io: restart demiurge-io only =="
  need_confirm
  [ -f "$OPT/src/demiurge-io/demiurge_io/arcade.py" ] || die "run phase 'stage' first (arcade.py missing in $OPT)"
  bk_init io
  if [ "$(systemctl is-active neptr-bridge 2>/dev/null || true)" = active ]; then
    say "neptr-bridge owns the Teensy serial port (and sends the CC113 tap): stopping it. demiurge-io replaces both."
    sysd stop neptr-bridge
  fi
  sysd restart demiurge-io
  if [ "$APPLY" = yes ] && [ "$SANDBOX" != 1 ]; then
    sleep 3
    systemctl is-active --quiet demiurge-io || { journalctl -u demiurge-io -n 30 --no-pager; die "demiurge-io did not stay up. Run rollback-neptr-pool.sh io"; }
    journalctl -u demiurge-io -n 15 --no-pager | sed 's/^/   | /'
    ss -ulnp | awk '{print $4}' | grep -q '127.0.0.1:9102$' && say "OK: pool ingress bound on 9102" || say "!! 9102 not bound (check the journal)"
  fi
  say "io done. Bench checks: docs/NEPTR_POOL_BENCH.md section 3."
}

# ---- engine -----------------------------------------------------------------------------------
phase_engine() {
  preflight; say; say "== engine: new launcher + PEDAL_VIA_POOL, RESTARTS demiurge.service (csound) =="
  need_confirm
  [ -x "$LAUNCHER_SRC" ] || die "no launcher build at $LAUNCHER_SRC. Build it OFF-PEAK first:
   cd $REPO/src/demiurge-launcher-rs && PATH=\$HOME/.cargo/bin:\$PATH taskset -c 0-2 nice -n 19 ionice -c3 cargo build --release"
  grep -q 'DEMIURGE_PARAM_PORT' "$LAUNCHER_SRC" || die "$LAUNCHER_SRC lacks the pool launch contract (stale build)"
  grep -q 'PEDAL_VIA_POOL' "$NEPTR/csound/midi_handler.orc" 2>/dev/null || die "$NEPTR/csound/midi_handler.orc has no PEDAL_VIA_POOL guard (phase 'stage' step 7)"
  [ -f "$OPT/src/demiurge-io/demiurge_io/arcade.py" ] || die "run phase 'stage' + 'io' first"
  [ "$SANDBOX" = 1 ] || [ "$(systemctl is-active demiurge-io 2>/dev/null || true)" = active ] || die "demiurge-io must be running (phase 'io') BEFORE the engine funnel is switched off, or the pedal goes dead"
  bk_init engine
  bk_item "$OPT/bin/demiurge-launcher"
  bk_item "$DEM/live.conf"
  sact install -m0755 "$LAUNCHER_SRC" "$OPT/bin/demiurge-launcher"
  # live.conf: add the macro to the (single) csound_extra line, keep everything else byte-for-byte
  if grep -q 'PEDAL_VIA_POOL' "$DEM/live.conf"; then say "= live.conf already carries PEDAL_VIA_POOL"
  elif grep -q '^csound_extra[[:space:]]*=' "$DEM/live.conf"; then
    act sed -i '/^csound_extra[[:space:]]*=/ s/[[:space:]]*$/ --omacro:PEDAL_VIA_POOL=1/' "$DEM/live.conf"
  else
    act sed -i '0,/^chain[[:space:]]*=/ s//csound_extra = --omacro:PEDAL_VIA_POOL=1\n\nchain =/' "$DEM/live.conf"
  fi
  act sync
  sysd restart demiurge.service
  if [ "$APPLY" = yes ] && [ "$SANDBOX" != 1 ]; then
    for i in $(seq 1 40); do pgrep -x csound >/dev/null && break; sleep 1; done
    pgrep -x csound >/dev/null || { journalctl -u demiurge -n 40 --no-pager; die "csound did not come back. ROLL BACK NOW: CONFIRM_LIVE_RESTART=yes APPLY=yes bash $REPO/setup-script/rollback-neptr-pool.sh engine"; }
    sleep 5
    say "csound up (pid $(pgrep -x csound | head -1)). links:"; pw-link -l 2>/dev/null | grep -A1 'csound6:output' | sed 's/^/   | /'
    grep -c PEDAL_VIA_POOL "$DEM/live.conf" | sed 's/^/live.conf PEDAL_VIA_POOL lines: /'
  fi
  say "engine done. NEXT: docs/NEPTR_POOL_BENCH.md sections 4-5 (pedal check, then USS slot)."
}

case "$PHASE" in
  plan)   preflight ;;
  stage)  phase_stage ;;
  io)     phase_io ;;
  engine) phase_engine ;;
  all)    phase_stage; phase_io; phase_engine ;;
  *)      die "unknown phase '$PHASE' (plan | stage | io | engine | all)" ;;
esac
[ "$APPLY" = yes ] || say "
(DRY RUN — nothing was changed. APPLY=yes to apply$( [ "$PHASE" = io -o "$PHASE" = engine -o "$PHASE" = all ] && echo ', plus CONFIRM_LIVE_RESTART=yes for restarts' ).)"
