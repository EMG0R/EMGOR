#!/usr/bin/env bash
# fwcheck.sh ELF : model the Daisy Seed (STM32H750, APP_TYPE=BOOT_SRAM) from the linked firmware, before anything is flashed.
#   1 region budget   app image in SRAM, DTCM data+bss, D2/D3 SRAM, SDRAM 64 MB; DTCM headroom (stack) >= FW_DTCM_MIN (16 KB)
#   2 stack depth     static worst case from the disassembly call graph: main + the two deepest ISRs (+ exception frames) must fit the
#                     real stack space (_estack - _ebss); recursion / dynamic calls are reported
#   3 SDRAM-before-init  no static constructor (or anything it calls) may reference SDRAM (0xC0000000..): SDRAM is brought up by hw.Init() in main
#   4 pre-USB boot    nothing reached from the static constructors or from hwBoot() before tud_connect() may touch FatFS / the SD / patch storage
#   5 watchdog        the IWDG may be refreshed only by bootguardKick(); only main() and pmor_usb_service() may call it; bootguardKick() must read the
#                     clock (the boot kick budget: a boot that overruns stops being fed and resets, arming the skip-storage flag). No boot-time pump
#                     may feed it forever (852e4af: a boot-time CDC pump kept the dog fed through a wedged boot).
#   7 usb service    pmor_usb_service() runs once per I2C byte (bbus.h pumps USB per byte) and at every OLED piece, so anything heavy reachable from it
#                     stretches every sensor/trellis transaction ~10x and delays the keypad poll (2026-11-11: fxMidiPoll, ~1.1 ms ungated-per-call: pad lag).
#                     (a) every fxMidiPoll( call site must sit on a line carrying its time gate (s_fx_t); (b) the call graph under pmor_usb_service may not
#                     reach FatFS, a linear schema search (pmorParamGet/Set/Find), the patch CRC/snapshot or the QSPI mirror.
#   6 cdc tx fifo    nothing may call tud_cdc_write_clear / tu_fifo_clear on the CDC TX FIFO (0c48eaa: clearing it while an IN transfer is in flight
#                     left the endpoint busy forever: HELLO + console replies never reached the host, RX kept working)
# Exit 1 on any failure. Prints the budget table every run.
set -uo pipefail
ELF="${1:-build/PMOR_V3.elf}"; DTCM_MIN="${FW_DTCM_MIN:-16384}"
command -v arm-none-eabi-objdump >/dev/null || { echo "fwcheck: no arm-none-eabi tools in PATH"; exit 2; }
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
arm-none-eabi-nm "$ELF" | awk 'NF==3' > "$W/nm.txt"
arm-none-eabi-objdump -d --no-show-raw-insn "$ELF" > "$W/dis.txt"
arm-none-eabi-objdump -h "$ELF" > "$W/sec.txt"
sym() { awk -v n="$1" '$3==n{print $1; exit}' "$W/nm.txt"; }
EBSS=$(sym _edtcmram_bss); [ -n "$EBSS" ] || EBSS=$(sym _ebss); ESTACK=$(sym _estack)
# vector table + init_array words -> handler / constructor addresses (thumb bit cleared)
words() { arm-none-eabi-objdump -s -j "$1" "$ELF" 2>/dev/null | awk 'NR>4 && NF>=2 { for (i=2;i<=5 && i<=NF;i++) if ($i ~ /^[0-9a-f]{8}$/) print $i }' | awk '{ b=substr($1,7,2) substr($1,5,2) substr($1,3,2) substr($1,1,2); print b }'; }
words .isr_vector | tail -n +2 > "$W/vec.txt"
words .init_array > "$W/init.txt"

