# SoundMagic Spectral (Michael Norris) - complete catalog

Compiled 2026-10-03 for re-implementation in Csound inside NEPTR. Rule: no parameter point is lost.

## Sources

- Product page: https://www.michaelnorris.info/software/soundmagic-spectral/
- Online guide: https://www.michaelnorris.info/software/soundmagic-spectral/guide/ (text quoted verbatim below)
- Download (v1.5, 3.6 MB DMG, contains `SoundMagic AU Manual.pdf` (rev. June 2016), `Version History.rtf`, `INSTALLATION - READ ME.rtf`, `SoundMagic Spectral Plug-ins 1.5.pkg`): https://www.michaelnorris.info/downloads/soundmagicspectral15.dmg
- Parameter names, min/max/defaults, units and enum labels were NOT scraped from prose: they were read from the real AU binaries (v1.5, universal x86_64+arm64) by instantiating each one and calling `AudioUnitGetProperty(kAudioUnitProperty_ParameterInfo / ParameterValueStrings)`. They are exact. Raw dump and tooling: `/private/tmp/claude-501/-Users-emgor-Documents--EMGOR-SYNTH-------2026NEW-DEMIURGE-OS/1188d03a-b110-4e08-ad94-f7501642fdc7/scratchpad/dl/params.txt` (scratchpad, may vanish; everything is also in this file).
- Community: https://cycling74.com/forums/michael-norris-spectral-shimmer-effect-recreation , https://scsynth.org/t/imitate-michael-norris-spectral-freeze/6733 , https://groups.google.com/g/cec-conference/c/a24Wo008-aI , https://scsynth.org/t/soundmagic-and-input-from-audiounits-to-supercollider/7764
- Author GitHub: https://github.com/mnorrisvuw

## Verdict: source code and license

- **Source code: NOT available.** GitHub `mnorrisvuw` hosts only notation plugins (MuseScore/Sibelius: LargeType2, MN-Better-Notation-*, MN-Post-tonal-*, MN-Utility-*) and MuseScore forks. GitHub repo search for "soundmagic spectral" returns nothing. Nothing on GitLab found. Nothing cloned (no `soundmagic_src` directory created because there is nothing to clone).
- **License: freeware, closed source.** Manual 3.2: "offered as freeware" ("eternal beta"). No open-source license text anywhere (DMG, pkg, page, manual). Treat as all-rights-reserved binaries; re-implementation from behaviour/documentation is clean, copying code is not available anyway.
- Language/framework (from binaries): C++ Audio Unit (`AUBase`-style kernels, e.g. class `SpectralShift::SpectralShiftKernel`), Apple Accelerate vDSP (`vDSP_fft_zrip`, `vDSP_hann_window`, `vDSP_polar`, `vDSP_ctoz`, ...). So: real-FFT with a Hann window via vDSP. Overlap factor is not exposed and not documented.
- History: algorithms date from his 1995 SoundMagic suite for OS 9 SoundEffects/SoundMaker (riccisoft.com/soundeffects), heavily influenced by Trevor Wishart (Audible Design / Composers Desktop Project). Mockfuneral write-up of the 1997 originals: https://mockfuneral.github.io/2020/12/09/soundmakersoundmagicfx
- Ports: the author states (product page) he is porting to **JUCE** (VST/AAX, Mac+Win); only **Spectral Blurring** done so far; no public repo. No Pd/SC/Faust/Csound/VCV ports found. Community only has partial imitations (SuperCollider PV_* attempts for Freeze, Max pfft~ attempt for Shimmer). Closest open code: CDP (Composers Desktop Project) and Csound pvs* opcodes.
- Gotcha: v1.5 `Spectral Bin Shift` Info.plist has `version` as a string ("66816") instead of an integer, so macOS refuses to register it ("Cannot get Component's Name strings"). Fix: set it to integer 66816 and ad-hoc re-sign. (I did this to a temp copy only, and removed all temporary installs from ~/Library/Audio/Plug-Ins/Components afterwards; NAMDrop and NEPTR components untouched.)

## Plugin list (23, v1.5)

19 spectral: Averaging, Bin Shift, Blurring, DroneMaker, Emergence, Filterbank, Freezing, Gate And Hold, Gliding Filters, Granulation, Harmonizer, Partial Glide, Pitch Shift, Pulsing, Shimmer, Shuffle, Stretch, Tracing, Weave.
4 time-domain (not spectral): Chorus, Grain Streamer, Idee Fixer, Super Filterbank (replaced "Mr Filterbank" in Beta 7; the web guide still calls a "Comb Filterbank").
Note the task brief named "Spectral Spread": there is no such plugin in v1.5.

Guide categories: Smoothing (Averaging, Blurring, DroneMaker); Sustaining (Freezing, Gate And Hold, Weave); Excitement (Shimmer, Shuffle, Pulsing); Pitch (Harmonizer, Bin Shift, Pitch Shift, Stretch); Filtering (Tracing, Filterbank, Gliding Filters); Texturizing (Granulation); Partial (Emergence, Partial Glide); Time Domain (Idee Fixer, Chorus, Grain Streamer, Super Filterbank).

## Shared / global behaviour (all spectral plugins unless noted)

AU parameter units: dB, %, Hz, cents, s, ms are as stated; "lin. gain" is the AU's LinearGain unit tag (the ranges shown look like dB-ish values; treat with care, see Filterbank Harmonic gain).

| Param | range | default (varies) | meaning |
|---|---|---|---|
| FFT Size | index 0..7 -> 512,1024,2048,4096,8192,16384,32768,65536 | 2 (2048); 3 (4096) for Filterbank, Gate And Hold, Stretch, Weave | Bigger = smoother, more CPU, more latency; DroneMaker-type smoothing likes 16384+. No overlap or window setting is exposed. Hann window (vDSP_hann_window) found in binary. |
| Lo bin cutoff | 0..100 % | 0 | bins below this % of the spectrum zeroed |
| Hi bin cutoff | 0..100 % | 100 | bins above zeroed |
| Brightness | -100..100 % | 0 (Shimmer 20) | filter boosting/cutting high-frequency strength; at extremes the top "sizzles" |
| Randomize phases | on/off | on for Averaging, Blurring, DroneMaker, Freezing, Gate And Hold, Granulation, Shuffle, Weave; off for the rest | see below |
| Gain | typically -40..+40 dB | 0 (Emergence +2) | output level |
| Feedback | 0..99/100 % | 0 unless stated | output fed back to input |
| Variance type (Blurring, Stretch, Chorus) | None/Random/LFO/Link amplitude/Link inverted amp | None | param modulation by random, LFO, input amplitude, inverse amplitude |

**Phase handling (important for fidelity):** no phase unwrapping / true phase vocoder. He drops unwrapping for 10-15 % CPU saving. Consequence: amplitude interpolation causes comb/ringing at the FFT period; fix = "Randomize phases" (random phase per component each frame), which kills ringing but smears transients and widens stereo. Peak-tracking plugins (Pitch Shift, Stretch, Emergence, Partial Glide) use the Dolson/Laroche peak-based method (regions of influence around tracked peaks).

**Latency/tail:** author states typically ~0.5 s or more latency and tail; AU-reported latency is 0 (not reported). Guitar use: choose small FFT, or accept the delay.

**Sample-rate dependence:** bin spacing = SR/FFT; param ranges like "Jitter only above" max 11025 Hz and "Hi bin cutoff" imply 44.1k/22.05 kHz Nyquist thinking.

**Stereo:** processing is per channel; Spectral Emergence's rotating pan arises from independent random behaviour on L vs R.

## Version history (for behaviours)

1.5 Universal; 1.4 Logic validation/notarized; Beta 9 Partial Glide crash fix; Beta 8 Weave 64-bit; Beta 7 Mr Filterbank -> Super Filterbank new options; Beta 6 64-bit, added Spectral Weave, "Gate Type" and silence stage in Gate And Hold, new Bin Shift options, Freezing fixes; Beta 5 Gate And Hold pulsing + delay-vs-freq, Tracing keep/remove loudness options, Stretch "variance quantization", Idee Fixer passes auval, per-effect default Randomize-phases; Beta 4 DroneMaker/Emergence fixes, bin shift in Gate And Hold; Beta 3 Gate And Hold envelopes, Idee Fixer; Beta 2 auval-valid.

## Per-plugin catalog

In each section: the "Author's manual text" is verbatim from the guide (description first, then each parameter name and its explanation). The parameter table is from the binary. Where table notes are empty, see the manual text above it.

## Chorus

AU id: `aufx` / `Chor` / `MNor`. Reported AU latency (s)=0 tail (s)=0

**Author's description**: "A time-varying chorus/flanger effect"

**Parameters (read from the AU binary via AudioUnitParameterInfo; exact):**

