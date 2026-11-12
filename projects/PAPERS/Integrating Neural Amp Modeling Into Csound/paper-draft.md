# Integrating Neural Amp Modeling into Csound

**Abstract.** Neural Amp Modeler (NAM) is the premier open-source system for
the black-box modeling of nonlinear audio hardware using neural networks. A
community library of several hundred thousand pre-trained captures exists,
but no way to use them inside Csound. This paper presents `NAMProcess`, a
Csound opcode that performs real-time NAM inference. The opcode loads
standard `.nam` model files, switches between models during performance
without interrupting audio, adds no latency beyond the block boundary, and
runs comfortably in real time on systems like the Raspberry Pi. This paper
details the implementation of a thin wrapper around the NeuralAmpModelerCore
C++ library and its integration as a native plugin opcode, a Cabbage plugin,
a live dry-referenced volume compensation system, and "tone selection" in a
Raspberry Pi-based guitar pedal.

**Keywords:** Neural Amp Modeling · Volume Compensation · Raspberry Pi ·
Cabbage Plugin · Csound Plugin Opcode

## 1 Introduction

Over the past several years a new approach has become standard practice in
guitar audio: record a real device, then train a neural network to reproduce
its input–output behavior. Neural Amp Modeler (NAM) [1, 8] is the most widely
used open-source implementation of this approach. Its model format has become
a de facto interchange standard with over 350,000 accumulated on TONE3000
alone [12] — covering amplifiers, effect pedals, preamps, and other outboard
hardware, freely downloadable.

`NAMProcess` wraps the existing NeuralAmpModelerCore inference library [8]
rather than reimplementing any DSP. Section 3 covers the opcode's design,
including a loader that never blocks the audio thread, correct handling of
model state in stereo, and measured latency. Section 4 presents a live
volume compensation system that references the player's dry signal,
making heterogeneous community captures usable side by side. Sections 5 and 6 carry the engine into two deployment
targets, a
drag-and-drop Cabbage plugin for DAW hosts and "tone selection" in a
Raspberry Pi-based "guitar pedal."

## 2 Background and Related Work

Black-box neural modeling of guitar amplifiers was established by Damskägg et
al., first against simulated tube-amplifier data [4] and then against real
distortion circuits in real time [5], using a feedforward variant of the
WaveNet convolutional architecture. Wright et al. showed that recurrent
(LSTM) models reach comparable accuracy at lower cost [15], and consolidated
the approach across amplifiers and pedals [14], training with an
error-to-signal ratio loss under a perceptual pre-emphasis filter [16].
Steinmetz and Reiss extended efficient neural modeling to analog dynamic
range compression [11]. NAM [1, 8] packages this family of techniques — its
model format supports WaveNet and LSTM architectures — together with a
training pipeline whose input is a fixed published reference signal reamped
through the target device.

Composers have extended music programming languages from within since the
MUSIC-N family itself — Tenney's instrument building at Bell Labs [17] sits
at the head of the lineage that leads to Csound — and an opcode continues
that practice with a learned model as the unit generator.

Real-time neural inference has precedent in other computer music
environments. Chowdhury's RTNeural [3] provides a general inference library
designed for audio callbacks, and UGens built on it exist for SuperCollider,
with ports to Max/MSP and Pure Data [10]. The `neural_tilde` external loads
`.nam` files directly in Max/MSP [9]. Within Csound the closest prior work is
the component-level triode opcode of Fink and Rabenstein [6], which models
one circuit stage physically rather than capturing a whole device from data.
We present a Csound [7] equivalent not present in the opcode manual, the
plugin-opcode collections, or community archives.

## 3 The NAMProcess Opcode

### 3.1 Interface

```
aoutL, aoutR  NAMProcess  ainL, ainR, kProfileIdx, SLibDir [, kAutoVol, kDriveDb]
icount        NAMCount
```

`SLibDir` names a directory scanned recursively for `.nam` files at
initialization. The sorted file list defines stable indices. `kProfileIdx`
selects the active model at k-rate. Writing a new index triggers an
asynchronous load, and the running model is replaced only when the new one
is ready. `NAMCount` returns the library size so orchestra logic can wrap or
clamp index changes. Because model choice is an ordinary k-rate parameter,
timbre selection is open to the same rule-based and generative processes as
any other Csound value — selection procedures in the sense Koenig practiced
— rather than confined to manual preset browsing. `kAutoVol` enables the volume compensation of Section 4
and defaults to on. `kDriveDb` is a pre-model gain in dB whose loudness cost
the compensation absorbs, making it a pure saturation control. Existing
four-argument call sites compile unchanged. A minimal live instrument:

