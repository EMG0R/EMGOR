# mic-pre — a new effect, deliberately not the preamp

Status: spec, nothing built. For the **New York** set. Date: 2026-10-07.

## The hard fact first, because it changes the wiring plan

**The Raspiaudio board has no input gain, no mic preamp, and no phantom/bias
power.** Measured on gorpi7v4, not assumed:

    amixer -c0 controls  ->  0 controls
    driver               ->  snd_rpi_hifiberry_dac8x / snd-soc-dummy-dai

Zero ALSA controls means there is no analog gain stage to turn up. The inputs are
**line level**. Therefore:

- **Both mics are clip-on**, so both are almost certainly **electret** and need
  **2-5 V bias** to work at all. The board supplies none. Wired straight to a
  TRS input they are silent or near-silent.
- Even with bias, a clip-on sits far below line level - the gap is tens of dB.
- So the requirement is **two channels** of mic preamp with bias/phantom, not
  one. That is the number to size the New York rig around.

**`mic-pre` cannot fix this and must not pretend to.** Digital gain after the ADC
lifts the signal and the converter's noise floor by exactly the same amount; 50 dB
of it is unusable hiss. **Gain staging has to be analog, ahead of the ADC.** Any
plan that skips that is a plan that fails on stage.

### Wiring, with no purchase
The zero-cost path is the mixer EMGOR already owns: mic into the mixer's XLR mic
input, mixer line out into the board's TRS input. The mixer supplies the 40-55 dB
and, for a condenser, the phantom power. The board then sees healthy line level
and `mic-pre` does what it is actually for — **tone, not gain.**

If a mixer on stage is unwanted, the category needed is a small 1-2 channel mic
preamp, or a mic with an inline powered adapter. **Links only; never buy.**

## What mic-pre IS

A **tone and character** stage for microphone sources, entirely separate from
`effects/preamp.orc`. The existing preamp family — bass radio pair, Marshall,
dual high-gain, bass distortion — is a **guitar/bass** lineage and stays that way.

**Hard requirement: the old preamp never starts on, and never touches the mic
path.** For guitar, NAM handles amp character. `mic-pre` neither reuses nor
re-enables those toggles.

| source | mic | low shelf | tube character |
|---|---|---|---|
| vocal | **clip-on** | **mad subtle** | moderate |
| trumpet | **clip-on** | **mad subtle** | moderate |
| PMOR / pocket operator | - | **none** | **none - fully dry** |

Both mics are **clip-on**, so both get the same treatment class. They do **not**
share a chain - see the stereo pairing below.

PMOR stays dry on purpose. It has no tube-mic quality to flatter and nothing to
warm; colouring it would only muddy a part that works because it is clean.
Enabling the other stages must cost the PMOR track nothing.

### Signal design
- **Low shelf:** gentle, well below either source's fundamental. Subtle enough
  that bypassing it is hard to hear in isolation and only reads in the mix. A
  shelf, not a bump - no resonance.
- **Tube character:** soft asymmetric saturation with a little even-harmonic
  content, deep enough to flatter a close-mic'd voice or trumpet without audible
  distortion. Moderate on both.
- **Unity loudness across settings**, following the `giPreVoice` convention in
  `effects/preamp.orc`, so changing character never changes level.
- **Default OFF**, and off is bit-identical bypass, not a null-gain path.

### Answered
"moderately ..." resolved: both sources are clip-on mics, treated alike -
**mad-subtle shelf, moderate tube.**

## Channel unification: uniform strips, unique contents, stereo pairs

EMGOR: *"stereo vibe w unique stereo chains paired."*

So each source is **mono in -> its own unique stereo chain -> a dedicated output
pair.** Uniform in shape, unique in contents. That is what "all channels unified
in the interface" means: every source presents the same strip, and no two strips
contain the same processing.

