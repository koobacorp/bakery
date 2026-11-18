# Learn drum synthesis with BAKERY

Short, practical lessons. Each one has a **Try this** that starts from a factory preset (PRESETS button, top bar).
Sound presets load into the channel you have selected on the left of DRUM DESIGN. Press its pad, or play its MIDI note, to hear it.
Hover over any knob for a one-line explanation.

---

## 1. How an 808 kick is made
An 808 kick is a **sine wave whose pitch drops fast**. It starts maybe 2-3 octaves high and falls within about 30-50 ms to a low resting note (45-55 Hz). Then the volume fades out slowly. That is all it is: one oscillator, a pitch envelope and a volume envelope.

**Try this:** load **808 Kicks > 808 Classic**.
- Turn **Decay** from about 650 ms down to 200 ms. You get a short, tight kick, and up to 1500 ms gives the long trap boom (compare with **808 Long Boom**).
- Turn **Pitch** between 40 and 60 Hz. This is the note of the kick; match it to your song's key.
- Set **Sweep** to 0. The kick turns into a dull sine "boom" with no punch.

## 2. Why pitch envelopes create punch
Your ear hears the fast high-to-low drop as a "hit". **Sweep** sets how far above the resting pitch the kick starts, in semitones. **Sweep Time** sets how fast it falls.
- Short Sweep Time (10-30 ms) gives a click-punch, like a 909.
- Long Sweep Time (100+ ms) gives a "laser" or tom-like "doom".

**Try this:** load **909 Kicks > 909 Punchy Electronic**.
- Sweep 36 st with Sweep Time 15 ms gives a hard techno punch.
- Sweep 24 st with Sweep Time 120 ms gives a zap.
- Then add **Click**, a separate short transient, if the kick vanishes on small speakers.

## 3. Body, sub and drive: big kicks
- **Body** is the main oscillator. **Sine>Tri** morphs it toward a brighter triangle wave.
- **Sub** is a clean sine that bypasses the distortion, so the low end stays solid even when you push **Drive**, **Saturation** or **Distortion** hard.

**Try this:** load **Distorted Kicks > Distorted Industrial**.
1. Set **Sub** to 0 and listen to the low end get thinner as the distortion eats it.
2. Bring Sub back up.
3. Change **Dist Mode** from Soft to Fold: the wavefolder makes buzzy, rich harmonics.

## 4. Noise makes snares and hats
Noise is every frequency at once: it hisses. Filter it and give it a short envelope, and you get the "wires" of a snare or the sizzle of a hat.
- **Noise Color**: left is bright white noise, the middle is pink, right is dark brown noise.
- **Noise Decay** sets how long the noise rings.
- A snare is **two tuned tones (the drum head) + filtered noise (the wires) + a snap**. Blend the two layers with **Body** and **Noise**.

**Try this:** load **Snares > 909 Snare**.
- Set **Noise** to 0 and you hear just the drum head tone.
- Set **Body** to 0 and you hear just the wires.
- Now set both to taste. Raise **Snap** for more stick crack, and lower **Noise Cutoff** for a darker, old-school snare.

## 5. Hi-hats: metal from square waves
The 808 hat is **six square-wave oscillators at odd, unrelated frequencies**, band-passed and high-passed. Their clashing harmonics sound like metal.
- **Metallic** spreads the frequency ratios: left is 808-like, right is clangy.
- **Brightness** moves the band-pass filter, and **High-pass** removes the body.

**Choke:** a closed hat stops a ringing open hat, like a real hi-hat pedal. Both are in choke group **A** by default.

**Try this:**
1. Load **Hi-hats > 808 Closed Hat** on channel 3 and **808 Open Hat** on channel 4.
2. In the SEQUENCER, put an open hat on step 3 and a closed hat on step 5. The closed hat cuts the open one off.
3. On the Open Hat channel, set **Choke** to None: now they overlap.

## 6. FM synthesis: metallic and digital percussion
FM means one oscillator (the modulator) wobbles the pitch of another (the carrier) hundreds of times per second. The wobble creates new frequencies called sidebands.
- **FM Ratio** is the modulator speed relative to the pitch. Whole numbers (1, 2, 3) sound harmonic and musical. Odd decimals (1.41, 3.7) sound clangy, like bells.
- **FM Amount** sets how bright the result is.

**Try this:** load **Metallic Percussion > FM Bell**.
- Move **FM Ratio** slowly and listen for "sweet spots" near whole numbers.
- Then load **Experimental Percussion > Experimental FM Kick**: the same idea on a kick.

