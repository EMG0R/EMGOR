# Speak & Spell TTS Engine Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Pure Python, cross-platform Speak & Spell TTS — type a line, press Enter, hear it spoken in the authentic TMS5100 voice; any English word works via allophone fallback.

**Architecture:** Python-native TMS5100 LPC decoder (numpy) replaces the compiled binary entirely. Known words use pre-recorded LPC byte data from the Talkie/TI ROM library; unknown words are synthesised via a text→ARPABET→TI-allophone pipeline. sounddevice handles cross-platform audio.

**Tech Stack:** Python 3.10+, numpy, sounddevice, rich, pronouncing, g2p_en

---

## File Map

```
_____SPEAK/
├── speak.py                    # entry point + rich terminal UI
├── engine/
│   ├── __init__.py             # empty
│   ├── lpc.py                  # TMS5100 bit-unpacker + lattice filter + decoder
│   ├── audio.py                # sounddevice play wrapper
│   ├── phoneme.py              # text → ARPABET (pronouncing + g2p_en fallback)
│   ├── allophone.py            # ARPABET → TI allophone LPC frames
│   └── router.py               # word bank / letter / allophone dispatch
├── data/
│   ├── __init__.py             # empty
│   ├── words.py                # ~400 pre-recorded word LPC bytes (auto-generated)
│   ├── letters.py              # A–Z, 0–9 LPC bytes
│   └── allophones.py           # 59 TI allophone LPC bytes + ARPABET mapping
├── tools/
│   └── extract_vocab.py        # one-shot: parses Vocab_US_Large.ino → data/words.py
├── tests/
│   ├── test_lpc.py
│   ├── test_phoneme.py
│   ├── test_allophone.py
│   └── test_router.py
└── requirements.txt
```

**Deleted after Task 1:** `speak_lib.py`, `out.wav`, `temp_speak.txt`, `zzdbg_dump.txt`, `zzzlast_utter_strhex.txt`, `__pycache__/`

---

## Task 1: Project Scaffolding & Cleanup

**Files:**
- Create: `engine/__init__.py`, `data/__init__.py`, `tools/`, `tests/`
- Create: `requirements.txt`
- Delete: `speak_lib.py`, `out.wav`, `temp_speak.txt`, `zzdbg_dump.txt`, `zzzlast_utter_strhex.txt`

- [ ] **Step 1: Create directory structure**

```bash
cd /Users/emgor/Documents/_EMGOR_SYNTH/_______DEMIURGE/_____SPEAK
mkdir -p engine data tools tests
touch engine/__init__.py data/__init__.py
```

- [ ] **Step 2: Create requirements.txt**

```
numpy
sounddevice
rich
pronouncing
g2p_en
pytest
```

- [ ] **Step 3: Install dependencies**

```bash
pip install numpy sounddevice rich pronouncing g2p_en pytest
```

Expected: all packages install without error.

- [ ] **Step 4: Remove old files**

```bash
cd /Users/emgor/Documents/_EMGOR_SYNTH/_______DEMIURGE/_____SPEAK
rm -f speak_lib.py out.wav temp_speak.txt zzdbg_dump.txt zzzlast_utter_strhex.txt
rm -rf __pycache__
```

- [ ] **Step 5: Verify structure**

```bash
find . -not -path './ti_lpc_src/*' -not -path './.git/*' -type f | sort
```

Expected output includes `engine/__init__.py`, `data/__init__.py`, `requirements.txt`.

- [ ] **Step 6: Commit**

```bash
git init  # only if not already a git repo
git add engine/ data/ tools/ tests/ requirements.txt
git rm --cached speak_lib.py out.wav temp_speak.txt zzdbg_dump.txt zzzlast_utter_strhex.txt 2>/dev/null || true
git commit -m "feat: scaffold new project structure, remove old files"
```

---

## Task 2: TMS5100 LPC Decoder

**Files:**
- Create: `engine/lpc.py`
- Create: `tests/test_lpc.py`

All constants are taken directly from `ti_lpc_src/cmd_line_vers/ti_lpc_cmd.cpp` (lines 227–267).

- [ ] **Step 1: Write the failing tests**

Create `tests/test_lpc.py`:

