#!/bin/bash
# Auto-reload the RNBO instance in slot 0 when a new version is pushed from Max.
# The always-on rnbo-runner recompiles on push and bumps the patcher's
# created_at, but the loaded instance does not switch on its own. We poll
# created_at and, on change, unload+reload slot 0. The demiurge launcher then
# re-wires the new "<patcher>-0" JACK client into the chain automatically.
#
# Deployed as a --user systemd service (demiurge-rnbo-autoreload.service) so it
# needs no sudo / no write to /opt. Installed at ~/.local/bin/ on the Pi.
set -u
export XDG_RUNTIME_DIR=/run/user/$(id -u)
LIVE="$HOME/demiurge/live.conf"
HTTP=5678; OSC=1234; SLOT=0; POLL="${RNBO_RELOAD_POLL:-2}"

patcher() {  # first *.rnbo token in the (non-comment) chain
  grep -vE '^[[:space:]]*#' "$LIVE" 2>/dev/null \
    | grep -oE '[A-Za-z0-9_-]+\.rnbo' | head -1 | sed 's/\.rnbo$//'
}
marker() {
  curl -sf --max-time 2 "http://localhost:$HTTP/rnbo/patchers/$1/created_at" 2>/dev/null \
    | sed -n 's/.*"VALUE":"\([^"]*\)".*/\1/p'
}
# NOTE: MIDI routing (the shared "Midi Through" pool -> <patcher>-0:midiin1) is
# owned by the demiurge launcher (graph.rs midi_pool_capture + desired set), NOT
# this watcher. The launcher re-applies it on every (re)load via graph::apply, so
# auto-reload below is all this service needs to do for MIDI to keep working.

# wait for runner
for _ in $(seq 1 60); do curl -sf -o /dev/null "http://localhost:$HTTP/rnbo/info/version" && break; sleep 0.5; done

P="$(patcher)"
if [ -z "$P" ]; then echo "no .rnbo token in $LIVE; idling"; while true; do sleep 3600; done; fi
last="$(marker "$P")"
echo "autoreload: watching '$P' (created_at=$last), poll=${POLL}s"
while true; do
  sleep "$POLL"
  P="$(patcher)"; [ -z "$P" ] && continue
  cur="$(marker "$P")"
  if [ -n "$cur" ] && [ "$cur" != "$last" ]; then
    echo "autoreload: new compile of '$P' ($last -> $cur); reloading slot $SLOT"
    oscsend localhost $OSC /rnbo/inst/control/unload i $SLOT 2>/dev/null
    sleep 1
    oscsend localhost $OSC /rnbo/inst/control/load is $SLOT "$P"
    last="$cur"
  fi
done
