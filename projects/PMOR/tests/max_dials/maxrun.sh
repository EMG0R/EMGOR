# maxrun.sh -- sourced helpers for the headless Max checks: launch a patcher in its own Max instance, kill only that instance.
CR="$HOME/Library/Application Support/Cycling '74/Max 9/Crash Recovery"
maxpids() { ps -axo pid=,comm= | awk '$2 ~ /Max\.app\/Contents\/MacOS\/Max$/ {print $1}' | sort; }
clean_recovery() { local c q; for c in "$CR"/maxworkspace-*.txt; do [ -e "$c" ] || continue; q="${c##*maxworkspace-}"; q="${q%.txt}"; kill -0 "$q" 2>/dev/null || rm -f "$c"; done; }
# maxrun_open PATCHER SECS [DONEFILE]  -- opens PATCHER (cwd = its dir) in a new Max, waits up to SECS (stops early once DONEFILE contains DONE), kills the new instance(s)
maxrun_open() {
  local f="$1" secs="$2" done_="${3:-}" before after t=0 p
  clean_recovery; before="$(maxpids)"
  ( cd "$(dirname "$f")" && open -n -a /Applications/Max.app "$f" )
  while [ $t -lt "$secs" ]; do [ -n "$done_" ] && grep -q '^DONE' "$done_" 2>/dev/null && break; sleep 1; t=$((t+1)); done
  sleep 1; after="$(maxpids)"
  for p in $after; do echo "$before" | grep -qx "$p" || kill "$p" 2>/dev/null; done
  sleep 1; for p in $after; do echo "$before" | grep -qx "$p" || kill -9 "$p" 2>/dev/null; done
  clean_recovery
}