```python
import numpy as np
import pytest
from engine.lpc import decode

# Silence frame: energy_idx=0 (4 bits = 0b0000) → first byte 0x00
SILENCE_BYTES = bytes([0x00])

# Stop frame: energy_idx=15 (4 bits = 0b1111) → first nibble = 0xF
# In LSB-first 4-bit read: byte 0x0F gives bits 1,1,1,1 = idx 15
STOP_BYTES = bytes([0x0F])

# Real 'a' LPC data from the original speak_lib.py
A_BYTES = bytes([
    0x63,0xA9,0x21,0xB4,0x3C,0x16,0xAF,0xB6,0x84,0xD2,0x0A,0x5F,0xB3,0xAA,
    0xE2,0xDA,0x32,0xF4,0xCD,0x2A,0x93,0xDF,0x28,0xD7,0x27,0xAB,0xC8,0xE2,
    0x5B,0x93,0x57,0xAF,0xB2,0xD0,0x1B,0x0B,0xEE,0xB2,0xCA,0x24,0xAE,0xC3,
    0x28,0x4B,0xA9,0x0B,0xAF,0x0E,0x13,0x47,0x02,0xA8,0xA6,0x4C,0x00,0x4D,
    0x57,0x08,0xA0,0x99,0x0C,0x01,0x74,0xED,0xB1,0xEA,0x22,0xBA,0xD4,0xA9,
    0xCD,0xAA,0x2B,0xEF,0xD6,0xE0,0xBD,0x6B,0x69,0xAC,0x5A,0x83,0xF7,0xAE,
    0xB3,0xB1,0x6A,0x0B,0xDE,0x32,0xF6,0xA2,0x3A,0xC2,0x79,0x73,0xBD,0xF2,
    0xFD,0xFF
])


def test_decode_silence_returns_zeros():
    samples = decode(SILENCE_BYTES)
    assert isinstance(samples, np.ndarray)
    assert samples.dtype == np.float32
    assert np.all(samples == 0.0)


def test_decode_stop_returns_empty():
    samples = decode(STOP_BYTES)
    assert isinstance(samples, np.ndarray)
    assert len(samples) == 0


def test_decode_letter_a_produces_audio():
    samples = decode(A_BYTES)
    assert len(samples) > 0
    # Real speech should have non-trivial amplitude
    assert np.max(np.abs(samples)) > 0.01


def test_decode_letter_a_length_is_multiple_of_200():
    # Each frame = 200 samples at 8000 Hz (25ms)
    samples = decode(A_BYTES)
    assert len(samples) % 200 == 0


def test_decode_returns_float32():
    samples = decode(A_BYTES)
    assert samples.dtype == np.float32


def test_decode_clamps_to_minus_one_plus_one():
    samples = decode(A_BYTES)
    assert np.all(samples >= -1.5)  # 1.5x gain applied, clamped after filter
    assert np.all(samples <= 1.5)
```

- [ ] **Step 2: Run tests to verify they fail**

```bash
cd /Users/emgor/Documents/_EMGOR_SYNTH/_______DEMIURGE/_____SPEAK
pytest tests/test_lpc.py -v
```

Expected: `ModuleNotFoundError: No module named 'engine.lpc'`

- [ ] **Step 3: Implement engine/lpc.py**

Create `engine/lpc.py`:

```python
import numpy as np

# TMS5100 lookup tables — sourced directly from ti_lpc_src/cmd_line_vers/ti_lpc_cmd.cpp
ENERGY = [0, 0, 1, 1, 2, 3, 5, 7, 10, 15, 21, 30, 43, 61, 86, 0]
PERIOD = [0, 41, 43, 45, 47, 49, 51, 53, 55, 58, 60, 63, 66, 70, 73, 76,
          79, 83, 87, 90, 94, 99, 103, 107, 112, 118, 123, 129, 134, 140, 147, 153]
CHIRP  = [0, 42, 212, 50, 178, 18, 37, 20, 2, 225, 197, 2, 95, 90, 5, 15,
          38, 252, 165, 165, 214, 221, 220, 252, 37, 43, 34, 33, 15, 255,
          248, 238, 237, 239, 247, 246, 250, 0, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0]

K = [
    [-501,-497,-493,-488,-480,-471,-460,-446,-427,-405,-378,-344,-305,-259,
     -206,-148,-86,-21,45,110,171,227,277,320,357,388,413,434,451,464,474,498],
    [-349,-328,-305,-280,-252,-223,-192,-158,-124,-88,-51,-14,23,60,97,133,
     167,199,230,259,286,310,333,354,372,389,404,417,429,439,449,506],
    [-397,-365,-327,-282,-229,-170,-104,-36,35,104,169,228,281,326,364,396],
    [-369,-334,-293,-245,-191,-131,-67,-1,64,128,188,243,291,332,367,397],
    [-319,-286,-250,-211,-168,-122,-74,-25,24,73,121,167,210,249,285,318],
    [-290,-252,-209,-163,-114,-62,-9,44,97,147,194,238,278,313,344,371],
    [-291,-256,-216,-174,-128,-80,-31,19,69,117,163,206,246,283,316,345],
    [-218,-133,-38,59,152,235,305,361],
    [-226,-157,-82,-3,76,151,220,280],
    [-179,-122,-61,1,62,123,179,231],
]
K_BITS = [5, 5, 4, 4, 4, 4, 4, 3, 3, 3]

SAMPLES_PER_FRAME = 200   # 8000 Hz × 25 ms
MKFRACT = 512.0
GAIN = 1.5


class _BitStream:
    """Reads bits LSB-first from a byte sequence."""

    def __init__(self, data: bytes):
        self._data = data
        self._byte = 0
        self._bit = 0

    def read(self, n: int) -> int:
        val = 0
        for i in range(n):
            if self._byte >= len(self._data):
                return val
            val |= ((self._data[self._byte] >> self._bit) & 1) << i
            self._bit += 1
            if self._bit == 8:
                self._bit = 0
                self._byte += 1
        return val


def decode(data: bytes) -> np.ndarray:
    """Decode TMS5100 LPC byte data to float32 audio samples at 8000 Hz."""
    bs = _BitStream(data)
    samples = []

    k = [0.0] * 10   # current reflection coefficients
    x = [0.0] * 10   # lattice delay line state
    period_cnt = 0
    synth_rand = 1

    while True:
        energy_idx = bs.read(4)

        if energy_idx == 0:
            # Silence frame
            samples.extend([0.0] * SAMPLES_PER_FRAME)
            continue

        if energy_idx == 0xF:
            # Stop frame — end of utterance
            break

        energy = ENERGY[energy_idx]
        repeat = bs.read(1)
        period_idx = bs.read(5)
        period = PERIOD[period_idx]

        if not repeat:
            new_k = [0.0] * 10
            for i in range(4):
                idx = bs.read(K_BITS[i])
                new_k[i] = float(K[i][idx])
            if period:
                for i in range(4, 10):
                    idx = bs.read(K_BITS[i])
                    new_k[i] = float(K[i][idx])
            k = new_k

        for _ in range(SAMPLES_PER_FRAME):
            if period:
                # Voiced: glottal chirp stimulus
                chirp_val = CHIRP[period_cnt] if period_cnt < 41 else 0
                u10 = (float(np.int8(chirp_val)) / 256.0) * (energy / 256.0)
                period_cnt = 0 if period_cnt >= period - 1 else period_cnt + 1
            else:
                # Unvoiced: LFSR white noise stimulus
                synth_rand = (synth_rand >> 1) ^ (0xB800 if (synth_rand & 1) else 0)
                wnoise = energy if (synth_rand & 1) else -energy
                u10 = wnoise / 2048.0

            # Lattice filter — forward pass
            u = [0.0] * 11
            u[10] = u10
            u[9]  = u[10] - (k[9] / MKFRACT) * x[9]
            u[8]  = u[9]  - (k[8] / MKFRACT) * x[8]
            u[7]  = u[8]  - (k[7] / MKFRACT) * x[7]
            u[6]  = u[7]  - (k[6] / MKFRACT) * x[6]
            u[5]  = u[6]  - (k[5] / MKFRACT) * x[5]
            u[4]  = u[5]  - (k[4] / MKFRACT) * x[4]
            u[3]  = u[4]  - (k[3] / MKFRACT) * x[3]
            u[2]  = u[3]  - (k[2] / MKFRACT) * x[2]
            u[1]  = u[2]  - (k[1] / MKFRACT) * x[1]
            u[0]  = u[1]  - (k[0] / MKFRACT) * x[0]
            u[0]  = max(-1.0, min(1.0, u[0]))

            # Lattice filter — reverse pass (update state)
            x[9] = x[8] + (k[8] / MKFRACT) * u[8]
            x[8] = x[7] + (k[7] / MKFRACT) * u[7]
            x[7] = x[6] + (k[6] / MKFRACT) * u[6]
            x[6] = x[5] + (k[5] / MKFRACT) * u[5]
            x[5] = x[4] + (k[4] / MKFRACT) * u[4]
            x[4] = x[3] + (k[3] / MKFRACT) * u[3]
            x[3] = x[2] + (k[2] / MKFRACT) * u[2]
            x[2] = x[1] + (k[1] / MKFRACT) * u[1]
            x[1] = x[0] + (k[0] / MKFRACT) * u[0]
            x[0] = u[0]

            samples.append(u[0] * GAIN)

    return np.array(samples, dtype=np.float32)
```