| # | name | min | max | default | unit | notes |
|---|---|---|---|---|---|---|
| 0 | Delay | 0 | 100 | 20 | ms |  |
| 1 | Delay Variance Type | 0 | 4 | 0 |  | values: [0]None; [1]Random; [2]LFO; [3]Link amplitude; [4]Link inverted amp; |
| 2 | Delay Variance | 0 | 100 | 5 | ms |  |
| 3 | LFO Rate | 0 | 200 | 1 | Hz |  |
| 4 | Feedback | 0 | 100 | 80 | % | output fed back to input |
| 5 | Wet/dry mix | 0 | 100 | 50 | % |  |
| 6 | Gain | -30 | 30 | 0 | dB | output level |

**Extraneous / built-in extras:** ENTIRE plugin is a time-domain chorus/flanger (not spectral).


## Grain Streamer

AU id: `aufx` / `GStm` / `MNor`. Reported AU latency (s)=0 tail (s)=0

**Author's description**: "Captures and repeats grains" (manual: "'Captures' and repeats grains")

**Parameters (read from the AU binary via AudioUnitParameterInfo; exact):**

| # | name | min | max | default | unit | notes |
|---|---|---|---|---|---|---|
| 0 | Freeze probability | 0 | 100 | 7 | % |  |
| 1 | Grain repetitions | 1 | 100 | 8 |  |  |
| 2 | Repetitions variation | 0 | 100 | 8 |  |  |
| 3 | Grain length at start | 1 | 200 | 13 | ms |  |
| 4 | Grain length start var | 0 | 200 | 12 | ms |  |
| 5 | Grain length at end | 1 | 200 | 13 | ms |  |
| 6 | Grain length end var | 0 | 200 | 12 | ms |  |
| 7 | Zero non-frozen grains | 0 | 1 | 0 | on/off |  |
| 8 | Fade amount | 0 | 100 | 50 | % |  |
| 9 | Gain | -20 | 20 | 0 | lin. gain | output level |

**Extraneous / built-in extras:** Time-domain (not spectral).


## Idee Fixer

AU id: `aufx` / `IFix` / `MNor`. Reported AU latency (s)=0 tail (s)=0

**Author's manual text** (verbatim from guide, parameter descriptions follow the paragraph):

> This is a very large, very complex granular/brassage effect. It works by capturing audio from the input into a buffer, before churning out granular versions of it.
> Sampling status
> Indicates the status of the buffer sampler. Ready for sample start is the armed status. To turn it off temporarily, switch to Mute mode. To switch it off and empty the buffer, switch to Off (reset). The plug-in will automatically switch to the last two modes.
> Sample start mode
> Trigger Threshold: this uses the Trigger @ Threshold param to wait until the input exceeds a gate before beginning the sampling
> Sample Start Delay: this waits for the duration specified in sample start delay parameter until beginning sampling
> Trigger @ threshold
> The threshold level to begin sampling, if sample start mode is set to Trigger Threshold
> Trigger start preroll
> Audio before or after the threshold being exceeded can be included in the buffer if this is set.
> Threshold window size
> Sets the granularity of the threshold-seeking algorithm. Will probably be removed in later versions.
> Sample start delay
> The length of time until sampling begins, if in Use Sample Start Delay mode.
> Sample stop mode
> Trigger Threshold: will stop sampling if the level falls below the threshold specified in the Trigger @ Threshold param, as long as it falls below for more than the Dur below threshold param.
> Use only Max Length: uses the Max. Sample Length param to set the buffer duration
> Trigger stop preroll
> Will include audio before or after can be included in the buffer if this is set.
> Dur below thresh
> How long below the threshold the audio should go until sampling stops
> Max. sample length
> In both sample stop modes, if sampling exceeds this duration, sampling will stop.
> Grain loop mode
> Currently only Forever works!
> Total loop dur
> Currently ignored.
> Total loop dur var
> Currently ignored.
> Total loop times
> Currently ignored.
> Granulation mode\
> Off: The whole buffer is used. This can be very CPU-intensive for long samples! However, the grain start and grain end params can be used to set a subsample of the buffer.
> Use Grain Dur: The Grain duration param is used to specify the length of the grain. When you move the grain start or grain end params, the other param will move too to reflect the length of the grain.
> Use Grain End param: not sure if this is currently working, or what it is supposed to do!
> Grain duration
> The length of grain if in Grain length lock mode.
> Grain start
> The grain start, as a percentage of the buffer size.
> Grain start var mode
> The plug-in can automatically vary the grain start
> Off: the grain start does not alter automatically
> Random: the grain start varies using the grain start var param
> Loop Forward: the grain start gradually progresses through the buffer and loops round to the beginning. The rate at which it does so is controlled by the grain start var rate param
> Loop Backward: the reverse
> Ping-pong: as before, except the progression changes direction at either end
> Random walk: sets up a random walk through the buffer using both the grain start var and the grain start var rate params to control
> Grain start var rate
> A value that controls the speed of the last four Grain start var modes
> Grain end
> The grain end, as a percentage of the buffer size. Is ignored if Use Grain Dur is set as the granulation mode
> Grain spacing
> The offset between the start of each grain, as a percentage of grain size. 100% indicates that the end of the first grain is immediately followed by the start of the next.
> Grain amplitude
> A gain parameter applied to each grain
> Grain fade
> A fade in and out on the beginning and ends of each grain
> Grain transposition
> A transposition factor for each grain. Currently a very quick and dirty rate-change transposition — not very beautiful.

**Parameters (read from the AU binary via AudioUnitParameterInfo; exact):**

| # | name | min | max | default | unit | notes |
|---|---|---|---|---|---|---|
| 0 | Sampling status | 0 | 4 | 2 |  | values: [0]Off (reset); [1]Mute; [2]Ready for sample start; [3]Ready for sample end; [4]Sample loaded; |
| 1 | Sample start mode | 0 | 1 | 1 |  | values: [0]Use 'Trigger Threshold'; [1]Use 'Sample Start Delay'; |
| 2 | Trigger threshold | -60 | 0 | -12 | dB |  |
| 3 | Trigger start preroll | -1000 | 1000 | 0 | ms |  |
| 4 | Sample start delay | 0 | 10 | 0 | s |  |
| 5 | Sample stop mode | 0 | 1 | 1 |  | values: [0]Use 'Trigger Threshold' param; [1]Use only 'Max Length' param; |
| 6 | Trigger stop preroll | -1000 | 1000 | 0 | ms |  |
| 7 | Dur below thresh | 0 | 500 | 80 | ms |  |
| 8 | Max. sample length | 0 | 10 | 2 | s |  |
| 9 | Granulation mode | 0 | 2 | 1 |  | values: [0]Off; [1]Use Grain Dur (ign. grain end); [2]Use 'Grain End' param; |
| 10 | Grain duration | 1 | 1000 | 60 | ms |  |
| 11 | Grain start | 0.001 | 100 | 0 | % |  |
| 12 | Grain start var mode | 0 | 5 | 2 |  | values: [0]Off; [1]Random; [2]Loop forward; [3]Loop backward; [4]Ping-pong; [5]Random walk; |
| 13 | Grain start var | 0.001 | 100 | 0 | % |  |
| 14 | Grain start var rate | 0.0001 | 2 | 0.1 |  |  |
| 15 | Grain end | 0 | 100 | 100 | % |  |
| 16 | Grain end var | 0 | 100 | 0 | % |  |
| 17 | Grain spacing | 1 | 1000 | 10 | % |  |
| 18 | Grain spacing var | 0.001 | 100 | 2 | % |  |
| 19 | Grain amplitude | -60 | 0 | 0 | dB |  |
| 20 | Grain amplitude var | 0 | 60 | 0 | dB |  |
| 21 | Grain fade | 0 | 100 | 100 | % |  |
| 22 | Dry mix | -60 | 0 | -60 | dB |  |
| 23 | Grain transposition | -2400 | 2400 | 0 | cents |  |
| 24 | Grain transposition var | 0 | 2400 | 0 | cents |  |
| 25 | Gain | -60 | 10 | 0 | dB | output level |

**Extraneous / built-in extras:** Time-domain granular/brassage sampler (not spectral). Contains its own dry mix, transposition (rate-change), sample-capture state machine.


## Spectral Averaging

AU id: `aufx` / `SAve` / `MNor`. Reported AU latency (s)=0 tail (s)=0

**Author's manual text** (verbatim from guide, parameter descriptions follow the paragraph):

> Averages the spectral information over n windows. Very similar effect to Spectral Blurring.
> Number of Frames
> The number of FFT frames over which to average. The more frames the smoother the result.

**Parameters (read from the AU binary via AudioUnitParameterInfo; exact):**

| # | name | min | max | default | unit | notes |
|---|---|---|---|---|---|---|
| 0 | Number of frames | 2 | 1000 | 200 |  |  |
| 1 | Brightness | -100 | 100 | 0 | % | tilt of high-frequency strength |
| 2 | Lo bin cutoff | 0 | 100 | 0 | % | % of spectrum; bins below set to 0 |
| 3 | Hi bin cutoff | 0 | 100 | 100 | % | % of spectrum; bins above set to 0 |
| 4 | Randomize phases | 0 | 1 | 1 | on/off | phases randomized per bin (no phase unwrapping; see Shared section) |
| 5 | FFT Size | 0 | 7 | 2 |  | index 0..7 = 512,1024,2048,4096,8192,16384,32768,65536 values: [0]512; [1]1024; [2]2048; [3]4096; [4]8192; [5]16384; [6]32768; [7]65536; |
| 6 | Gain | -40 | 40 | 0 | dB | output level |

