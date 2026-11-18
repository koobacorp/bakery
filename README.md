# BAKERY

A drum synthesizer and drum machine plugin (VST3 + AU instrument) for macOS, built with JUCE 8.0.4.
Made for FL Studio on Apple Silicon (the binaries are universal, so they also run on Intel Macs).

All sound is synthesised: 14 separate synthesis engines generate it from oscillators, noise, resonators, envelopes,
FM, ring modulation and waveshaping. You can add a sample layer (WAV/AIFF) to any of the 12 channels, but you never need one.

> **Status:** compiled and tested automatically on a macOS CI machine (engine tests, plugin tests, pluginval
> strictness 10, auval). **It has not been tested inside FL Studio yet.** See [TESTING.md](TESTING.md).

## What is inside

| Area | What you get |
|---|---|
| Engines | Kick, Snare, Closed Hat, Open Hat, Clap, Tom, Rimshot, Cowbell, Woodblock, Metallic (FM/ring/bar modes), Click, Impact, Industrial, Experimental |
| Channels | 12 channels (Kick, Snare, Closed Hat, Open Hat, Perc 1-8). Any engine fits in any channel |
| Sample layer | Per channel: start/end, pitch, reverse, loop, granular time-stretch, low-pass, attack/decay, sample/synth blend. Samples are saved inside the FL project |
| Sequencer | 16 patterns, 1-64 steps, 12 tracks. Each step has trigger, velocity, probability, accent and ratchet (x1-x4). Also swing, humanize, 1/8, 1/16, 1/16T and 1/32 rates, mute/solo, copy/paste/duplicate/variation/random, plus MIDI export (drag or .mid) |
| Random | Random sound, kit or pattern in 7 modes (Classic, Analog, Experimental, Industrial, Minimal, Complex, Ambient), with an intensity control. You can lock a sound, a track or a single knob so Random leaves it alone. Levels are auto-normalised, so random sounds never blast |
| Effects | Per channel: 2 insert slots. Master bus: 4 slots in series. Two parallel send buses: A (reverb by default) and B (delay by default). Effect types: Distortion (saturate/soft/hard/fold/waveshape), Compressor, Filter (LP/HP/BP/Notch with envelope), Transient shaper, Delay (tempo sync), Reverb, Bitcrush, Widener, Ring mod, Pitch shift, Degrade |
| Mixer | Per channel: volume, pan, mute, solo, phase invert, send A/B, output select and meter. Master section: volume, limiter ceiling, return levels |
| Outputs | Main stereo plus 12 separate stereo outputs (Out 2-13), for individual FL mixer tracks |
| Presets | 57 factory sounds in 10 categories, 12 factory kits and 12 factory patterns. You can save, load, rename and delete your own presets and mark favourites |
| Control | 896 automatable parameters. Also MIDI note map, MIDI channel, velocity sensitivity, MIDI learn (right-click a knob) and 8 macro knobs |

## Install

You get one zip: `BAKERY-macOS-universal-v1.0.zip`. Unzip it, then:

```bash
cd ~/Downloads/BAKERY          # the unzipped folder
./install_mac.sh               # asks for your Mac password, asks before replacing, keeps a backup on the Desktop
```

The script installs:
- `BAKERY.vst3` to `/Library/Audio/Plug-Ins/VST3/`
- `BAKERY.component` (AU) to `/Library/Audio/Plug-Ins/Components/`

It uses the system folders because `~/Library/Audio/Plug-Ins/VST3` is root-owned on the test Mac.

Next, in FL Studio, go to **Options > Manage plugins > Find installed plugins**. BAKERY then appears under Generators (manufacturer koobacorp).

The bundles are ad-hoc signed, not notarized. The script removes the download quarantine flag from these two bundles only.

To uninstall, delete the two bundles from those folders. Your presets are in `~/Library/Application Support/BAKERY/Presets/`.

## Using it

1. In FL, click **Channel rack > + > BAKERY**. Press FL's Play: the 909 House kit pattern plays, in sync with FL's tempo and transport.
2. **DRUM DESIGN** tab:
   - Click a channel on the left to select it. Its number pad plays it.
   - Pick a module (engine) at the top and shape the sound with the knobs.
   - The waveform shows the actual hit.
   - Drop a WAV onto SAMPLE LAYER to add a sample.
3. **SEQUENCER** tab:
   - Click steps to turn them on or off (drag to paint). Shift+click sets an accent. Right-click (or Alt+click) cycles the ratchet.
   - Shape velocity, probability, ratchet and accent for the selected track in the lanes below.
   - Pattern buttons 1-16 switch patterns.
4. **KIT MIXER** tab: levels, pan, sends, outputs and the master effects.
5. **PRESETS** (top bar): browse the 12 categories. Sound presets load into the selected channel. Kits replace everything, including the current pattern. Patterns load into the current pattern slot.
6. **RANDOM** (top bar): choose a mode and an amount, then press **SOUND** (selected channel), **KIT** or **PATTERN**.
7. Right-click any knob for:
   - **MIDI Learn** (move a hardware knob after choosing it)
   - **Lock** (Random keeps it)
   - **Link to Macro 1-8**
   - **Reset**

### Key display and Keep in key (DRUM DESIGN tab)
Kicks, toms and impacts show their settled body note next to the Pitch knobs (for example **G1 +35 cents**), in concert pitch (A4 = 440 Hz, C4 = middle C). The pitch-sweep click is ignored.
Turn on **Keep in key**, pick a key and scale, and those drums snap to the nearest scale note — or to the root, if Snap is set to Root. Hats and other unpitched sounds are left alone.