```
instr 1
  aL, aR  ins
  kIdx    chnget "model"          ; from MIDI, OSC, or score
  aoL,aoR NAMProcess aL, aR, kIdx, "/home/user/models"
  outs aoL, aoR
endin
```

Until the first model finishes loading, and whenever the directory contains
no models, the opcode passes audio through unchanged, so a misconfigured
path degrades to bypass rather than silence.

### 3.2 Loader design

All file and network-weight handling runs on a background thread owned by the
opcode, following standard real-time audio discipline [2]. The audio thread
never allocates, parses JSON, or blocks on I/O. Per k-cycle it takes one
uncontended mutex acquisition to copy a pointer to the active model pair. The
loader thread builds the requested model off the audio thread, selects the
full-quality submodel for slimmable models [1], and pre-warms internal
buffers before publishing the swap. A load failure retains the previous
model, so a corrupt file cannot interrupt a performance. Model switching is
consequently click-safe and non-blocking. The old model runs until the exact
block boundary at which the new pair is published.

When a model's training sample rate differs from the engine rate, the
loader wraps it in a Lanczos resampling container (the same mechanism the
reference plugin uses), so the network always runs at its trained rate.
Measured on a 48 kHz model under a 44.1 kHz engine, the harmonic spectrum
matches the matched-rate reference within 0.3 dB; without the wrapper the
model's learned filtering is stretched by the rate ratio and its whole
voicing shifts audibly.

### 3.3 Model state and stereo processing

NAM models are stateful. WaveNet layers keep per-layer convolution history in
ring buffers, and LSTM models carry hidden state across process calls.
`NAMProcess` loads two independent instances of the same model file, resets
both off-thread, and publishes them atomically as a pair. Each channel owns
one instance. The cost is one extra load per switch and double the model
memory. It buys exact channel isolation. Processing both channels through a
shared instance instead convolves each block against the other channel's
immediate past, which measures as −4.7 dB crosstalk on decorrelated stereo
material.

### 3.4 Registering an a-rate plugin opcode in Csound 6.18

One implementation detail is worth recording for other opcode developers.
Registered conventionally for an audio-rate generator, `OENTRY` with
`thread = 5`, the a-rate callback is never scheduled under Csound 6.18's
plugin registration path and the opcode emits silence with no error. The
working registration is `thread = 3`, computing the full `ksmps` block
inside the k-rate callback. Declaring `outypes = "aa"` still allocates
a-rate outputs.

### 3.5 Latency

Because the supported architectures are causal, the opcode should add no
delay beyond the `ksmps` block granularity that every Csound opcode shares.
Offline impulse renders confirm this. Response onset lies within one sample
of the input impulse. Total added latency is therefore one k-cycle — 128
samples, 2.7 ms at 48 kHz, in the pedal deployment of Section 6.

## 4 Live Volume Compensation

The opcode continuously matches the wet output to the perceived loudness of
the player's dry signal. Load any capture — clean, cranked, mislabeled —
and the volume does not move.

Without this, browsing a community library is unusable live: fed the same
signal, 25 representative captures span about 10 dB, and their embedded
loudness metadata does not predict the ranking (the quietest-tagged capture
measures second-loudest). No pre-computed correction can close the gap,
because saturating models compress: a loudness measurement is only valid
near the input level at which it was taken, and one compression-dominated
capture holds its output within 1 dB across a 14 dB input sweep, so no
per-model gain equalizes it at all. The correction has to be relative to
what the player is actually doing, and it has to be live.

Dry input and wet output are metered as ITU-R BS.1770 K-weighted mean
squares averaged with a 2.5 s time constant, and a slow output trim tracks
their ratio. Program dynamics move both meters together and leave the ratio
untouched, so the mechanism is not a compressor. The result is an
engineered perceptual invariance — loudness held constant while spectrum
and saturation move — the kind of controlled percept Tenney built pieces
around. Measured after convergence,
a −12 dB input step passes through as −10.2 dB of output, the residual being
the amp's own compression. The trim engages after one second of active
signal, slews at most 2 dB/s, holds within a 0.75 dB deadband, freezes below
−55 dBFS input, and is clamped to ±18 dB. Converged trims are remembered per
model file, so returning to a previously played capture lands at level
immediately (+0.6 dB measured, inside the deadband). A load-time probe
seeds the trim for unseen models; the live loop owns the result.

