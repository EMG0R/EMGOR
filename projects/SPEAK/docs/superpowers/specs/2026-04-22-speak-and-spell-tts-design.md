# Speak & Spell TTS Engine — Design Spec
**Date:** 2026-04-22

---

## 1. Goal

A pure Python, cross-platform (macOS / Linux / Windows) text-to-speech engine that emulates the authentic Texas Instruments TMS5100 Speak & Spell voice. Type a line of text, press Enter, hear it spoken in the original hardware voice. Works for any English word — known words use pre-recorded LPC data, unknown words are synthesized via allophone concatenation.

---

## 2. Architecture Overview

```
User types text + presses Enter
          ↓
     Word tokenizer
          ↓
  ┌───────────────────────────────┐
  │        Word Router            │
  │  1. In word bank?  → LPC frames (best quality)     │
  │  2. Single letter? → letter LPC frames             │
  │  3. Unknown word?  → g2p_en → ARPABET              │
  │                     → TI allophones → LPC frames   │
  └───────────────────────────────┘
          ↓
     LPC Decoder (TMS5100 — pure numpy)
          ↓
     sounddevice playback (cross-platform)
```

No compiled binaries. No subprocess calls. No platform-specific shell commands. Pure Python + pip.

---

## 3. Components

### 3.1 `engine/lpc.py` — TMS5100 Decoder

Implements the TMS5100 Linear Predictive Coding filter in numpy. Takes raw hex byte sequences (same format as `Vocab_US_Large.ino`) and outputs a float32 audio sample array at 8000 Hz.

**Responsibilities:**
- Bit-unpack the frame data from byte sequences (energy, pitch, 10 reflection coefficients per frame)
- Run the 10th-order lattice filter per frame to produce audio samples
- Handle voiced/unvoiced frame classification
- Return numpy float32 array ready for playback

**Key spec:** TMS5100 uses 10 reflection coefficients (K1–K10), 4-bit energy, 6-bit pitch, variable frame encoding. Fully documented in TI application notes and MAME source.

### 3.2 `engine/audio.py` — Playback

Single function: `play(samples: np.ndarray)`. Uses `sounddevice` for blocking playback. Works identically on macOS, Linux, Windows.

### 3.3 `engine/phoneme.py` — Text to ARPABET

Takes a single word string, returns a list of ARPABET phoneme tokens.

- **Primary:** `pronouncing` library (CMU Pronouncing Dictionary) — fast lookup for ~125,000 English words
- **Fallback:** `g2p_en` (neural grapheme-to-phoneme) — covers any word not in CMU dict
- Handles uppercase, punctuation stripping, numbers spelled out as words

### 3.4 `engine/allophone.py` — ARPABET to TI Allophones

Maps ARPABET phoneme tokens to TI's 59 allophones and returns concatenated LPC frame bytes.

- Static mapping table: ~60 ARPABET tokens → TI allophone IDs
- Each allophone has pre-recorded LPC hex data (from TMS5110 Speak ROM, extracted in Talkie library — open source, same hex format as Vocab_US_Large.ino)
- Handles stress markers (ARPABET uses AH0/AH1/AH2 — map all to appropriate allophone variant)

### 3.5 `engine/router.py` — Word Router

Decision logic for each word token:

```python
def route(word: str) -> bytes:
    if word.upper() in WORD_BANK:
        return WORD_BANK[word.upper()]       # pre-recorded, best quality
    if word.upper() in LETTER_BANK:
        return LETTER_BANK[word.upper()]     # letter/digit sound
    return allophone.synthesize(word)        # allophone pipeline
```

### 3.6 `data/words.py` — Word Bank

All pre-recorded word LPC data extracted from `Vocab_US_Large.ino` (VM61003/4/5 ROMs). ~400 words including numbers, operations, engineering/military vocabulary. Stored as a plain Python dict: `{"AFFIRMATIVE": bytes(...), "FORTY": bytes(...), ...}`.

Extraction script (`tools/extract_vocab.py`) parses the `.ino` file automatically — no manual entry. **Note:** most entries in `Vocab_US_Large.ino` are commented out with `//` — the extraction script must handle both commented and uncommented `uint8_t` array declarations.

### 3.7 `data/letters.py` — Letter/Digit Bank

Pre-recorded LPC data for A–Z and 0–9. Source: Talkie library's letter/digit header files (`Vocab_US_Large.ino` also contains some letter data). Same hex format as word bank. Enables letter-by-letter spelling of unknown words.

### 3.8 `data/allophones.py` — Allophone Bank