### Sequencer modes (SEQUENCER tab, Mode box)
- **Follow host** (default): the pattern plays while FL plays and follows FL's position, loop and tempo.
- **Host + internal**: same as Follow host, but the top-bar **PLAY** button also runs the pattern while FL is stopped. It uses the internal tempo if FL gives none.
- **Off**: the sequencer is off and only MIDI notes trigger sounds. Use this to play BAKERY from FL's piano roll or step sequencer.

You can use the internal sequencer and MIDI notes at the same time.

### MIDI note map (defaults; change per channel with the **MIDI Note** knob in DRUM DESIGN)

| Channel | Note | FL name (C5 = 60) | GM name |
|---|---|---|---|
| 1 Kick | 36 | C3 | Bass Drum |
| 2 Snare | 38 | D3 | Snare |
| 3 Closed Hat | 42 | F#3 | Closed HH |
| 4 Open Hat | 46 | A#3 | Open HH |
| 5 Perc 1 (Clap) | 39 | D#3 | Hand Clap |
| 6 Perc 2 (Tom) | 45 | A3 | Low Tom |
| 7 Perc 3 (Rim) | 37 | C#3 | Side Stick |
| 8 Perc 4 (Cowbell) | 56 | G#4 | Cowbell |
| 9 Perc 5 (Wood) | 76 | E6 | Hi Wood Block |
| 10 Perc 6 (Metal) | 53 | F4 | Ride Bell |
| 11 Perc 7 (Impact) | 49 | C#4 | Crash |
| 12 Perc 8 (Experimental) | 75 | D#6 | Claves |

How MIDI behaves:
- **Velocity:** sets the volume of the hit, scaled by each channel's **Vel Sens** knob.
- **MIDI channel:** the MIDI Channel parameter is Omni by default; set it to 1-16 to listen to one channel only.
- **Note-off:** only matters when a sound's Sustain is above 0. With Sustain at 0 the sound is a classic one-shot.
- **CC 120/123:** stops all sounds.

### Separate outputs into FL mixer tracks

BAKERY has 13 stereo outputs: Main, plus `Out 2 Kick` through `Out 13 Perc 8`. By default everything plays through Main. To send a channel to its own FL mixer track:

1. In BAKERY's **KIT MIXER**, set the channel's **Output** box (top of its strip) to its own output. For example, set Kick to `Out 2` and Snare to `Out 3`.
   A channel routed to an output leaves the Main mix. Its sends still go to the master reverb/delay on Main.
2. In FL, set BAKERY's channel to a free mixer track. In the Channel rack, right-click BAKERY and choose **Route to free mixer track** (Ctrl+L). Say it lands on track 10.
3. Open BAKERY's wrapper settings: the gear icon at the top-left of the plugin window, then the **Processing** tab.
4. In **Connections**, click **Auto map outputs**. FL assigns BAKERY's outputs to the mixer tracks after BAKERY's own track:
   - Main: track 10
   - Out 2 (Kick): track 11
   - Out 3 (Snare): track 12
   - and so on
   To change one by hand, right-click its output number. These numbers are offsets from BAKERY's mixer track.
5. Name the mixer tracks (for example "BAKERY Kick"). FL's manual recommends this so FL knows a plugin is using them.

If your FL version does not show the extra outputs, there is a fallback:
1. Load one BAKERY per drum.
2. Solo the channel you want inside each instance.
3. Route each instance to its own mixer track.

The extra outputs have their own limiter at the ceiling, so they cannot clip. These steps follow Image-Line's documented multi-output workflow (Plugin Wrapper manual: Processing > Connections > Auto map outputs) but **have not yet been verified in FL Studio**.

### Export the pattern as MIDI

In SEQUENCER, press and drag **DRAG MIDI** onto the FL playlist or a piano roll. If FL does not accept the drag, press **SAVE .MID** and import or drag the file from Finder.

The MIDI file is a one-bar loop on MIDI channel 10, using each channel's note number. Ratchets are written out as separate notes and accents as velocity 120 or higher.

## Docs
- [LEARN.md](LEARN.md): a beginner's guide to drum synthesis, with a "try this" for each topic tied to a factory preset.
- [BUILD.md](BUILD.md): how to build it yourself.
- [TESTING.md](TESTING.md): the status of every feature: implemented, tested, or verified in FL.
- [PLAN.md](PLAN.md): architecture decisions.

## Source layout
```
src/engine/   pure C++20 DSP (no JUCE): Dsp.h building blocks, Voice.h (all 14 engines + sample player),
              Effects.h, Sequencer.h (pattern bank, timing, MIDI export), Machine.h (voices, MIDI, sequencer, mixer, routing),
              Params.h (parameter model), Factory.h (factory sounds/kits/patterns), Random.h, EngineControls.h (UI labels/tooltips)
src/plugin/   JUCE wrapper: PluginProcessor (parameters, buses, state, presets, MIDI learn, macros),
              Widgets.h (look and feel, knobs), Pages.h (DRUM DESIGN / SEQUENCER / KIT MIXER / preset browser), PluginEditor
tests/        test_engine.cpp (DSP/sequencer/random/long-run), plugin_tests.cpp (JUCE level + screenshots), render_demos.cpp
scripts/      install_mac.sh, INSTALL.txt
```