Because the dry reference is tapped before the input stage, `kDriveDb`
pushes the model into saturation without a level change. +18 dB of drive
measured +0.1 dB of loudness and −4.6 dB of crest factor. Across the
25-capture library on identical material, the best static probe left a
9.9 dB spread (standard deviation 2.60 dB). The dry-referenced loop leaves
3.1 dB (0.83 dB), with every capture within ±2 dB of the dry signal,
including the compression-dominated capture that no input gain can
equalize. The official NAM plugin's normalized output mode applies a static
gain from stored metadata or a fixed target. To our knowledge no published
NAM host performs live dry-referenced compensation.

## 5 A Drag-and-Drop Cabbage Plugin

We present the packaging of this opcode as a VST3/AU plugin through
Cabbage [13] not only to demonstrate our opcode's integration into a plugin
but to encourage an open-source platform for NAM development in Csound. The
plugin window is a single drop target. Cabbage publishes a dropped file's
path on its reserved `LAST_FILE_DROPPED` channel, and a path-input variant
of the loader hot-swaps to the dropped model through the background
mechanism of Section 3.2. The previous amp keeps running until the new one
is ready. Companion opcodes report the path of the actually-loaded model, so
the UI shows real state rather than the last request, and step through the
sorted `.nam` siblings of the active file for in-folder browsing. The volume
compensation of Section 4 defaults on, with a toggle exposing raw model
level for A/B comparison. The input control is the loudness-compensated
drive of Section 3.1, so it shapes saturation rather than volume. A gate,
three-band tone stack, and DC blocker mirror the outboard chain of the
reference NAM plugin.

The plugin is architected to degrade gracefully rather than go silent. A
plugin whose orchestra fails to compile emits silence yet still passes host
validation, so the base orchestra contains no NAM references at all. The
engine instrument is compiled at runtime with `compilestr`. If the opcode
library fails to load, audio passes through dry and the failure is reported
on the status display. The plugin also disables Csound's default MIDI
instrument assignment with `massign 0, 0`. The loader registry is global,
and a MIDI-spawned duplicate instrument instance would otherwise free the
loader out from under it.

## 6 Tone Selection in a Raspberry Pi Guitar Pedal

The opcode's original deployment target is a Raspberry Pi 5-based guitar
pedal running Csound 6.18 at 48 kHz with `ksmps` 128, the audio chain pinned
to a dedicated core. A library of 25 captures acts as a bank of
different "tones". Model selection is mapped to MIDI foot controls and OSC,
arriving at the opcode as the k-rate index of Section 3.1. Because loading
is asynchronous and click-safe, the player steps between amps mid-phrase.
Because compensation runs inside the opcode, no volume ride accompanies a
tone change, even for brand-new additions to the library. Per-model trim
memory keeps repeated tone-stepping stable. Added latency is the one
k-cycle of Section 3.5 (2.7 ms), well
inside a live monitoring budget. CPU load with a full-quality WaveNet-family
model has been informally observed around 15% of the audio core during
performance.

## 7 Conclusions

`NAMProcess` gives Csound direct access to NAM's model format and its
community library. Any of several hundred thousand captures — amps, pedals,
preamps, full chains — becomes a drop-in nonlinear processor addressable by
a k-rate index, composable with everything else in the orchestra. The same
engine ships as a drag-and-drop Cabbage plugin and as the tone section of a
Raspberry Pi guitar pedal.

Several extensions are planned. Only one `NAMProcess` instance
per orchestra is currently supported, and lifting this is planned, as is an
n-channel variant following Csound's array conventions, per-model trim
memory persisted across sessions, and support for the plugin mechanisms of
Csound 7. Source code and measurement tooling are available at [link
withheld for review].

## References

<!-- llncs numbered style, alphabetical by author/project. -->

1. Atkinson, S.: Slimmable NAM: Neural Amp Models with adjustable runtime
   computational cost. arXiv:2511.07470 (2025)