- [ ] **Step 4: Run tests**

```bash
pytest tests/test_lpc.py -v
```

Expected: all 6 tests PASS.

- [ ] **Step 5: Commit**

```bash
git add engine/lpc.py tests/test_lpc.py
git commit -m "feat: implement TMS5100 LPC decoder in pure Python/numpy"
```

---

## Task 3: Audio Playback

**Files:**
- Create: `engine/audio.py`

- [ ] **Step 1: Write failing test**

Create `tests/test_audio.py`:

```python
import numpy as np
from unittest.mock import patch, call
from engine.audio import play


def test_play_calls_sounddevice_with_correct_args():
    samples = np.zeros(400, dtype=np.float32)
    with patch('engine.audio.sd.play') as mock_play, \
         patch('engine.audio.sd.wait') as mock_wait:
        play(samples)
        mock_play.assert_called_once_with(samples, samplerate=8000)
        mock_wait.assert_called_once()


def test_play_empty_array_does_nothing():
    samples = np.array([], dtype=np.float32)
    with patch('engine.audio.sd.play') as mock_play:
        play(samples)
        mock_play.assert_not_called()
```

- [ ] **Step 2: Run to verify failure**

```bash
pytest tests/test_audio.py -v
```

Expected: `ModuleNotFoundError: No module named 'engine.audio'`

- [ ] **Step 3: Implement engine/audio.py**

```python
import numpy as np
import sounddevice as sd

SAMPLE_RATE = 8000


def play(samples: np.ndarray) -> None:
    """Play float32 audio samples at 8000 Hz. Blocks until done."""
    if len(samples) == 0:
        return
    sd.play(samples, samplerate=SAMPLE_RATE)
    sd.wait()
```

- [ ] **Step 4: Run tests**

```bash
pytest tests/test_audio.py -v
```

Expected: 2 tests PASS.

- [ ] **Step 5: Commit**

```bash
git add engine/audio.py tests/test_audio.py
git commit -m "feat: add sounddevice audio playback wrapper"
```

---

## Task 4: Vocab Extraction Tool

**Files:**
- Create: `tools/extract_vocab.py`
- Create: `data/words.py` (generated)

The source file `ti_lpc_src/Vocab_US_Large.ino` contains lines like:
```
uint8_t spFOURTY[] PROGMEM = {0x04,0xC8,...};
//uint8_t spZERO[] PROGMEM = {0x69,0xFB,...};
```
Both commented (`//`) and uncommented entries should be extracted.

- [ ] **Step 1: Write failing test**

Create `tests/test_extract.py`:

```python
import importlib, sys, os

def test_words_dict_exists_and_is_nonempty():
    sys.path.insert(0, os.path.dirname(os.path.dirname(__file__)))
    from data import words
    assert isinstance(words.WORDS, dict)
    assert len(words.WORDS) > 10


def test_words_keys_are_uppercase_strings():
    from data import words
    for key in words.WORDS:
        assert key == key.upper(), f"Key not uppercase: {key}"


def test_words_values_are_bytes():
    from data import words
    for key, val in words.WORDS.items():
        assert isinstance(val, bytes), f"Value for {key} is not bytes"


def test_forty_is_present():
    from data import words
    assert 'FORTY' in words.WORDS


def test_affirmative_is_present():
    from data import words
    assert 'AFFIRMATIVE' in words.WORDS
```

- [ ] **Step 2: Run to verify failure**

```bash
pytest tests/test_extract.py -v
```

Expected: `ModuleNotFoundError: No module named 'data.words'` or `AttributeError`

- [ ] **Step 3: Implement tools/extract_vocab.py**

```python
#!/usr/bin/env python3
"""Parse ti_lpc_src/Vocab_US_Large.ino → data/words.py"""
import re, os

INO_PATH = os.path.join(os.path.dirname(__file__), '..', 'ti_lpc_src', 'Vocab_US_Large.ino')
OUT_PATH = os.path.join(os.path.dirname(__file__), '..', 'data', 'words.py')

# Matches both:  uint8_t spWORD[] PROGMEM = {0x04,...};
# and:         //uint8_t spWORD[] PROGMEM = {0x04,...};
PATTERN = re.compile(r'//?\s*uint8_t\s+sp([A-Z0-9_]+)\[\]\s+PROGMEM\s*=\s*\{([^}]+)\}')


def parse_ino(path: str) -> dict:
    words = {}
    with open(path, 'r') as f:
        text = f.read()
    for m in PATTERN.finditer(text):
        name = m.group(1).upper()
        hex_str = m.group(2).strip()
        # Parse hex values: 0x04, 0xC8, ...
        vals = re.findall(r'0x([0-9A-Fa-f]{2})', hex_str)
        if vals:
            words[name] = bytes(int(v, 16) for v in vals)
    return words


def write_words_py(words: dict, path: str) -> None:
    lines = ['# Auto-generated by tools/extract_vocab.py — do not edit\n',
             'WORDS: dict[str, bytes] = {\n']
    for name, data in sorted(words.items()):
        hex_repr = ', '.join(f'0x{b:02X}' for b in data)
        lines.append(f'    {name!r}: bytes([{hex_repr}]),\n')
    lines.append('}\n')
    with open(path, 'w') as f:
        f.writelines(lines)


if __name__ == '__main__':
    words = parse_ino(INO_PATH)
    write_words_py(words, OUT_PATH)
    print(f"Extracted {len(words)} words → {OUT_PATH}")
```

- [ ] **Step 4: Run extraction**

```bash
cd /Users/emgor/Documents/_EMGOR_SYNTH/_______DEMIURGE/_____SPEAK
python tools/extract_vocab.py
```

Expected output: `Extracted N words → data/words.py` where N > 10.

- [ ] **Step 5: Run tests**

```bash
pytest tests/test_extract.py -v
```

Expected: all 5 tests PASS.

- [ ] **Step 6: Commit**

```bash
git add tools/extract_vocab.py data/words.py tests/test_extract.py
git commit -m "feat: extract word LPC bank from Vocab_US_Large.ino"
```

---

## Task 5: Letter & Digit Bank

**Files:**
- Create: `data/letters.py`

Source: The three letters already in the old `speak_lib.py` are the starting point. The remaining letters A–Z and digits 0–9 must be sourced from the Talkie library.

**How to get the data:**
1. Clone the Talkie library: `git clone https://github.com/going-digital/Talkie /tmp/talkie`
2. Search for letter arrays: `grep -rl "spA\b\|spB\b\|spC\b" /tmp/talkie/`
3. Look in `utility/` subdirectories for `Vocab_US_*.h` or `Vocab_US_*.ino` files
4. Extract `uint8_t spA[]`, `uint8_t spB[]` ... `uint8_t spZ[]`, `uint8_t spZERO[]` ... `uint8_t spNINE[]`
5. Convert to Python bytes format matching the word bank

If the full set is not found in Talkie, the three letters from `speak_lib.py` (a, b, c) are available as a bootstrap — the test requires at least those plus whatever else is found.

- [ ] **Step 1: Write failing test**

Create `tests/test_letters.py`:

```python
from data import letters


def test_letters_dict_exists():
    assert isinstance(letters.LETTERS, dict)


def test_letter_a_present():
    assert 'A' in letters.LETTERS


def test_letter_b_present():
    assert 'B' in letters.LETTERS


def test_letter_c_present():
    assert 'C' in letters.LETTERS


def test_values_are_bytes():
    for k, v in letters.LETTERS.items():
        assert isinstance(v, bytes), f"{k} not bytes"


def test_digits_present_if_available():
    # Digits are optional at this stage — at least check type if present
    for k, v in letters.LETTERS.items():
        if k.isdigit():
            assert isinstance(v, bytes)
```

- [ ] **Step 2: Run to verify failure**

```bash
pytest tests/test_letters.py -v
```

Expected: `ModuleNotFoundError: No module named 'data.letters'`

