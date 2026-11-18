# BAKERY architecture

## Layers
1. **`src/engine/` is pure C++20, header-only, with no JUCE.** Every sound, effect and timing decision lives here, so it can be tested on any machine (the 3,600+ engine checks run without JUCE). Files:
   - `Dsp.h`: building blocks
   - `Voice.h`: the 14 engines and the sample player
   - `Effects.h`
   - `Sequencer.h`
   - `Machine.h`: the whole instrument
   - `Params.h`: the parameter model
   - `Factory.h`
   - `Random.h`
2. **`src/plugin/` is a thin JUCE wrapper.** It covers:
   - parameters as an APVTS generated from `bk::registry()`
   - buses and state
   - presets
   - MIDI learn and macros
   - the editor

## Key decisions
- **Fixed parameter model: 896 parameters.** That is 12 channels × 69 + 26 global + 4 master FX slots × 7 + 2 send slots × 7. v1.1 added Key, Scale, Keep in key and Snap To at the end of the global block (saved sessions key off parameter IDs, so older projects still load).
  - Hosts need a fixed, automatable list, so every engine control is a real host parameter (`c1_freq`, `g_swing`, `m1type`, ...).
  - Effect slots use normalised P1-P5 knobs whose meaning follows the chosen effect type. The editor shows their real units.
- **Engines.**
  - One `DrumVoice` with a per-engine synthesis path, each using a technique suited to that drum: sine with pitch envelope, modal membranes, a six-square metal bank, a burst clap, resonator rim and wood, FM/ring/bar modes, comb/fold/crush, and feedback FM.
  - Two voices per channel give click-free retriggers. Choke groups fade other channels out in 8 ms.
- **Anti-aliasing.**
  - Voices run at 2x the internal rate below 80 kHz and are decimated with a 47-tap windowed-sinc half-band.
  - Oscillators use polyBLEP.
  - The distortion effect is oversampled 2x.
- **Safety.**
  - DC blockers after the shapers, flush-to-zero, and a zero-latency instant-attack limiter on Main and on every aux output.
  - Factory and random sounds are level-normalised: 3 noise seeds per sound, with the worst sample peak capped at -3 dBFS.
- **Real-time rules.**
  - All buffers are allocated in `prepare`; `process` does no allocation or locking.
  - Patterns are per-byte relaxed atomics (lock-free UI edits).
  - Samples are handed over through an atomic pointer with an audio-thread acknowledgement, and old samples are freed later on the message thread.
  - MIDI learn uses a lock-free CC ring that the message thread drains.
- **Sequencer.**
  - Stateless per block: the step index is computed from host PPQ, so loops, jumps and tempo changes are exact.
  - Sample-accurate triggers; a pending queue handles ratchets and humanize.
  - Modes: Off, Host, and Host + internal clock.
- **Outputs.** Main stereo plus 12 stereo aux buses, all enabled by default. A channel's Output choice falls back to Main if the host disables that bus.
- **State.** XML holding the APVTS state plus:
  - patterns (text)
  - embedded samples (gzip + base64 float32, at most 10 s each, plus the original path as a fallback)
  - MIDI learn, macros, locks and UI state
- **Presets.**
  - Factory presets are compiled in.
  - User presets are XML `.bakery` files in `~/Library/Application Support/BAKERY/Presets/<Category>/`, plus a favourites file.
  - Sound presets carry one channel (and its sample); kits carry everything except the note map plus the pattern; patterns carry steps, swing and rate.
- **UI.** A fixed logical 1360×860 canvas scaled by an affine transform, which gives sharp vector scaling for resize and zoom. Widgets bind to a parameter index (cheap rebinding when the selected channel changes). A 30 Hz timer refreshes the widgets.
