# Voice Stack — Options & Tradeoffs

> **Status:** Decision-groundwork doc. Research only — nothing here is built by this document.
> **Purpose:** Give you enough real information to make voice-stack upgrade choices later, without having to become a speech-processing specialist first.
> **Companion to:** `SERVER_AGENT_SPEC.md` §3 (brain), §4 (speech-in), §11 (open items).

---

## 0. Where things actually stand right now

Facts, not plans:

| Thing | Reality |
|---|---|
| Host | Raspberry Pi 4, 8GB RAM, Debian 13 aarch64 |
| Pi audio hardware | **None connected.** No mic, no speakers. An audio interface is planned but not present. |
| STT installed | `faster-whisper` 1.2.1 + `ctranslate2`, working on CPU |
| TTS installed | `espeak-ng` available; `ffmpeg` available |
| Being built now | Local faster-whisper (`base.en`, int8) + espeak-ng, both behind HTTP endpoints, consumed by an **Android PWA** doing push-to-talk in the browser |
| LLM access | Claude **Max subscription** via the `claude` CLI on the Pi — not a metered API key |
| Cloud speech keys | **None.** No Groq key, no Deepgram key. |
| Stated preference | "Highest accuracy, higher than average STT; down for free online processing; also down for a good local model." |

**The one structural fact that shapes everything below:** because the Pi has no mic yet, *the phone is currently the only microphone in the system*. That makes the phone a far more capable and far more convenient place to do speech work than it will be six months from now, and it means some of the best options today are ones that stop being the obvious choice once the Pi gets ears.

Note also that `SERVER_AGENT_SPEC.md` §4 was written against a different assumption — that a Groq API key would exist and the Pi would have a good mic. Neither is true yet. This doc supersedes the *practical* recommendation in §4 without contradicting its long-term direction.

---

## 1. Speech-to-text

### 1a. Local faster-whisper on the Pi 4 CPU — *what's being built now*

The Pi 4's CPU is a 4-core Cortex-A72 at ~1.5–1.8 GHz. It is a genuinely weak host for Whisper. This is worth saying plainly rather than discovering it later: **the Pi 4 is the slowest realistic place in your whole system to run speech recognition.** Your phone is several times faster at this. Any cloud endpoint is dozens of times faster.

Rough expectations for a **~5 second utterance**, int8 quantized, 4 threads. These are **estimates** based on typical ARM CPU real-time factors, not benchmarks measured on your box — treat them as order-of-magnitude, and measure once it's running:

| Model | Est. transcribe time (5s audio) | RAM | Accuracy character |
|---|---|---|---|
| `tiny.en` | ~1–2 s | ~150 MB | Poor. Mangles names, technical terms, anything unusual. This is what made speech-in "bad" historically. |
| `base.en` | ~2–4 s | ~250 MB | Usable for everyday phrasing. Still weak on jargon, proper nouns, acronyms. **Current choice.** |
| `small.en` | ~7–15 s | ~700 MB | Noticeably better — this is where Whisper starts feeling reliable. Too slow for conversation on this CPU. |
| `medium.en` | ~25–60 s | ~2.5 GB | Good accuracy, completely impractical here. |
| `large-v3` | minutes | ~5 GB | Not viable on a Pi 4. Don't. |

Add **model load time on first call** (a few seconds for base, tens of seconds for small/medium) unless you keep the model warm in a long-lived process. Keep it warm — this is free latency savings.

The shape of the tradeoff: on this hardware, the models that are fast enough to feel conversational are the ones that aren't accurate enough to satisfy your stated "higher than average accuracy" preference, and vice versa. `base.en` is the compromise point, and it's a real compromise, not a sweet spot. There's no local-on-Pi-4 configuration that gives you both.

Two things partially rescue it:
- **A good microphone is worth roughly one model tier.** Whisper's accuracy is extremely sensitive to input quality. `base.en` with a clean close-mic signal beats `small.en` with a bad far-field mic. When the audio interface arrives, re-evaluate before assuming you need a bigger model.
- **Warm process + short utterances.** Push-to-talk gives you clean utterance boundaries and short clips, which is the best case for batch Whisper.