echo "=== fwcheck: $ELF"
fail=0
# ---------------------------------------------------------------- 1. region budget
awk -v ebss="$EBSS" -v estack="$ESTACK" -v dmin="$DTCM_MIN" '
function hexv(s,   i,c,v,d){ v=0; s=tolower(s); sub(/^0x/,"",s); for(i=1;i<=length(s);i++){ c=substr(s,i,1); d=index("0123456789abcdef",c)-1; if(d<0) return -1; v=v*16+d } return v }
BEGIN { DT0=536870912; DT1=DT0+131072; D20=805306368; D21=D20+262144; D30=939524096; D31=D30+65536; SD0=3221225472; SD1=SD0+67108864; AX0=603979776; AX1=AX0+524288 }
/^ *[0-9]+ / { name=$2; size=hexv($3); vma=hexv($4); lma=hexv($5); getline fl; noload = (fl !~ /LOAD/);
  if (size==0) next
  if (vma>=DT0 && vma<DT1) dtcm+=size
  if (vma>=D20 && vma<D21) d2+=size
  if (vma>=D30 && vma<D31) d3+=size
  if (vma>=SD0 && vma<SD1) sdram+=size
  if (!noload && lma>=AX0 && lma<AX1) img+=size
  if (vma>=AX0 && vma<AX1 && noload) axibss+=size
}
END { eb=hexv(ebss); es=hexv(estack); room=es-eb; bad=0
  printf "%-34s %10s %10s %6s\n","region","used","limit","%"
  printf "%-34s %10d %10d %5.1f%%\n","app image in SRAM (AXI 480K)",img+axibss,480*1024,100*(img+axibss)/(480*1024); if (img+axibss>480*1024) bad=1
  printf "%-34s %10d %10d %5.1f%%\n","DTCM .data+.bss",dtcm,128*1024,100*dtcm/(128*1024); if (dtcm>128*1024) bad=1
  printf "%-34s %10d %10d\n","DTCM headroom (stack+heap)",room,dmin; if (room<dmin) { bad=1; printf "  FAIL: DTCM headroom %d B < %d B\n",room,dmin }
  printf "%-34s %10d %10d %5.1f%%\n","D2 SRAM (dma 32K + 256K)",d2,288*1024,100*d2/(288*1024); if (d2>288*1024) bad=1
  printf "%-34s %10d %10d %5.1f%%\n","D3 SRAM",d3,64*1024,100*d3/(64*1024); if (d3>64*1024) bad=1
  printf "%-34s %10d %10d %5.1f%%\n","SDRAM",sdram,64*1024*1024,100*sdram/(64*1024*1024); if (sdram>64*1024*1024) bad=1
  print "ROOM " room; exit bad }' "$W/sec.txt" > "$W/r1.txt"; r1=$?
grep -v '^ROOM' "$W/r1.txt"; ROOM=$(awk '/^ROOM/{print $2}' "$W/r1.txt")
[ $r1 -eq 0 ] && echo "PASS  1 region budget" || { echo "FAIL  1 region budget"; fail=1; }

