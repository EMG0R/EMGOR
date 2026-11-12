#!/bin/bash
# Installed on the Bela as /opt/Bela/local/bela_startup.sh - the stock
# /opt/Bela/bela_startup.sh execs this INSTEAD of its own runner when it
# exists (that is the designed override hook, first lines of the stock
# script). Runs the hyperGuitar supervisor at boot: relaunch loop for
# the codec watchdog, -p 64, and the usbaudio-bridge bounce - the stock
# runner is single-shot at whatever /opt/Bela/startup_env ARGS says
# (-p 128) and respawns nothing.
while true; do [ "$(cut -d. -f1 /proc/uptime)" -gt 15 ] && break; sleep 1; done
exec /root/hyperGuitar-supervisor.sh
