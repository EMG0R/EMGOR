/* demiurge_params.udo -- join the Demiurge parameter pool from Csound.
   Contract: docs/parameters.md (§1 0..1 floats, §11 two-way). #include this file.

     dparam_init Sstage, iport       ; once, in instr 0 / an always-on instr (i-time)
     kv dparam "/stage/name", idefault   ; k-rate 0..1, updated by /p from the pool
     dpout "/stage/name", kv         ; sends /pout <path> <val> whenever kv changes

   Design: ONE OSClisten call site (instr DParamRx, one message per k-cycle: keep ksmps <= 64) fills a
   global slot table; every dparam reads its slot. (Several OSClisten instances on the same
   handle/address do not all receive the message, so per-dparam listeners do not work.)

   Config (Csound has no getenv opcode, so the launcher maps env -> macros):
     iport   argument of dparam_init, if > 0 (normally the manifest `port`)
     else    --omacro:DEMIURGE_PARAM_PORT=N   else global giDParamPort
     pool    --omacro:DEMIURGE_POOL_HOST='"127.0.0.1"'  --omacro:DEMIURGE_POOL_PORT=9102
             (these are the defaults). The HOST macro is pasted as Csound source, so its
             value must CONTAIN the double quotes: in a shell '"10.0.0.2"' (single outside,
             double inside); a bare 10.0.0.2 is a syntax error. The launcher does this for you.

   ORDER: this file defines `giDParamPort init 0`, so set your own value AFTER the #include
   (Csound cannot test whether a global already exists, so it cannot be preserved from before).
   dparam_init reads it at i-time, after the whole header ran, so after-the-include always works.
   `#define DEMIURGE_PARAM_PORT #N#` / `giDParamPool init M` follow the same rule: macros go
   BEFORE the include, the gi variables AFTER it.
   Demiurge's launcher passes these from DEMIURGE_PARAM_PORT / DEMIURGE_POOL_HOST /
   DEMIURGE_POOL_PORT; a hand-run csound can pass them itself.
*/
#ifndef DEMIURGE_PARAM_PORT
#define DEMIURGE_PARAM_PORT #0#
#end
#ifndef DEMIURGE_POOL_HOST
#define DEMIURGE_POOL_HOST #"127.0.0.1"#
#end
#ifndef DEMIURGE_POOL_PORT
#define DEMIURGE_POOL_PORT #9102#
#end

giDParamPort    init 0              ; to override: set AFTER the #include (or pass iport / the macro)
giDParamHandle  init -1
giDParamMax     init 256
giDParamN       init 0
giDParamPool    init $DEMIURGE_POOL_PORT
gSDParamHost    init $DEMIURGE_POOL_HOST
gSDParamStage   init ""
gSDParamPaths[] init 256
giDParamTab     ftgen 0, 0, 256, 2, 0     ; slot -> current 0..1 value

opcode dparam_init, 0, Si
  Sstage, iport xin
  if giDParamHandle >= 0 goto done          ; open once
  iuse = (iport > 0 ? iport : ($DEMIURGE_PARAM_PORT > 0 ? $DEMIURGE_PARAM_PORT : giDParamPort))
  if iuse <= 0 then
    prints "dparam_init: no port (pass iport, --omacro:DEMIURGE_PARAM_PORT=N or set giDParamPort AFTER the #include)\n"
    goto done
  endif
  giDParamHandle OSCinit iuse
  gSDParamStage = Sstage
  schedule "DParamRx", 0, -1
  done:
endop

; the single receiver: /p <path:s> <val:f> -> slot table
instr DParamRx
  Sp init ""
  kval init 0
  kans OSClisten giDParamHandle, "/p", "sf", Sp, kval
  if kans == 1 then
    kj = 0
    while kj < giDParamN do
      if strcmpk(gSDParamPaths[kj], Sp) == 0 then
        tabw (kval < 0 ? 0 : (kval > 1 ? 1 : kval)), kj, giDParamTab
        kj = giDParamMax
      endif
      kj += 1
    od
  endif
endin

opcode dparam, k, Si
  Spath, idefault xin
  islot = -1
  ij = 0
  while ij < giDParamN do
    if strcmp(gSDParamPaths[ij], Spath) == 0 then
      islot = ij
    endif
    ij += 1
  od
  if islot < 0 then
    islot = giDParamN
    gSDParamPaths[islot] = Spath
    tabw_i idefault, islot, giDParamTab
    giDParamN = giDParamN + 1
  endif
  kix = islot
  kv tab kix, giDParamTab
  xout kv
endop

opcode dpout, 0, Sk
  Spath, kv xin
  ktrig changed kv
  ; OSCsend fires when kwhen rises above its last value, so it must be called
  ; every cycle (ktrig drops back to 0 between changes), never inside an if.
  OSCsend ktrig, gSDParamHost, giDParamPool, "/pout", "sf", Spath, kv
endop
