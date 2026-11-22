#!/bin/bash
# hyperTrumpet relaunch loop (ported from hyperGuitar-supervisor.sh).
# Why it exists: the instrument exits on a codec fault/watchdog and must be
# relaunched to re-init the codec; Bela's own runner is single-shot.
# Changes vs hyperGuitar: project name/paths, block size from
# /etc/default/hypertrumpet, waits for the binary instead of spinning, backoff
# when the process dies fast (no cape / bad hardware), log size cap.
[ -f /etc/default/hypertrumpet ] && . /etc/default/hypertrumpet
PROJECT="${HT_PROJECT:-hyperTrumpet}"
BLOCK="${HT_BLOCK:-64}"
DIR="/root/Bela/projects/$PROJECT"
LOG="/root/$PROJECT-live.log"
cd "$DIR" 2>/dev/null || { echo "supervisor: $DIR missing" >&2; sleep 30; exit 1; }
delay=1
while true; do
	if [ ! -x "./$PROJECT" ]; then       # not built yet: idle quietly
		sleep 10; continue
	fi
	# cap the log (rt_printf can be chatty; the SD card is small)
	[ "$(stat -c %s "$LOG" 2>/dev/null || echo 0)" -gt 20000000 ] && mv -f "$LOG" "$LOG.1"
	# the instrument recreates its USB ring (shm) on every start and the bridge
	# maps it only once - bounce the bridge so it re-opens the fresh segment
	# (no-op if the unit is disabled)
	(sleep 2; systemctl try-restart usbaudio-bridge 2>/dev/null) &
	t0=$(date +%s)
	# stdbuf -oL: stdout is a FILE, without it rt_printf sits in a 4 KB buffer
	ARGS=(-p "$BLOCK"); [ -f settings.json ] && ARGS=(-Jsettings.json "${ARGS[@]}")
	stdbuf -oL "./$PROJECT" "${ARGS[@]}" >> "$LOG" 2>&1
	rc=$?
	echo "=== $(date) process exited rc=$rc, codec re-init relaunch ===" >> "$LOG"
	# lived >=20 s: normal watchdog relaunch (1 s). died fast: back off up to 30 s.
	if [ $(( $(date +%s) - t0 )) -ge 20 ]; then delay=1; else delay=$(( delay * 2 > 30 ? 30 : delay * 2 )); fi
	sleep "$delay"
done
