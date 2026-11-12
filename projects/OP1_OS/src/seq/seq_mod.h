/* seq_mod.h - the per-step value stream the generative sequencer publishes for the engines.
 *
 * An engine parameter whose GEN mode is SEQ (PLAN.md control grammar) reads one of these fields
 * once per sequencer step. The struct is refreshed on EVERY step boundary (also when the step is a
 * rest or killed by probability), so a SEQ-mode parameter keeps moving with the clock.
 * Same seed + params + inputs = same stream.
 *
 * Delivery: poll seq_mod_get(host) (copy out; the Seq thread writes it, readers must not hold a
 * pointer across blocks) or register host->mod_cb, called synchronously inside seq_tick_one().
 * All values are unsigned 16-bit fixed point (0..65535 = 0.0..1.0) unless noted.
 */
#ifndef SEQ_MOD_H
#define SEQ_MOD_H
#include <stdint.h>

typedef struct seq_mod {
    uint32_t step;      /* absolute step counter since transport start */
    uint16_t pitch;     /* first note of the step, normalised over the current pitch window
                         * (base+root .. base+root+11+spread semitones). Rests keep the last value. */
    uint16_t rnd;       /* fresh seeded random draw per step (feeds STEP sample&hold style GEN) */
    uint16_t walk;      /* slow seeded random walk, one small move per step (feeds DRIFT style GEN) */
    uint16_t aux;       /* mode specific scalar: TURING register, EUCLID layer hit mask, MARKOV
                         * transition weight of the step, GAMELAN part (0 polos / 1 sangsih),
                         * CELLULAR population, TOTAL breathing density. Scaled to 0..65535 */
    uint16_t phase;     /* position inside the bar (steps per bar = 16 steps of the current RATE) */
    uint16_t energy;    /* gate density over the last 16 steps */
    uint8_t  gate;      /* 1 = at least one note-on was scheduled this step (after probability) */
    uint8_t  vel;       /* velocity of the first note (0..127), 0 on a rest */
    uint8_t  ratchet;   /* sub-hits of this step (1 = none) */
    uint8_t  notes;     /* notes scheduled this step */
    uint8_t  note;      /* MIDI note of the first note (0 on a rest) */
    uint8_t  accent;    /* 1 on the downbeat step of a bar */
} seq_mod;

#endif