**Extraneous / built-in extras:** None (pure core process + common params).


## Spectral Blurring

AU id: `aufx` / `SBlr` / `MNor`. Reported AU latency (s)=0 tail (s)=0

**Author's manual text** (verbatim from guide, parameter descriptions follow the paragraph):

> Applies a low-pass filter to the spectral changes from one window to the next, progressively smearing the sound. Has similar spurious problems as Spectral Averaging, but may still be useful.
> Blur amount
> The amount of smearing that takes place. This can be varied using the variance controls detailed above

**Parameters (read from the AU binary via AudioUnitParameterInfo; exact):**

| # | name | min | max | default | unit | notes |
|---|---|---|---|---|---|---|
| 0 | Blur amount | 0 | 100 | 50 | % |  |
| 1 | Variance type | 0 | 4 | 0 |  | values: [0]None; [1]Random; [2]LFO; [3]Link amplitude; [4]Link inverted amp; |
| 2 | Blur variance | 0 | 100 | 0.5 |  |  |
| 3 | LFO rate | 0 | 50 | 1 | Hz |  |
| 4 | Lo bin cutoff | 0 | 100 | 0 | % | % of spectrum; bins below set to 0 |
| 5 | Hi bin cutoff | 0 | 100 | 100 | % | % of spectrum; bins above set to 0 |
| 6 | Randomize phases | 0 | 1 | 1 | on/off | phases randomized per bin (no phase unwrapping; see Shared section) |
| 7 | FFT Size | 0 | 7 | 2 |  | index 0..7 = 512,1024,2048,4096,8192,16384,32768,65536 values: [0]512; [1]1024; [2]2048; [3]4096; [4]8192; [5]16384; [6]32768; [7]65536; |
| 8 | Gain | -40 | 40 | 0 | dB | output level |

**Extraneous / built-in extras:** Contains its own LFO (LFO rate) for modulating blur amount via variance type. No FFT-independent extras.


## Spectral DroneMaker

AU id: `aufx` / `SDmk` / `MNor`. Reported AU latency (s)=0 tail (s)=0

**Author's manual text** (verbatim from guide, parameter descriptions follow the paragraph):

> Spectral DroneMaker is the king of spectral drone effects. It can turn any sound into a beautiful, slowly-changing drone based on the sonic characteristics of the underlying sound. The idea came from the software called Mammut developed by Øyvind Hammer of Notam, which used very large FFTs tosmear out the detail, thus creating slowly changing drones. Unlike Mammut, Spectral DroneMaker still uses normal-sized FFTs, instead creating the drone by gradually interpolating between frequency components sampled at a distance. Other parameters for making the drone even more sexy include a bank of comb filters added prior to the interpolation for a subtle, or not-so-subtle pitched effect, the ability to gate frequency components as in Spectral Tracing, and the ability to add a flanger to the output.
> Interpolation length
> Each bin is interpolated between samples taken at a distance. This parameter sets the amount of time that the bins are sampled at.
> Interp length variance
> The above parameter can be randomized within a range of interp length ± variance
> Use peak amplitudes
> With this switched on, the process will scan for the highest amplitude that occurs in each bin the input file while they are being interpolated. The process will then use that stored amplitude instead of the actual value that occurs at the next interpolation point in the input file.
> Gate level
> Only lets partials above a certain threshold into the output sound.
> Comb filterbank level
> Spectral DroneMaker allows you to harmonize the input sound by putting it through a comb filterbank. This parameter sets the wet/dry level of the comb filtering.
> Comb filter fundamental
> Sets the fundamental frequency of the filterbank. The filters are built up from this frequency.
> Scale type
> A list of preset scale types for building the filterbank
> Number of octaves
> How many octaves you want the filterbank to cover. The filterbank can be quite computationally expensive with a high number of octaves.
> Filter resonance
> The strength of the comb filters. Watch out for distortion from high resonance filters.
> Flanger amount
> Lets you apply a time-varying flanger to the output
> Flanger depth
> The maximum delay length of the flanger. The higher the number the more intense the pitch-varying aspect of the flanger
> Flanger rate
> The speed of the flanger.

**Parameters (read from the AU binary via AudioUnitParameterInfo; exact):**

| # | name | min | max | default | unit | notes |
|---|---|---|---|---|---|---|
| 0 | Interpolation length | 0.1 | 30 | 4 | s |  |
| 1 | Interp length variance | 0 | 10 | 2 | s |  |
| 2 | Use peak amplitudes | 0 | 1 | 0 | on/off |  |
| 3 | Brightness | -100 | 100 | 0 | % | tilt of high-frequency strength |
| 4 | Comb filterbank level | 0 | 100 | 0 | % |  |
| 5 | Comb filter fundamental | 20 | 2000 | 200 | Hz |  |
| 6 | Scale type | 0 | 9 | 6 |  | values: [0]None; [1]Octaves; [2]Fifths; [3]Major triad; [4]Minor triad; [5]Major scale; [6]Dorian scale; [7]Lydian scale; [8]Mixolydian scale; [9]Aeolian scale; |
| 7 | Number of octaves | 1 | 10 | 2 |  |  |
| 8 | Filter resonance | 1 | 99 | 99 | % |  |
| 9 | Flanger amount | 0 | 100 | 0 | % |  |
| 10 | Flanger depth | 0.1 | 15 | 2 | ms |  |
| 11 | Flanger rate | 0.01 | 5 | 0.1 | Hz |  |
| 12 | Gate level | -60 | 0 | -60 | dB |  |
| 13 | Lo bin cutoff | 0 | 100 | 0 | % | % of spectrum; bins below set to 0 |
| 14 | Hi bin cutoff | 0 | 100 | 100 | % | % of spectrum; bins above set to 0 |
| 15 | Randomize phases | 0 | 1 | 1 | on/off | phases randomized per bin (no phase unwrapping; see Shared section) |
| 16 | FFT Size | 0 | 7 | 2 |  | index 0..7 = 512,1024,2048,4096,8192,16384,32768,65536 values: [0]512; [1]1024; [2]2048; [3]4096; [4]8192; [5]16384; [6]32768; [7]65536; |
| 17 | Gain | -40 | 40 | 0 | dB | output level |

**Extraneous / built-in extras:** EXTRANEOUS built-ins: (1) comb filterbank pre-stage with scale presets (Comb filterbank level, fundamental, Scale type, Number of octaves, Filter resonance); (2) time-varying flanger on output (Flanger amount/depth/rate); (3) amplitude gate (Gate level, as Spectral Tracing). Core process = per-bin interpolation between spectra sampled at a distance (Interpolation length).


## Spectral Emergence

AU id: `aufx` / `SEmg` / `MNor`. Reported AU latency (s)=0 tail (s)=0

**Author's manual text** (verbatim from guide, parameter descriptions follow the paragraph):

> This effect tracks the partials, but applies an amplitude envelope to them to create a strange pulsating sound. Because this happens separately for left and right channels, you often get a rotating panning effect as well.
> Fade in (windows)
> The number of windows over which to fade in a tracked partial
> Hold (windows)
> Once the tracked partial has been faded in, how long should it be held at 0dB for?
> Fade out (windows)
> Once the tracked partial has been faded in and held, how long it should be faded out for?
> Recycle faded peaks
> If this is checked, then if a partial is still being tracked after it has finished all three phases of its envelope, it will be restarted from the fade in section. If unchecked, it will remain silence until the end of its life.

**Parameters (read from the AU binary via AudioUnitParameterInfo; exact):**

| # | name | min | max | default | unit | notes |
|---|---|---|---|---|---|---|
| 0 | Fade in (windows) | 0 | 400 | 80 |  |  |
| 1 | Hold (windows) | 0 | 400 | 10 |  |  |
| 2 | Fade out (windows) | 0 | 400 | 80 |  |  |
| 3 | Fuzziness | 0 | 10 | 2 |  |  |
| 4 | Max. bins per partial | 1 | 50 | 6 |  |  |
| 5 | Lo bin cutoff | 0 | 100 | 0 | % | % of spectrum; bins below set to 0 |
| 6 | Hi bin cutoff | 0 | 100 | 100 | % | % of spectrum; bins above set to 0 |
| 7 | Recycle faded peaks | 0 | 1 | 0 | on/off |  |
| 8 | Randomize phases | 0 | 1 | 0 | on/off | phases randomized per bin (no phase unwrapping; see Shared section) |
| 9 | FFT Size | 0 | 7 | 2 |  | index 0..7 = 512,1024,2048,4096,8192,16384,32768,65536 values: [0]512; [1]1024; [2]2048; [3]4096; [4]8192; [5]16384; [6]32768; [7]65536; |
| 10 | Gain | -40 | 40 | 2 | dB | output level |