**Verdict:** genuinely valuable as the *offline fallback* and as the thing that works with zero accounts and zero network. Not the right primary if accuracy is your top priority.

### 1b. Groq-hosted `whisper-large-v3-turbo` — *the accuracy answer*

Groq serves `whisper-large-v3-turbo` on their inference hardware with an OpenAI-compatible API. You send a WAV, you get a transcript back.

- **Accuracy:** effectively large-v3 class — the top tier of open Whisper. Handles technical vocabulary, proper nouns, and accents dramatically better than `base.en`. This is the single biggest accuracy jump available to you.
- **Latency:** typically a few hundred milliseconds for a short clip, plus your network round-trip. Faster than `base.en` locally *despite* going over the internet — that's how large the compute gap is.
- **Cost:** Groq has a free tier with rate limits that is comfortably sufficient for personal use. Paid usage is priced per hour of audio and is cheap enough to be a rounding error at household volume. **Verify current free-tier limits and pricing when you sign up — these change.**
- **Setup friction:** low. Create a Groq account, generate an API key, set an env var, point the STT backend at their endpoint. `SERVER_AGENT_SPEC.md` §4 already anticipates a swappable backend behind `engine/stt.py`, so this is a small change.
- **Privacy:** **your audio leaves the house.** Every utterance goes to a third party. Worth deciding deliberately for a device that lives in a room you live in — especially if you ever add always-on listening. Push-to-talk limits exposure to things you deliberately spoke into the app; a wake-word loop does not.

That privacy point is the only real argument against, and how much it weighs is a personal call, not a technical one.

### 1c. On-device / on-phone STT

Two genuinely different things get called this. They're worth separating.

**Browser Web Speech API (`SpeechRecognition`) on Android Chrome**

- **It is not on-device.** Despite feeling instant and native, Chrome's `SpeechRecognition` streams audio to Google's servers for recognition. It has the same "audio leaves the house" property as Groq, just to a different company, and with less transparency about it. Don't choose it *for* privacy.
- **Free, no API key, no account, essentially zero setup.** A handful of lines of JS in the PWA you're already building. This is by far the lowest-effort option in this entire document.
- **Accuracy:** very good for everyday conversational speech — it's the same engine behind Android voice typing, tuned hard on exactly that. Noticeably weaker than large-v3 on technical jargon, code identifiers, product names, and unusual proper nouns. If you say "set the FCOB strip to a sunset scene," expect to fight it.
- **Latency:** low, with live interim results as you speak, which feels great in a UI.
- **Requires network.** No offline mode you can rely on.
- **Caveat:** it's a Chrome/Android-flavored API. Support and behavior on iOS Safari is inconsistent — if the PWA ever needs to work on an iPhone, this is a portability hazard.

**whisper.cpp / Whisper running natively on the phone**

- A modern Android phone has *far* more usable compute than a Pi 4 — a recent flagship can run `small` or even `medium` Whisper faster than the Pi runs `base`. This is the one option that gives you both real accuracy and real on-device privacy.
- **But it requires a native Android app.** A PWA cannot do this — you'd be leaving the browser, taking on an Android build toolchain, app packaging, model shipping (hundreds of MB), and a whole second codebase to maintain. That is a large jump in effort from "a web page served off the Pi."
- Worth keeping on the map as the endgame for a privacy-maximal, network-independent setup. Not worth doing now.

### 1d. Deepgram Nova-3 (streaming)

- **True streaming STT** — transcripts arrive while you're still talking, not after you stop.
- The reason to care isn't accuracy (it's competitive with large-v3-turbo, not obviously better for your use); it's that **streaming is the prerequisite for barge-in** — interrupting the agent mid-reply. See §3c.
- **Paid.** Priced per minute of audio; there's typically signup credit. Meaningful integration work: WebSocket streaming, interim-vs-final result handling, endpointing.
- Nothing about the current push-to-talk design needs it. Adding it now would be building the plumbing for a feature you don't have.

### 1e. Recommendation — STT

Given "highest accuracy, fine with free online processing":

**Do this now (in order):**

