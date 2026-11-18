# BAKERY update — show the key, keep drums in key, and label Mode

Status: **v1.1 built** in the private repo. Engine tests passed on the box. Mac zip depends on GitHub Actions. Not installed on your Mac until you ask.
This is a small update to the BAKERY you already have (repo koobacorp/bakery, project on the box at `/workspace/bakery`). It does not change how the engines sound, and it does not turn BAKERY into CRUMBS.

You asked for two things: see what key a kick or tom is in, and an option to keep those parts in key. This plan also finally adds the visible **Mode** label on the sequencer, which was promised earlier and is still missing.

---

## 1. What you will see

When you select a pitched drum (a kick, a tom, or an 808-style / long low impact), BAKERY will show the note it is actually tuned to, in plain language, for example:

**G1 +35 cents**

That means the body of the drum is a little sharp of G1. Cents are the fine leftover: 0 is dead on the note, +50 is halfway to the next one, −50 is halfway to the one below.

The display lives on the DRUM DESIGN page, next to the Pitch and Tune knobs, so you can see it change as you turn them. It uses the same high-contrast cream-on-charcoal look as the rest of BAKERY, and it stays readable when you resize or zoom the window.

### Which drums get a key

Always:

- **Kick**
- **Tom**
- **Impact** (the long, low, 808-like engine)

Optionally, when the sound is clearly tonal (a cowbell, a woodblock, a metallic bell), we can show the note too.

Left alone, because they usually have no useful song pitch: closed and open hats, claps, clicks, and most noisy industrial hits. A snare’s “body” tone is a drum-head pitch, not really a bass-note in your song, so we do not snap snares by default. We can show that body note quietly if it is helpful, without forcing it into the scale.

---

## 2. Keep in key

A new option, with a key and scale picker (for example E minor, C major, or a simple chromatic / major / minor / pentatonic set — the same idea as other plugins you already have, kept small).

When **Keep in key** is on, each pitched drum above snaps to the nearest note in that scale. A second choice lets you snap them all to the **root** of the key instead (every kick and tom to E, if the key is E minor), which is handy when you want the low end to sit on one note.

Turning it off leaves Pitch and Tune exactly where you put them, and the display still shows the current note.

If several channels are pitched (kick and two toms), each one snaps on its own, so a high tom can sit on B while the kick sits on E, both in the same key.

---

## 3. Pitch sweeps (the important detail)

A kick in BAKERY is often a sine wave that starts higher and falls to a resting note. That drop is what your ear hears as punch. The knobs are:

- **Pitch** — the resting note (the body)
- **Tune** — a shift on top of that, in semitones
- **Sweep** — how far above the resting note the hit starts
- **Sweep Time** — how fast it falls

The note we show, and the note we snap to, is the **settled body pitch** after the sweep, not the click at the start.

In other words: a kick whose Pitch is 50 Hz still reads as around **G1** (about 49 Hz, so roughly G1 +35 cents), even if Sweep is turned up and the first few milliseconds scream higher. That resting note is the one that has to fit your song. Changing Sweep or Sweep Time will not change the displayed key, and Keep in key will not flatten your punch — it only moves the body.

Under the hood, that resting pitch is already how BAKERY works: the engine’s body frequency is Pitch, shifted by Tune. This update just names that frequency as a note, and can move Pitch so the body lands on the scale.

---

## 4. The Mode label

On the SEQUENCER tab there is already a dropdown for how the pattern plays:

- **Follow host** — plays when FL plays, and follows FL’s position and tempo (the default)
- **Host + internal** — the same, plus the top-bar PLAY button can run the pattern while FL is stopped
- **Off** — sequencer off, MIDI notes only

That box works, but it has no caption. It just sits there showing “Follow host”, and you have to already know what it is. The README even calls it the “Mode box.”

This update adds a clear **Mode** label next to that dropdown, in the same style as the other sequencer captions, so it is obvious at a glance. No change to the behaviour, just the missing label.

---

## 5. How it is tested

- **Note display against textbook math.** For known frequencies (A4 = 440 Hz, C2, E1, the default kick at 50 Hz, and so on), the shown note and cents must match the standard conversion, including Tune offsets (for example +12 semitones is exactly one octave up, 0 cents).
- **Keep in key.** Given a pitch and a scale, the snapped Pitch lands on the expected scale note (or on the root, when that option is on). Notes already in the scale do not move. Hats and other unpitched engines are not touched.
- **Sweeps do not confuse it.** A kick with a large Sweep still displays and snaps the settled body pitch, not the start of the drop. We check this with the same pitch-envelope math the engine already uses.
- **Mode label.** A plugin UI check confirms the sequencer Mode caption is present and the three choices still work as before.
- Existing engine tests, plugin tests, and pluginval at strictness 10 must still pass. This is a small, safe update, not a new synth.

As always, how clear the label and the note display feel in FL is something only you can confirm once it is installed.

---

## 6. Risks and limits

- Short, clicky kicks without a clear body tone can be musically “in between” notes. The display will still name the Pitch knob’s frequency; it cannot invent a musical note that is not really there.
- Keep in key moves the body to a scale note. It will not retune a kick that was deliberately a bit sour unless you turn the option on, and it will not sing a melody for you.
- We will not auto-detect the key of a whole kit from the drums. You pick the key. Drums alone are a weak clue, and a wrong guess would be worse than asking you.
- The Mode label is a tiny UI fix. If anything about the three sequencer modes still feels unclear after the label is there, we can revisit the wording of the choices themselves.

No install on your Mac until you ask. This plan waits for your go, and it can be built as a small BAKERY version bump after you approve it.