- [ ] **Step 3: Create data/letters.py with known letters**

Using the three letters from the old `speak_lib.py` plus whatever was sourced from Talkie. At minimum, include A, B, C:

```python
# Letter and digit LPC byte data
# Source: Talkie library (going-digital/Talkie) + speak_lib.py bootstrap
# Add full A-Z, 0-9 entries after sourcing from Talkie utility files

LETTERS: dict[str, bytes] = {
    'A': bytes([
        0x63,0xA9,0x21,0xB4,0x3C,0x16,0xAF,0xB6,0x84,0xD2,0x0A,0x5F,0xB3,0xAA,
        0xE2,0xDA,0x32,0xF4,0xCD,0x2A,0x93,0xDF,0x28,0xD7,0x27,0xAB,0xC8,0xE2,
        0x5B,0x93,0x57,0xAF,0xB2,0xD0,0x1B,0x0B,0xEE,0xB2,0xCA,0x24,0xAE,0xC3,
        0x28,0x4B,0xA9,0x0B,0xAF,0x0E,0x13,0x47,0x02,0xA8,0xA6,0x4C,0x00,0x4D,
        0x57,0x08,0xA0,0x99,0x0C,0x01,0x74,0xED,0xB1,0xEA,0x22,0xBA,0xD4,0xA9,
        0xCD,0xAA,0x2B,0xEF,0xD6,0xE0,0xBD,0x6B,0x69,0xAC,0x5A,0x83,0xF7,0xAE,
        0xB3,0xB1,0x6A,0x0B,0xDE,0x32,0xF6,0xA2,0x3A,0xC2,0x79,0x73,0xBD,0xF2,
        0xFD,0xFF
    ]),
    'B': bytes([
        0x45,0xAB,0x36,0xAE,0xD5,0x56,0xA7,0x3E,0xCA,0xD4,0x2A,0xEE,0x96,0x73,
        0xD5,0x55,0x57,0x5F,0x73,0x9C,0x6B,0x91,0x1E,0x27,0xFB,0x04,0x9F,0x34,
        0xA3,0xC6,0xCE,0x89,0x29,0x9A,0xA5,0x5F,0xEC,0x13,0x73,0x72,0x0D,0xCF,
        0x27,0x37,0xDE,0x7E,0x46,0x32,0x19,0x29,0xFA,0xFA,0x8C,0x20,0xB2,0x9A,
        0x7D,0xF3,0x9A,0x89,0x7B,0x8F,0x70,0xEF,0x36,0x13,0xF3,0x39,0xA5,0xDE,
        0x69,0x46,0x1A,0x3B,0x82,0xBB,0xF3,0xAC,0x73,0xCC,0x40,0xA2,0x43,0x44,
        0x4A,0x9F,0x76,0x3E,0x00,0x00,0x95
    ]),
    'C': bytes([
        0x89,0x44,0x1E,0x58,0x48,0x78,0x0D,0x17,0x42,0xA8,0x21,0x19,0x64,0x31,
        0x6E,0xD2,0x4C,0xD1,0x33,0x59,0x22,0x37,0xBC,0x49,0x9D,0x93,0x57,0x44,
        0x55,0x68,0x0C,0x5B,0x0A,0x3A,0x53,0xA5,0xA2,0x24,0x58,0x56,0x62,0x59,
        0x87,0x28,0xD3,0x87,0x49,0x5D,0x57,0xA6,0x89,0x24,0x64,0xD9,0x59,0x29,
        0x54,0x65,0x07,0x6B,0x92,0x6C,0x29,0x3D,0x34,0x3E,0x35,0x4B,0x29,0x8B,
        0x7B,0x94,0x5D,0x97,0x47,0x1A,0xD3,0x4F,0x49,0x63,0x65,0x2B,0x39,0x67,
        0x5D,0x87,0x56,0x9D,0x58,0x4A,0x9C,0x54,0x3A,0x59,0x53,0x68,0x53,0x89,
        0x64,0x76,0x73,0x78,0x57,0x8B,0x76,0x8B,0x96,0x4A,0x98,0x68,0x9A,0x58,
        0x88,0x55,0x9A,0x6A,0x8B,0x58,0x77,0x45,0x59,0x56,0x8B,0x56,0x8B,0x56,
        0x8B,0x56,0x57,0x58,0x01
    ]),
    # Add D–Z and 0–9 here after sourcing from Talkie library
    # See: https://github.com/going-digital/Talkie — search for spD[], spE[], etc.
    # in utility/Vocab_US_Large.h or Vocab_US_Acorn.h
}
```

- [ ] **Step 4: Run tests**

```bash
pytest tests/test_letters.py -v
```

Expected: all 6 tests PASS (digits test passes vacuously if none present yet).

- [ ] **Step 5: Source remaining letters from Talkie**

```bash
git clone https://github.com/going-digital/Talkie /tmp/talkie 2>/dev/null || true
grep -rl "uint8_t spD\b\|uint8_t spE\b\|uint8_t spF\b" /tmp/talkie/
```

Find the file containing `spD[]`, `spE[]` etc. Copy each letter's byte array into `data/letters.py` following the same format as A, B, C above. Re-run `pytest tests/test_letters.py -v` after each addition.

- [ ] **Step 6: Commit**

```bash
git add data/letters.py tests/test_letters.py
git commit -m "feat: add letter and digit LPC data bank"
```

---

## Task 6: Allophone Data & ARPABET Mapping

