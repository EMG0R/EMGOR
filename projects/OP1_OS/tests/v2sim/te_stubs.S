/* Stand-ins at TE addresses for the OP FREAK v2 integration bench (assembled per address, -defsym WHICH=n).
 * Each forwards to a C function in driver.c (same ABI; P1 is scratch). */
        .section .text
        .global _stub
_stub:
.if WHICH == 0                  /* 0x0197c870 drwave voice render(self, out, knobs) */
        P1.L = DRWAVE_RENDER;  P1.H = DRWAVE_RENDER;
        JUMP (P1);
.endif
.if WHICH == 1                  /* 0x0197a826 drwave voice note_on */
        P1.L = DRWAVE_NOTE_ON;  P1.H = DRWAVE_NOTE_ON;
        JUMP (P1);
.endif
.if WHICH == 2                  /* 0x01a00d14 patch set knobs(patch, knobs) */
        P1.L = LOG_SAVE;  P1.H = LOG_SAVE;
        JUMP (P1);
.endif
.if WHICH == 3                  /* 0x01a85750 VoiceCtl note on(te_note*, extra) */
        P1.L = LOG_NOTE_ON;  P1.H = LOG_NOTE_ON;
        JUMP (P1);
.endif
.if WHICH == 4                  /* 0x01a85840 VoiceCtl note off(te_note*) */
        P1.L = LOG_NOTE_OFF;  P1.H = LOG_NOTE_OFF;
        JUMP (P1);
.endif