59 TI allophones with pre-recorded LPC hex data. Source: Talkie library allophone header files (e.g. `Vocab_US_Acorn.h` or the dedicated allophone set) — open source, MIT-compatible, available on GitHub at `going-digital/Talkie`. Same hex format as Vocab_US_Large.ino — drops straight into the same LPC decoder. **Note:** these files are NOT in the current `ti_lpc_src/` directory and must be downloaded as part of setup.

### 3.9 `speak.py` — Entry Point + Terminal UI

Main loop with `rich` terminal UI:
- Header: "SPEAK & SPELL" in retro style
- Input prompt at bottom
- Spoken text echoed back with word-routing indicators (shows which words used word bank vs allophone synthesis)
- Press Enter to speak the line
- Press Ctrl+C to exit

**Input:** standard `input()` — fully cross-platform, no `tty`/`termios`.

---

## 4. Data Flow — Example

Input: `"hello world"`

```
tokenize → ["hello", "world"]

"hello":
  HELLO not in word bank
  g2p_en("hello") → ['HH', 'AH0', 'L', 'OW1']
  → allophone map → [sp_HH, sp_AH, sp_LL, sp_OW]
  → concatenated LPC bytes

"world":
  WORLD not in word bank
  g2p_en("world") → ['W', 'ER1', 'L', 'D']
  → allophone map → [sp_WW, sp_ER1, sp_LL, sp_DD1]
  → concatenated LPC bytes

all bytes → lpc.decode() → samples → audio.play()
```

Input: `"forty times nine"`

```
"forty" → FORTY in word bank → pre-recorded LPC bytes  (best quality)
"times" → TIMES in word bank → pre-recorded LPC bytes
"nine"  → NINE  in word bank → pre-recorded LPC bytes
concatenate → decode → play
```

---

## 5. Project Structure (after cleanup)

```
_____SPEAK/
├── speak.py                    # entry point
├── engine/
│   ├── __init__.py
│   ├── lpc.py                  # TMS5100 decoder
│   ├── audio.py                # sounddevice playback
│   ├── phoneme.py              # text → ARPABET
│   ├── allophone.py            # ARPABET → LPC frames
│   └── router.py               # word routing logic
├── data/
│   ├── __init__.py
│   ├── words.py                # ~400 pre-recorded words
│   ├── letters.py              # A–Z, 0–9
│   └── allophones.py           # 59 TI allophones
├── tools/
│   └── extract_vocab.py        # one-shot: parses Vocab_US_Large.ino → words.py
├── docs/
│   └── superpowers/specs/
│       └── 2026-04-22-speak-and-spell-tts-design.md
├── ti_lpc_src/                 # kept for reference, not used at runtime
├── speak.md                    # updated project doc
└── requirements.txt            # numpy, sounddevice, rich, pronouncing, g2p_en
```

**Deleted:** `speak_lib.py`, `out.wav`, `temp_speak.txt`, `zzdbg_dump.txt`, `zzzlast_utter_strhex.txt`, `__pycache__/`

---

## 6. Dependencies

| Package | Purpose |
|---|---|
| `numpy` | LPC filter math, audio sample generation |
| `sounddevice` | Cross-platform audio playback |
| `rich` | Terminal UI |
| `pronouncing` | CMU dict text → ARPABET (fast, offline) |
| `g2p_en` | Neural G2P fallback for words not in CMU dict |

All installable via `pip`. No compiled binaries shipped.

---

## 7. Error Handling

- Unknown phoneme in ARPABET → map to closest TI allophone or skip
- Word with no pronunciation (acronym, gibberish) → spell letter-by-letter
- Audio device not available → clear error message, no crash
- Empty input → no-op

---

## 8. What Gets Kept from Current Code

| Current file | Fate |
|---|---|
| `speak_lib.py` | Replaced by `data/words.py` + `data/letters.py` (auto-extracted) |
| `speak.py` | Replaced by new `speak.py` with proper architecture |
| `ti_lpc_src/Vocab_US_Large.ino` | Parsed by `tools/extract_vocab.py`, kept as reference |
| `ti_lpc_src/cmd_line_vers/ti_lpc_cmd` | Kept for reference only, not used at runtime |
| `speak.md` | Updated |

---

## 9. Out of Scope (Future)

- Circuit-bending DSP effects (pitch shift, bit-crush) — architecture supports adding post-processing to `audio.py`
- GUI / non-terminal interface
- Non-English languages
- Real-time keypress mode (type and each key speaks immediately)
