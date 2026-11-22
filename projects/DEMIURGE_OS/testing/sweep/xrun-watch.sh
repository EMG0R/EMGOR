#!/usr/bin/env bash
# DEMIURGE sweep — xrun + below-xrun grazing detector.
#
# Wraps the canonical sources of "the audio thread had trouble":
#
#   1. PipeWire per-node xrun counters     (pw-dump → diff)
#   2. PipeWire process-time vs cycle-time (pw-top batch sample → grazing %)
#   3. system journal                      (journalctl, xrun|underrun|late)
#   4. kernel ring buffer                  (dmesg, xhci|underrun|usb_audio)
#   5. csound stderr                       (caller passes csound's log path)
#
# Items 1 and 3-5 catch classic xruns. Item 2 catches the case the user
# is hitting: the chain doesn't miss a deadline but spends most of every
# cycle on DSP, leaving no headroom — the "no xrun glitch" signature.
#
# Usage:
#   xrun-watch.sh start <rundir>
#   xrun-watch.sh stop  <rundir> [csound-log-path]
#
# `start` snapshots state. `stop` diffs and writes a report at
# <rundir>/xrun-report.txt.

set -euo pipefail

cmd="${1:-}"; rundir="${2:-}"; csound_log="${3:-}"
[[ -z "${cmd}" || -z "${rundir}" ]] && {
  echo "usage: xrun-watch.sh {start|stop} <rundir> [csound-log]" >&2
  exit 1
}
mkdir -p "${rundir}"

have_jq=0; command -v jq >/dev/null 2>&1 && have_jq=1
have_pwdump=0; command -v pw-dump >/dev/null 2>&1 && have_pwdump=1
have_pwtop=0;  command -v pw-top  >/dev/null 2>&1 && have_pwtop=1

if [[ "${cmd}" == "start" ]]; then
  date +%s.%N > "${rundir}/.watch-start"
  if [[ "${have_pwdump}" == 1 ]]; then
    pw-dump >"${rundir}/pw-before.json" 2>/dev/null || true
  fi
  if [[ "${have_pwtop}" == 1 ]]; then
    timeout 2 pw-top -b -n 1 >"${rundir}/pwtop-before.txt" 2>/dev/null || true
  fi
  exit 0
fi

if [[ "${cmd}" != "stop" ]]; then
  echo "xrun-watch: unknown subcommand '${cmd}'" >&2
  exit 1
fi

start_ts="$(cat "${rundir}/.watch-start" 2>/dev/null || echo 0)"
start_epoch="${start_ts%.*}"
[[ -z "${start_epoch}" || "${start_epoch}" == "0" ]] && start_epoch="$(date +%s)"
window_s=$(( $(date +%s) - start_epoch + 2 ))   # +2s grace

# ── 1+2: PipeWire after-snapshot ─────────────────────────────────────────────
if [[ "${have_pwdump}" == 1 ]]; then
  pw-dump >"${rundir}/pw-after.json" 2>/dev/null || true
fi
if [[ "${have_pwtop}" == 1 ]]; then
  timeout 2 pw-top -b -n 1 >"${rundir}/pwtop-after.txt" 2>/dev/null || true
fi

# ── 1: per-node xrun-count delta ─────────────────────────────────────────────
xrun_diff_text="(pw-dump or jq missing)"
if [[ "${have_pwdump}" == 1 && "${have_jq}" == 1 \
      && -s "${rundir}/pw-before.json" && -s "${rundir}/pw-after.json" ]]; then
  for tag in before after; do
    jq -r '
      .[] | select(.type=="PipeWire:Interface:Node") |
      [.info.props["node.name"] // "?",
       (.info.props["state.process.xrun-count"] //
        .info.props["xrun"] // 0)
      ] | @tsv
    ' "${rundir}/pw-${tag}.json" >"${rundir}/xrun-${tag}.tsv" 2>/dev/null || true
  done
  if [[ -s "${rundir}/xrun-before.tsv" && -s "${rundir}/xrun-after.tsv" ]]; then
    xrun_diff_text="$(
      awk -F'\t' '
        NR==FNR { before[$1] = $2 + 0; next }
        {
          a = $2 + 0
          b = (($1 in before) ? before[$1] : 0)
          if (a > b) printf "  %-50s +%d\n", $1, (a - b)
        }
      ' "${rundir}/xrun-before.tsv" "${rundir}/xrun-after.tsv"
    )"
    [[ -z "${xrun_diff_text}" ]] && xrun_diff_text="  none"
  fi
