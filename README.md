# HolyVox – vocoder / voice-synth VST3

Inspired by the layout you supplied: two voices (HOLY 1 / HOLY 2), note mapper, arpeggiator, gate, drive,
tolerance, sibilance, correction, mix section and limiter. Every control is wired to a real DSP parameter.

## How it works
* **Modulator** = your audio input (mic/voice). **Carrier** = internal 2-voice polyBLEP saw synth.
* 16-band channel vocoder (analysis bank on the input, synthesis banks per voice).
* **Note Source – Auto**: the plugin tracks the pitch of your voice (NSDF), snaps it to **Key + Scale**, and
  the two voices play the **Harmony** (e.g. 1st and 5th). **MIDI**: play notes on a MIDI track routed to the plugin.
* **Tolerance (Cents / Time)**: how far / how long the sung pitch must stray before the note changes.
* **Min Note**: pitches below this are ignored by the tracker.
* **Arp / Rate**: cycles chord tones (scale degrees 1-3-5-8) in sync with the host tempo.
* **HUMANIZE** (button): slow random pitch drift, natural vibrato, formant wobble and level variation so the
  voices sound less robotic.
* **Per voice**: on/off, Tune (±12 st), Formant (±12 st), Gain.
* **Bottom row**: Mods (vibrato), Effects (reverb), EXC (adds breath noise), Sibilance (keeps "s/t" consonants),
  Correction (pitch-glide speed, right = hard snap), Voice (dry), Synth (raw carrier), HolyVox (vocoded), Tone, Out, LIMIT.

## Get the .vst3
The plugin is built with JUCE 8 (downloaded automatically by CMake). Pick one:

### A) No setup – GitHub builds it for you (Windows + macOS + Linux)
1. Create an empty GitHub repo and push this folder.
2. Open the **Actions** tab → *Build HolyVox VST3* → when it finishes, download the artifact for your OS.
   It contains `HolyVox.vst3`.

### B) Build locally
* **Windows**: install Visual Studio 2022 (Desktop C++), CMake, Git → double-click `build_windows.bat`.
  Copy `build\HolyVox_artefacts\Release\VST3\HolyVox.vst3` to `C:\Program Files\Common Files\VST3\`.
* **macOS**: `./build_mac_linux.sh` (installs into `~/Library/Audio/Plug-Ins/VST3`).
* **Linux**: `sudo apt install build-essential cmake git libasound2-dev libfreetype-dev libfontconfig1-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev libgl1-mesa-dev` then `./build_mac_linux.sh`.

## Usage tips
Put HolyVox on a vocal track (mono or stereo). Gate should sit just above your noise floor. Start with
Key/Scale matching your song, Harmony "1st and 5th", Correction ~0.8, HolyVox mix ~0.8.

## Tests
`tests/test_engine.cpp` exercises the DSP engine (pitch tracking, scale snapping, MIDI, arp, humanize,
44.1/48/96 kHz, NaN safety): `g++ -std=c++17 -O2 tests/test_engine.cpp Source/VocoderEngine.cpp -o t && ./t`