**Extraneous / built-in extras:** None extraneous. Note per-channel (L/R) independent envelopes yield rotating pan effect (stereo-dependent).


## Spectral Filterbank

AU id: `aufx` / `SFbk` / `MNor`. Reported AU latency (s)=0 tail (s)=0

**Author's manual text** (verbatim from guide, parameter descriptions follow the paragraph):

> A bank of extremely narrow, very pure bandpass filters (similar to the GRM Reson plug-in). You can choose the interval between centre frequencies of your filterbank, and how many filters should be created. Because of the inaccuracies in pitch in the FFT, the filterbank cannot guarantee the actual frequencies you ask for will be exactly rendered; however, it will be as accurate as possible within the limitations of the FFT.
> Number of filters
> The number of filters in the filterbank
> Base frequency
> The lowest frequency in the filterbank, from which the other frequencies are built-up
> Interval
> The interval that will be applied to the base frequency and successive harmonics to build up the filterbank, using the techniques described in the transposition type parameter
> Transposition type
> Stacked: each interval is simply applied to the previous interval, stacking the interval on top of each other. For an interval of 300 cents (a minor third), this would create a diminished seventh chord, for instance. An interval of 200 cents would create a whole-tone scale.
> Chord: the fundamental and the upper note defined by the interval are simply repeated at the octave. If the fundamental was C, and the interval was a perfect fifth (700 cents), then the notes are just C and G repeated up through the octaves
> Harmonics: a harmonic series is based on integer multiples of a fundamental (n, 2n, 3n, 4n, etc). The harmonics setting extends this by using multiples of other intervals. It assumes that the standard harmonic series would have an interval setting of 1200 cents (an octave) — therefore, halving this to 600 cents (a tritone) would give you filters at 0.5n, n, 1.5n, 2n, etc..., while 2400 cents (two octaves) would give you 2n, 4n, 6n, 8n, etc.
> Harmonic gain
> Harmonic gain indicates how much each progressive harmonic as you get higher should be attenuated by.
> Side bins
> Allows you to create a wider filter by adding side bins on either side of the central filter bin. While this lets more of the original sound through, and therefore less pure filtering, it also creates some interesting pulsating dynamic effects
> Side bin gain
> How much each side bin is attenuated by as you get further away from the central filter

**Parameters (read from the AU binary via AudioUnitParameterInfo; exact):**

| # | name | min | max | default | unit | notes |
|---|---|---|---|---|---|---|
| 0 | Number of filters | 1 | 400 | 100 |  |  |
| 1 | Base frequency | 20 | 22050 | 50 | Hz |  |
| 2 | Interval | 0 | 2400 | 200 | cents |  |
| 3 | Transposition type | 0 | 2 | 0 |  | values: [0]Stacked; [1]Chord; [2]Harmonics; |
| 4 | Harmonic gain | -5 | 1 | 0 | lin. gain |  |
| 5 | Side bins | 0 | 40 | 0 |  |  |
| 6 | Side bin gain | -10 | 0 | -3 | lin. gain |  |
| 7 | Brightness | -100 | 100 | 0 | % | tilt of high-frequency strength |
| 8 | Lo bin cutoff | 0 | 100 | 0 | % | % of spectrum; bins below set to 0 |
| 9 | Hi bin cutoff | 0 | 100 | 100 | % | % of spectrum; bins above set to 0 |
| 10 | Feedback | 0 | 100 | 0 | % | output fed back to input |
| 11 | Wet/Dry Mix | 0 | 100 | 100 | % |  |
| 12 | Randomize phases | 0 | 1 | 0 | on/off | phases randomized per bin (no phase unwrapping; see Shared section) |
| 13 | FFT Size | 0 | 7 | 3 |  | index 0..7 = 512,1024,2048,4096,8192,16384,32768,65536 values: [0]512; [1]1024; [2]2048; [3]4096; [4]8192; [5]16384; [6]32768; [7]65536; |
| 14 | Gain | -20 | 40 | 0 | dB | output level |

**Extraneous / built-in extras:** Built-in Wet/Dry Mix and Feedback; side-bin widening. Otherwise core.


## Spectral Freezing

AU id: `aufx` / `SFrz` / `MNor`. Reported AU latency (s)=0 tail (s)=0

**Author's manual text** (verbatim from guide, parameter descriptions follow the paragraph):

> Sometimes also known as Spectral Accumulation. Each bin is watched until its amplitude reaches a peak. It is then held at that peak until it is exceeded by another peak in that bin. The Freeze Factor parameter puts a decay onto the peak holding, so that you don**t just end up with a whole series of very high-amplitude peaks.
> Freeze Factor
> Once a bin is frozen, it can then be decayed. If this is set to 100%, no decaying takes place, allowing for a complete bin freeze, which will only be superseded by a peak.
> Use threshold param
> If this is checked, a freeze will only take place if the new peak is above a threshold level.
> Threshold
> The threshold above which a freeze can take place
> Zero bins under threshold
> If this is checked, unfrozen bins under the threshold level will be zeroed

**Parameters (read from the AU binary via AudioUnitParameterInfo; exact):**

| # | name | min | max | default | unit | notes |
|---|---|---|---|---|---|---|
| 0 | Freeze factor | 0 | 100 | 50 | % |  |
| 1 | Use threshold param | 0 | 1 | 0 | on/off |  |
| 2 | Threshold | -60 | 0 | -20 | dB |  |
| 3 | Zero bins under theshold | 0 | 1 | 0 | on/off |  |
| 4 | Lo bin cutoff | 0 | 100 | 0 | % | % of spectrum; bins below set to 0 |
| 5 | Hi bin cutoff | 0 | 100 | 100 | % | % of spectrum; bins above set to 0 |
| 6 | Randomize phases | 0 | 1 | 1 | on/off | phases randomized per bin (no phase unwrapping; see Shared section) |
| 7 | FFT Size | 0 | 7 | 2 |  | index 0..7 = 512,1024,2048,4096,8192,16384,32768,65536 values: [0]512; [1]1024; [2]2048; [3]4096; [4]8192; [5]16384; [6]32768; [7]65536; |
| 8 | Gain | -40 | 40 | 0 | dB | output level |

**Extraneous / built-in extras:** None (core + common params).


## Spectral Gliding Filters

AU id: `aufx` / `SGlf` / `MNor`. Reported AU latency (s)=0 tail (s)=0

**Author's manual text** (verbatim from guide, parameter descriptions follow the paragraph):

> Creates a series of spectral filters that glide in pitch
> Number of filters
> How many filters are to be created
> Filter width
> The size of the filter in bins
> Start frequency
> The frequency at which the filter should start its life
> Start frequency variation
> Randomization of the filter Start Frequency parameter
> Pitch glide per second
> How much drift in pitch each filter should have (per second)
> Pitch glide var
> Randomization of the Pitch Glide parameter
> Filter duration
> The lifetime of each filter
> Filter duration var
> Randomization of the Filter Duration parameter
> Silence duration
> The duration of silence before a new filter is created
> Silence duration var
> Randomization of the Silence Duration parameter

**Parameters (read from the AU binary via AudioUnitParameterInfo; exact):**

| # | name | min | max | default | unit | notes |
|---|---|---|---|---|---|---|
| 0 | Number of filters | 1 | 100 | 10 |  |  |
| 1 | Filter width | 1 | 50 | 5 |  |  |
| 2 | Start frequency | 20 | 22050 | 1200 | Hz |  |
| 3 | Start frequency variation | 1 | 11025 | 1200 | Hz |  |
| 4 | Pitch glide per second | -1200 | 1200 | 0 | cents |  |
| 5 | Pitch glide var | 0 | 1200 | 100 | cents |  |
| 6 | Filter duration | 0 | 20 | 5 | s |  |
| 7 | Filter duration var | 0 | 10 | 2 | s |  |
| 8 | Silence duration | 0 | 20 | 2 | s |  |
| 9 | Silence duration var | 0 | 10 | 1 | s |  |
| 10 | Wet dry mix | 0 | 100 | 100 | % |  |
| 11 | Lo bin cutoff | 0 | 100 | 0 | % | % of spectrum; bins below set to 0 |
| 12 | Hi bin cutoff | 0 | 100 | 100 | % | % of spectrum; bins above set to 0 |
| 13 | Randomize phases | 0 | 1 | 0 | on/off | phases randomized per bin (no phase unwrapping; see Shared section) |
| 14 | FFT Size | 0 | 7 | 2 |  | index 0..7 = 512,1024,2048,4096,8192,16384,32768,65536 values: [0]512; [1]1024; [2]2048; [3]4096; [4]8192; [5]16384; [6]32768; [7]65536; |
| 15 | Gain | -40 | 40 | 0 | dB | output level |

**Extraneous / built-in extras:** Built-in Wet/Dry mix. Many random-variance parameters (per-filter lifetime/silence scheduler).


## Spectral Granulation

AU id: `aufx` / `SGrn` / `MNor`. Reported AU latency (s)=0 tail (s)=0

**Author's manual text** (verbatim from guide, parameter descriptions follow the paragraph):