## 7. Ring modulation: inharmonic tones
Ring modulation multiplies two tones. You hear their **sum and difference** frequencies, not the originals, so the result is rarely in tune. That makes it great for gongs, robot clanks and broken-radio sounds.

**Try this:** load **Metallic Percussion > Gong Hit** and turn **Ring Mod** up and down.

You can also put a **Ring Mod** effect in a Voice FX slot on a snare:
- Freq around 800 Hz gives a metallic snare.
- Raise LFO Depth for movement.

## 8. Transient shaping
A transient is the first few milliseconds of a hit. A transient shaper turns the **attack** and the **sustain** up or down separately, without touching the overall level like a compressor does.

**Try this:** load the **909 House Kit** (Complete Drum Kits) and select **Kick**.
1. In **VOICE FX 1**, choose **Transient**.
2. Attack to the right gives a snappier kick. Attack to the left gives a softer one.
3. Sustain to the left gives a tighter, shorter kick. Sustain to the right gives more boom.

## 9. Compression on drums
A compressor turns down the parts that go above the **Threshold**, by a **Ratio**.
- **Slow attack** (10-30 ms) lets the punch through before the compressor clamps down, so the drum sounds punchier.
- **Fast attack** flattens the punch, so the drum sounds smoother and further away.
- **Makeup** gain brings the level back up.

The master bus starts with a gentle "glue" compressor, so the whole kit sits together.

**Try this:**
1. On a snare's VOICE FX 1, choose **Compressor**.
2. Set Threshold to about -30 dB and Ratio to about 6:1.
3. Switch Attack between 1 ms and 30 ms and listen to the crack appear and disappear.
4. In KIT MIXER, watch the master **Comp GR** read-out.

## 10. Layering synth and samples
Synthesis gives you control; samples give you realism. Layer them:
- Synth kick + acoustic kick click
- Synth snare + recorded snare noise
- Synth hat + metal sample

**Try this:**
1. Select the Kick and drop any short WAV onto **SAMPLE LAYER**.
2. Lower **Decay** in the sample panel so only its attack plays. You now have a synth body with a real transient.
3. Use **Synth** to balance the two layers, **Pitch** to tune the sample and **Start** to skip its silence.

If the layers "fight" in the low end, try the **Ø** (phase) button on the mixer strip.

## 11. Velocity and accent
- **Velocity** is how hard a step is hit (the VELOCITY lane). Each channel's **Vel Sens** knob sets how much velocity changes the volume.
- **Accent** (Shift+click a step, or the ACCENT lane) makes a step 25 % louder. That is how 808s made grooves breathe.

**Try this:** load the **Classic 808 Kit**.
1. Select the Closed Hat track and draw a velocity "wave" in the VELOCITY lane: high, low, mid, low.
2. The same notes suddenly groove.

## 12. Probability, ratchets and swing
- **Probability** means the step plays only that % of the time. Ghost notes at 30-60 % keep a loop from sounding like a loop.
- **Ratchet** plays a step 2-4 times in a row, inside its length. That is the trap hi-hat roll: right-click a step to set it.
- **Swing** delays every second 1/16. 54-58 % is subtle; 62-66 % is a heavy shuffle.

**Try this:** load the pattern **Rhythmic Patterns > Trap Rolls** and look at the ratchets on the hats. Then set Swing to 60 % on **Boom Bap**.

## 13. Random generation as a starting point
Pick a **RANDOM** mode and an **Amount**, then press **KIT** or **PATTERN**.

Random always starts from a believable base sound and keeps pitches and lengths in a sensible range for each drum type. Every result is level-normalised, so nothing blasts your speakers.

**Try this:**
1. Set the mode to **Industrial**, Amount to about 0.5, and press **KIT** a few times.
2. When you like the kick, click **LOCK** on its slot (left rack) and keep pressing **KIT**: the kick stays.
3. Save the result with PRESETS > Complete Drum Kits > SAVE AS NEW.
4. Right-click a single knob > **Lock** to protect just that knob.

## 14. Macros and MIDI learn
- Right-click any knob > **Link to Macro 1-8**. Turning the macro then moves every linked knob together (for example kick Decay + hat Decay + reverb send).
- Right-click > **MIDI Learn**, then move a knob on your controller: it is mapped. The mapping saves with the project.

**Try this:** link the Kick's **Pitch** and the Tom's **Pitch** to Macro 1. Now one knob re-tunes the whole low end.