**Files:**
- Create: `data/allophones.py`
- Create: `engine/allophone.py`
- Create: `tests/test_allophone.py`

The 59 TI allophones must be sourced from the Talkie library.

**How to get allophone data:**
1. In the cloned Talkie repo (`/tmp/talkie`), search: `grep -rl "spAW\b\|spEH\b\|spAY\b" /tmp/talkie/`
2. Look for a file containing individual phoneme/allophone arrays (names like `spPA`, `spOY`, `spAY`, `spEH`, etc.)
3. The Talkie repo may have `utility/Vocab_US_Acorn.h` or a similar dedicated phoneme file
4. Extract each allophone's byte array and convert to Python bytes in the same format as the word bank

- [ ] **Step 1: Write failing tests**

Create `tests/test_allophone.py`:

```python
from engine.allophone import synthesize
import numpy as np


def test_synthesize_returns_bytes():
    result = synthesize('hello')
    assert isinstance(result, bytes)
    assert len(result) > 0


def test_synthesize_unknown_word_returns_bytes():
    result = synthesize('xylophone')
    assert isinstance(result, bytes)


def test_synthesize_empty_string_returns_empty():
    result = synthesize('')
    assert result == b''


def test_synthesize_produces_decodable_lpc():
    from engine.lpc import decode
    data = synthesize('hello')
    samples = decode(data)
    assert isinstance(samples, np.ndarray)
```

- [ ] **Step 2: Run to verify failure**

```bash
pytest tests/test_allophone.py -v
```

Expected: `ModuleNotFoundError: No module named 'engine.allophone'`

- [ ] **Step 3: Create data/allophones.py**

Source allophone byte data from the Talkie library (see sourcing instructions above). The structure is:

```python
# TI 59-allophone LPC data
# Source: going-digital/Talkie — phoneme/allophone header files
# Each key is the TI allophone name; value is raw LPC bytes

ALLOPHONES: dict[str, bytes] = {
    'PA':  bytes([...]),   # pause / silence
    'OY':  bytes([...]),   # as in "boy"
    'AY':  bytes([...]),   # as in "hide"
    'EH':  bytes([...]),   # as in "bed"
    'KK3': bytes([...]),
    'PP':  bytes([...]),
    'JH':  bytes([...]),
    'NN1': bytes([...]),
    'IH':  bytes([...]),   # as in "bit"
    'TT2': bytes([...]),
    'RR1': bytes([...]),
    'AX':  bytes([...]),   # as in "about" (schwa)
    'MM':  bytes([...]),
    'TT1': bytes([...]),
    'DH1': bytes([...]),
    'IY':  bytes([...]),   # as in "beat"
    'EY':  bytes([...]),   # as in "bait"
    'DD1': bytes([...]),
    'UW1': bytes([...]),   # as in "boot"
    'AO':  bytes([...]),   # as in "caught"
    'AA':  bytes([...]),   # as in "father"
    'YY2': bytes([...]),
    'AE':  bytes([...]),   # as in "cat"
    'HH1': bytes([...]),
    'BB1': bytes([...]),
    'TH':  bytes([...]),
    'UH':  bytes([...]),   # as in "book"
    'UW2': bytes([...]),
    'AW':  bytes([...]),   # as in "cow"
    'DD2': bytes([...]),
    'GG3': bytes([...]),
    'VV':  bytes([...]),
    'GG1': bytes([...]),
    'SH':  bytes([...]),
    'ZH':  bytes([...]),
    'RR2': bytes([...]),
    'FF':  bytes([...]),
    'KK2': bytes([...]),
    'KK1': bytes([...]),
    'ZZ':  bytes([...]),
    'NG':  bytes([...]),
    'LL':  bytes([...]),
    'WW':  bytes([...]),
    'XR':  bytes([...]),
    'WH':  bytes([...]),
    'YY1': bytes([...]),
    'CH':  bytes([...]),
    'ER1': bytes([...]),
    'ER2': bytes([...]),
    'OW':  bytes([...]),   # as in "boat"
    'DH2': bytes([...]),
    'SS':  bytes([...]),
    'NN2': bytes([...]),
    'HH2': bytes([...]),
    'OR':  bytes([...]),
    'AR':  bytes([...]),
    'YR':  bytes([...]),
    'GG2': bytes([...]),
    'EL':  bytes([...]),
    'BB2': bytes([...]),
}
```

Replace each `bytes([...])` with the actual bytes sourced from Talkie.

- [ ] **Step 4: Create engine/allophone.py**