> Grains or chunks of the spectrum, of a certain frequency range and duration are taken and delayed by a certain length. Optionally, you can also apply a bin shift and a fade in/fade out to the grains.
> Grain length
> The length of each grain. NB: the actual length is rounded down to the nearest FFT length
> Grain length variance
> Randomization of the grain length (l ± var)
> Grain delay
> The length of time to delay each group
> Grain delay variance
> Randomization of the grain delay
> Bin shift
> An optional amount to shift each grain up or down the spectrum by a number of bins
> Bin shift variance
> Randomization of the bin shift parameter
> Bins per grain
> How many bins constitute a grain. The larger this parameter the more of the original sound can be detected in the output.
> Fade amount
> Fades in and out each grain for extra smoothness. 100% is maximum fading.
> Density
> The percentage of grains that are actually delayed. 100% means the entire sound is granulation. 0% means none of the sound makes it out.

**Parameters (read from the AU binary via AudioUnitParameterInfo; exact):**

| # | name | min | max | default | unit | notes |
|---|---|---|---|---|---|---|
| 0 | Grain length | 0.1 | 1 | 0.2 | s |  |
| 1 | Grain length variance | 0 | 1 | 0.1 | s |  |
| 2 | Grain delay | 0.1 | 5 | 1 | s |  |
| 3 | Grain delay variance | 0 | 2 | 0.5 | s |  |
| 4 | Bin shift | -200 | 200 | 0 |  |  |
| 5 | Bin shift variance | 0 | 200 | 0 |  |  |
| 6 | Bins per grain | 1 | 150 | 5 |  |  |
| 7 | Fade amount | 0 | 100 | 0 | % |  |
| 8 | Density | 0 | 100 | 100 | % |  |
| 9 | Lo bin cutoff | 0 | 100 | 0 | % | % of spectrum; bins below set to 0 |
| 10 | Hi bin cutoff | 0 | 100 | 100 | % | % of spectrum; bins above set to 0 |
| 11 | Randomize phases | 0 | 1 | 1 | on/off | phases randomized per bin (no phase unwrapping; see Shared section) |
| 12 | FFT Size | 0 | 7 | 2 |  | index 0..7 = 512,1024,2048,4096,8192,16384,32768,65536 values: [0]512; [1]1024; [2]2048; [3]4096; [4]8192; [5]16384; [6]32768; [7]65536; |
| 13 | Feedback | 0 | 100 | 0 | % | output fed back to input |
| 14 | Gain | -40 | 40 | 0 | dB | output level |

**Extraneous / built-in extras:** Built-in Feedback. Core = spectral-domain grains (bin-band x time chunk) delayed.


## Spectral Gate And Hold

AU id: `aufx` / `SGth` / `MNor`. Reported AU latency (s)=0 tail (s)=0

**Author's manual text** (verbatim from guide, parameter descriptions follow the paragraph):

> A similar effect to Spectral Freezing, in which each bin is watched until its amplitude exceeds a threshold level. The main difference between this effect and Spectral Freezing, is that a bin is frozen for a specific length of time, rather than until it is exceeded by a higher amplitude. In Spectral Gate and Hold, once a bin is frozen, it can have an amplitude envelope applied to it (indicated by the Fade in Length, Hold length, Hold variance and Fade Out Length parameters). During that time there is the possibility of an optional decay (indicated by the Decay factor parameter).
> Threshold
> The level above which a bin will be frozen
> Threshold brightness
> This parameter alters the threshold level over the range of the spectrum. A high brightness setting indicates a threshold that gets progressively lower as you go up the spectrum. Zero is the default.
> Fade in length
> The length of time to fade in a frozen bin for
> Hold length
> The length of time to hold a frozen bin for (after the fade in length)
> Hold variance
> The hold duration parameter can be randomized on a per-bin basis within a range of hold duration ± var
> Fade out length
> The length of time to fade out a frozen bin for
> Decay factor
> The amount that the frozen bin is decayed by over its lifetime. 100% = no decay, 0% = instantaneous decay
> Bin shift (& bin shift var)
> The number of bins to shift the sustained portion of sound. Can be useful for distorting or disguising the nature of the original sound.
> Delay vs. freq
> Can delay the onset of a frozen bin depending on its frequency. With a setting below 100%, high-frequency components enter first, while lower-frequency components enter later; with a setting above 100%, this is reversed.
> Pulsing rate & var
> An amplitude pulse can be applied to held bins to create a shimmering effect. The pulse remains constant over the lifetime of each bin, but the specific frequency of pulsing can be varied from bin to bin.
> Zero partials under threshold
> Partial falling under the threshold will be zeroed. If this is checked, then nothing of the original sound will come through, except the frozen bins.
> Normalize held partials
> Frozen bins should be set to the same level
> Normalize level
> The level to normalize frozen bins to, assuming previous parameter is checked
> Hold silence first
> A random silence is created on a per-bin basis at the beginning, so you don**t get all bins triggered at once

**Parameters (read from the AU binary via AudioUnitParameterInfo; exact):**

| # | name | min | max | default | unit | notes |
|---|---|---|---|---|---|---|
| 0 | Gate Type | 0 | 4 | 0 |  | values: [0]All partials above Threshold; [1]Use n loudest partials; [2]n loudest above Threshold; [3]Use n loudest (wait until done); [4]n loudest above Threshold (wait); |
| 1 | Threshold | -60 | 0 | -28 | dB |  |
| 2 | Threshold brightness | -100 | 100 | 0 | % |  |
| 3 | Num. partials | 0 | 50 | 2 |  |  |
| 4 | Fade in length | 0 | 10 | 5 | s |  |
| 5 | Hold length | 0 | 10 | 1 | s |  |
| 6 | Hold variance | 0 | 5 | 1 | s |  |
| 7 | Fade out length | 0 | 10 | 5 | s |  |
| 8 | Silence length | 0 | 20 | 0.5 | s |  |
| 9 | Silence length var | 0 | 10 | 0.5 | s |  |
| 10 | Decay factor | 0 | 100 | 75 | % |  |
| 11 | Bin shift | -100 | 100 | 0 |  |  |
| 12 | Bin shift var | 0 | 100 | 0 |  |  |
| 13 | Delay vs. freq | 1 | 200 | 100 | % |  |
| 14 | Pulsing rate | 0 | 5 | 0 | Hz |  |
| 15 | Pulsing rate var | 0 | 5 | 0 | Hz |  |
| 16 | Zero partials under threshold | 0 | 1 | 1 | on/off |  |
| 17 | Normalize held partials | 0 | 1 | 0 | on/off |  |
| 18 | Normalize level | -60 | 0 | -30 | dB |  |
| 19 | Hold silence first | 0 | 1 | 1 | on/off |  |
| 20 | Lo bin cutoff | 0 | 100 | 0 | % | % of spectrum; bins below set to 0 |
| 21 | Hi bin cutoff | 0 | 100 | 100 | % | % of spectrum; bins above set to 0 |
| 22 | Randomize phases | 0 | 1 | 1 | on/off | phases randomized per bin (no phase unwrapping; see Shared section) |
| 23 | FFT Size | 0 | 7 | 3 |  | index 0..7 = 512,1024,2048,4096,8192,16384,32768,65536 values: [0]512; [1]1024; [2]2048; [3]4096; [4]8192; [5]16384; [6]32768; [7]65536; |
| 24 | Gain | -40 | 40 | 0 | dB | output level |

**Extraneous / built-in extras:** EXTRANEOUS: per-bin amplitude envelope (fade in/hold/fade out), decay, bin shift, delay-vs-frequency, amplitude pulsing (own pulse oscillator), normalization, silence stage. This is a multi-effect.


## Spectral Harmonizer

AU id: `aufx` / `SHrm` / `MNor`. Reported AU latency (s)=0 tail (s)=0

**Author's manual text** (verbatim from guide, parameter descriptions follow the paragraph):

> The spectrum is transposed up or down by a set series of intervals (you have three to play with) and mixed back into the original. Each interval can then be transposed in a similar manner to the Spectral Filterbank transpositions.
> Intervals
> There are three intervals that you can define to harmonize the original sound file. If, for instance, you define interval 1 to be 400 cents, this will harmonize the sound file up a major third.
> Transposition Type
> If the no. partials per interval setting is higher than 1, you can define how the interval will be used to determine the other harmonizations. See Spectral Filterbank for more information on these transpositions
> Transpositions per interval
> For each interval, how many transpositions there are. For instance, 4 transpositions of a minor third, with the transposition type set to stacked would give you a diminished seventh chord.

**Parameters (read from the AU binary via AudioUnitParameterInfo; exact):**