1. **Keep local `base.en` as the fallback path.** It's already built, it costs nothing, it works with no network and no accounts. Don't throw it away — it's your offline mode and your "the internet is down" mode.
2. **Get a Groq API key and make Groq `whisper-large-v3-turbo` the default backend.** This is the highest-value single change available to your voice stack, full stop. It directly satisfies your accuracy preference, it's free at your volume, it's *faster* than the local path, and it's maybe an hour of work behind an already-planned backend switch. The only cost is the privacy tradeoff, which you should make consciously.
3. Keep the backend swappable by env var — `STT_BACKEND=groq|local` — so falling back is a config change, not a code change.

**Explicitly don't:** don't chase `small.en`/`medium.en` on the Pi 4. You'll spend the effort and end up with something slow *and* still worse than the free cloud option.

**When to upgrade, and to what:**

| Trigger | Move to |
|---|---|
| Transcription of technical terms / names / acronyms is frustrating | **Groq large-v3-turbo** — this is the fix |
| You want the absolute lowest-effort phone voice input and can tolerate jargon errors | **Web Speech API in the PWA** — free, ~20 lines, no key |
| You start caring that audio leaves the house (esp. if you add a wake word) | **Local faster-whisper**, and revisit a native phone app with whisper.cpp |
| You want to interrupt the agent mid-sentence | **Deepgram Nova-3** streaming — nothing else gets you there |
| The Pi gets a good mic and local accuracy suddenly seems better than expected | Re-measure `base.en`. Good input is worth a model tier. |
| Pi 4 STT is the round-trip bottleneck and you don't want cloud | Move the Pi's STT job to a beefier box on the LAN, or to a Pi 5 |

---

## 2. Text-to-speech

The framing here is different from STT, because you're not optimizing for quality — you're optimizing for **identity**. The agent is supposed to sound like a Speak-and-Spell. That changes which tradeoffs are actually tradeoffs.

### 2a. espeak-ng on the Pi — *what's being built now*

- Free, fully offline, **effectively instant** (formant synthesis, no neural model, no meaningful latency at all).
- Sounds robotic. Thin, buzzy, formant-synth robotic.
- **This is arguably a feature, not a compromise.** The persona is explicitly a Speak-and-Spell voice on a Raspberry Pi. Every other TTS project treats "robotic" as the failure mode to escape; here it's the target. espeak-ng lands closer to the intended identity out of the box than any modern neural voice would.
- It's also already a first-class mode in the SPEAK engine (`VALID_MODES` includes `"espeak"`, `engine/tts.py:espeak_synth()`), so it isn't a hack — it's a supported path that has simply never been the default.

It is *not* the authentic TI-LPC sound. espeak-ng is a generic formant synth; the Speak-and-Spell character in SPEAK comes from the `ti_color()` post-processor, which espeak mode currently bypasses. Which leads directly to:

### 2b. espeak-ng → the existing SPEAK TI-LPC chain — *the identity-preserving path*

**Feasibility: high. This is a smaller change than it sounds like.**

I looked at the engine. The critical detail:

```python
# engine/ticolor.py
def ti_color(samples: np.ndarray, sr: int, amount=1.0, sing=False, chirp_gain=None)
```

`ti_color()` takes **raw samples and a sample rate**. It has no idea where the audio came from. It resamples to 8 kHz internally, does the 3-bit LPC recolor, bandpasses to 300–3400 Hz, and stamps TMS5100 chirp transients on voiced regions. Nothing in it is macOS-specific.

The macOS dependency is not in the TI processing — it's in the *routing*:

- `engine/voice.py` line ~324: `if mode == "espeak": return espeak_synth(text)` — espeak output is returned **raw**, never passed through `ti_color()`. That's the whole reason espeak mode sounds like plain espeak instead of like a Speak-and-Spell.
- The `mix` mode (the actual S&S voice) hardcodes `say_synth(phrase)` in several places (lines ~357, ~375, ~433, ~447) rather than calling a configurable base-voice function.

**What would need to change:**