```python
import re
from data.allophones import ALLOPHONES
from engine.phoneme import to_arpabet

# ARPABET phoneme token → TI allophone name
# ARPABET stress digits (0,1,2) stripped before lookup
ARPABET_TO_TI: dict[str, str] = {
    'AA': 'AA', 'AE': 'AE', 'AH': 'AX', 'AO': 'AO',
    'AW': 'AW', 'AY': 'AY', 'EH': 'EH', 'ER': 'ER1',
    'EY': 'EY', 'IH': 'IH', 'IY': 'IY', 'OW': 'OW',
    'OY': 'OY', 'UH': 'UH', 'UW': 'UW1',
    'B': 'BB1', 'CH': 'CH', 'D': 'DD1', 'DH': 'DH1',
    'F': 'FF',  'G': 'GG1', 'HH': 'HH1', 'JH': 'JH',
    'K': 'KK1', 'L': 'LL',  'M': 'MM',   'N': 'NN1',
    'NG': 'NG', 'P': 'PP',  'R': 'RR1',  'S': 'SS',
    'SH': 'SH', 'T': 'TT1', 'TH': 'TH',  'V': 'VV',
    'W': 'WW',  'Y': 'YY1', 'Z': 'ZZ',   'ZH': 'ZH',
}

_STRESS = re.compile(r'[012]$')


def _strip_stress(token: str) -> str:
    return _STRESS.sub('', token)


def synthesize(word: str) -> bytes:
    """Convert a single word to concatenated TI allophone LPC bytes."""
    if not word:
        return b''
    phonemes = to_arpabet(word)
    frames = []
    for p in phonemes:
        base = _strip_stress(p)
        ti_name = ARPABET_TO_TI.get(base)
        if ti_name and ti_name in ALLOPHONES:
            frames.append(ALLOPHONES[ti_name])
        # unknown phoneme: skip silently
    return b''.join(frames)
```

**Fallback if allophone data is unavailable:** If you cannot source the 59-allophone byte arrays from the Talkie library, leave `ALLOPHONES = {}` and the system degrades gracefully — `synthesize()` returns `b''`, and `router.route_word()` falls back to spelling the word letter-by-letter using `LETTERS`. The allophone path can be populated incrementally without breaking the rest of the system.

- [ ] **Step 5: Run tests**

```bash
pytest tests/test_allophone.py -v
```

Expected: all 4 tests PASS (once allophone data is populated; with empty ALLOPHONES, `test_synthesize_returns_bytes` will fail — that is expected until data is sourced).

- [ ] **Step 6: Commit**

```bash
git add data/allophones.py engine/allophone.py tests/test_allophone.py
git commit -m "feat: add TI allophone data bank and ARPABET synthesis pipeline"
```

---

## Task 7: Phoneme Conversion

**Files:**
- Create: `engine/phoneme.py`
- Create: `tests/test_phoneme.py`

- [ ] **Step 1: Write failing tests**

Create `tests/test_phoneme.py`:

```python
from engine.phoneme import to_arpabet


def test_known_word_returns_list():
    result = to_arpabet('hello')
    assert isinstance(result, list)
    assert len(result) > 0


def test_known_word_tokens_are_strings():
    result = to_arpabet('hello')
    for token in result:
        assert isinstance(token, str)


def test_cat_contains_ae():
    # "cat" should produce AE phoneme
    result = to_arpabet('cat')
    bases = [t.rstrip('012') for t in result]
    assert 'AE' in bases


def test_unknown_word_still_returns_list():
    result = to_arpabet('flurblezorp')
    assert isinstance(result, list)


def test_empty_returns_empty():
    assert to_arpabet('') == []


def test_uppercase_handled():
    lower = to_arpabet('hello')
    upper = to_arpabet('HELLO')
    assert lower == upper


def test_router_letter_fallback_when_phonemes_empty():
    # If phoneme conversion returns [] for a word,
    # router.route_word must still return bytes (letter-by-letter spelling)
    from engine.router import route_word
    from unittest.mock import patch
    with patch('engine.allophone.synthesize', return_value=b''):
        result = route_word('hi')
        assert isinstance(result, bytes)
```

- [ ] **Step 2: Run to verify failure**

```bash
pytest tests/test_phoneme.py -v
```

Expected: `ModuleNotFoundError: No module named 'engine.phoneme'`

- [ ] **Step 3: Implement engine/phoneme.py**

```python
import pronouncing

_G2P_AVAILABLE = False
try:
    from g2p_en import G2p
    _g2p = G2p()
    _G2P_AVAILABLE = True
except Exception:
    pass


def to_arpabet(word: str) -> list[str]:
    """Convert a single word to a list of ARPABET phoneme tokens."""
    if not word:
        return []
    w = word.upper()
    phones = pronouncing.phones_for_word(w.lower())
    if phones:
        return phones[0].split()
    if _G2P_AVAILABLE:
        result = _g2p(w)
        return [t for t in result if t.isalpha() or (len(t) > 1 and t[:-1].isalpha())]
    # Last resort: return empty (router will fall back to letter spelling)
    return []
```

- [ ] **Step 4: Run tests**

```bash
pytest tests/test_phoneme.py -v
```

Expected: all 6 tests PASS.

- [ ] **Step 5: Commit**

```bash
git add engine/phoneme.py tests/test_phoneme.py
git commit -m "feat: add text-to-ARPABET phoneme conversion (pronouncing + g2p_en)"
```

---

## Task 8: Word Router

**Files:**
- Create: `engine/router.py`
- Create: `tests/test_router.py`

- [ ] **Step 1: Write failing tests**

Create `tests/test_router.py`:

```python
from engine.router import route_word, route_text
from data.words import WORDS
from data.letters import LETTERS


def test_known_word_returns_bytes():
    # FORTY is in the word bank
    result = route_word('forty')
    assert isinstance(result, bytes)
    assert len(result) > 0


def test_known_word_case_insensitive():
    assert route_word('FORTY') == route_word('forty')


def test_single_letter_uses_letter_bank():
    result = route_word('a')
    assert result == LETTERS['A']


def test_unknown_word_returns_bytes_via_allophone():
    result = route_word('flurblezorp')
    assert isinstance(result, bytes)


def test_route_text_splits_on_whitespace():
    result = route_text('hello world')
    assert isinstance(result, bytes)
    assert len(result) > 0


def test_route_text_empty_returns_empty():
    assert route_text('') == b''


def test_route_text_strips_punctuation():
    result_clean = route_text('hello')
    result_punct = route_text('hello!')
    # Both should produce audio (may differ slightly)
    assert len(result_clean) > 0
    assert len(result_punct) > 0
```