| # | name | min | max | default | unit | notes |
|---|---|---|---|---|---|---|
| 0 | Use Interval 1 | 0 | 1 | 1 | on/off |  |
| 1 | Interval 1 | -1200 | 1200 | 400 | cents |  |
| 2 | Gain 1 | -20 | 0 | -0.5 | dB |  |
| 3 | Use Interval 2 | 0 | 1 | 0 | on/off |  |
| 4 | Interval 2 | -1200 | 1200 | 700 | cents |  |
| 5 | Gain 2 | -20 | 0 | -0.5 | dB |  |
| 6 | Use Interval 3 | 0 | 1 | 0 | on/off |  |
| 7 | Interval 3 | -1200 | 1200 | 0 | cents |  |
| 8 | Gain 3 | -20 | 0 | 0 | dB |  |
| 9 | Transposition Type | 0 | 2 | 1 |  | values: [0]Stacked; [1]Chord; [2]Harmonics; |
| 10 | Transpositions per interval | 0 | 10 | 1 |  |  |
| 11 | Feedback | 0 | 99 | 0 | % | output fed back to input |
| 12 | Wet/Dry Mix | 0 | 100 | 50 | % |  |
| 13 | Lo bin cutoff | 0 | 100 | 0 | % | % of spectrum; bins below set to 0 |
| 14 | Hi bin cutoff | 0 | 100 | 100 | % | % of spectrum; bins above set to 0 |
| 15 | Randomize phases | 0 | 1 | 0 | on/off | phases randomized per bin (no phase unwrapping; see Shared section) |
| 16 | FFT Size | 0 | 7 | 2 |  | index 0..7 = 512,1024,2048,4096,8192,16384,32768,65536 values: [0]512; [1]1024; [2]2048; [3]4096; [4]8192; [5]16384; [6]32768; [7]65536; |
| 17 | Gain | -40 | 40 | 0 | dB | output level |

**Extraneous / built-in extras:** Built-in Wet/Dry Mix and Feedback, 3 interval voices with individual gains.


## Spectral Partial Glide

AU id: `aufx` / `SPGl` / `MNor`. Reported AU latency (s)=0 tail (s)=0

**Author's manual text** (verbatim from guide, parameter descriptions follow the paragraph):

> This effect tracks partials, but shifts their pitches at a set amount. However, this amount can vary for each partial, to the point where you can get partials gliding in opposite directions!
> Pitch shift per second
> The central pitch shift, in cents, per second
> Pitch shift variance
> Randomization of the Pitch Shift per Second parameter, on a per partial basis
> Mirror bin
> A somewhat experimental parameter: bins below this number will glide in the opposite direction from bins above this number
> Fade in (windows)
> Apply a gradual fade in to tracked partials
> Zero transients
> Partials that only exist for a single window should be removed.

**Parameters (read from the AU binary via AudioUnitParameterInfo; exact):**

| # | name | min | max | default | unit | notes |
|---|---|---|---|---|---|---|
| 0 | Pitch shift per second | -1200 | 1200 | 0 | cents |  |
| 1 | Pitch shift variance | 0 | 1200 | 100 | cents |  |
| 2 | Mirror Bin | 0 | 100 | 0 | % |  |
| 3 | Fade in (windows) | 0 | 100 | 0 |  |  |
| 4 | Fuzziness | 0 | 10 | 5 |  |  |
| 5 | Max Bins per Partial | 1 | 50 | 30 |  |  |
| 6 | Zero Transients | 0 | 1 | 0 | on/off |  |
| 7 | Lo bin cutoff | 0 | 100 | 0 | % | % of spectrum; bins below set to 0 |
| 8 | Hi bin cutoff | 0 | 100 | 100 | % | % of spectrum; bins above set to 0 |
| 9 | Randomize phases | 0 | 1 | 0 | on/off | phases randomized per bin (no phase unwrapping; see Shared section) |
| 10 | FFT Size | 0 | 7 | 2 |  | index 0..7 = 512,1024,2048,4096,8192,16384,32768,65536 values: [0]512; [1]1024; [2]2048; [3]4096; [4]8192; [5]16384; [6]32768; [7]65536; |
| 11 | Gain | -40 | 40 | 0 | dB | output level |

**Extraneous / built-in extras:** None extraneous (partial tracker is core).


## Spectral Pitch Shift

AU id: `aufx` / `SPSh` / `MNor`. Reported AU latency (s)=0 tail (s)=0

**Author's manual text** (verbatim from guide, parameter descriptions follow the paragraph):

> A simple spectral pitch shift up or down by a set amount, using the method suggested by Mark Dolson and Jean Laroche in their paper New Phase-Vocoder Techniques for Real-Time Pitch-Shifting, Chorusing, Harmonizing and Other Exotic Audio Modifications, in which spectral peaks are tracked and shifted independently of one another. You can optionally use a Bins to Shift parameter to shift all the spectral peaks up in an inharmonic manner.
> Pitch Shift
> The amount to shift
> Use Bins to Shift parameter
> Instead of using an interval, shift by a set number of bins (this is different from the spectral bin shift, in that it shifts individual peaks by a set number of bins, rather than the entire spectrum)
> Bins to shift
> The number of bins to shift, if the above parameter is checked
> Rotate
> Should bins that fall off the end be rotated around to the other end?

**Parameters (read from the AU binary via AudioUnitParameterInfo; exact):**

| # | name | min | max | default | unit | notes |
|---|---|---|---|---|---|---|
| 0 | Pitch shift | -2400 | 2400 | 1200 | cents |  |
| 1 | Use 'Bins to Shift' Parameter | 0 | 1 | 0 | on/off |  |
| 2 | Bins to Shift | -50 | 50 | 1 |  |  |
| 3 | Rotate | 0 | 1 | 0 | on/off |  |
| 4 | Feedback | 0 | 99 | 10 | % | output fed back to input |
| 5 | Gain | -40 | 40 | 0 | dB | output level |

**Extraneous / built-in extras:** Built-in Feedback (default 10 %). Dolson/Laroche peak-tracking shifter.


## Spectral Pulsing

AU id: `aufx` / `SPsg` / `MNor`. Reported AU latency (s)=0 tail (s)=0

**Author's manual text** (verbatim from guide, parameter descriptions follow the paragraph):

> A rather experimental effect, that switches bins on and off across the spectrum in a pulsating fashion.
> On duration
> The length of time to keep bin zero switched on
> Off duration
> The length of time to keep bin zero switched off (after the on duration)
> On multiplier freq
> As you go up the frequency spectrum, multiply the on duration by this number
> Off multiplier freq
> As you go up the frequency spectrum, multiply the off duration by this number
> On multiplier time
> As you move forward in time, multiply the on duration by this number
> Off multiplier time
> As you move forward in time, multiply the off duration by this number
> Min on duration
> Set the minimum length for the on duration
> Max on duration
> Set the maximum length for the on duration
> Min off duration
> Set the minimum length for the off duration
> Max off duration
> Set the maximum length for the off duration
> Modulo type
> 1= ping-pong between min and max; 2 = wrap round from max to min/min to max

**Parameters (read from the AU binary via AudioUnitParameterInfo; exact):**

| # | name | min | max | default | unit | notes |
|---|---|---|---|---|---|---|
| 0 | On duration | 0 | 5 | 0.2 | s |  |
| 1 | Off duration | 0 | 5 | 0.2 | s |  |
| 2 | On multiplier freq | 0.1 | 5 | 1.1 |  |  |
| 3 | Off multiplier freq | 0.1 | 5 | 1.2 |  |  |
| 4 | On multiplier time | 0.1 | 5 | 1 |  |  |
| 5 | Off multiplier time | 0.1 | 5 | 1 |  |  |
| 6 | Min on duration | 0 | 5 | 0.2 | s |  |
| 7 | Max on duration | 0 | 5 | 0.6 | s |  |
| 8 | Min off duration | 0 | 5 | 0.2 | s |  |
| 9 | Max off duration | 0 | 5 | 0.4 | s |  |
| 10 | Modulo type | 1 | 2 | 1 |  |  |
| 11 | Brightness | -100 | 100 | 0 | % | tilt of high-frequency strength |
| 12 | Gain | -40 | 40 | 0 | dB | output level |

**Extraneous / built-in extras:** Has only Brightness and Gain as common params: NO FFT size, NO lo/hi cutoff, NO randomize-phases control exposed (fixed internal FFT).


## Spectral Stretch

AU id: `aufx` / `SSch` / `MNor`. Reported AU latency (s)=0 tail (s)=0

**Author's manual text** (verbatim from guide, parameter descriptions follow the paragraph):

> An effect to create a variety of inharmonic stretches. Spectral peaks are tracked and shifted by the quadratic equation: βω+αω2, where ω is the frequency of each bin.
> Alpha stretch
> The value for α in the equation above
> Beta stretch
> The value for β in the equation above, which can be varied using the beta stretch variance parameter
> Variance type
> Allows the β value to be varied automatically (Random = randomly; LFO = sin wave; Link amplitude = amount of variance relates to amplitude of sound; Link inv amp = related inversely to amplitude)
> Variance quantization
> While the value of β is being varied, it can be rounded so that the pitch steps up and down, often creating a form of harmonic series. The higher this value, the more stepped the pitch is.

**Parameters (read from the AU binary via AudioUnitParameterInfo; exact):**