The 8-in/8-out board makes this expressible for the first time; at 2 channels it
was not. Proposed allocation, 4 stereo pairs:

    out 1/2   guitar            NAM owns the amp character; preamp family off
    out 3/4   vocal clip-on     mic-pre + its own stereo chain
    out 5/6   trumpet clip-on   mic-pre + a DIFFERENT stereo chain
    out 7/8   PMOR              dry + glitch, no mic-pre

Inputs used: guitar, vocal, trumpet, PMOR L/R = 5 of 8. Three spare.

"Paired" is the load-bearing word: the two mic chains are **siblings, not clones.**
Same strip, same controls, independently voiced - a trumpet and a voice through
identical processing would collapse into each other in the mix, which is the
thing a unique chain per source exists to prevent.

## The PMOR track: dry + glitch, nothing else

Corrected from the dictation: it is **glitch**, not "bridge". So:

**PMOR gets the glitch block and nothing else.** No shelf, no tube, no mic-pre.
The glitch is "the stuff on top" — it sits over an otherwise dry PMOR signal,
which is the whole point: the pocket operator has no tube-mic quality to flatter,
and glitch reads best against something clean.

**Enabling the other stages must cost the PMOR track nothing.** A PMOR-only
configuration should be cheap enough to be practical on its own, so the mic-pre
and the rest cannot sit in its path even bypassed. That argues for a real
per-channel strip, not one chain with everything bypassed in it.

### Re-architecting the glitch block
There is already a **GLITCH v2 contract** (`signal_chain.orc` 19.2: "the whole
glitch block now mangles loops + live", see `stutter.orc` INTEGRATION NOTES item
A), and a glitch family spread across `stutter.orc`, `granular.orc`,
`freezeverb.orc`, `bincrush.orc`, `bincrusher.orc` and
`bincrusher_v1_coarsen.orc`.

**The first thing re-architecture should fix is that there are three bincrush
files for one job:**

    bincrush.orc                 referenced 1x   (live)
    bincrusher.orc               referenced 1x   (live)
    bincrusher_v1_coarsen.orc    referenced 0x   (dead)

Two live crushers and one dead one is exactly the "two mechanisms for one job"
shape to consolidate before building anything new, and the dead file belongs in
`_attic/` rather than deleted. Consolidate first, then extend — otherwise the
new glitch work inherits the ambiguity about which crusher is the real one.

## Not specified here
Nothing. The earlier open item ("bridge stuff") was a dictation error for
glitch and is now specified above. One item still needs EMGOR: the cut-off
"on trumpet and vocals I want moderately ..." line.

## Privacy
This file stays **private**. Instrument tone, menu and parameter detail is exactly
the material that leaked publicly twice this week. It is not in
`setup-script/public-manifest.txt` and must not be added.

---

## Glitch: the redo IS happening, and GLITCH3 is it

**Superseded 2026-10-09.** The section this replaces said the glitch redo was
deferred and that nobody should touch glitch behaviour meanwhile. Both are now
wrong: EMGOR asked for the rebuild in NEPTR's window on the night of 10-08/09,
and **GLITCH3 is that rebuild**, not the old block.

Spec: `gorpi7v4:~/NEPTR_mp_work/glitch3/SPEC.md`. CHOP is killed and replaced by
**PITCH-ROLL**; generative patterns; **per loop-channel instances**. From EMGOR:
"glitch menu is cool but unsatisfactory... per loop channel... replace the one
that feels like a fast amp mod... crazy epic upgrade."

So work on GLITCH3 lands *in* the rebuild and is not throwaway. Its
vectorisation measured 0.80 -> 0.25 s per 20 s (3.2x, +4.0% of the 13.67 s
baseline) and counts.

### What is still legacy, and still subject to the original argument
The retired advice applies only to glitch code the rebuild does NOT absorb. The
**bincrush family** is the open case — `bincrush.orc` and `bincrusher.orc` are
both live and `bincrusher_v1_coarsen.orc` is referenced nowhere. The GLITCH3
spec does not mention bincrush, so until it is known whether the rebuild keeps
them, neither consolidating nor vectorising them is clearly worth doing. Ask
before spending a window on it; the dead file can go to `_attic/` regardless.
