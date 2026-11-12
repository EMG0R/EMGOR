#!/bin/bash
# VCV Rack beside csound on gorpi7v5 (2026-10-23).
#   rack-try.sh [start] [patch.vcv]   (re)start Rack with that patch (default 05_flsamp.vcv)
#   rack-try.sh stop
# Rack: headless, core 1, PipeWire JACK, node.async (runs one quantum behind and
# can never make csound late). RtAudio connects Rack's outs to the HAT ("Built-in
# Audio Pro") itself: outport 0/1 -> main out 1/2; to_csound() below then moves
# them onto csound6:input9/10 so Rack plays THROUGH NEPTR (2026-10-27).
# Patch = wired copy of ~/demiurge_vcv/instruments/<patch> (originals untouched),
# rebuilt every start by wire_patch.py because ALSA client numbers change.
STATE=~/vcv/state; mkdir -p "$STATE" ~/vcv/patches
LOG=~/PRIME_RECOVERY/logs/rack.log
[ "$1" = start ] && shift
CMD="$1"

stop() {
    [ -f "$STATE/rack.pgid" ] && kill -- -"$(cat "$STATE/rack.pgid")" 2>/dev/null
    pkill -x Rack 2>/dev/null
    for i in $(seq 1 20); do pgrep -x Rack >/dev/null || break; sleep 0.1; done
    pkill -9 -x Rack 2>/dev/null
    rm -f "$STATE/rack.pgid"
}

# VCV -> csound -> out (EMGOR 2026-10-27). Rack's RtAudio opens the HAT and links
# outport 0/1 straight to main out 1/2, which skips NEPTR entirely. Move them onto
# csound6:input9/10 (routing: inputs 10, rack_l 9, rack_r 10) so the synth runs
# through the rack. demiurge-graph is the single write path and keeps the links
# across graph re-applies. The direct HAT links are dropped only once csound has
# taken the signal, so an 8-input engine (old routing) still plays Rack direct.
G=/opt/demiurge/bin/demiurge-graph
to_csound() {
    local i dst
    for i in 0 1; do
        if ! "$G" connect "VCV Rack:outport $i" "csound6:input$((9 + i))" >/dev/null 2>&1; then
            echo "rack: csound6:input$((9 + i)) missing (engine not on 10 inputs?) -> Rack stays direct to the HAT"
            return 1
        fi
    done
    sleep 1   # RtAudio makes its own HAT links just after the ports appear
    # every Rack output, not just 0/1: outports 2-7 also land on HAT outs 3-8
    for i in $(pw-link -o 2>/dev/null | sed -n 's/^VCV Rack:outport //p'); do
        for dst in $(pw-link -l 2>/dev/null | awk -v p="VCV Rack:outport $i" '$0==p{f=1;next} /^[^ \t]/{f=0} f&&/\|->/{print $2}'); do
            case "$dst" in csound6:*) ;; *) "$G" disconnect "VCV Rack:outport $i" "$dst" >/dev/null 2>&1 ;; esac
        done
    done
    echo "rack -> csound6:input9/10 (through NEPTR)"
    # Rack's DSP runs in the RtAudio callback, which came up SCHED_OTHER nice 5:
    # ~9 xruns/s. FIFO 70 (below pipewire 83 and csound 74) -> ~0.5/s.
    local t
    for t in $(ps -L -o tid=,comm= -p "$(pgrep -xo Rack)" | awk '$2=="RtAudio"||$2=="data-loop.0"{print $1}'); do
        sudo -n chrt -f -p 70 "$t" 2>/dev/null
    done
}

if [ "$CMD" = stop ]; then stop; echo "rack stopped"; exit 0; fi
PATCH="${CMD:-$(cat "$STATE/current" 2>/dev/null || echo 05_flsamp.vcv)}"   # default: last used
[ -f ~/demiurge_vcv/instruments/"$PATCH" ] || { echo "no patch $PATCH"; exit 1; }

stop
~/kivy-venv/bin/python ~/PRIME_RECOVERY/vcv/wire_patch.py ~/demiurge_vcv/instruments/"$PATCH" ~/vcv/patches/"$PATCH" 2>&1 | grep -v "LIBARCHIVE.xattr"
[ "${PIPESTATUS[0]}" = 0 ] || exit 1

cd ~/vcv-build/Rack || exit 1
# A killed Rack leaves its log "truncated"; the next start then pops an invisible
# "Rack crashed" dialog that blocks the patch load. Rotate it (kept, not deleted).
RL=~/.local/share/Rack2/log.txt; [ -f "$RL" ] && mv "$RL" "$RL.prev"
# -h = real headless (no window, no GL); it waits on stdin, so stdin never closes.
LD_LIBRARY_PATH=/usr/lib/aarch64-linux-gnu/pipewire-0.3/jack \
PIPEWIRE_LATENCY=128/48000 \
PIPEWIRE_PROPS='{ node.async=true node.lock-quantum=true node.lock-rate=true node.autoconnect=false node.dont-reconnect=true }' \
  setsid bash -c "sleep infinity | exec taskset -c 1 nice -n 5 ./Rack -h -s . '$HOME/vcv/patches/$PATCH'" > "$LOG" 2>&1 < /dev/null &
echo $! > "$STATE/rack.pgid"
echo "$PATCH" > "$STATE/current"

# Rack forks a child at startup that can deadlock in libc exit teardown
# (pthread_cond_destroy in __cxa_finalize) while the main thread waits on it
# forever. Kill any Rack child older than 2 s; done once Rack's ports exist.
for i in $(seq 1 60); do
    sleep 0.5
    main=$(pgrep -xo Rack) || continue
    for c in $(pgrep -P "$main" -x Rack); do
        [ "$(ps -o etimes= -p "$c" | tr -d ' ')" -ge 2 ] 2>/dev/null && kill -9 "$c"
    done
    pw-link -o 2>/dev/null | grep -q "^VCV Rack:outport 0" && { echo "rack up: $PATCH ($((i/2)) s)"; to_csound; exit 0; }
done
echo "rack did not come up with $PATCH; log:"; tail -5 "$LOG"; exit 1
