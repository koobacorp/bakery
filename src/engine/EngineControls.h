// BAKERY — which controls each engine module shows, with engine-specific labels and beginner tooltips.
#pragma once
#include "Params.h"

namespace bk {
enum Section { SEC_PITCH, SEC_AMP, SEC_TONE, SEC_NOISE, SEC_SHAPE, SEC_MOD, SEC_COUNT };
inline const char* sectionName(int s) { static const char* n[] = {"PITCH", "ENVELOPE", "BODY / TONE", "NOISE / TRANSIENT", "DRIVE", "MODULATION"}; return n[s]; }
struct Ctl { int key; const char* label; const char* tip; int section; };

inline std::vector<Ctl> engineControls(int e) {
    auto pitch = [](const char* l = "Pitch", const char* t = "Main pitch of the drum in Hz.") { return Ctl{V_FREQ, l, t, SEC_PITCH}; };
    Ctl tune{V_TUNE, "Tune", "Fine/coarse tuning in semitones on top of Pitch.", SEC_PITCH};
    Ctl sweep{V_SWEEP, "Sweep", "How far the pitch starts ABOVE its resting pitch (semitones). This fast drop is what makes a kick punch.", SEC_PITCH};
    Ctl sweept{V_SWEEPT, "Sweep Time", "How fast the pitch drop happens. Short = clicky punch, long = laser/boom.", SEC_PITCH};
    Ctl att{V_ATT, "Attack", "Fade-in time. Keep at 0 for punchy drums.", SEC_AMP};
    Ctl dec{V_DEC, "Decay", "How long the sound rings (time to fall 60 dB).", SEC_AMP};
    Ctl sus{V_SUS, "Sustain", "Level held while the note/step is held. 0 = classic one-shot drum.", SEC_AMP};
    Ctl rel{V_REL, "Release", "Fade after the note ends (only matters when Sustain is above 0).", SEC_AMP};
    Ctl noise{V_NOISE, "Noise", "Level of the noise layer.", SEC_NOISE};
    Ctl ncol{V_NCOLOR, "Noise Color", "0 = bright white noise, middle = pink, right = dark brown noise.", SEC_NOISE};
    Ctl ndec{V_NDEC, "Noise Decay", "How long the noise layer rings.", SEC_NOISE};
    Ctl click{V_CLICK, "Click", "Short transient at the very start of the hit.", SEC_NOISE};
    Ctl drive{V_DRIVE, "Drive", "Pushes the sound into soft saturation: louder-feeling, denser.", SEC_SHAPE};
    Ctl sat{V_SAT, "Saturation", "Warm, tube-like even harmonics.", SEC_SHAPE};
    Ctl dist{V_DIST, "Distortion", "Waveshaping distortion using the Dist Mode below.", SEC_SHAPE};
    Ctl dmode{V_DMODE, "Dist Mode", "Soft = smooth, Hard = clipped, Fold = wavefolder, Asym = gritty, Cubic = gentle.", SEC_SHAPE};
    Ctl tone{V_TONE, "Tone", "Tilt EQ: left darker, right brighter.", SEC_TONE};
    Ctl ftype{V_FTYPE, "Filter", "Filter type for the voice filter.", SEC_TONE};
    Ctl cut{V_CUT, "Cutoff", "Filter cutoff frequency.", SEC_TONE};
    Ctl res{V_RES, "Resonance", "Filter resonance: emphasis at the cutoff.", SEC_TONE};
    Ctl fm{V_FMAMT, "FM Amount", "Frequency modulation: one oscillator wobbles another very fast. Adds metallic/digital tones.", SEC_MOD};
    Ctl fmr{V_FMRATIO, "FM Ratio", "Modulator speed relative to the pitch. Whole numbers = harmonic, odd decimals = clangy.", SEC_MOD};
    Ctl ring{V_RING, "Ring/AM", "Ring modulation: multiplies two tones, creating inharmonic metallic partials.", SEC_MOD};
    Ctl width{V_WIDTH, "Width", "Stereo width of the voice.", SEC_TONE};
    switch (e) {
        case E_KICK: return {pitch("Pitch", "Resting pitch of the kick (40-60 Hz is classic)."), tune, sweep, sweept, att, dec, sus, rel,
            {V_BODY, "Body", "Level of the main oscillator.", SEC_TONE}, {V_SUB, "Sub", "Clean sine that bypasses the distortion: keeps the low end solid.", SEC_TONE}, {V_WAVE, "Sine>Tri", "Morph from pure sine to triangle (brighter).", SEC_TONE}, tone, ftype, cut, res,
            {V_PUNCH, "Punch", "Boosts the first 30 ms of the body.", SEC_NOISE}, click, noise, ndec, ncol, drive, sat, dist, dmode, fm, fmr, {V_RING, "AM", "Amplitude modulation by the FM oscillator.", SEC_MOD}};
        case E_SNARE: return {pitch("Body Freq", "Pitch of the drum head tone (150-250 Hz)."), tune, sweep, sweept, att, {V_DEC, "Body Decay", "How long the tonal body rings.", SEC_AMP}, sus, rel,
            {V_BODY, "Body", "Level of the tonal drum-head layer.", SEC_TONE}, {V_HARM, "Overtone", "Pitch of the second head mode (higher = more ring).", SEC_TONE}, tone, width,
            noise, ncol, ndec, {V_FTYPE, "Noise Filter", "Filter type for the snare wires (noise).", SEC_NOISE}, {V_CUT, "Noise Cutoff", "Where the noise is filtered.", SEC_NOISE}, {V_RES, "Noise Res", "Resonance of the noise filter.", SEC_NOISE},
            {V_SNAP, "Snap", "Bright crack at the start (the stick hit).", SEC_NOISE}, click, drive, sat, dist, dmode, fm, fmr, ring};
        case E_CHAT: case E_OHAT: return {pitch("Metal Freq", "Base frequency of the 6 square oscillators."), tune, att, dec, sus, rel,
            {V_HARM, "Metallic", "Spreads the 6 oscillator ratios: left = 808-ish, right = clangy/inharmonic.", SEC_TONE}, {V_BODY, "Metal Level", "Level of the oscillator bank.", SEC_TONE},
            {V_TONE, "Brightness", "Centre of the band-pass filter (dark to bright).", SEC_TONE}, {V_CUT, "High-pass", "High-pass cutoff: removes body/low end.", SEC_TONE}, {V_RES, "Resonance", "High-pass resonance.", SEC_TONE}, width,
            noise, ncol, ndec, click, drive, sat, dist, dmode, {V_FMAMT, "Cross FM", "Oscillators modulate each other: harsher, noisier metal.", SEC_MOD}, ring, {V_MOD, "Sizzle", "Noise modulates the oscillator pitch.", SEC_MOD}, {V_CHOKE, "Choke", "Hats in the same choke group cut each other off (closed cuts open).", SEC_AMP}};
        case E_CLAP: return {{V_FREQ, "Band Freq", "Centre of the band-pass filter that shapes the clap.", SEC_TONE}, {V_RES, "Band Q", "Narrower = more 'room' tone.", SEC_TONE}, tone, width,
            {V_HARM, "Bursts", "How many hand-claps happen in a row (2-7).", SEC_NOISE}, {V_SWEEPT, "Spread", "Time between the bursts (ms).", SEC_NOISE}, {V_MOD, "Looseness", "Random timing of the bursts (more human).", SEC_NOISE}, {V_CLICK, "Burst Level", "Level of the bursts.", SEC_NOISE},
            {V_BODY, "Tail Level", "Level of the reverb-like tail after the bursts.", SEC_AMP}, {V_DEC, "Tail Decay", "Length of the tail.", SEC_AMP}, att, sus, rel, {V_NOISE, "Noise", "Overall noise level.", SEC_NOISE}, ncol, drive, sat, dist, dmode};
        case E_TOM: return {pitch("Pitch", "Head pitch of the tom."), tune, sweep, sweept, att, dec, sus, rel,
            {V_BODY, "Body", "Level of the membrane modes.", SEC_TONE}, {V_HARM, "Overtones", "Level of the 1.59x and 2.14x membrane modes.", SEC_TONE}, {V_WAVE, "Sine>Tri", "Brighter fundamental.", SEC_TONE}, {V_RES, "Shell Res", "Resonant shell ring.", SEC_TONE}, tone, ftype, cut,
            {V_NOISE, "Stick Noise", "Noisy stick attack.", SEC_NOISE}, ndec, ncol, click, drive, sat, dist, dmode, fm, fmr, {V_RING, "AM", "Amplitude modulation.", SEC_MOD}};
        case E_RIM: return {pitch("Pitch", "First resonator (455 Hz on an 808)."), tune, {V_HARM, "Ratio", "Second resonator ratio (3.66x on an 808).", SEC_TONE}, att, {V_DEC, "Ring", "Resonator ring time.", SEC_AMP},
            click, {V_NOISE, "Noise", "Noise in the excitation.", SEC_NOISE}, ndec, tone, ftype, cut, res, drive, sat, dist, dmode};
        case E_COWBELL: return {pitch("Pitch", "First square oscillator (540 Hz on an 808)."), tune, {V_HARM, "Interval", "Second oscillator ratio (800/540 on an 808).", SEC_TONE}, att, dec, sus, rel,
            {V_PUNCH, "Strike", "Loud fast first decay stage.", SEC_AMP}, {V_CUT, "Band Freq", "Band-pass centre.", SEC_TONE}, {V_RES, "Band Q", "Band-pass resonance.", SEC_TONE}, tone, drive, sat, dist, dmode};
        case E_WOOD: return {pitch("Pitch", "Main wooden mode."), tune, {V_HARM, "Ratio", "Upper mode ratios.", SEC_TONE}, {V_RES, "Hardness", "Longer, ringier modes.", SEC_TONE}, att, dec, click, tone, ftype, cut, drive, sat, dist, dmode};
        case E_METAL: return {pitch("Pitch", "Carrier pitch."), tune, att, dec, sus, rel, {V_BODY, "Tone Level", "FM/ring tone level.", SEC_TONE}, {V_MOD, "Bar Modes", "Mix of the inharmonic bar resonators.", SEC_TONE}, {V_HARM, "Inharmonic", "Stretches the bar and ring ratios.", SEC_TONE}, tone, ftype, cut, res, width,
            {V_FMAMT, "FM Amount", "FM index: more = brighter, bell-like.", SEC_MOD}, fmr, {V_RING, "Ring Mod", "Blend FM with ring modulation.", SEC_MOD}, noise, ndec, ncol, drive, sat, dist, dmode};
        case E_CLICK: return {pitch("Freq", "Filter and blip frequency."), tune, sweep, sweept, att, dec, {V_BODY, "Blip", "Sine blip level.", SEC_TONE}, {V_NOISE, "Noise", "Filtered noise tick.", SEC_NOISE}, ndec, {V_CLICK, "Impulse", "Single-sample impulse.", SEC_NOISE}, {V_RES, "Q", "Filter sharpness.", SEC_TONE}, tone, drive, dist, dmode};
        case E_IMPACT: return {pitch("Boom Pitch", "Resting pitch of the boom."), tune, sweep, sweept, att, dec, sus, rel, {V_BODY, "Boom", "Boom level.", SEC_TONE}, {V_SUB, "Sub", "Octave-down sub.", SEC_TONE}, {V_MOD, "Space", "Built-in diffusion: smears the hit into a big room.", SEC_TONE}, width,
            {V_NOISE, "Noise", "Noise swell level.", SEC_NOISE}, ndec, ncol, {V_CUT, "Noise Tone", "Low-pass on the noise.", SEC_NOISE}, click, tone, drive, sat, dist, dmode};
        case E_INDUSTRIAL: return {pitch("Pitch", "Square oscillator pitch."), tune, sweep, sweept, att, dec, {V_BODY, "Square", "Square oscillator level.", SEC_TONE}, {V_HARM, "Comb Pitch", "Metallic comb resonator pitch.", SEC_TONE}, {V_RES, "Comb FB", "Comb feedback: metallic ringing.", SEC_TONE}, tone, ftype, cut, width,
            noise, ndec, ncol, drive, sat, dist, dmode, {V_MOD, "Crush", "Bit and sample-rate reduction.", SEC_MOD}};
        case E_EXPERIMENTAL: return {pitch("Pitch", "Centre pitch."), {V_SWEEP, "Jump Range", "Random pitch jump range (semitones).", SEC_PITCH}, {V_SWEEPT, "Jump Rate", "Time between random pitch jumps.", SEC_PITCH}, att, dec, sus, rel,
            {V_BODY, "Osc Level", "Feedback-FM oscillator level.", SEC_TONE}, tone, ftype, cut, res, width, {V_FMAMT, "Chaos FM", "Feedback FM: from tone to chaotic noise.", SEC_MOD}, fmr, {V_RING, "Noise Ring", "Ring-mod with filtered noise.", SEC_MOD}, {V_MOD, "Stutter", "Rhythmic gate: glitchy stutter.", SEC_MOD},
            noise, ndec, ncol, click, drive, sat, dist, dmode};
        default: return {};
    }
}
} // namespace bk