1. Introduce a base-voice indirection — a `_base_synth(text)` that resolves to `say_synth` on macOS and `espeak_synth` on Linux (env var override), and replace the hardcoded `say_synth(...)` call sites in `voice.py` with it. Mechanical, maybe a dozen lines.
2. Optionally add a new mode (e.g. `espeak_ti`) rather than changing `mix`'s meaning, so the Mac behavior is untouched.
3. **Retune `ti_amount` and the chirp gain for the new source.** This is the real work, and it's ears-on, not code. The TI recolor was tuned against Fred's spectral character; espeak's formant output is already thin and buzzy, so applying full-strength TI crunch on top may over-cook it. Expect to want a *lower* `ti_amount` than on macOS. Budget an evening of listening, not an afternoon of coding.
4. **The elongation path needs checking.** `voice.py`'s held-tone feature uses pyworld freeze-frame resynthesis of the base voice's own vowel plus pitch detection (`_natural_pitch_hz`). That's source-agnostic in principle, but espeak's flatter, more synthetic pitch contour may make detection less stable. Test `hellooooo` early. Same for the singer.

Dependency check: `pyworld` and `librosa` need to build/install on aarch64. `pyworld` in particular has historically been the fussiest of SPEAK's deps. Verify that before committing to this path — if it won't install, elongation and singing are off the table on the Pi regardless of the base voice.

**This is the correct long-term answer for TTS.** It preserves the identity, it's fully offline, it's free, and the engine is 90% there already. It's just not the *first* thing to do.

### 2c. Piper TTS — *the "sound good instead of retro" option*

- Modern neural TTS, small ONNX models, designed for exactly this hardware. Runs comfortably on a Pi 4 — typically faster than real time for the low/medium quality voices, meaning a sentence synthesizes in well under the time it takes to speak it. (Estimate; measure it.)
- Fully offline, no accounts, many voices, genuinely pleasant quality — this is the current default answer for "good offline TTS on a Pi."
- Installation is straightforward: a binary plus a voice model file (tens of MB).

**But:** it makes the agent sound like a normal, decent TTS voice. That is the opposite of the design goal. Choosing Piper is choosing to abandon the Speak-and-Spell identity for audio quality.

One interesting middle path worth noting: **Piper → `ti_color()`**. Because `ti_color()` is source-agnostic, a clean neural base voice would feed it *better* than espeak does — arguably better than Fred did, since Fred was itself just a convenient clean source. If the espeak→TI experiment sounds too thin, Piper→TI is the obvious next thing to try, and it costs you nothing extra architecturally. This is probably the highest-ceiling TTS configuration available to you.

### 2d. Browser `speechSynthesis` on Android

