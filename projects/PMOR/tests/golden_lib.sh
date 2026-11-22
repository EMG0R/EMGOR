# golden_lib.sh -- shared by test_golden.sh / test_golden_engines.sh.
# On Linux (the platform the goldens were captured on: gcc + glibc libm) a render must be BIT-identical (sha256).
# On other platforms (macOS arm64: Apple libm differs in the last ulp of sinf/expf/..., so hashes legitimately differ) a hash
# mismatch is accepted only if the case has the same sample count, no non-finite samples and peak and RMS (printed to 6 decimals)
# agree with the golden to within GOLDEN_TOL (default 1e-5 absolute). Anything else is still a failure.
GOLDEN_TOL="${GOLDEN_TOL:-0.00001}"
golden_ok() {   # golden_ok MANIFEST NAME HASH SAMPLES PEAK RMS  -> 0 if acceptable
  local man="$1" name="$2" h="$3" samples="$4" peak="$5" rms="$6" line want_h want_s want_p want_r
  line=$(grep -m1 " $name " "$man") || return 1
  read -r want_h _ want_s want_p want_r _ <<< "$line"
  [ "$h" = "$want_h" ] && return 0
  [ "$(uname -s)" = "Linux" ] && return 1
  [ "$samples" = "$want_s" ] || return 1
  awk -v a="$peak" -v b="$want_p" -v c="$rms" -v d="$want_r" -v t="$GOLDEN_TOL" 'function ab(x){return x<0?-x:x} BEGIN{exit !(ab(a-b)<=t && ab(c-d)<=t)}'
}
count_lines() { wc -l < "$1" | tr -d ' '; }
