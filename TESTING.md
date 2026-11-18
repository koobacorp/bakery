# BAKERY: testing status

The statuses below mean:
- **NOT STARTED**: no code yet.
- **IN DEVELOPMENT**: partly written.
- **IMPLEMENTED**: the code exists but no automated test checks it.
- **TESTED**: an automated test actually ran and passed. The tests ran on a GitHub Actions macOS 15 runner, CI run 42627999890 (commit 2331e05), and locally on Linux for the engine.
- **VERIFIED IN FL STUDIO**: nothing has this status yet. BAKERY has **not** been opened in FL Studio yet. That needs the test Mac.

Test results for that run:

| Check | Result |
|---|---|
| Engine suite (`bakery_engine_tests`, including a 10-minute continuous run) | **3632 passed, 0 failed** |
| Plugin suite (`bakery_plugin_tests`, the real JUCE processor and editor) | **565 passed, 0 failed** |
| pluginval, strictness 10, VST3 | SUCCESS |
| pluginval, strictness 10, AU | SUCCESS |
| `auval -strict -v aumu Bkry Koob` | AU VALIDATION SUCCEEDED |
| Universal binary (`lipo`: arm64 + x86_64) | Verified |
| Ad-hoc code signature (`codesign --verify --deep --strict`) | Verified |
| macOS deployment target | minos 11.0 |

| Feature | Status | Test notes | Known bugs / remaining |
|---|---|---|---|
| Plugin loads as a VST3 instrument and an AU (aumu) | TESTED | Checked by pluginval VST3 + AU, auval, and the plugin tests | Not yet loaded in FL Studio |
| FL Studio loading, transport, tempo, automation, multi-out | IMPLEMENTED | Host behaviour was simulated with a fake playhead in the plugin tests | **Needs checking in FL** |
| Sample rates 44.1/48/88.2/96/192 kHz, block sizes 1-4096, varying block sizes | TESTED | Engine and plugin tests; output stays finite and in range | |
| Kick engine (sine/tri, pitch sweep, FM, AM, click, sub, drive/sat/dist) | TESTED | Measured pitch, decay, sweep, FM sidebands and AM | |
| Snare engine (two-mode body + noise + snap, filter, width) | TESTED | Noise/body balance and width checked | |
| Closed and open hats (6-square metal bank + noise, BP/HP) and choke | TESTED | Partials checked; a closed hat chokes the open hat | |
| Clap, Tom, Rim, Cowbell, Wood, Metallic, Click, Impact, Industrial, Experimental | TESTED | Each one's characteristic spectrum/modes measured (tom 1/1.59/2.14 modes, cowbell 540/800 Hz, ...) | |
| Noise colours, pitch envelopes, ring mod, wave shaping, resonant filters | TESTED | Engine tests | |
| Anti-aliasing (2x voice oversampling, polyBLEP, oversampled distortion), DC blocking, denormals | TESTED | Engine tests, including denormal checks and a 10-minute run | |
| Waveform display | TESTED | Rendered in the editor screenshot | Display only |
| Sample layer: WAV/AIFF load, start/end, pitch, reverse, loop, stretch, LP, envelope, blend | TESTED | Engine tests; plugin test loads a WAV | Drag-and-drop from Finder was not exercised; max 10 s per sample |
| Samples saved in the project/state | TESTED | State round trip | |
| Sequencer: 1-64 steps, 16 patterns, 12 tracks, velocity, probability, accent, ratchet | TESTED | Timing exact to ±1 sample | |
| Swing, humanize, rates 1/8, 1/16, 1/16T, 1/32 | TESTED | Engine tests | |
| Host transport/tempo sync, loop jumps, internal clock | TESTED (simulated host) | Fake playhead in plugin tests | FL itself not verified |
| Mute/solo, copy/paste/duplicate/variation/random/clear | TESTED (mute/solo/variation/random) / IMPLEMENTED (copy/paste buttons) | Buttons rendered | Button clicks were not automated |
| MIDI export (.mid file, drag-out) | TESTED (file) / IMPLEMENTED (drag) | The file is parsed back by `juce::MidiFile` | Drag-to-FL not verified |
| MIDI notes, note map, velocity, MIDI channel, note-off, CC 120/123 | TESTED | Plugin + engine tests | |
| MIDI learn | TESTED | CC mapping through processBlock | |
| Macros (8) | TESTED | Plugin tests | |
| Random sound/kit/pattern in 7 modes, intensity, locks (sound, track, parameter) | TESTED | 1400 random sounds and 140 kits are level-checked | |
| Effects: Distortion, Compressor, Filter, Transient, Delay, Reverb, Bitcrush, Widener, Ring, Pitch shift, Degrade | TESTED | Engine tests | |
| Per-voice inserts (2), master chain (4, serial), sends A/B (parallel) | TESTED | Engine tests | |
| Limiter on Main and on the aux outs | TESTED | Engine tests | |
| Mixer: volume, pan, mute, solo, phase, sends, meters | TESTED | Engine tests | Meters were only seen in screenshots |
| 12 separate stereo outputs, with fallback to Main when a bus is disabled | TESTED | Plugin tests | FL routing not verified |
| Factory presets: 57 sounds, 12 kits, 12 patterns | TESTED | All 80 load (plugin test); kit levels checked | |
| User presets: save, load, rename, delete, favourites | TESTED | Plugin tests, using a temp folder | |
| Full state round trip (params, patterns, samples, learn, macros, locks, UI) | TESTED | Plugin tests | |
| 896 automatable host parameters | TESTED | Count, text and round trip checked | FL automation lanes not verified |
| UI: 3 workspaces, module browser, preset browser, tooltips, zoom/resize | TESTED (renders and resizes headless) | Screenshots in `dist/` | Not tried by hand with a mouse |
| CPU | TESTED | Busy kit at 48 kHz: about 1.6 % of one core. 12 channels on every 1/16 with 24 inserts: about 16 % (main or 12 outs). 10-minute busy run: about 6 % | Measured on a CI Mac and not on an M-series laptop |

Validator warnings, none of them failures:
- auval printed float-precision "did not retain default value" notes for a few parameters. The value it read back equals the value it set.
- pluginval (AU) printed "Disabling non-main buses failed". The AU host keeps all 13 outputs; BAKERY falls back to Main when a bus is disabled.

| Key display (note + cents) and Keep in key | TESTED | Engine math + plugin UI screenshots | Not yet tried in FL |
| Sequencer Mode label | TESTED | Caption present in UI test | |
