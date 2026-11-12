# SAM Reciter + TI Allophone LPC — Design Spec

**Goal:** Replace the broken letter-by-letter fallback with a SAM-style reciter and the 59 TI TMS5100 allophone LPC sequences so that any English word is spoken in the authentic Speak & Spell voice with correct pronunciation, syllable timing, and stress.

**Architecture:** SAM's English reciter (rule-based text → phoneme codes) feeds into a phoneme-to-TI-allophone mapper, which concatenates actual TMS5100 LPC byte sequences from the 59-allophone ROM table, with a 2-frame linear crossfade at every allophone boundary. The existing four-tier router keeps the 378-word bank and 36 letter/digit clips as higher-priority tiers; this replaces only tier 3 (currently returning empty bytes) and eliminates tier 4 (letter spelling).

**Tech Stack:** Pure Python 3.10+, numpy; no new dependencies.

---

## 1. Data: `data/allophones.py`

Replace the current empty `ALLOPHONES = {}` with the 59 TI TMS5100 allophone LPC byte sequences, keyed by TI allophone name (e.g. `"AA"`, `"EH"`, `"BB1"`, etc.).

Source: the allophone byte sequences have been extracted from the Speak & Spell ROM and are publicly documented in multiple open-source projects (MAME `tms5110r.hxx`, `go-mmc/lpc-player`, etc.). They are hardcoded as `bytes` literals — no external file dependency.

Each entry is a valid TMS5100 LPC-encoded clip of 1–6 frames (~25–150 ms). The existing `engine/lpc.py` decoder accepts them unchanged.

---

## 2. Engine: `engine/reciter.py` (new file)

A Python port of SAM's reciter — the ~500-line rule engine that converts English text to a phoneme string with stress markers.

**Input:** uppercase English word string, e.g. `"HELLO"`  
**Output:** list of `(phoneme_code, stress)` tuples, e.g. `[("HH", 0), ("EH", 1), ("LL", 0), ("OW", 0)]`

Internals:
- 120 suffix/prefix rules applied left-to-right via regex (ported from SAM's `reciter.c`)
- Stress assignment: primary stress on the loudest vowel, secondary on others
- Handles common English patterns: silent-e, digraphs (TH, CH, SH, PH), -tion, -ness, -ing, -ed endings, etc.
- Returns empty list for words it cannot parse (router then falls back to letter-by-letter)

SAM's reciter is public domain (original by Don't Ask Software, 1982; C port by Jonathan Mayer, 2001).

---

## 3. Engine: `engine/allophone.py` (replace current)

Current `synthesize()` queries `to_arpabet()` then maps ARPABET → TI names — but since `ALLOPHONES` was empty, always returned `b''`.

New implementation:

```
synthesize(word):
  1. phonemes = reciter.phonemize(word)          # SAM reciter (new)
  2. if not phonemes: phonemes = arpabet_fallback(word)  # existing ARPABET path as backup
  3. lpc_clips = [ALLOPHONES[ti_name] for each phoneme]
  4. return crossfade_join(lpc_clips)
```

`crossfade_join`: for each boundary between clip A and clip B, decode 2 trailing frames of A and 2 leading frames of B, linearly blend the LPC K-coefficients and energy, re-encode to LPC bytes, and splice in. This removes the hard click at every allophone junction.

**SAM phoneme → TI allophone mapping** (subset, full table in code):

| SAM | TI |
|-----|----|
| /IY/ | IY |
| /IH/ | IH |
| /EH/ | EH |
| /AE/ | AE |
| /AA/ | AA |
| /AH/ | AX |
| /AO/ | AO |
| /OW/ | OW |
| /UW/ | UW1 |
| /ER/ | ER1 |
| /b/ | BB1 |
| /d/ | DD1 |
| /g/ | GG1 |
| /p/ | PP |
| /t/ | TT1 |
| /k/ | KK1 |
| /s/ | SS |
| /z/ | ZZ |
| /f/ | FF |
| /v/ | VV |
| /m/ | MM |
| /n/ | NN1 |
| /l/ | LL |
| /r/ | RR1 |
| /h/ | HH1 |
| /w/ | WW |
| /y/ | YY1 |
| /ch/ | CH |
| /jh/ | JH |
| /sh/ | SH |
| /th/ | TH |
| /dh/ | DH1 |
| /ng/ | NG |

---

## 4. Router: `engine/router.py` (minor change)

Remove tier 4 (letter-by-letter spelling fallback). With a working reciter, any word that fails the reciter still gets an ARPABET result from `pronouncing` — and the ARPABET path maps to TI allophones. Letter spelling is only needed for individual character input, which is already covered by tier 2 (LETTERS dict).

Updated tiers:
1. Pre-recorded word bank (WORDS) — best quality
2. Single letter/digit (LETTERS) — authentic TI letter clips
3. Allophone synthesis via SAM reciter + TI allophones — works for any English word

---

## 5. Tests

- `tests/test_allophones.py` — verify all 59 ALLOPHONES keys present, all values are non-empty bytes, all decode without error via `engine/lpc.py`
- `tests/test_reciter.py` — verify `phonemize("HELLO")` contains HH/EH/LL/OW phonemes; `phonemize("CAT")` contains AE; empty/garbage input returns list
- `tests/test_allophone.py` (existing) — update `test_synthesize_returns_bytes` to assert `len(result) > 0` now that ALLOPHONES is populated

---

## Out of Scope

- Pitch envelope / prosody (SAM's stress-based pitch variation) — the LPC frames already have pitch baked in; adding pitch modification would require decoding + re-encoding every frame
- Duration stretching — allophone clips play at native duration; acceptable for v1
- Silence insertion between words — currently handled by the audio playback gap between `route_word` calls; no change needed