- [ ] **Step 2: Run to verify failure**

```bash
pytest tests/test_router.py -v
```

Expected: `ModuleNotFoundError: No module named 'engine.router'`

- [ ] **Step 3: Implement engine/router.py**

```python
import re
from data.words import WORDS
from data.letters import LETTERS
from engine.allophone import synthesize

_PUNCT = re.compile(r"[^A-Z0-9' -]")


def route_word(word: str) -> bytes:
    """Return LPC bytes for a single word token."""
    w = word.upper().strip()
    if not w:
        return b''

    # 1. Pre-recorded word bank (best quality)
    if w in WORDS:
        return WORDS[w]

    # 2. Single letter or digit
    if len(w) == 1 and w in LETTERS:
        return LETTERS[w]

    # 3. Allophone pipeline (arbitrary words)
    result = synthesize(w)
    if result:
        return result

    # 4. Last resort: spell letter by letter
    return b''.join(LETTERS.get(ch, b'') for ch in w)


def route_text(text: str) -> bytes:
    """Convert a full line of text to concatenated LPC bytes."""
    if not text:
        return b''
    cleaned = _PUNCT.sub('', text.upper())
    tokens = cleaned.split()
    return b''.join(route_word(t) for t in tokens)
```

- [ ] **Step 4: Run tests**

```bash
pytest tests/test_router.py -v
```

Expected: all 7 tests PASS.

- [ ] **Step 5: Commit**

```bash
git add engine/router.py tests/test_router.py
git commit -m "feat: add word router with word bank / letter / allophone dispatch"
```

---

## Task 9: Terminal UI & Entry Point

**Files:**
- Create: `speak.py`

- [ ] **Step 1: Implement speak.py**

```python
#!/usr/bin/env python3
import sys
from rich.console import Console
from rich.panel import Panel
from rich.text import Text
from rich import box

from engine.router import route_text
from engine.lpc import decode
from engine.audio import play

console = Console()

HEADER = Text("SPEAK & SPELL", style="bold green", justify="center")
SUBHEAD = Text("TMS5100 Voice Synthesizer  //  type text, press Enter to speak  //  Ctrl+C to exit",
               style="dim green", justify="center")


def speak_line(line: str) -> None:
    """Route text → LPC bytes → samples → audio."""
    lpc_data = route_text(line)
    if not lpc_data:
        return
    samples = decode(lpc_data)
    play(samples)


def main() -> None:
    console.print(Panel.fit(HEADER, subtitle=SUBHEAD.plain, box=box.DOUBLE, style="green"))
    console.print()

    while True:
        try:
            line = console.input("[bold green]> [/bold green]")
        except (KeyboardInterrupt, EOFError):
            console.print("\n[dim green]* POWER OFF *[/dim green]")
            sys.exit(0)

        if not line.strip():
            continue

        # Echo back with routing labels
        words = line.upper().split()
        from data.words import WORDS
        from data.letters import LETTERS
        labeled = []
        for w in words:
            if w in WORDS:
                labeled.append(f"[green]{w}[/green]")
            elif len(w) == 1 and w in LETTERS:
                labeled.append(f"[cyan]{w}[/cyan]")
            else:
                labeled.append(f"[yellow]{w}[/yellow]")
        console.print("  " + " ".join(labeled))
        console.print(
            "  [dim](green=word bank  cyan=letter  yellow=allophone)[/dim]"
        )
        console.print()

        speak_line(line)


if __name__ == '__main__':
    main()
```

- [ ] **Step 2: Smoke test**

```bash
cd /Users/emgor/Documents/_EMGOR_SYNTH/_______DEMIURGE/_____SPEAK
python speak.py
```

Type: `forty times nine`  
Expected: terminal echoes the words in green (all in word bank), then audio plays the three words in sequence in the Speak & Spell voice.

Type: `hello world`  
Expected: terminal echoes in yellow (allophone pipeline), audio plays synthesised speech.

Type a single letter: `a`  
Expected: terminal echoes in cyan, audio plays the letter sound.

Press Ctrl+C  
Expected: `* POWER OFF *` message, clean exit.

- [ ] **Step 3: Run full test suite**

```bash
pytest tests/ -v
```

Expected: all tests PASS.

- [ ] **Step 4: Commit**

```bash
git add speak.py
git commit -m "feat: add rich terminal UI and main entry point"
```

---

## Task 10: Final Cleanup

- [ ] **Step 1: Update speak.md**

Replace the old milestone plan in `speak.md` with a brief summary:
```markdown
# Speak & Spell TTS

Pure Python TMS5100 Speak & Spell voice synthesiser.

## Run
```
pip install -r requirements.txt
python speak.py
```

## Architecture
See `docs/superpowers/specs/2026-04-22-speak-and-spell-tts-design.md`
```

- [ ] **Step 2: Run tests one final time**

```bash
pytest tests/ -v --tb=short
```

Expected: all tests PASS, zero failures.

- [ ] **Step 3: Final commit**

```bash
git add speak.md
git commit -m "docs: update speak.md with new architecture summary"
```