| # | name | min | max | default | unit | notes |
|---|---|---|---|---|---|---|
| 0 | Alpha stretch | 0.01 | 5 | 2 |  |  |
| 1 | Beta stretch | 0.5 | 3 | 1 |  |  |
| 2 | Variance type | 0 | 4 | 0 |  | values: [0]None; [1]Random; [2]LFO; [3]Link amplitude; [4]Link inverted amp; |
| 3 | Beta stretch variance | 0 | 2 | 0.5 |  |  |
| 4 | Variance quantization | 0 | 100 | 1 |  |  |
| 5 | LFO rate | 0.01 | 50 | 1 | Hz |  |
| 6 | Lo bin cutoff | 0 | 100 | 0 | % | % of spectrum; bins below set to 0 |
| 7 | Hi bin cutoff | 0 | 100 | 100 | % | % of spectrum; bins above set to 0 |
| 8 | Randomize phases | 0 | 1 | 0 | on/off | phases randomized per bin (no phase unwrapping; see Shared section) |
| 9 | FFT Size | 0 | 7 | 3 |  | index 0..7 = 512,1024,2048,4096,8192,16384,32768,65536 values: [0]512; [1]1024; [2]2048; [3]4096; [4]8192; [5]16384; [6]32768; [7]65536; |
| 10 | Gain | -40 | 40 | 0 | dB | output level |

**Extraneous / built-in extras:** Own LFO (LFO rate) for beta variance.


## Spectral Shuffle

AU id: `aufx` / `SSfl` / `MNor`. Reported AU latency (s)=0 tail (s)=0

**Author's manual text** (verbatim from guide, parameter descriptions follow the paragraph):

> Randomly shuffles blocks of bins around within the spectrum. You have control over the Shuffle factor (the amount of blocks that are shuffled), the Shuffle range (how far away from the original position each block is moved) and the number of bins per block.
> Shuffle factor
> The number of groups that are shuffled, as a percentage of the FFT size.
> Shuffle range
> How far away, in bins, each group is moved from its original position.
> Bins per group
> The number of bins that are kept together in a block.

**Parameters (read from the AU binary via AudioUnitParameterInfo; exact):**

| # | name | min | max | default | unit | notes |
|---|---|---|---|---|---|---|
| 0 | Shuffle factor | 0 | 100 | 10 | % |  |
| 1 | Shuffle range | 1 | 50 | 5 | % |  |
| 2 | Bins per group | 1 | 50 | 1 | % |  |
| 3 | Lo bin cutoff | 0 | 100 | 0 | % | % of spectrum; bins below set to 0 |
| 4 | Hi bin cutoff | 0 | 100 | 100 | % | % of spectrum; bins above set to 0 |
| 5 | Randomize phases | 0 | 1 | 1 | on/off | phases randomized per bin (no phase unwrapping; see Shared section) |
| 6 | FFT Size | 0 | 7 | 2 |  | index 0..7 = 512,1024,2048,4096,8192,16384,32768,65536 values: [0]512; [1]1024; [2]2048; [3]4096; [4]8192; [5]16384; [6]32768; [7]65536; |
| 7 | Gain | -40 | 40 | 0 | dB | output level |

**Extraneous / built-in extras:** None (core + common params).


## Spectral Bin Shift

AU id: `aufx` / `SSft` / `MNor`. Reported AU latency (s)=0 tail (s)=0

**Author's manual text** (verbatim from guide, parameter descriptions follow the paragraph):

> Shifts the entire spectrum up or down a set number of bins. Because the bins of the FFT are equally-spaced, not logarithmically spaced, the spectrum becomes rather compressed or expanded the further away you get, and as a result, strange, inharmonic spectra result which sound quite distant from the original. Can be useful for then feeding into another effect (e.g. spectral gran, or time stretching)
> Bins to shift
> The number of bins to offset the entire spectrum
> Rotate
> Should bins that fall off the end be rotated around to the other side?
> Instantaneous feedback
> The bin-shifting is fed back within the same frame (as opposed to normal feedback, in which the output is fed back into the following input frame)
> Num instant feedback
> How many times the instantaneous feedback algorithm should be calculated

**Parameters (read from the AU binary via AudioUnitParameterInfo; exact):**

| # | name | min | max | default | unit | notes |
|---|---|---|---|---|---|---|
| 0 | Bins to Shift | -200 | 200 | -10 |  |  |
| 1 | Rotate | 0 | 1 | 0 | on/off |  |
| 2 | Instantaneous feedback | 0 | 99 | 0 | % |  |
| 3 | Num instant feedback | 0 | 10 | 0 |  |  |
| 4 | Feedback | 0 | 99 | 0 | % | output fed back to input |
| 5 | Original multiplier | 0 | 100 | 0 | % |  |
| 6 | Shifted multiplier | 0 | 100 | 100 | % |  |
| 7 | Brightness | -100 | 100 | 0 | % | tilt of high-frequency strength |
| 8 | Lo bin cutoff | 0 | 100 | 0 | % | % of spectrum; bins below set to 0 |
| 9 | Hi bin cutoff | 0 | 100 | 100 | % | % of spectrum; bins above set to 0 |
| 10 | Randomize phases | 0 | 1 | 0 | on/off | phases randomized per bin (no phase unwrapping; see Shared section) |
| 11 | FFT Size | 0 | 7 | 2 |  | index 0..7 = 512,1024,2048,4096,8192,16384,32768,65536 values: [0]512; [1]1024; [2]2048; [3]4096; [4]8192; [5]16384; [6]32768; [7]65536; |
| 12 | Gain | -40 | 40 | 0 | dB | output level |

**Extraneous / built-in extras:** Built-in Feedback, instantaneous (same-frame) feedback, and Original/Shifted multipliers acting as an internal dry/wet-style mix of unshifted vs shifted spectrum.


## Spectral Shimmer

AU id: `aufx` / `SShr` / `MNor`. Reported AU latency (s)=0 tail (s)=0

**Author's manual text** (verbatim from guide, parameter descriptions follow the paragraph):

> A variety of ways to add shimmer, jitter, sparkle, pulsation or sizzle to your sound.
> Jitter amount
> The percentage of bins that are jittered (a random fluctuation in amplitude)
> Jitter only above
> Allows jitter only above a certain frequency
> Low-frequency jitter
> The maximum amount of jitter (in dB) at 0Hz
> High-frequency jitter
> The maximum amount of jitter (in dB) at the Nyquist limit. The amount of jitter is interpolated between this value and the low-frequency jitter over the range.
> Jitter gate level
> Only apply jitter if the bin is above a certain amplitude (a low level here can create a real sizzle in the higher frequencies)
> Jitter hold (windows)
> The number of windows for which a jitter is held on a bin — a larger value makes less of a jitter and more of a kind of surging sound
> Amplitude pulse type
> Applies an amplitude pulsation wave across the spectrum. Square uses blocks of bins (of size set by the Pulse Width parameter), either of max or min pulse level. Sin uses a sin wave across the spectrum ranging between max & min levels.
> Pulse level max
> The maximum amplification of the pulse wave
> Pulse level min
> The minimum amplification of the pulse wave
> Pulse width
> The number of bins that a pulsation wave is spread across
> Pulse rate
> The rate at which the pulsation wave is cycled up or down across the spectrum
> Delay variance
> Delays each bin in a programmatic way, creating sweeps up or down or random delay fluctuations. The Delay Variance parameter sets the maximum delay, while the Delay Type parameter sets the particular method by which bins are delayed.

**Parameters (read from the AU binary via AudioUnitParameterInfo; exact):**

| # | name | min | max | default | unit | notes |
|---|---|---|---|---|---|---|
| 0 | Jitter amount | 0 | 100 | 100 | % |  |
| 1 | Jitter only above | 0 | 11025 | 0 | Hz |  |
| 2 | Low-frequency jitter | -20 | 30 | 0 | dB |  |
| 3 | High-frequency jitter | -20 | 30 | 15 | dB |  |
| 4 | Jitter gate level | -60 | 0 | -50 | dB |  |
| 5 | Jitter hold (windows) | 1 | 100 | 100 |  |  |
| 6 | Jitter hold var | 0 | 100 | 60 |  |  |
| 7 | Amplitude pulse type | 0 | 2 | 1 |  | values: [0]None; [1]Square; [2]Sin; |
| 8 | Pulse level max | 0 | 20 | 0 | dB |  |
| 9 | Pulse level min | -60 | 0 | -10 | dB |  |
| 10 | Pulse width | 10 | 1024 | 200 |  |  |
| 11 | Pulse rate | -1000 | 1000 | 100 | Hz |  |
| 12 | Delay variance type | 0 | 5 | 3 |  | values: [0]None; [1]Descent; [2]Ascent; [3]LFO; [4]Link amp; [5]Random; |
| 13 | LFO rate (delay var) | 0.01 | 50 | 50 | Hz |  |
| 14 | Delay variance | 0 | 250 | 70 |  |  |
| 15 | Brightness | -100 | 100 | 20 | % | tilt of high-frequency strength |
| 16 | Feedback | 0 | 100 | 0 | % | output fed back to input |
| 17 | Lo bin cutoff | 0 | 100 | 0 | % | % of spectrum; bins below set to 0 |
| 18 | Hi bin cutoff | 0 | 100 | 100 | % | % of spectrum; bins above set to 0 |
| 19 | Randomize phases | 0 | 1 | 0 | on/off | phases randomized per bin (no phase unwrapping; see Shared section) |
| 20 | FFT Size | 0 | 7 | 2 |  | index 0..7 = 512,1024,2048,4096,8192,16384,32768,65536 values: [0]512; [1]1024; [2]2048; [3]4096; [4]8192; [5]16384; [6]32768; [7]65536; |
| 21 | Gain | -40 | 40 | 0 | dB | output level |