- Free, zero setup, no network needed (uses the phone's installed voices), effectively zero latency, and it removes TTS from the Pi's workload entirely.
- **It sounds like a generic phone assistant.** Total loss of identity. The agent stops being a Speak-and-Spell on a Raspberry Pi and becomes Google TTS reading text.
- Voice availability varies by phone and Android version, so it's not consistent across devices.

Genuinely useful as a **fallback** — if the Pi's TTS endpoint is unreachable or slow, having the PWA fall back to `speechSynthesis` means the app still talks. Bad as a primary.

### 2e. Recommendation — TTS

**Now:** keep espeak-ng on the Pi. It's built, it's free, it's instant, and being robotic is on-brand. Ship it and use it.

**Optionally, cheaply:** add `speechSynthesis` in the PWA as a fallback for when the Pi TTS endpoint fails. Small, defensive, no identity cost since it only fires when the real voice is unavailable.

**The identity upgrade (do this when the current voice starts annoying you):** wire espeak-ng through `ti_color()` via the base-voice indirection described in §2b. This is the path that actually delivers the Speak-and-Spell character on Linux, and it's the one that keeps `SERVER_AGENT_SPEC.md` §3's promise that "the S&S *character* is preserved; only the base-voice source changes."

**If espeak→TI sounds too thin:** try Piper→TI before giving up on the identity. Clean base voice, same TI recolor.

**Only go to raw Piper** if you consciously decide you'd rather the agent sound good than sound like itself. That's a legitimate choice, but make it on purpose.

---

## 3. Architecture questions worth deciding later

Framing only. Don't build for these yet.

### 3a. Where should processing live?

Right now there's no real choice: the phone has the only microphone, so the phone captures, and everything else can live wherever. The current split (phone captures → Pi transcribes and synthesizes → phone plays) is reasonable *given the Pi is the only always-on box with the memory and the tools on it*.

**What changes when the Pi gets its own mic and speakers:** the Pi becomes a first-class client of itself, and the phone demotes from "the interface" to "one client among several." At that point:

- The Pi needs a local capture→VAD→STT→LLM→TTS→playback loop that doesn't involve the phone at all.
- The phone PWA becomes remote/text-first, useful when you're not in the room.
- Wake word suddenly becomes the primary interaction model rather than an exotic extra (§3d).
- The *design pressure* moves from "minimize round-trips to the phone" to "minimize Pi CPU contention," because the Pi is now doing capture, STT, TTS, playback, MQTT, and the dashboard simultaneously on 4 weak cores. That's the moment cloud STT stops being a preference and starts being a relief valve.

Worth designing the STT/TTS endpoints so they're indifferent to who calls them. If the Pi's own loop hits the same HTTP endpoints the PWA does, adding local audio is additive rather than a rewrite.

### 3b. Latency budget — what "conversational" actually costs

Rough human thresholds:

- **under ~1 s** total round-trip — feels like talking to someone
- **1–2 s** — noticeably a computer, but fine
- **2–4 s** — you start wondering if it heard you; you'll add filler like "hello?"
- **over ~4 s** — no longer conversation, it's a request-response terminal

Current design, estimated per stage for a 5s utterance:

| Stage | Est. | Notes |
|---|---|---|
| Record + upload from phone | 0.1–0.3 s | LAN, small WAV. Negligible. |
| **STT — local `base.en` on Pi 4** | **2–4 s** | **The bottleneck.** |
| LLM — Claude via `claude` CLI | 1–3 s | CLI process startup + first-token latency. Varies a lot. |
| TTS — espeak-ng | <0.2 s | Effectively free. |
| Playback | ~0 s | |
| **Total** | **~3.5–7.5 s** | Firmly in "not conversation" territory |

Two conclusions fall out of this:

1. **STT is the bottleneck, and swapping to Groq removes it** — it takes that 2–4 s stage down to a few hundred ms. That's the single biggest latency win available, and it's the same change as the biggest accuracy win. They're the same move.
2. **After that, the LLM becomes the bottleneck**, and it's a subtler problem: invoking the `claude` CLI per turn pays process-startup cost every time. If turns feel sluggish after the STT fix, look at keeping a persistent session rather than spawning per turn, before blaming anything else. `SERVER_AGENT_SPEC.md` §3's thinking-disabled / low-effort conversation path is the other half of this.

TTS is not worth optimizing. It's already ~2% of the budget.

### 3c. Barge-in / interruption

Talking over the agent mid-reply. Feels enormously better than waiting for it to finish, and it's what separates "voice assistant" from "conversation."

**What it requires:**
- **Streaming STT** — you have to be transcribing while the agent is still talking. Batch/push-to-talk can't do it by construction.
- **Continuous open mic** during playback, which means **echo cancellation** — otherwise the agent hears its own voice through the speakers and interrupts itself. Once the Pi has speakers in the same room as the mic, this becomes a real audio-engineering problem, not a software flag.
- **Interruptible TTS playback** — the ability to cut audio mid-sentence and discard the queued remainder.
- **State management** — what happens to a half-delivered reply? Does the agent remember what it said, or what it *meant* to say?

**Out of scope now, and correctly so.** Push-to-talk makes barge-in structurally impossible and structurally unnecessary — you're not going to interrupt something you have to hold a button to talk to. This becomes interesting only after the Pi has its own always-open mic. Deepgram Nova-3 (§1d) is the enabler when that day comes.

### 3d. Wake word ("hey neptr")

Options, roughly by weight:

- **openWakeWord** — already in SPEAK's dependency list, and `engine/wakeword.py` exists. Small ONNX models, trainable on custom phrases, light enough for continuous Pi CPU use. The natural choice given it's already in the codebase.
- **Porcupine (Picovoice)** — very light, very accurate, free tier for personal use with an account. Custom wake words are a paid/portal flow.
- **Whisper-based wake loop** — what SPEAK does today (README: "Whisper-based, always-on"). It works but it's the most expensive option by far, since it runs STT continuously rather than a tiny always-on classifier. On a Pi 4 that's a meaningful chunk of your CPU burning permanently. `SERVER_AGENT_SPEC.md` §11 already flags moving off this.

**The important point:** wake word is nearly irrelevant to the current setup. A push-to-talk PWA has a button — the button *is* the wake word, and it's more reliable than any acoustic model. Browsers also can't hold a hot mic in the background in a way you'd want to depend on.

Wake word matters when the Pi has its own mic and you want to talk to the room instead of to your phone. Defer it entirely until then, and when you do it, prefer openWakeWord over the current Whisper loop.

One thing to decide *before* building it: an always-listening wake loop plus cloud STT means a microphone in your room that's continuously evaluating audio and periodically shipping it to a third party. That's a different privacy posture than push-to-talk, and it's the scenario where the §1b privacy tradeoff actually bites. A reasonable resolution is wake-word local (openWakeWord, never leaves the Pi) and only post-wake audio going to the cloud — but decide it deliberately.

---

## 4. Recommended path

Ordered. Each step is independently useful; stop whenever it's good enough.

**0. Already built / in flight — let it land**
Local faster-whisper `base.en` + espeak-ng behind HTTP endpoints, Android PWA push-to-talk. Finish it, use it for a week, and notice what actually annoys you. Real annoyance is better prioritization than any document.

**1. Highest-value next upgrade: Groq `whisper-large-v3-turbo` as the default STT backend**
Nothing else on this list comes close. One free API key gets you:
- large-v3-class accuracy, which is directly the thing you said you wanted
- the 2–4 s STT stage collapsed to a few hundred ms — the round-trip bottleneck removed
- roughly an hour of work behind a backend switch the spec already calls for

Keep `base.en` as the offline fallback, selected by env var. The only real cost is that audio leaves the house — decide that on purpose, not by accident.

**2. Cheap, optional, same afternoon**
- `speechSynthesis` fallback in the PWA so it still talks when the Pi's TTS endpoint is down.
- Keep the Whisper model warm in a long-lived process (if it isn't already) so the fallback path doesn't pay load time.
- If the LLM stage feels slow after step 1, look at persistent `claude` sessions before anything else.

**3. When the current voice starts bothering you: espeak-ng → `ti_color()`**
The identity-preserving TTS upgrade (§2b). Small code change — a base-voice indirection in `voice.py` — plus an evening of retuning `ti_amount` by ear. Verify `pyworld`/`librosa` install on aarch64 first. If espeak→TI sounds too thin, try Piper→TI before abandoning the S&S character.

**4. When the audio interface arrives**
- Re-measure local `base.en` with a good close mic before assuming you need a bigger model. Input quality is worth roughly a model tier.
- Build the Pi's own capture→playback loop against the *same* HTTP endpoints the PWA uses, so the phone demotes cleanly to one client among several.
- Now — and only now — wake word becomes worth building. Prefer openWakeWord over the existing Whisper loop.

**Leave alone until there's a concrete reason:**

| Thing | Don't touch until |
|---|---|
| `small.en` / `medium.en` on the Pi 4 | Never on this hardware. Cloud is faster *and* better. |
| Deepgram Nova-3 | You have a Pi mic and specifically want barge-in |
| Native Android app w/ whisper.cpp | Privacy becomes a hard requirement, or you want offline phone STT badly enough to maintain a second codebase |
| Raw Piper (no TI color) | You consciously decide sounding good beats sounding like itself |
| Barge-in / echo cancellation | Pi has an always-open mic *and* streaming STT |
| Wake word | Pi has a mic. Until then the PWA button is strictly better. |

---

## 5. One-line summary

**STT:** ship the local `base.en` you're building as the offline fallback, then get a free Groq key and make `whisper-large-v3-turbo` the default — it's simultaneously the biggest accuracy win and the biggest latency win available, for about an hour of work.

**TTS:** keep espeak-ng for now (robotic is on-brand), and when it starts grating, route it through the existing `ti_color()` chain rather than replacing it — the TI engine is source-agnostic and the Linux port is a routing change plus an evening of tuning, not a rewrite.
