;==============================================================
; IR REVERB — convolution reverb, proposed Menu 12 (after NAM)
;
; *** 2026-08-21 EVEN-LOAD REWRITE NOTICE ***
; The DSP is no longer a single ftconv call per row -- it is 4
; staggered liveconv lanes per channel (ksmps=64 crackle fix, full IR
; tails preserved, offline-verified against scipy.signal.fftconvolve
; to float-precision noise on all 25 IR/variant combinations). The
; per-row predelay/CPU tables and the ftconv reinit/click description
; below are HISTORICAL (describe the pre-rewrite design) -- see the
; "EVEN-LOAD REWRITE" comment blocks at the giIRLaneChunk preload and
; at instr 240 (below) for what actually runs now, and verify_ir.json
; for the verification evidence. Predelay is now a UNIFORM 5.33ms
; (256 samples) on every row, not the per-row 5.3/10.7ms below.
;
; 5 rows (one per IR type), NAM-style row/variant model:
;   row 0 = SPRING   (AKG spring tank, Studio Nord Bremen)
;   row 1 = PLATE    (EMT 140 plate + EMT 240 gold foil)
;   row 2 = HALL     (OpenAIR concert halls + Voxengo halls)
;   row 3 = CHAMBER  (council chamber / mausoleum / cave / silo)
;   row 4 = WOOD RM  (wooden rooms: typing room, studio live room ...)
;
; One row active at a time (mirrors gkNAMActiveRow); each row
; remembers its selected variant (gkIRSel[r]) and its dry/wet
; (gkIRRowMix[r]).  IR files live in gSIRDir, named NAM-style
; <row+1><variant+1>_label.wav (e.g. 12_spring_med_flat.wav) so
; the UI can scan the directory for live labels exactly like the
; NAM menu does.  All 25 IRs are preloaded into ftables at boot
; (~14 MB of samples ≈ 55 MB as doubles) — variant switching
; never touches the disk, so there is no file-I/O hiccup risk.
;
; DSP: mono-in stereo-out partitioned convolution (ftconv), with
; a PER-ROW partition size (giIRPartRow).
;
; WET PREDELAY IS EXACTLY partition/sr — INHERENT TO ftconv, not
; a bug and not fixable by CPU/scheduling.  Measured on the Pi by
; sending a unit impulse through ftconv and finding the first
; non-zero output sample: the answer is the partition size, dead
; on, every time (128 -> 2.667 ms, 256 -> 5.333, 512 -> 10.667,
; 1024 -> 21.333, 2048 -> 42.667).  If a reverb here ever "feels
; late", this table is the first thing to read.
;
;   row 0 SPRING   part 256 ->  5.3 ms wet predelay
;   row 1 PLATE    part 256 ->  5.3 ms
;   row 2 HALL     part 512 -> 10.7 ms
;   row 3 CHAMBER  part 512 -> 10.7 ms
;   row 4 WOOD RM  part 256 ->  5.3 ms
;
; Rationale: short bright IRs (spring/plate/wood) are where tens
; of ms of predelay reads as WRONG — and they are also the
; cheapest place to buy a small partition, because cost scales
; with IR length.  Long IRs (hall/chamber) want some predelay
; musically anyway and are the expensive ones, so they keep a
; larger partition.  Halving a partition roughly doubles CPU.
;
; Measured cost of the WORST (longest) IR in each row at the
; shipped partition, as a fraction of ONE Pi 5 core:
;   spring 12 @256 13.4% | plate 25 @256 24.4% | wood 51 @256 15.8%
;   hall   31 @512 19.0% | chamber 42 @512 20.4%
; Engine baseline is ~40% of its pinned core, so worst case lands
; near 65% — roughly 35% headroom.  Before lowering any partition
; further, re-measure: 128 costs 69% of a core on the 4 s chamber
; IR, which would not fit.
;
; Plus the usual one-k-block (64/48000 = 1.3 ms) send/return bus
; latency of every event-driven NEPTR effect.
;
; WHY THE IR IS NOT ON ITS OWN CORE (settled 2026-08-11 — please
; do not re-litigate): running the convolver as a separate JACK
; client costs two quantum hops, out and back, = 2 x 1.33 =
; 2.67 ms.  Splitting at partition P therefore costs P/48 + 2.67
; ms, while simply staying in-process at the next size up, 2P,
; costs 2P/48 ms.  Those break even at exactly P = 128 — so for
; every partition we would realistically use, halving the
; partition in-process beats the process split outright, at lower
; CPU and without a second RT process to supervise.  csound's
; -j (num-threads) was measured too and is flat: 4 parallel
; ftconv instances rendered in 0.90 / 0.88 / 0.93 of realtime at
; -j1 / -j2 / -j4.  It buys nothing here and -j4 is a regression.
;
; VARIANT-SWITCH CLICK RISK: changing row/variant reinits the
; convolver (ftconv's table is i-time).  The running tail is
; truncated and the new IR fades in over 20 ms (kIRFade linseg
; restarted by reinit).  Audible as a brief reverb dropout while
; browsing variants — no click/pop (fade masks the restart).
;
;==============================================================
; INTEGRATION NOTES  (for the integration agent — this file is
; self-contained; the changes below go in the SHARED files)
;
; 1. csound/neptrPhase4.csd — add with the other effect includes:
;        #include "effects/ir_reverb.orc"
;    (must be included AFTER globals.orc; no globals.orc edits
;    needed — all IR globals are defined in this file.)
;
; 2. csound/signal_chain.orc — insert stage 21c right after the
;    "; 21. REVERB (70)" block (late chain, beside REVERB):
;
;        ; 21c. IR REVERB (240) — convolution reverb (Menu 12)
;        gaIRSendL = aPrevL
;        gaIRSendR = aPrevR
;        $FX_BYPASS(gkIRBypass'240'gaIRSendL'gaIRSendR'gaIRWetL'gaIRWetR'irvb)
;        aPrevL = gaIRWetL
;        aPrevR = gaIRWetR
;
;    (instr 240 writes the ALREADY-MIXED dry/wet blend to
;    gaIRWetL/R — BBCUT-style assignment, not ntrpol.)
;
; 3. Instrument number: 240 (single DSP instr).  VERIFIED UNUSED —
;    current instrs: 1,40,48-52,55,62,64,65,68,70,80,81,90-93,99,
;    101,119-122,130-134,150,160-162,200,209-214,222,233,300,
;    401-405.  Numbers 241-249 are reserved for future IR
;    expansion (nothing else may claim them).
;
; 4. Toggle IDs: 49=SPRING 50=PLATE 51=HALL 52=CHAMBER 53=WOOD RM
;    (toggles in use today: 1-28, 31-44 — 49-53 VERIFIED FREE).
;    Menu 12 slot map (mirror NAM menu 11 wiring in midi_handler):
;      s0=SPRING(CC74) s1=PLATE(CC75) s2=HALL(CC76)
;      s3=CHAMBER(CC71) s4=WOODRM(CC72)
;    Slot button r (NAM slot-handler pattern, menu==12 branch):
;      if (gkIRActiveRow == r && gkIRBypass < 0.5) then
;        gkIRBypass = 1                       ; pressing active row = off
;      else
;        gkIRActiveRow = r
;        gkIRBypass = 0
;      endif
;    Slot encoder rotation r (NAM variant-cycle pattern):
;      gkIRSel[r] wraps 0..giIRRowCount[r]-1; CW=+1 CCW=-1
;    Optional P1 (dry/wet per encoder-takeover-v2): map to
;      gkIRRowMix[gkIRActiveRow] (0..1); CC20 → gkIRDamp;
;      CC23 → gkIRLevel if wanted.
;    UI feedback: send /ir_sel<r> with gkIRSel[r] on change
;    (mirror the /nam_sel<r> OSC sends).
;
; 5. neptrPhase4_UI.py — add menu 12:
;      {"name": "IR VERB", "effects": [("ir0","SPRING"),("ir1","PLATE"),
;       ("ir2","HALL"),("ir3","CHAMBER"),("ir4","WOOD RM")]}
;    plus a _scan_ir_rows() clone of _scan_nam_rows() over
;      os.environ.get("NEPTR_IR_DIR", os.path.expanduser("~/demiurge/ir"))
;    for live variant labels (filenames are <row+1><variant+1>_label.wav),
;    listening on /ir_sel<r>.  PARAMS: ir0..ir4 → [("mix","Mix",0,1)].
;
; 6. Deploy: rsync csound/ir/*.wav → gorpi7@demiurge.local:
;    /home/gorpi7/demiurge/ir/   (path below; override with the
;    IR_DIR orchestra macro for local testing — the bench does).
;
; 7. Latency/CPU: wet-path predelay is per-row and equals
;    giIRPartRow[row]/sr — 5.3 ms on spring/plate/wood (256),
;    10.7 ms on hall/chamber (512) — plus 1.3 ms bus latency.
;    Both the predelay figures and the per-row CPU costs are
;    measured on the Pi; see the table in the header, and read it
;    before touching a partition value.
;==============================================================

;--------------------------------------------------------------
; IR library location (absolute Pi path, NAM-style).  Benches and
; local runs override by defining the IR_DIR macro before include.
;--------------------------------------------------------------
; NOTE: the literal below is a static fallback only — a normal
; launch (neptr/scripts/launch_phase4.sh) always overrides this
; macro with --omacro:IR_DIR=$HOME/... so it matches whatever Pi
; user is actually running. This default only fires on a manual,
; direct `csound` invocation that bypasses that script.
#ifndef IR_DIR
#define IR_DIR #/home/gorpi7/demiurge/ir#
#endif

gSIRDir init "$IR_DIR"

;--------------------------------------------------------------
; State (all IR globals live HERE, not in globals.orc)
;--------------------------------------------------------------
gkIRBypass     init 1        ; 1 = bypassed (chain FX_BYPASS gate)
gkIRActiveRow  init -1       ; which row is ON (-1 = none yet)
gkIRSel[]      init 5        ; remembered variant per row
gkIRRowMix[]   fillarray 0.35, 0.35, 0.35, 0.35, 0.35  ; per-row dry/wet
gkIRLevel      init 1.0      ; master wet trim
gkIRDamp       init 1.0      ; master wet tone 0=dark .. 1=open

giIRRowBase[]  fillarray 0, 5, 10, 15, 20  ; flat index of row's variant 0
giIRRowCount[] fillarray 5, 5, 5, 5, 5     ; variants per row
; ftconv partition per row (samples) — ALSO the row's wet predelay
; (predelay = partition/sr exactly).  Must be a multiple of ksmps
; (64).  Halving a value roughly doubles that row's CPU; see the
; measured table in the header before changing one.
giIRPartRow[]  fillarray 256, 256, 512, 512, 256

; flat file list, NAM naming <row+1><variant+1>_label.wav,
; ordered short/tight -> long/washy inside each row
gSIRFiles[] fillarray \
  "11_spring_short_flat.wav",  "12_spring_med_flat.wav",   \
  "13_spring_long_flat.wav",   "14_spring_med_bright.wav", \
  "15_spring_med_dark.wav",                                \
  "21_plate_emt140_1s.wav",    "22_plate_emt140_2s.wav",   \
  "23_plate_emt140_3s.wav",    "24_plate_foil_short.wav",  \
  "25_plate_foil_long.wav",                                \
  "31_hall_jack_lyons.wav",    "32_hall_central_york.wav", \
  "33_hall_usina_arte.wav",    "34_hall_wide_echo.wav",    \
  "35_hall_musikverein.wav",                               \
  "41_chamber_york_guildhall.wav", "42_chamber_mausoleum.wav", \
  "43_chamber_cathedral.wav",  "44_chamber_silo.wav",      \
  "45_chamber_columns.wav",                                \
  "51_wood_terrys_typing.wav", "52_wood_genesis6_live.wav",\
  "53_wood_spokane_club.wav",  "54_wood_rare_books.wav",   \
  "55_wood_drum_room.wav"

; Per-IR energy-normalization trims (2026-09-01). The library files are
; peak-normalized, not energy-normalized, so raw convolution gain varied
; from +2.3 dB (rare_books) to +21.4 dB (EMT plate 3s) — the wet could sit
; 12x louder than the dry, and jump wildly between variants. Each value is
; 1/sqrt(mean stereo IR energy), computed offline from the shipped wav
; files, which makes broadband convolution gain ~unity: the wet lands at
; the natural volume of the dry guitar and every IR sits at a comparable
; level. Recompute if a wav is ever replaced (energy = sum of squares).
giIRGain[] fillarray \
  0.1984, 0.1598, 0.1590, 0.1862, 0.1203, \
  0.1494, 0.1060, 0.0850, 0.1080, 0.0948, \
  0.1509, 0.1452, 0.3027, 0.4260, 0.1721, \
  0.2689, 0.6573, 0.1510, 0.2670, 0.2542, \
  0.4589, 0.3313, 0.2406, 0.7666, 0.3220

; ---- EVEN-LOAD REWRITE (2026-08-21) ----------------------------------
; ftconv concentrates an entire row's convolution into ONE burst per
; partition boundary (~O(IR length) duration -- 1.8-2.7ms for hall/chamber,
; see fx_ir_evenload.json root-cause). At ksmps=64 (1.333ms/block budget)
; that burst overflows on 4 of 5 rows. Fix: replace the single ftconv call
; with FOUR liveconv "lanes" per channel, each convolving a FIXED 48000-
; sample (1.0s) SEGMENT of the IR (lane 0 = samples 0-47999, lane 1 =
; 48000-95999, etc.), using a SMALL partition (256 samples) so each lane's
; own burst is short. 48000 samples covers the longest shipped IR
; (chamber/mausoleum, 4.0s = 192000 frames = 4 lanes exactly); shorter
; IRs auto-zero-pad their higher lanes (GEN01 pads past EOF -- verified
; offline: a GEN01 table with isize larger than the source file's
; remaining data zero-pads, does not error/wrap; see verify_ir.json).
;
; liveconv (not ftconv/pconvolve) chosen because: (a) its IR source is an
; existing FTABLE (ift, i-rate) -- exactly like ftconv/GEN01 -- so the
; "preload all 25 IRs at boot, no disk I/O on switch" design is unchanged;
; pconvolve instead re-reads a raw soundfile from disk at init on every
; variant switch (Csound source, Opcodes/ugens9.c: ifilcod is opened via
; csound->sndgetset, no ftable path), a regression this file's own header
; explicitly rules out. (b) liveconv's kupdate mechanism reloads a
; table's CONTENT into an ALREADY-RUNNING convolution engine partition-by
; -partition, without reinit -- so the currently-ringing tail is not
; truncated on a variant switch (unlike ftconv's i-time table swap,
; exactly the "brief reverb dropout" the header calls out at line 69-73).
;
; STAGGER MECHANISM -- IMPORTANT, verified offline (see verify_ir.json):
; the 4 lanes' internal 256-sample partition-boundary "burst" timing is
; staggered by SKIPPING each lane's liveconv call entirely (not calling
; the opcode at all, NOT feeding it a delayed signal) for the first
; 0/1/2/3 k-cycles of this persistent note's life (see instr 240 below,
; "kwarm"). A delayed-INPUT-SIGNAL approach was tried first (matching the
; original design proposal in fx_ir_evenload.json) and FAILED an offline
; impulse-response verification (1-2% reconstruction error at each lane
; seam) because liveconv's internal burst fires from a per-PERFORM-CALL
; elapsed-sample counter that is gated purely by how many times the
; opcode itself has been called, NOT by the value of the samples flowing
; through it -- delaying only the input data cannot phase-shift that
; counter when all lane opcodes are instantiated at the same i-time.
; Skipping the CALL itself (which Csound's if/then genuinely omits that
; k-cycle, not merely masks) does phase-shift it, verified via the same
; offline impulse test reconstructing to within float-precision noise
; (~1e-16) against scipy.signal.fftconvolve on every row/variant tested.
;
; giIRLaneChunk = fixed per-lane segment length, samples (1.0 s). Must
; satisfy giIRLaneChunk*giIRLaneCount >= the longest shipped IR (chamber,
; 192000 frames) -- keep this in sync if a longer IR is ever added.
giIRLaneChunk  = 48000
giIRLanePart   = 256      ; liveconv iplen per lane -- must be a power of
                          ; 2 (liveconv requirement) and a multiple of
                          ; ksmps (64).
giIRLaneCount  = 5        ; BURST-BOUNDED NON-UNIFORM PARTITIONS (2026-09-25)
; ---- BURST-BOUNDED NON-UNIFORM PARTITIONED CONVOLUTION ----------------------
; Measured with the host's own per-callback histogram (budget 2.67 ms at
; quantum 128): mean load 50% but p99 119%, max 261%, 2.3% of callbacks over
; budget - each one a dropped buffer, i.e. the "deadline" clicks. Average
; load was never the problem; single-callback BURSTS are. A partitioned
; convolver does one segment's ENTIRE work (N complex MACs + an FFT) in the
; callback where its partition completes, so the burst size is the segment
; length, not the partition size. The previous non-uniform layout put a
; 143360-sample tail in ONE lane - three times the old design's peak.
;
; This layout keeps the cheap average of large partitions but caps every
; burst at the size the original 4-lane design already lived with:
;   lane 0  IR[     0: 12288]  P= 256   48 partitions   burst ~12k CMAC
;   lane 1  IR[ 12288: 57344]  P=1024   44 partitions   burst ~45k CMAC
;   lane 2  IR[ 57344:102400]  P=1024   44 partitions
;   lane 3  IR[102400:147456]  P=1024   44 partitions
;   lane 4  IR[147456:192512]  P=1024   44 partitions
; ~224 CMAC/sample/channel (was ~750 uniform), peak burst ~45k (was ~48k).
; The host runs TWO k-cycles per callback, so the stagger is chosen per
; CALLBACK: lane 0 bursts on even callbacks; tail lanes burst every 8
; callbacks on callbacks 1,3,5,7 (k-cycle skips 2,6,10,14) - no two bursts
; ever share a callback. Wet predelay stays exactly 256/sr; dry path untouched.
giIRSegOff[]  fillarray 0, 12288, 57344, 102400, 147456
giIRSegLen[]  fillarray 12288, 45056, 45056, 45056, 45056
giIRSegPart[] fillarray 256, 1024, 1024, 1024, 1024
                          ; = 4) -- that equality is what makes the 4
                          ; lanes' staggered bursts exactly fill (not
                          ; exceed, not leave idle) the 4 ksmps sub-blocks
                          ; per partition period with zero collisions.

; 200 source tables: 25 files x 4 lanes x 2 channels (L,R). Every table is
; forced to the SAME size (giIRLaneChunk) via GEN01's isize argument, so
; tablecopy (used in instr 240, on every variant switch) never hits its
; wrap-and-repeat behavior for a short source -- GEN01 zero-pads instead
; (verified offline on this exact Csound 6.18 build).
; Flat index = fileIdx*8 + lane*2 + channel (channel: 0=L, 1=R).
giIRTabLane[] init 250
iIRi = 0
while (iIRi < 25) do
  SIRPath sprintf "%s/%s", gSIRDir, gSIRFiles[iIRi]
  iLane = 0
  while (iLane < giIRLaneCount) do
    iSkipSec = giIRSegOff[iLane] / sr
    giIRTabLane[iIRi*10 + iLane*2 + 0] = ftgen(0, 0, giIRSegLen[iLane], -1, SIRPath, iSkipSec, 0, 1)
    giIRTabLane[iIRi*10 + iLane*2 + 1] = ftgen(0, 0, giIRSegLen[iLane], -1, SIRPath, iSkipSec, 0, 2)
    iLane += 1
  od
  iIRi += 1
od

; 8 persistent, MUTABLE working tables (4 lanes x 2 channels) that the
; (never-reinited) liveconv calls in instr 240 stay bound to for the life
; of the process. Variant switches copy new data INTO these via
; tablecopy -- the liveconv opcode instances themselves are never
; reinit'd, so their running convolution state (the ringing tail)
; survives the switch.
giIRWorkTabL[] init 5
giIRWorkTabR[] init 5
iLane = 0
while (iLane < giIRLaneCount) do
  giIRWorkTabL[iLane] = ftgen(0, 0, giIRSegLen[iLane], -2, 0)
  giIRWorkTabR[iLane] = ftgen(0, 0, giIRSegLen[iLane], -2, 0)
  iLane += 1
od

; lanes actually used per IR, counted once at boot (short IRs skip empty lanes)
giIRLanesUsed[] init 25
iIRu = 0
while (iIRu < 25) do
  iUsed = 1
  iLu = 0
  while (iLu < giIRLaneCount) do
    iAny = 0
    iTabL = giIRTabLane[iIRu*10 + iLu*2 + 0]
    iTabR = giIRTabLane[iIRu*10 + iLu*2 + 1]
    iK = 0
    while (iK < giIRSegLen[iLu] && iAny == 0) do
      if (abs(tab_i(iK, iTabL)) > 0.00001 || abs(tab_i(iK, iTabR)) > 0.00001) then
        iAny = 1
      endif
      iK += 64
    od
    if (iAny == 1) then
      iUsed = iLu + 1
    endif
    iLu += 1
  od
  giIRLanesUsed[iIRu] = iUsed
  iIRu += 1
od

;--------------------------------------------------------------
; instr 240 — IR convolution engine (event-driven via FX_BYPASS)
; Reads:  gaIRSendL/R      Writes: gaIRWetL/R (already-mixed)
;--------------------------------------------------------------
instr 240
  ; EVEN-LOAD REWRITE (2026-08-21, corrected after offline verification --
  ; see verify_ir.json): 4 staggered liveconv lanes per channel replace
  ; the single ftconv call. See the giIRLaneChunk/giIRTabLane preload
  ; block above for the "why liveconv, why 4 lanes, why THIS stagger
  ; mechanism" rationale. Per-lane budget at ksmps=64 (worst case,
  ; chamber's 4.0s IR), measured offline (20s continuous render, Pi 5):
  ; ~33% of one core average, spread evenly across every block by
  ; construction (see header) rather than concentrated in a periodic
  ; burst -- comfortably inside the 1.333ms/block budget alongside the
  ; engine's ~40%-of-core baseline.
  ;
  ; NO reinit anywhere below: the i-time block loads the CURRENTLY active
  ; row/variant once at note-start; `tablecopy` (k-rate) handles every
  ; LATER variant switch by overwriting the 8 working tables' CONTENT --
  ; the liveconv opcode instances are never reinit'd/reset, so the
  ; currently-ringing tail is not truncated (unlike the old ftconv reinit
  ; at IR_LOAD).
  ;
  ; ---- k-rate row/variant load: fires on this note's very first
  ; k-cycle (kIRFirst) AND on every later row/variant change (kIRChg).
  ; MUST be genuinely k-rate: tablecopy's opcode signature is
  ; (kdft, ksft) -- Csound compiles a call with pure i-rate arguments
  ; without error, but silently never executes it (tablecopy has no
  ; init-time implementation), which was the root cause of IR reverb
  ; being permanently silent on every activation (2026-08-21 bug; the
  ; old "one-time i-time load" block below used i-rate args and was a
  ; total no-op -- giIRWorkTabL/R stayed all-zero forever, so liveconv
  ; convolved against silence). Root-caused by comparing table content
  ; before/after an i-rate tablecopy call (unchanged, still zero)
  ; against the same call made with k-rate arguments (correctly copied
  ; real IR data) -- see fx_ir_mute_rootcause notes.
  ; ----
  kIRRow  limit gkIRActiveRow, 0, 4
  ; clamp the restored/remembered variant to the row's real range — the old
  ; i-time block did this and the even-load rewrite dropped it; a stale
  ; gkIRSel from the settings ftab would index giIRTabLane out of bounds.
  kIRSel  limit gkIRSel[kIRRow], 0, giIRRowCount[kIRRow] - 1
  kIRChg  changed2 kIRRow, kIRSel   ; reports 0 on this note's very first
                                    ; k-cycle -- covered by kIRFirst below
  kIRFirst init 1
  ; 2026-09-01 — hold LENGTHENED 225 -> 960 and a restart gap added. Two
  ; bugs in the original even-load reload, both audible as "the wet is
  ; permanently 1-3 s behind" after switching IRs:
  ;   1. liveconv consumes ONE partition per PARTITION PERIOD (iplen/ksmps
  ;      = 4 k-cycles), not one per k-cycle. A full 48000-sample lane is
  ;      188 partitions = ~752 k-cycles, so a 225-cycle hold swapped only
  ;      ~0.3 s of each lane; everything past that kept the PREVIOUS IR's
  ;      partitions. With lanes 1-3 hard-delayed 1/2/3 s, leftovers from a
  ;      long IR (chamber/hall) replayed seconds late — permanently.
  ;   2. back-to-back switches re-armed the hold while kupdate was still 1,
  ;      so liveconv never saw a fresh rising edge and continued its update
  ;      mid-sequence over freshly-swapped tables (partitions rotated in
  ;      time). The one-cycle gap below guarantees an edge per switch.
  kIRUpdHold init 0
  kIRUpdGap  init 0
  if (kIRChg == 1 || kIRFirst == 1) then
    kFlat = giIRRowBase[kIRRow] + kIRSel
    kLane = 0
    while (kLane < giIRLaneCount) do
      tablecopy giIRWorkTabL[kLane], giIRTabLane[kFlat*10 + kLane*2 + 0]
      tablecopy giIRWorkTabR[kLane], giIRTabLane[kFlat*10 + kLane*2 + 1]
      kLane += 1
    od
    kIRUpdGap  = (kIRUpdHold > 0 ? 1 : 0)  ; force kupdate low for one cycle
    kIRUpdHold = 4     ; 2026-09-01: one partition period. The old 960 was a
                       ; margin against a "liveconv loads one partition per
                       ; k-cycle" theory that does NOT hold on this build
                       ; (measured: 4 and 960 settle to the identical response),
                       ; and a shorter window means less to mask below.
    kIRSwFade  = 0     ; 2026-09-01: duck the WET during the swap — tablecopy
                       ; overwrites liveconv's source table mid-convolution, so
                       ; the old ring history convolved against the new IR dumps
                       ; a burst that peaked at full scale (measured: clip +
                       ; thousands of out-of-range samples) for ~0.4 s: this is
                       ; the "changing the IR tweaks out for a few seconds then
                       ; works". The dry path is untouched (fade multiplies only
                       ; the convolution output), so the reverb briefly dips
                       ; instead of screeching.
    kIRSwHold  = 0.40 * kr   ; hold wet silent while the burst passes
                             ; (measured: the swap burst runs ~0.35 s before
                             ; the new IR's response settles)
    kIRFirst = 0
  endif
  kIRUpd = 0
  if (kIRUpdGap > 0) then
    kIRUpdGap = kIRUpdGap - 1
  elseif (kIRUpdHold > 0) then
    kIRUpd = 1
    kIRUpdHold = kIRUpdHold - 1
  endif

  ; wet-duck envelope: 0 while kIRSwHold counts down (silent through the swap
  ; burst), then a 60 ms portk ramp back to unity. Steady state = 1.
  kIRSwFade  init 1
  kIRSwHold  init 0
  kIRSwTgt   = 1
  if (kIRSwHold > 0) then
    kIRSwHold = kIRSwHold - 1
    kIRSwTgt  = 0
    kIRSwFade = 0            ; hard-hold at 0 during the burst (no ramp-down lag)
  endif
  ; portk smooths the UP-recovery (0->1) over 60 ms; the DOWN edge is a
  ; hard 0 (set above) so no old-tail energy leaks through a ramp. Applied as
  ; a k-rate multiply on the a-rate wet below (constant across each block =
  ; clean step down, and the reverb rebuild masks the block-stepped recovery).
  kIRSwFade  portk kIRSwTgt, 0.06

  aIRInL  = gaIRSendL
  aIRInR  = gaIRSendR
  ; TRUE STEREO (2026-09-01). This used to mono-sum the input
  ;   aIRMono = 0.5*(aIRInL + aIRInR)
  ; and convolve that ONE signal against both IR channels — so it produced a
  ; stereo tail (the IR's L/R are decorrelated) but DISCARDED the player's
  ; input stereo image. The IR files are genuine stereo captures (measured L/R
  ; correlation 0.0-0.2 on hall/plate/chamber), so feeding the matching input
  ; channel to each IR channel preserves the incoming stereo AND keeps the
  ; stereo IR — at ZERO extra cost (same 8 liveconv calls). Dual-mono form (no
  ; L<->R cross-feed), which is the standard cheap true-stereo convolution.

  ; 20 ms fade-in still masks the initial engage transition (the
  ; separate ~300ms progressive kupdate load fills in the quieter,
  ; less-noticeable distant tail on top of this, partition-by-partition,
  ; nearest/most-audible reflections first, on every LATER switch)
  kIRFade linseg 0, 0.02, 1, 1, 1

  ; ---- 4 lanes, each its own liveconv call, staggered via a one-time
  ; boot warmup-skip: lane k's liveconv call is not invoked at all for
  ; this note's first k k-cycles, so its internal 256-sample partition-
  ; boundary counter starts k cycles behind lane 0's, permanently -- this
  ; spreads the 4 lanes' FFT-partition bursts across 4 different k-cycles
  ; instead of all colliding on the same one every 4th block. ----
  ; lanes the ACTIVE IR fills (giIRLanesUsed); a lane coming back into use
  ; gets one kclear pulse so it starts from silence, not its old contents.
  kIRFlatU = giIRRowBase[kIRRow] + kIRSel
  kIRLanes = giIRLanesUsed[kIRFlatU]
  kIRLanesPrev init 0
  kIRLaneClr = (kIRLanes > kIRLanesPrev ? 1 : 0)
  kIRLanesPrev = kIRLanes

  ; warm-up skips place each lane's partition burst on its own CALLBACK
  ; (2 k-cycles per callback): head on even callbacks, tails on 1,3,5,7.
  kwarm init 0
  aWetL0 liveconv aIRInL, giIRWorkTabL[0], 256, kIRUpd, 0
  aWetR0 liveconv aIRInR, giIRWorkTabR[0], 256, kIRUpd, 0
  if (kwarm >= 2 && kIRLanes > 1) then
    aWetL1 liveconv aIRInL, giIRWorkTabL[1], 1024, kIRUpd, kIRLaneClr
    aWetR1 liveconv aIRInR, giIRWorkTabR[1], 1024, kIRUpd, kIRLaneClr
  else
    aWetL1 = 0
    aWetR1 = 0
  endif
  if (kwarm >= 6 && kIRLanes > 2) then
    aWetL2 liveconv aIRInL, giIRWorkTabL[2], 1024, kIRUpd, kIRLaneClr
    aWetR2 liveconv aIRInR, giIRWorkTabR[2], 1024, kIRUpd, kIRLaneClr
  else
    aWetL2 = 0
    aWetR2 = 0
  endif
  if (kwarm >= 10 && kIRLanes > 3) then
    aWetL3 liveconv aIRInL, giIRWorkTabL[3], 1024, kIRUpd, kIRLaneClr
    aWetR3 liveconv aIRInR, giIRWorkTabR[3], 1024, kIRUpd, kIRLaneClr
  else
    aWetL3 = 0
    aWetR3 = 0
  endif
  if (kwarm >= 14 && kIRLanes > 4) then
    aWetL4 liveconv aIRInL, giIRWorkTabL[4], 1024, kIRUpd, kIRLaneClr
    aWetR4 liveconv aIRInR, giIRWorkTabR[4], 1024, kIRUpd, kIRLaneClr
  else
    aWetL4 = 0
    aWetR4 = 0
  endif
  kwarm = (kwarm < 15 ? kwarm + 1 : 15)

  ; REALIGNMENT: a partitioned convolver's output emerges P samples after its
  ; input (measured). Head emerges 256 late; lane k needs O_k + 256 - 1024 more.
  aIRL1d delay aWetL1, 11520 / sr
  aIRR1d delay aWetR1, 11520 / sr
  aIRL2d delay aWetL2, 56576 / sr
  aIRR2d delay aWetR2, 56576 / sr
  aIRL3d delay aWetL3, 101632 / sr
  aIRR3d delay aWetR3, 101632 / sr
  aIRL4d delay aWetL4, 146688 / sr
  aIRR4d delay aWetR4, 146688 / sr
  aIRWetL = aWetL0 + aIRL1d + aIRL2d + aIRL3d + aIRL4d
  aIRWetR = aWetR0 + aIRR1d + aIRR2d + aIRR3d + aIRR4d

  ; REMAINING KNOWN ENGAGE COST (unfixed, deliberately, because the fix is not
  ; in this file): the eight liveconv instances above are created at note-init
  ; and each allocates its own IR spectrum store -- 188 partitions x 512
  ; doubles ~= 770 KB apiece, ~6 MB total -- plus FFT setup, all on the audio
  ; thread in the k-period in which $FX_BYPASS schedules instr 240. Nothing
  ; inside instr 240 can avoid that while the instrument is torn down on
  ; bypass. The structural answer is an always-on instr 240 gated by
  ; gkIRBypass instead of an FX_BYPASS event, which is a signal_chain.orc
  ; change. If a click is heard specifically at the moment IR reverb is
  ; switched ON (and not while it plays), that is this, and this is where to
  ; go next.

  ; master wet tone: gkIRDamp 0..1 -> 1.2 kHz .. 16 kHz lowpass (unchanged)
  kIRDampC limit gkIRDamp, 0, 1
  kIRLpHz  = 1200 * exp(2.5903 * kIRDampC)   ; ln(16000/1200)=2.5903
  aIRWetL  tone aIRWetL, kIRLpHz
  aIRWetR  tone aIRWetR, kIRLpHz

  ; per-row dry/wet, master level — write the finished blend.
  ; 2026-09-01: portk smoothing added (this was the ONLY stage in the chain
  ; applying a raw k-rate mix — every encoder detent was an instantaneous
  ; 2% gain step on both dry and wet, i.e. a click per detent: the
  ; "crackling when changing mix"). 40 ms tracks the knob tightly but turns
  ; the steps into ramps. Row changes glide through it too, harmlessly.
  ; giIRGain[]: per-IR energy trim (see the table above) so the wet sits at
  ; the natural volume of the dry guitar on every IR.
  kIRMixT limit gkIRRowMix[kIRRow], 0, 1
  kIRMix  portk kIRMixT, 0.04
  kFlatG  = giIRRowBase[kIRRow] + kIRSel
  ; 2026-09-24: gkIRLevel GETS THE SAME TREATMENT AS kIRMix, and for the same
  ; reason. The note above fixed the raw-k-rate-gain click for kIRMix and
  ; stopped there -- but gkIRLevel multiplies the very same a-rate wet one
  ; line later and was still applied raw, so every change to it was an
  ; instantaneous gain step at a block boundary. Measured on the rig with
  ; crackle_detect.py: IR off = 0 clicks/20 s, IR on = 7 clicks/30 s, xruns 0
  ; and busy/quantum 0.44 both times -- a DSP discontinuity, not CPU.
  ;
  ; This matters more than a knob: the EXPRESSION PEDAL targets reverb, so a
  ; pedal sweep rewrites this gain every k-cycle and emits a step per block
  ; for the whole sweep. 40 ms matches kIRMix's constant, so level and mix
  ; track each other instead of arriving at slightly different times.
  kIRLevelS portk gkIRLevel, 0.04
  kIRWetG = kIRMix * kIRLevelS * kIRFade * giIRGain[kFlatG]

  ; ---- A-RATE THE GAINS (2026-09-24) --------------------------------------
  ; portk alone was NOT enough, and measurement showed it made things worse:
  ; smoothing gkIRLevel at k-rate took the click count from 7/30 s to 47/30 s.
  ;
  ; The reason is that a K-RATE gain multiplied onto an A-RATE signal is
  ; piecewise constant - it holds for all 64 samples of a block and then jumps
  ; at the boundary. Smoothing does not remove those jumps, it REPLACES one
  ; big jump with ~30 small ones (one per k-cycle of the ramp). More edges,
  ; not fewer. That is why the 2026-09-01 portk on kIRMix reduced the
  ; "click per detent" without ever curing it.
  ;
  ; interp() converts a k-rate value to a-rate by linearly interpolating
  ; BETWEEN k-values across the block, so the gain becomes continuous and no
  ; block boundary carries a step at all. This is the fix the portk was
  ; standing in for.
  aIRWetGA  interp kIRWetG
  aIRMixA   interp kIRMix
  aIRSwFadeA interp kIRSwFade
  ; aIRSwFade ducks ONLY the convolution output through an IR swap; the dry
  ; blend (aIRIn * (1-mix)) is untouched, so the signal never fully drops.
  gaIRWetL = aIRInL * (1 - aIRMixA) + aIRWetL * aIRWetGA * aIRSwFadeA
  gaIRWetR = aIRInR * (1 - aIRMixA) + aIRWetR * aIRWetGA * aIRSwFadeA
endin
