#!/bin/bash
# relaunch loop: the render watchdog exits on 2s of pathological codec
# input; restarting the binary re-inits the codec and clears the ESD latch
cd /root/Bela/projects/hyperGuitar6chPan
while true; do
	# the instrument recreates its USB-audio shm ring on every start and
	# the bridge maps it only once - bounce the bridge so it re-opens the
	# fresh segment (no-op if the service is disabled)
	(sleep 2; systemctl try-restart usbaudio-bridge 2>/dev/null) &
	# stdbuf -oL: stdout is a FILE here, so without it the rt_printf diag
	# lines sit in a 4 KB stdio buffer for minutes (2026-10-01: nothing
	# reached the log for a whole 125 s test). Line-buffered = live log.
	stdbuf -oL ./hyperGuitar6chPan -Jsettings.json -p 64 >> /root/hyperGuitar-live.log 2>&1
	echo "=== $(date) process exited, codec re-init relaunch ===" >> /root/hyperGuitar-live.log
	sleep 1
done