**Extraneous / built-in extras:** EXTRANEOUS: multi-effect: (a) per-bin amplitude jitter, (b) spectrum-wide amplitude pulse wave (square/sin), (c) per-bin delay variance with own LFO, plus Brightness and Feedback.


## Spectral Tracing

AU id: `aufx` / `STra` / `MNor`. Reported AU latency (s)=0 tail (s)=0

**Author's manual text** (verbatim from guide, parameter descriptions follow the paragraph):

> Typically used to retain only the n loudest bins in a spectrum and remove the others. Alternatively you can retain the n softest bins, or set a threshold level (a gate) below which all partials are zeroed, or above which all partials are zeroed.
> Tracing type
> Choose from eight different algorithms: retain the n loudest, the n softest, bins between a range, bins above the threshold or bins below the threshold; you can also remove the n loudest, softest or between a range.
> Number of bins 1
> The number of bins to retain/remove
> Number of bins 2
> Used to define the lower end of the range when using one of the two n1 & n2 tracing type settings
> Threshold
> The threshold in dB; retains bins above or below this threshold if using one of the last two trancing types

**Parameters (read from the AU binary via AudioUnitParameterInfo; exact):**

| # | name | min | max | default | unit | notes |
|---|---|---|---|---|---|---|
| 0 | Tracing type | 0 | 7 | 0 |  | values: [0]Keep loudest n1 bins; [1]Remove loudest n1 bins; [2]Keep softest n1 bins; [3]Remove softest n1 bins; [4]Keep bins between n1 & n2; [5]Remove bins between n1 & n2; [6]Keep bins above threshold; [7]Keep bins below threshold; |
| 1 | Number of bins 1 | 1 | 512 | 16 |  |  |
| 2 | Number of bins 2 | 1 | 512 | 50 |  |  |
| 3 | Threshold | -60 | 0 | -20 | dB |  |
| 4 | Lo bin cutoff | 0 | 100 | 0 | % | % of spectrum; bins below set to 0 |
| 5 | Hi bin cutoff | 0 | 100 | 100 | % | % of spectrum; bins above set to 0 |
| 6 | Randomize phases | 0 | 1 | 0 | on/off | phases randomized per bin (no phase unwrapping; see Shared section) |
| 7 | FFT Size | 0 | 7 | 2 |  | index 0..7 = 512,1024,2048,4096,8192,16384,32768,65536 values: [0]512; [1]1024; [2]2048; [3]4096; [4]8192; [5]16384; [6]32768; [7]65536; |
| 8 | Gain | -40 | 40 | 0 | dB | output level |

**Extraneous / built-in extras:** Gate-style threshold modes built in (keep above/below). Otherwise core.


## Spectral Weave

AU id: `aufx` / `SWve` / `MNor`. Reported AU latency (s)=0 tail (s)=0

**Author's manual text** (verbatim from guide, parameter descriptions follow the paragraph):

> This effect waits until the amplitude of the sound exceeds a threshold, and then further waits until the sound is proven stable enough to be considered a steady-state sound (such as a sustained pitch). Once this state has been reached, a snapshot of the entire spectrum (called a thread) is saved into one of the effect**s buffers. These threads are then faded in and out (weaved), in a wave-like fashion, creating an undulating texture of frozen audio from the input file.
> Threshold
> The level above which the effect will start listening for steady-state sound
> Num. threads
> The maximum number of spectral snapshots to be maintained in the effect
> Thread life
> The length of time a thread should exist for
> Max. stability
> The maximum dynamic variation permitted within the sound to constitute a steady state
> Min . stability length
> The minimum length of time that the dynamic variation needs to remain within the max.stability threshold to be considered a steady state
> Hold length
> The length of each weave of the thread
> Hold length variance
> The amount of randomization (+/-) for the hold length
> Fade in out %
> The amount of fade in and fade out envelope applied to each weave. 50% means that the first half of the weave will be faded in, then immediately faded out
> Silence length
> The amount of silence between each weave
> Silence length var
> The amount of randomization (+/-) for the silence length
> Hold out length
> The amount of time following the detection of a steady state that the effect stops listening for further steady states (to avoid multiple threads of the same audio input)
> Pulse frequency
> A pulsation can be applied to each weave
> Pulse gain
> The gain of the troughs of each pulse (peaks are always assumed to be 0dB from the original dynamic of the weave)
> Normalize
> Should the effect normalize each thread before weaving?
> Normalize level
> If so, what level should it normalize to?

**Parameters (read from the AU binary via AudioUnitParameterInfo; exact):**

| # | name | min | max | default | unit | notes |
|---|---|---|---|---|---|---|
| 0 | Threshold | -60 | 0 | -28 | dB |  |
| 1 | Num. threads | 1 | 15 | 8 |  |  |
| 2 | Thread life | 1 | 120 | 40 | s |  |
| 3 | Max. stability | 1 | 10 | 5 | dB |  |
| 4 | Min. stability length | 0.1 | 5 | 0.8 | s |  |
| 5 | Hold length | 0 | 30 | 10 | s |  |
| 6 | Hold length variance | 0 | 5 | 0 | s |  |
| 7 | Fade in/out % | 0 | 50 | 50 | % |  |
| 8 | Silence length | 0 | 20 | 5 | s |  |
| 9 | Silence length var | 0 | 10 | 0.5 | s |  |
| 10 | Hold out Length | 0 | 10 | 5 | s |  |
| 11 | Pulse Frequency | 0 | 10 | 0 | Hz |  |
| 12 | Pulse Gain | -60 | 0 | 0 | dB |  |
| 13 | Normalize | 0 | 1 | 0 | on/off |  |
| 14 | Normalize level | -60 | 0 | -30 | dB |  |
| 15 | Lo bin cutoff | 0 | 100 | 0 | % | % of spectrum; bins below set to 0 |
| 16 | Hi bin cutoff | 0 | 100 | 100 | % | % of spectrum; bins above set to 0 |
| 17 | Randomize phases | 0 | 1 | 1 | on/off | phases randomized per bin (no phase unwrapping; see Shared section) |
| 18 | FFT Size | 0 | 7 | 3 |  | index 0..7 = 512,1024,2048,4096,8192,16384,32768,65536 values: [0]512; [1]1024; [2]2048; [3]4096; [4]8192; [5]16384; [6]32768; [7]65536; |
| 19 | Gain | -40 | 40 | 0 | dB | output level |
| 20 | Wet/dry | 0 | 1 | 0.5 | % |  |

**Extraneous / built-in extras:** Built-in Wet/dry (0..1 %-type) mix, pulse oscillator (Pulse Frequency/Gain), normalization.


## Super Filterbank

AU id: `aufx` / `SuFB` / `MNor`. Reported AU latency (s)=0 tail (s)=0

**Author's description**: "A standard time-domain implementation of a bank of bandpass filters, with the option to choose the filter type, including comb filter, biquad, butterworth." (v1.5 manual; replaced "Mr Filterbank"/Comb Filterbank in Beta 7). No per-parameter text in the guide.

**Parameters (read from the AU binary via AudioUnitParameterInfo; exact):**

| # | name | min | max | default | unit | notes |
|---|---|---|---|---|---|---|
| 0 | Fundamental frequency | 10 | 20000 | 200 | Hz |  |
| 1 | Interpolation time | 0 | 60 | 5 | s |  |
| 2 | Scale | 0 | 12 | 5 |  | values: [0]Octaves; [1]Fifths; [2]Major triad; [3]Minor triad; [4]Major scale; [5]Dorian scale; [6]Lydian scale; [7]Mixolydian scale; [8]Aeolian scale; [9]Whole-tone scale; [10]Octatonic scale; [11]Hexatonic scale; [12]Acoustic scale; |
| 3 | Number of filters | 1 | 120 | 36 |  |  |
| 4 | Bandwidth | 0.01 | 100 | 10 | cents |  |
| 5 | Filter type | 0 | 5 | 1 |  | values: [0]Comb; [1]Second-order bandpass; [2]Butterworth; [3]Biquad; [4]Constant gain; [5]Goertzel; |
| 6 | Wet/dry mix | 0 | 100 | 100 | % |  |
| 7 | Gain | -30 | 30 | 0 | dB | output level |

**Extraneous / built-in extras:** Time-domain (not spectral). Built-in Wet/dry mix, interpolation time (glide between fundamentals) and 6 filter types.
