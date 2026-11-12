/*
 * hg_ctrl.h - TRS expression pedal + joystick whammy, and the encoder
 * identification diagnostic.
 *
 * ===================== PIN MAP =====================
 *
 * DIGITAL (Bela digital 0-15)
 *   0  ENC1 CLK      1  ENC1 DT      2  ENC1 SW      (existing)
 *   3  ENC2 CLK      4  ENC2 DT      5  ENC2 SW      (existing)
 *   6  JOY SW        joystick push-button, active low, internal pull-up
 *
 * ANALOG IN (Bela analog 0-7, 0-3.3V)
 *   0  TRS EXPRESSION   tip = wiper
 *   1  JOY X            left/right
 *   2  JOY Y            up/down
 *   3..7 free
 *
 * AUDIO IN (Bela Gem Multi, 10 channels)
 *   0  guitar TRS tip  (mono now, L later)
 *   1  guitar TRS ring (silent with a TS plug, R later)
 *   2  contact mic 1   <-- BROKEN JACK, needs replacing
 *   3  contact mic 2   <-- BROKEN JACK, needs replacing
 *   4..9 hex pickup, per STRING_CHANNEL = { 9, 7, 6, 8, 4, 5 }
 *
 * ===================== WHY THE ARM-AT-ZERO =====================
 *
 * An unplugged TRS jack does not read 0. The input floats, and a floating
 * high-impedance analog pin drifts around mid-scale and jitters - which on a
 * pitch whammy means the guitar sits a random number of semitones sharp and
 * wobbles, with no pedal attached to explain it.
 *
 * So the pedal is DISARMED at boot and does nothing at all until it has been
 * swept down to (near) zero at least once. A floating input never gets there:
 * it hovers mid-scale, so it can never arm itself. A real pedal at heel gets
 * there in the first second of use. One sweep to heel per session is the entire
 * ritual, and it doubles as a calibration of where the player's heel actually
 * is.
 *
 * ===================== WHY THE JOYSTICK IS AN OFFSET =====================
 *
 * The joystick is a WHAMMY BAR, not a parameter knob: it must spring back to
 * exactly where the sound was before it was pushed. So it never writes the
 * pitch or formant value - it writes an OFFSET, and while the stick is centred
 * the base value tracks whatever the pads and encoders are doing. Let go, and
 * you are exactly back where the preset put you, however you got there.
 *
 * The centre deadzone is generous on purpose. A cheap thumbstick does not
 * return to the same ADC value twice, and a whammy that leaves the guitar a
 * few cents sharp when released is worse than one with a slightly wide neutral.
 */
#ifndef HG_CTRL_H
#define HG_CTRL_H

#include "hyperguitar.h"   /* BelaContext + the effect globals we modulate */

/* ---- digital ---- */
#define PIN_JOY_SW 6

/* ---- analog ---- */
#define AIN_EXPR 0
#define AIN_JOY_X 1
#define AIN_JOY_Y 2
#define AIN_POT   3   /* panel pot - was wired but never read until now */

/*
 * Arm threshold. The pedal must read below this once before it does anything.
 * 0.06 of full scale is comfortably below any real heel position and far below
 * where a floating input idles.
 */
#define HG_EXPR_ARM_BELOW 0.06f

/*
 * Joystick centre deadzone, as a fraction either side of the measured centre.
 * 0.12 is wide - see the header note. Below this the offset is exactly 0, not
 * merely small, so a released stick is bit-for-bit the un-modulated sound.
 */
#define HG_JOY_DEADZONE 0.12f

/* ---- JOYSTICK ORIENTATION ----
 * The module is not mounted square to the player. Described by the user:
 * "towards the wires of the joystick is my right, and right of the wires is my
 * down". So the module's axes are rotated a quarter turn relative to the hand:
 * the raw axis that points at the cable is the player's RIGHT, and the raw axis
 * 90 degrees clockwise from it is the player's DOWN.
 *
 * Rather than bake a guess into the maths, the mapping is four switches. If the
 * stick moves the wrong way in practice, flip one - no re-derivation needed.
 *
 *   JOY_SWAP_XY   raw X/Y exchanged (the quarter-turn itself)
 *   JOY_INVERT_X  player-right is the raw axis' negative direction
 *   JOY_INVERT_Y  player-up is the raw axis' negative direction
 *
 * The defaults encode "wires = right, clockwise from wires = down" for the
 * common module wiring; they are a starting point to be confirmed by feel.
 */
#define JOY_SWAP_XY   1
#define JOY_INVERT_X  0
#define JOY_INVERT_Y  1

/* Whammy range. Up/down is +/- an octave; left/right shifts formant ratio. */
#define HG_JOY_PITCH_SEMIS 12.0f
#define HG_JOY_FORMANT_RANGE 0.45f

/* Set from setup(); read every block from render(). */
void hgCtrlSetup(void);
void hgCtrlTick(BelaContext *context);

/*
 * JOYSTICK BUTTON — deliberately UNASSIGNED, and live.
 *
 * It is debounced and decoded here so the switch is known-good and provable
 * from the log, but nothing in this file acts on it. That is on purpose: it is
 * a spare gesture for whatever turns out to be worth reaching for mid-phrase,
 * and guessing a default would just be something to undo later.
 *
 *   gHgJoyClick  ONE-SHOT, set on the press edge. Whoever consumes it MUST
 *                clear it, exactly like gEnc1Click/gEnc2Click in render.cpp.
 *   gHgJoyHeld   LEVEL, 1 while held. Use this for anything momentary (a
 *                latch on the whammy, a kill switch, a hold-to-freeze).
 *
 * Idle polarity is MEASURED at boot rather than assumed active-low, so it
 * works whether the module pulls up or down and whether or not the internal
 * pull-up is doing anything. This mirrors how the rotary encoder switches
 * already handle it (gEnc1Idle/gEnc2Idle) — the same trick, same reason: one
 * miswired switch should not invert the whole gesture.
 */
extern volatile int gHgJoyClick;
extern volatile int gHgJoyHeld;

/*
 * ENCODER IDENTIFICATION. With this on, every encoder movement and click
 * prints which physical encoder produced it, so an unplugged one can be found
 * without opening the box: turn each knob, read the log, the silent one is the
 * one that is unplugged. Cheap enough to leave compiled in - it only prints on
 * actual movement.
 */
extern int gHgEncDiag;

/*
 * TELEMETRY MIRRORS. Every control's current value, exported purely so the web
 * UI can show it. These are read-only snapshots written once per block by
 * hgCtrlTick(); nothing downstream should drive behaviour off them, because
 * the real state lives in the statics they mirror.
 */
extern volatile float gHgExpr;        /* 0..1, expression pedal */
extern volatile int   gHgExprArmed;   /* 0 until swept to heel once */
extern volatile float gHgJoyXv;       /* -1..+1, 0 inside deadzone */
extern volatile float gHgJoyYv;
extern volatile float gHgPot;         /* 0..1, analog 3 */
extern volatile float gHgExprRaw;     /* unsmoothed pedal, for diagnosis */
void hgEncDiag(int mainDelta, int auxDelta, int mainClick, int auxClick);

#endif /* HG_CTRL_H */