# ---------------------------------------------------------------- 2-4 call-graph analysis
awk -v room="$ROOM" -v w="$W" '
function hexv(s,   i,c,v,d){ v=0; s=tolower(s); sub(/^0x/,"",s); for(i=1;i<=length(s);i++){ c=substr(s,i,1); d=index("0123456789abcdef",c)-1; if(d<0) return -1; v=v*16+d } return v }
function addname(a){ return (a in a2n) ? a2n[a] : "" }
function depth(f,   n,i,cs,best,d,bp) {
  if (f in memo) return memo[f]
  if (f in vis) { rec[f]=1; return 0 }
  vis[f]=1; best=0; bp=""
  n=split(edges[f], cs, " ")
  for (i=1;i<=n;i++) { if (!(cs[i] in start)) continue; d=depth(cs[i]); if (d>best) { best=d; bp=cs[i] } }
  delete vis[f]; memo[f]=frame[f]+best; nxt[f]=bp; return memo[f]
}
function reach(f, set,   n,i,cs) {   # transitive callees into set
  if (f in set) return; set[f]=1
  n=split(edges[f], cs, " "); for (i=1;i<=n;i++) if (cs[i] in start) reach(cs[i], set)
}
FILENAME ~ /nm.txt/ { if ($2 ~ /^[tTwW]$/) { a=hexv($1); a2n[a]=$3 } next }
FILENAME ~ /vec.txt/ { a=hexv($1); if (a>=0) { a=a-(a%2); vecA[a]=1 } next }
FILENAME ~ /init.txt/ { a=hexv($1); a=a-(a%2); initA[a]=1; next }
{ if ($0 ~ /^[0-9a-f]+ <.*>:$/) { cur=$2; gsub(/[<>:]/,"",cur); start[cur]=1; ni[cur]=0; lab[hexv($1)]=cur; next }
  if (cur=="") next
  n=split($0, t, "\t"); if (n<2) next
  mn=t[2]; op=t[3]; sub(/;.*/,"",op)
  if (mn==".word") { v=hexv(op); if (v>=3221225472 && v<3288334336) sd[cur]=sd[cur] " " op; next }
  ni[cur]++
  if (ni[cur]<=14) {
    if (mn ~ /^push/) { c=gsub(/,/,",",op); frame[cur]+=4*(c+1) }
    else if (mn ~ /^vpush/) { o2=op; gsub(/[{} d]/,"",o2); if (o2 ~ /-/) { split(o2,rg,"-"); frame[cur]+=8*(rg[2]-rg[1]+1) } else frame[cur]+=8*(gsub(/,/,",",op)+1) }
    else if (mn ~ /^sub(w|\.w)?$/ && op ~ /^sp,/) { if (match(op,/#[0-9]+/)) frame[cur]+=substr(op,RSTART+1,RLENGTH-1)+0 }
  }
  if (mn ~ /^(bl|blx|b|b\.w|b\.n|b[a-z][a-z]|b[a-z][a-z]\.w|b[a-z][a-z]\.n)$/ && mn !~ /^(bic|bfi|bfc|bkpt|bx|blx)/) {
    if (match(op,/<[^>]+>/)) { tg=substr(op,RSTART+1,RLENGTH-2); sub(/\+0x[0-9a-f]+$/,"",tg); if (tg!=cur) edges[cur]=edges[cur] " " tg; if (tg ~ /^(f_open|f_read|f_write|f_mount|f_stat|f_mkdir|f_unlink|f_sync|f_lseek|f_close|f_rename|f_opendir|f_readdir|f_truncate|f_expand|f_getfree|f_chdir)$/) fatfs[cur]=1 }
  }
  if (mn=="blx" && op ~ /^r[0-9]+$/) dyn[cur]++
  if (mn=="bx" && op ~ /^r[0-9]+$/ && op!="r14") dyn[cur]++
}
END {
  fails=0
  for (a in vecA) if (a in lab) vecn[lab[a]]=1
  for (a in initA) if (a in lab) initn[lab[a]]=1
  # ---- 2. stack depth
  md=("main" in start) ? depth("main") : 0
  nroots=0
  for (r in vecn) if (r ~ /IRQHandler|Handler/ && r !~ /Default|Reset/ ) { if (r in start) { d=depth(r); isr[r]=d } }
  if ("AudioCallback" in start) isr["AudioCallback"]=depth("AudioCallback")
  b1=0;b2=0;n1="";n2=""; for (r in isr) printf "  isr %s %d B\n", r, isr[r]
  for (r in isr) { d=isr[r]; if (d>b1) { b2=b1;n2=n1;b1=d;n1=r } else if (d>b2) { b2=d;n2=r } }
  need=md+b1+b2+2*104
  printf "stack: main %d B, deepest ISRs %s %d B + %s %d B, exception frames 208 B => worst case %d B of %d B available\n", md,n1,b1,n2,b2,need,room
  p="main"; s=""; for (i=0;i<9 && p!="";i++) { s=s p " > "; p=nxt[p] } printf "  main path: %s...\n", s
  nrec=0; for (r in rec) nrec++
  nd=0; for (r in memo) if (r in dyn) nd+=dyn[r]
  printf "  recursion in %d function(s), %d indirect call site(s) in the reachable graph (their callees are NOT counted)\n", nrec, nd
  if (nrec>0) for (r in rec) printf "  recursive: %s\n", r
  if (need>room) { printf "FAIL  2 stack depth: need %d B > %d B available\n", need, room; fails++ } else printf "PASS  2 stack depth\n"
  # ---- 3. SDRAM before init
  bad3=0
  for (c in initn) { delete set; reach(c, set); for (f in set) if (f in sd) { printf "  static constructor %s reaches %s which references SDRAM:%s\n", c, f, sd[f]; bad3++ } }
  nin=0; for (c in initn) nin++
  printf "constructors checked: %d\n", nin
  if (bad3>0) { printf "FAIL  3 SDRAM touched by static initialisation (before hw.Init brings SDRAM up): %d reference(s)\n", bad3; fails++ } else printf "PASS  3 no static constructor touches SDRAM\n"
  # ---- 4. pre-USB boot
  bad4=0
  for (c in initn) { delete set; reach(c, set); for (f in set) if ((f in fatfs) || f ~ /^(patchInit|sdMount|storeDevice|storeQueueAll|storeLoadManifest|scanExists)/) { printf "  static constructor %s reaches %s (FatFS / storage) before USB\n", c, f; bad4++ } }
  # hwBoot call order up to tud_connect
  hb="_Z6hwBootPFvPPfS0_jE"; for (f in start) if (f ~ /^_Z6hwBoot/) hb=f
  if (hb in start) {
    nb=split(edges[hb], hbc, " "); np=0
    for (i=1;i<=nb;i++) { if (hbc[i] ~ /tud_connect/) break; np++; delete set; reach(hbc[i], set); for (f in set) if ((f in fatfs) || f ~ /^(patchInit|sdMount|storeDevice|storeQueueAll|storeLoadManifest)/) { printf "  hwBoot -> %s reaches %s (FatFS / storage) before tud_connect\n", hbc[i], f; bad4++ } }
    printf "hwBoot: %d call(s) before tud_connect checked\n", np
    if (nb>0 && np==nb) { print "  note: tud_connect not found among hwBoot callees (inlined?) - order not verified"; }
  } else print "  note: hwBoot symbol not found"
  # ---- 5. watchdog feeding
  bad5=0; kk=""; for (f in start) if (f ~ /^_Z13bootguardKickv/ || f=="bootguardKick") kk=f
  for (f in edges) { n5=split(edges[f], cs5, " "); for (i=1;i<=n5;i++) if (cs5[i]==kk && kk!="") if (f != "main" && f != "pmor_usb_service") { printf "  %s calls bootguardKick (only main and pmor_usb_service may feed the watchdog)\n", f; bad5++ } }
  if (kk=="") { print "  bootguardKick not found"; bad5++ } else { has=0; n5=split(edges[kk], cs5, " "); for (i=1;i<=n5;i++) if (cs5[i] ~ /GetNow/) has=1; if (!has) { print "  bootguardKick() does not read the clock: no boot kick budget"; bad5++ } }
  if (bad5>0) { printf "FAIL  5 watchdog fed outside the allowed paths or without a boot budget: %d\n", bad5; fails++ } else printf "PASS  5 watchdog: only main/pmor_usb_service feed it, bootguardKick has a boot budget\n"
  # ---- 7b. nothing heavy under pmor_usb_service
  bad7=0; us="pmor_usb_service"
  if (us in start) {
    delete set; reach(us, set); nr=0
    for (f in set) { nr++; if ((f in fatfs) || (f ~ /(pmorParamGet|pmorParamSet|pmorSchemaFind|patchLiveCrc|patchSnapshot|pmorCrc32|qspiMirrorWrite|patchSave)/ && f !~ /GetAt/)) { printf "  pmor_usb_service reaches %s (heavy: it runs once per I2C byte)\n", f; bad7++ } }
    printf "pmor_usb_service: %d function(s) reachable\n", nr
  } else { print "  pmor_usb_service symbol not found"; bad7++ }
  if (bad7>0) { printf "FAIL  7b heavy work reachable from pmor_usb_service: %d\n", bad7; fails++ } else printf "PASS  7b nothing heavy (FatFS, linear schema search, patch CRC, QSPI) under pmor_usb_service\n"
  if (bad4>0) { printf "FAIL  4 pre-USB boot touches storage: %d path(s)\n", bad4; fails++ } else printf "PASS  4 nothing storage-related runs before USB is up\n"
  exit fails
}' "$W/nm.txt" "$W/vec.txt" "$W/init.txt" "$W/dis.txt" > "$W/r2.txt"; r2=$?
cat "$W/r2.txt"
# ---- 6. CDC TX FIFO clear: only TinyUSB's own open/reset paths may clear a FIFO (callers of tu_fifo_clear / tud_cdc_write_clear)
BADCLR=$(awk '/^[0-9a-f]+ <.*>:$/{fn=$2} /<(tu_fifo_clear|tud_cdc_write_clear|tud_cdc_n_write_clear)>/{ if (fn !~ /^<(cdcd_|midid_|audiod_|usbd_|tud_rhport_init|tu_fifo_clear|tud_cdc_n_write_clear|tud_cdc_write_clear)/) print fn }' "$W/dis.txt" | sort -u)
if [ -n "$BADCLR" ]; then echo "FAIL  6 application code clears a USB FIFO (wedges the IN endpoint): $BADCLR"; fail=1; else echo "PASS  6 the CDC TX FIFO is only cleared inside TinyUSB's own open/reset"; fi
[ $r2 -ne 0 ] && fail=1
# ---- 7a. fxMidiPoll must never be called ungated (it is called from code that runs per I2C byte)
SRCDIR="$(cd "$(dirname "$0")/.." && pwd)"
UNGATED=$(grep -rn --include='*.cpp' --include='*.h' --include='*.c' 'fxMidiPoll[[:space:]]*(' "$SRCDIR/hw" "$SRCDIR/main.cpp" "$SRCDIR/ui" "$SRCDIR/store" "$SRCDIR/usb" 2>/dev/null | grep -v 's_fx_t' || true)
if [ -n "$UNGATED" ]; then echo "FAIL  7a fxMidiPoll called without its time gate (s_fx_t):"; echo "$UNGATED"; fail=1; else echo "PASS  7a every fxMidiPoll call site is time-gated"; fi
[ $fail -eq 0 ] && echo "fwcheck: ALL PASS" || echo "fwcheck: FAILED"
exit $fail
