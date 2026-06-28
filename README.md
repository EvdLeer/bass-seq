# BASS SEQ

A monophonic **acid / bassline synthesizer** with a built-in **16-step sequencer** and
**arpeggiator** — a native audio plugin built with [JUCE](https://juce.com) and the
[Open303](https://github.com/RobinSchmidt/RS-MET) engine.

Builds as **Standalone**, **VST3** and **LV2** (Linux/Windows/macOS), plus **AU** and
**AUv3** on Apple platforms.

---

## Features

- **303-style voice** (Open303): saw/square oscillator, resonant low-pass filter,
  decay envelope, accent, slide, drive and delay.
- **16-step sequencer** with per-step note, gate length (rest … tie), ratchet, slide
  and accent.
- **8-mode arpeggiator** (up, down, down-up, random, ±-octave variants) that replaces
  the sequencer as note source while active.
- **128 pattern banks**, stored in one shared file so the standalone and the plugin see
  the same library.
- **Live-performance modifiers** — hold a button during playback for transpose, accent,
  slide, ratchet, gate-length or mute overlays (never written into the pattern).
- **MIDI I/O** — channel filter, input transpose, accent-velocity threshold, note
  forwarding (keyboard / sequencer / arp), 24 PPQN clock out and MIDI-clock slave sync.
- **Hardware-style UI** with a settings overlay and computer-keyboard shortcuts
  (press `?` in the app for the list).

## Download

Prebuilt binaries for **Linux**, **macOS** and **Windows** are attached to each
[**Release**](../../releases). Unzip and run the standalone, or drop the plugin into your
DAW's plugin folder.

Every release also ships a `SHA256SUMS.txt`. Verify a download with
`sha256sum -c SHA256SUMS.txt` (Linux) or `shasum -a 256 -c SHA256SUMS.txt` (macOS).

> macOS/Windows builds are unsigned — your OS may warn on first launch
> (macOS: right-click → Open; Windows: *More info → Run anyway*).
> iOS builds require Apple code-signing and are not distributed here.

## Building from source

Requirements: **CMake ≥ 3.22** and a **C++17** compiler. JUCE is pulled in automatically
via CMake `FetchContent` on first configure.

**Linux** also needs the usual audio/GUI dev packages:

```bash
sudo apt install build-essential cmake git \
  libasound2-dev libjack-jackd2-dev \
  libx11-dev libxrandr-dev libxcursor-dev libxinerama-dev \
  libfreetype6-dev libfontconfig1-dev libgl1-mesa-dev libcurl4-openssl-dev
```

Then:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Artifacts land in `build/BassSeq_artefacts/Release/` (Standalone, VST3, LV2, …).

### Tests

```bash
cmake -B build -DBASSSEQ_BUILD_TEST=ON
cmake --build build --target test_editcontroller
./build/test_editcontroller_artefacts/Release/test_editcontroller
```

## Credits & license

- Built with **JUCE** — https://juce.com (dual-licensed: AGPLv3 or commercial).
- DSP engine: **Open303 / rosic** by Robin Schmidt — MIT.

This project is licensed under the **GNU Affero General Public License v3.0**
(AGPLv3, matching JUCE's open-source terms) — see [`LICENSE`](LICENSE).