2. Bencina, R.: Real-time audio programming 101: time waits for nothing,
   http://www.rossbencina.com/code/real-time-audio-programming-101-time-waits-for-nothing
3. Chowdhury, J.: RTNeural: fast neural inferencing for real-time systems.
   arXiv:2106.03037 (2021)
4. Damskägg, E.-P., Juvela, L., Thuillier, E., Välimäki, V.: Deep learning for
   tube amplifier emulation. In: Proceedings of the IEEE International
   Conference on Acoustics, Speech and Signal Processing (ICASSP), pp.
   471–475. Brighton (2019)
5. Damskägg, E.-P., Juvela, L., Välimäki, V.: Real-time modeling of audio
   distortion circuits with deep learning. In: Proceedings of the 16th Sound
   and Music Computing Conference (SMC), pp. 332–339. Málaga (2019)
6. Fink, M., Rabenstein, R.: A Csound opcode for a triode stage of a vacuum
   tube amplifier. In: Proceedings of the 14th International Conference on
   Digital Audio Effects (DAFx-11), pp. 365–370. Paris (2011)
7. Lazzarini, V., Yi, S., ffitch, J., Heintz, J., Brandtsegg, Ø., McCurdy, I.:
   Csound: A Sound and Music Computing System. Springer, Cham (2016)
8. Neural Amp Modeler, https://www.neuralampmodeler.com and NeuralAmpModelerCore,
   https://github.com/sdatkinson/NeuralAmpModelerCore
9. neural_tilde: Neural Amp Modeler external for Max/MSP,
   https://github.com/apresta/neural_tilde
10. Pluta, S.: RTNeural real-time neural inference UGen,
    https://scsynth.org/t/rtneural-real-time-neural-inference-ugen/10074
11. Steinmetz, C.J., Reiss, J.D.: Efficient neural networks for real-time
    modeling of analog dynamic range compression. In: Proceedings of the 152nd
    Audio Engineering Society Convention, paper 10596 (2022)
12. TONE3000, https://www.tone3000.com
13. Walsh, R.: Developing Csound plugins with Cabbage. In: Heintz, J.,
    Hofmann, A., McCurdy, I. (eds.) Ways Ahead: Proceedings of the First
    International Csound Conference. Cambridge Scholars Publishing,
    Newcastle upon Tyne (2013)
14. Wright, A., Damskägg, E.-P., Juvela, L., Välimäki, V.: Real-time guitar
    amplifier emulation with deep learning. Applied Sciences 10(3), 766 (2020)
15. Wright, A., Damskägg, E.-P., Välimäki, V.: Real-time black-box modelling
    with recurrent neural networks. In: Proceedings of the 22nd International
    Conference on Digital Audio Effects (DAFx-19), pp. 173–180. Birmingham
    (2019)
16. Wright, A., Välimäki, V.: Perceptual loss function for neural modelling of
    audio systems. In: Proceedings of the IEEE International Conference on
    Acoustics, Speech and Signal Processing (ICASSP), pp. 251–255. Barcelona
    (2020)
17. Tenney, J.: Computer Music Experiments, 1961–1964. Electronic Music
    Reports 1, 23–60. Institute of Sonology, Utrecht State University (1969).
    Reprinted as "Computer Music Experiences, 1961–1964"

<!-- Submission notes (delete before LaTeX conversion):
     ICSC 2026 deadline 2026-07-11 23:59 Rome time. llncs.cls template,
     4 pages max, 5th page references-only OK. Blind: AuthorA/AuthorB
     placeholders. Plugin name "NAM Drop", real repo link, acknowledgments
     (Dr. B co-authorship) at camera-ready only. Optional figure: plugin
     screenshot in a DAW (ICSC norm), crop name for blind. Verify Walsh [13]
     cite form ("Ways Ahead", Cambridge Scholars 2013) before camera-ready.
     Static-calibration table (per-model input gains) removed 2026-07-09 per
     author: live dry-referenced comp IS the method; static/pre-determined
     schemes appear only as compressed motivation. Pedal + plugin library =
     the 25-capture set in cabbage-plugin/models/. §4 numbers all from
     volume-compensation-writeup.md (verified against shipped sources
     csound_nam_opcode.cpp / nam_loader.cpp / kweight.h, 2026-07-09).
     Full 9-approach experiment history lives in cabbage-plugin/archive/
     volume-compensation-history.md — repo material, not paper. -->