fi

# ── 2: pw-top grazing (BUSY% vs cycle) ──────────────────────────────────────
# pw-top batch output columns (PipeWire ≥0.3.50):
#   S   ID  QUANT   RATE    WAIT    BUSY  W/Q B/Q ERR FMT NAME
# We want B/Q (BUSY/QUANTUM ratio) — anything ≥ 0.85 is grazing.
grazing_text="(pw-top missing)"
if [[ -s "${rundir}/pwtop-after.txt" ]]; then
  grazing_text="$(
    awk '
      NR == 1 { next }                       # header
      /^[[:space:]]*$/ { next }
      {
        # Try to find a column that looks like a ratio 0.00-1.99
        # We pick the last numeric token before NAME-ish columns.
        ratio = 0
        for (i = 1; i <= NF; i++) {
          if ($i ~ /^[01]\.[0-9]+$/) ratio = $i + 0
        }
        if (ratio >= 0.85) {
          # last 1-2 fields are NAME; join them
          name = $NF
          if (NF > 1 && $(NF-1) !~ /^[0-9.]+$/) name = $(NF-1) " " $NF
          printf "  %-50s B/Q=%.2f\n", name, ratio
        }
      }
    ' "${rundir}/pwtop-after.txt"
  )"
  [[ -z "${grazing_text}" ]] && grazing_text="  none above 85%"
fi

# ── 3: journalctl lines ─────────────────────────────────────────────────────
journal_text="(journalctl unavailable)"
if command -v journalctl >/dev/null 2>&1; then
  journal_text="$(
    journalctl --user --no-pager --since "${window_s} seconds ago" 2>/dev/null \
      | grep -Ei 'xrun|underrun|overrun|late|missed|underflow' \
      | head -30 \
      | sed 's/^/  /'
  )"
  [[ -z "${journal_text}" ]] && journal_text="  none"
fi

# ── 4: dmesg lines ──────────────────────────────────────────────────────────
dmesg_text="(dmesg unavailable / not permitted)"
if dmesg -T 2>/dev/null | head -1 >/dev/null; then
  dmesg_text="$(
    dmesg -T --since "${window_s} seconds ago" 2>/dev/null \
      | grep -Ei 'xhci|xrun|underrun|usb_audio|snd_usb|underflow' \
      | head -20 \
      | sed 's/^/  /'
  )"
  [[ -z "${dmesg_text}" ]] && dmesg_text="  none"
fi

# ── 5: csound stderr ────────────────────────────────────────────────────────
csound_text="(no csound log path passed)"
if [[ -n "${csound_log}" && -r "${csound_log}" ]]; then
  csound_text="$(
    grep -Ei 'underrun|buffer|late|missed|xrun|cannot|fail|error' "${csound_log}" \
      | head -20 \
      | sed 's/^/  /'
  )"
  [[ -z "${csound_text}" ]] && csound_text="  none"
fi

# ── render report ───────────────────────────────────────────────────────────
{
  echo "-- XRUN COUNTS (PipeWire per-node delta) --"
  echo "${xrun_diff_text}"
  echo ""
  echo "-- PIPEWIRE GRAZING (BUSY/QUANTUM ≥ 0.85) --"
  echo "${grazing_text}"
  echo ""
  echo "-- JOURNAL (last ${window_s}s, xrun/underrun/late) --"
  echo "${journal_text}"
  echo ""
  echo "-- DMESG (last ${window_s}s, xhci/usb_audio) --"
  echo "${dmesg_text}"
  echo ""
  echo "-- CSOUND STDERR (underrun/buffer/late/error) --"
  echo "${csound_text}"
} > "${rundir}/xrun-report.txt"

cat "${rundir}/xrun-report.txt"
