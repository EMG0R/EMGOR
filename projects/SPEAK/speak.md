# Speak & Spell Speech Synthesizer Project Plan

## 1. Project Goal
To implement an open-source speech synthesizer that mimics the distinctive male voice and characteristics of the classic Texas Instruments Speak & Spell, using Linear Predictive Coding (LPC). The solution should be deployable on both Raspberry Pi (Linux) and macOS, support real-time English text input, and be modular enough to allow for "circuit-bending" sound modifications in a later phase.

## 2. Key Requirements
*   **Voice:** Closely emulate the male voice of the original Speak & Spell.
*   **Technology:** Utilize or emulate the TMS5100/TMS5220 LPC synthesis chip sound.
*   **Platforms:** Compatible with Raspberry Pi (Linux) and macOS.
*   **Development Languages:** C++ or Python.
*   **Functionality:** Real-time Text-to-Speech (TTS) for English input, like typing.
*   **Modifiability:** Designed to allow for "circuit-bending" style audio manipulation later.

## 3. Research Summary

### Speak & Spell Emulation / LPC
*   The original Speak & Spell used Texas Instruments TMS5100/TMS5220 chips, employing Linear Predictive Coding (LPC).
*   **`ti_lpc` (BrerDawg):** A C/C++ utility that specifically emulates TMS5100/TMS5220 and can render ROM bitstreams to audio. This is highly promising for achieving authentic sound.
*   **`Talkie` (Arduino Library):** An Arduino library implementing TMS5100/TMS5220 synthesis. Useful for source code reference.
*   **MAME:** Contains a highly accurate reference implementation of TMS5100/TMS5220. Its source (`src/devices/sound/tms5110.cpp`) is valuable for understanding the core LPC algorithm.
*   **`SpeakAndSpell` (briansm-github):** A C-based project for Linux/Raspberry Pi that recreates the game logic and voice. Will be investigated for its approach to text-to-LPC data mapping.

### General LPC Libraries
*   Generic C++ or Python LPC libraries (e.g., `librosa`, `pysptk`) are less suitable for directly replicating the specific Speak & Spell character without significant custom parameter tuning or specific data. `ti_lpc` is more direct.

### Circuit Bending Software
*   This is a secondary goal. It will involve applying Digital Signal Processing (DSP) effects to the audio output or manipulating low-level synthesis parameters, if exposed.

## 4. Implementation Plan

### Milestone 1: Core Synthesis & Setup (Authentic Voice)

1.  **Project Directory Creation (Post-Plan-Mode):**
    *   Create `/Users/emgor/Documents/_EMGOR_SYNTH/_______DEMIURGE/_____SPEAK/`.
    *   Move the `speak.md` file (this document) to this newly created directory.
2.  **`ti_lpc` Acquisition & Initial Testing:**
    *   Clone the `ti_lpc` repository from GitHub (https://github.com/BrerDawg/ti_lpc).
    *   Compile `ti_lpc` on macOS (preferably using a standard C++ compiler like `g++` or `clang`).
    *   Compile `ti_lpc` on a Raspberry Pi (or a compatible Linux development environment).
    *   **Verification:** Run `ti_lpc` with its included example ROM data to confirm it produces audible Speak & Spell-like speech on both platforms.
    *   **Investigation:** Analyze `ti_lpc`'s source code (especially `tms5110.h`, `tms5110.cpp`, and `main.cpp` for its usage) to understand its API, expected input format (e.g., raw LPC coefficients, specific ROM data structure), and assess if any synthesis parameters are exposed for potential real-time manipulation.

### Milestone 2: Text-to-LPC Data Management

1.  **`SpeakAndSpell` Project Review:**
    *   Clone the `SpeakAndSpell` repository from GitHub (https://github.com/briansm-github/speakandspell).
    *   **Investigation:** Examine its source code, focusing on how it stores and retrieves speech data. Identify any word lists, phoneme-to-LPC mappings, or data structures that associate English words with their corresponding TMS5100-compatible LPC data.
2.  **Curated Word List Strategy (Primary Approach):**
    *   If a suitable word-to-LPC dictionary is found or can be constructed from existing Speak & Spell ROM data (e.g., using MAME's data or tools to extract from `snspell.zip` equivalent), implement a mechanism to:
        *   Accept English text input from the user.
        *   Parse the input into individual words.
        *   Look up each word in the curated dictionary to retrieve its sequence of LPC data (frame by frame).
        *   Feed this LPC data to `ti_lpc`'s synthesis core.
        *   **Action:** Implement basic handling for unknown words (e.g., spell them out letter-by-letter using pre-encoded letter sounds, or output a "word not found" indicator).
3.  **Research on TMS5100/TMS5220 Encoders (Secondary/Long-Term):**
    *   Simultaneously, conduct a focused search for any academic projects or open-source attempts to create a *general-purpose encoder* that can convert arbitrary English text or phonemes into TMS5100/TMS5220-compatible LPC parameters. This would be the ideal solution for arbitrary vocabulary but is known to be complex.

### Milestone 3: Real-time Interaction & Audio Output

1.  **Audio Library Selection & Integration:**
    *   Choose a cross-platform audio library for real-time output (e.g., `PortAudio` for C++, `PyAudio` for Python). `PortAudio` is a good candidate for C++.
    *   Integrate the chosen library to stream the audio output generated by `ti_lpc` (or its synthesis core) directly to the system's audio device.
2.  **User Input Handling:**
    *   Create a simple command-line interface (CLI) application.
    *   The application should continuously read text input from the user.
    *   As words are typed and completed (e.g., on space or enter), they should be processed for speech synthesis.
    *   **Verification:** Confirm that typed words are spoken in real-time with minimal latency.

### Milestone 4: "Circuit-Bending" Effects (Future Expansion)

1.  **Post-Synthesis DSP Effects:**
    *   Once real-time speech is working, integrate DSP effects that can be applied to the audio stream *after* it has been synthesized by `ti_lpc`.
    *   Examples: Bit-crushing, distortion, pitch shifting, delay, glissando/random pitch modulation.
    *   This could use custom C++/Python DSP code or existing libraries.
2.  **In-Synthesis Parameter Manipulation:**
    *   If `ti_lpc`'s source code investigation reveals exposed internal LPC parameters (e.g., pitch, energy, filter coefficients), explore methods to manipulate these parameters *during* the synthesis process in real-time. This could lead to more nuanced and integrated "circuit-bent" sounds than post-processing.

## 5. Verification Steps
*   Successful compilation and execution of `ti_lpc` on both macOS and Raspberry Pi.
*   Authentic Speak & Spell voice quality from `ti_lpc` using example data.
*   Correct word lookup and concatenation for a sample vocabulary.
*   Functional real-time text input and audio output with low latency.
*   (Later) Effective application of "circuit-bending" DSP effects.

## 6. Next Steps
Proceed with Milestone 1: Create the project directory, download and compile `ti_lpc`, and perform initial tests.
