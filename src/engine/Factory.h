// BAKERY — factory sounds, kits and patterns, plus the helpers that apply them to the parameter array.
#pragma once
#include <map>
#include <mutex>
#include <sstream>
#include "Sequencer.h"
#include "Voice.h"

namespace bk {
enum Category { C_808K, C_909K, C_DEEPK, C_DISTK, C_SNARE, C_CLAP, C_HAT, C_TOM, C_METAL, C_EXPER, C_KIT, C_PATTERN, C_COUNT };
inline const char* categoryName(int c) {
    static const char* n[] = {"808 Kicks", "909 Kicks", "Deep Kicks", "Distorted Kicks", "Snares", "Claps", "Hi-hats", "Toms", "Metallic Percussion", "Experimental Percussion", "Complete Drum Kits", "Rhythmic Patterns"};
    return n[c >= 0 && c < C_COUNT ? c : 0];
}
struct SoundPreset { const char* name; int category; int engine; const char* kv; };
struct KitPreset { const char* name; const char* sounds[kChannels]; const char* mix; const char* pattern; };
struct PatternPreset { const char* name; int length; int rate; const char* tracks[kChannels]; float swing; };

inline const std::vector<SoundPreset>& factorySounds() {
    static const std::vector<SoundPreset> s = {
        // 808 kicks
        {"808 Classic", C_808K, E_KICK, "freq=49 sweep=24 sweept=28 dec=1100 click=0.12 punch=0.2 sub=0.25 sat=0.1 tone=-0.2"},
        {"808 Long Boom", C_808K, E_KICK, "freq=45 sweep=26 sweept=35 dec=2600 rel=800 click=0.08 sub=0.5 sat=0.25 drive=0.15 tone=-0.3"},
        {"808 Short Tight", C_808K, E_KICK, "freq=58 sweep=20 sweept=18 dec=260 click=0.3 punch=0.5 sub=0.1"},
        {"808 Saturated", C_808K, E_KICK, "freq=47 sweep=28 sweept=30 dec=1500 drive=0.45 sat=0.4 sub=0.35 tone=0.1 click=0.15"},
        // 909 kicks
        {"909 Classic", C_909K, E_KICK, "freq=55 sweep=34 sweept=22 dec=480 click=0.55 punch=0.6 noise=0.15 ndec=8 wave=0.25 sat=0.25 tone=0.2"},
        {"909 Punchy Electronic", C_909K, E_KICK, "freq=58 sweep=38 sweept=18 dec=380 click=0.45 punch=0.9 drive=0.3 sat=0.3 wave=0.15"},
        {"909 Hard Electronic", C_909K, E_KICK, "freq=60 sweep=40 sweept=15 dec=420 click=0.7 punch=1 drive=0.6 dist=0.25 sat=0.4 tone=0.3"},
        {"909 Short", C_909K, E_KICK, "freq=62 sweep=30 sweept=14 dec=220 click=0.5 punch=0.7 wave=0.3"},
        // deep kicks
        {"Deep Sub Kick", C_DEEPK, E_KICK, "freq=40 sweep=14 sweept=40 dec=1800 click=0.05 punch=0.1 sub=0.7 body=0.7 tone=-0.6"},
        {"Deep Techno Rumble", C_DEEPK, E_KICK, "freq=44 sweep=22 sweept=30 dec=900 sub=0.4 sat=0.35 drive=0.25 tone=-0.4 click=0.2"},
        {"Deep Dub Kick", C_DEEPK, E_KICK, "freq=42 sweep=18 sweept=55 att=1 dec=1300 sub=0.5 cut=1200 res=0.25"},
        {"Deep Soft Thump", C_DEEPK, E_KICK, "freq=50 sweep=12 sweept=30 dec=600 punch=0.2 sub=0.3 wave=0.1 tone=-0.5 click=0"},
        // distorted kicks
        {"Distorted Industrial", C_DISTK, E_KICK, "freq=52 sweep=36 sweept=25 dec=700 drive=0.8 dist=0.7 dmode=2 sat=0.4 noise=0.25 ndec=60 click=0.4 tone=0.2"},
        {"Hardcore Gabber", C_DISTK, E_KICK, "freq=58 sweep=42 sweept=20 dec=600 drive=1 dist=0.85 dmode=1 sat=0.6 click=0.5 punch=0.8 tone=0.3 cut=9000"},
        {"Clipped Trap 808", C_DISTK, E_KICK, "freq=46 sweep=20 sweept=40 dec=1600 drive=0.6 dist=0.4 sub=0.4 sat=0.3"},
        {"Fuzz Kick", C_DISTK, E_KICK, "freq=55 sweep=28 sweept=22 dec=500 dist=0.6 dmode=3 drive=0.5 fmamt=0.2 fmratio=1 tone=0.1"},
        // snares
        {"Analog Snare", C_SNARE, E_SNARE, "freq=190 sweep=8 sweept=18 dec=160 noise=0.7 ndec=200 ncolor=0.2 ftype=1 cut=1500 res=0.1 snap=0.5 body=0.65 harm=0.45"},
        {"808 Snare", C_SNARE, E_SNARE, "freq=238 harm=0.5 sweep=5 dec=120 body=0.8 noise=0.55 ndec=150 ftype=1 cut=2000 snap=0.3 ncolor=0"},
        {"909 Snare", C_SNARE, E_SNARE, "freq=195 harm=0.35 sweep=10 sweept=15 dec=110 body=0.6 noise=0.9 ndec=260 ncolor=0 ftype=1 cut=3000 res=0.2 snap=0.8 sat=0.2"},
        {"Acoustic-Style Snare", C_SNARE, E_SNARE, "freq=170 harm=0.6 sweep=4 dec=140 body=0.55 noise=0.85 ndec=300 ncolor=0.35 ftype=2 cut=3500 res=0.1 snap=0.7 width=0.5 click=0.3"},
        {"Digital Snare", C_SNARE, E_SNARE, "freq=220 fmamt=0.6 fmratio=2.7 dec=130 noise=0.6 ndec=140 ftype=2 cut=5000 res=0.4 snap=0.6 dist=0.2 dmode=1"},
        {"Metallic Snare", C_SNARE, E_SNARE, "freq=260 ring=0.7 fmratio=3.3 harm=0.9 dec=200 noise=0.5 ndec=180 ftype=1 cut=4000 res=0.5 snap=0.4"},
        {"Distorted Snare", C_SNARE, E_SNARE, "freq=200 dec=180 noise=0.8 ndec=240 drive=0.7 dist=0.5 sat=0.5 snap=0.7 ftype=1 cut=1200"},
        {"Experimental Snare", C_SNARE, E_SNARE, "freq=310 sweep=24 sweept=10 fmamt=0.8 fmratio=5.1 ring=0.4 dec=250 noise=0.6 ncolor=0.7 ndec=320 ftype=3 cut=2500 res=0.6 dist=0.3 dmode=2"},
        // claps
        {"808 Clap", C_CLAP, E_CLAP, "freq=1100 res=0.35 harm=0.4 sweept=10 dec=280 click=0.85 body=0.6"},
        {"909 Clap", C_CLAP, E_CLAP, "freq=1400 res=0.25 harm=0.6 sweept=8 dec=200 click=0.9 body=0.5 sat=0.2"},
        {"Big Room Clap", C_CLAP, E_CLAP, "freq=1250 res=0.2 harm=1 sweept=14 mod=0.6 dec=600 click=0.8 body=0.8 width=0.8"},
        {"Tight Clap", C_CLAP, E_CLAP, "freq=1600 res=0.45 harm=0.2 sweept=6 dec=110 click=0.9 body=0.4"},
        // hats
        {"808 Closed Hat", C_HAT, E_CHAT, "freq=330 dec=45 harm=0.2 noise=0.25 cut=7500 tone=0.2"},
        {"808 Open Hat", C_HAT, E_OHAT, "freq=330 dec=480 harm=0.2 noise=0.25 ndec=400 cut=7000 tone=0.2"},
        {"909 Closed Hat", C_HAT, E_CHAT, "freq=400 harm=0.55 noise=0.6 ndec=40 dec=40 cut=8500 res=0.3 tone=0.5"},
        {"909 Open Hat", C_HAT, E_OHAT, "freq=400 harm=0.55 noise=0.6 ndec=350 dec=380 cut=8000 tone=0.5"},
        {"Digital Hat", C_HAT, E_CHAT, "freq=520 ring=0.7 fmamt=0.5 harm=0.8 dec=35 noise=0.2 cut=9000 res=0.5 dist=0.2 dmode=1"},
        {"Experimental Open Hat", C_HAT, E_OHAT, "freq=280 harm=1 fmamt=0.8 mod=0.6 ring=0.3 dec=260 noise=0.4 ncolor=0.5 ndec=300 cut=5000 res=0.6 dist=0.4 dmode=2"},
        {"Dark Shaker", C_HAT, E_CHAT, "freq=250 body=0.2 noise=0.9 ncolor=0.3 att=8 dec=70 ndec=70 cut=5000 tone=-0.2 choke=0"},
        // toms
        {"808 Low Tom", C_TOM, E_TOM, "freq=95 sweep=7 sweept=80 dec=600 harm=0.2 noise=0.05 click=0.1 res=0.1"},
        {"808 Mid Tom", C_TOM, E_TOM, "freq=140 sweep=7 sweept=70 dec=500 harm=0.2 noise=0.05 click=0.1 res=0.1"},
        {"Acoustic Floor Tom", C_TOM, E_TOM, "freq=85 sweep=4 dec=700 harm=0.6 noise=0.3 ndec=40 click=0.4 res=0.6 tone=0.1"},
        {"Electro Syn Tom", C_TOM, E_TOM, "freq=180 sweep=24 sweept=120 dec=450 fmamt=0.2 wave=0.4 sat=0.3"},
        // metallic
        {"808 Cowbell", C_METAL, E_COWBELL, ""},
        {"Bright Cowbell", C_METAL, E_COWBELL, "freq=590 harm=0.6 dec=200 cut=3200 res=0.5 punch=0.8"},
        {"FM Bell", C_METAL, E_METAL, "freq=660 fmamt=0.5 fmratio=3.5 ring=0 mod=0.3 dec=1800"},
        {"Gong Hit", C_METAL, E_METAL, "freq=110 fmamt=0.6 fmratio=1.41 ring=0.4 mod=0.7 harm=0.3 dec=4000 noise=0.2 ndec=500 width=0.7"},
        {"Ride Bell", C_METAL, E_METAL, "freq=740 fmamt=0.3 fmratio=2.76 ring=0.5 harm=0.7 mod=0.5 dec=1500 tone=0.4"},
        {"808 Rimshot", C_METAL, E_RIM, ""},
        {"Woodblock", C_METAL, E_WOOD, ""},
        {"Claves", C_METAL, E_WOOD, "freq=2500 harm=0.1 dec=50 res=0.8"},
        // experimental
        {"Experimental FM Kick", C_EXPER, E_KICK, "freq=52 sweep=48 sweept=30 dec=700 fmamt=0.8 fmratio=2.3 ring=0.2 click=0.3 dist=0.2 dmode=2"},
        {"Cinematic Impact", C_EXPER, E_IMPACT, "sweep=40 dec=3500"},
        {"Industrial Hit", C_EXPER, E_INDUSTRIAL, ""},
        {"Machine Stomp", C_EXPER, E_INDUSTRIAL, "freq=70 res=0.85 harm=0.2 dec=500 noise=0.8 mod=0.6"},
        {"Glitch Click", C_EXPER, E_CLICK, ""},
        {"Laser Zap", C_EXPER, E_CLICK, "freq=900 sweep=48 sweept=60 dec=200 body=1 noise=0.1"},
        {"Chaos Perc", C_EXPER, E_EXPERIMENTAL, ""},
        {"Glitch Stutter", C_EXPER, E_EXPERIMENTAL, "mod=0.9 freq=440 sweep=24 sweept=20 dec=800 fmamt=0.7"},
        {"Ambient Drop", C_EXPER, E_IMPACT, "freq=60 sweep=12 sweept=200 att=40 dec=4000 noise=0.7 ncolor=0.9 ndec=3000 cut=1500 mod=1 sub=0.2 width=1 drive=0 sat=0"},
    };
    return s;
}
inline const SoundPreset* findSound(const std::string& name) { for (auto& s : factorySounds()) if (name == s.name) return &s; return nullptr; }

inline const std::vector<PatternPreset>& factoryPatterns() {
    // chars: . rest, x hit (100), X accent, o ghost (55), r ratchet x2, t ratchet x3, ? 50% chance, : 25% chance
    static const std::vector<PatternPreset> p = {
        {"Four On The Floor", 16, 1, {"x...x...x...x...", "", "x.x.x.x.x.x.x.x.", "..x...x...x...x.", "....x.......x...", "", "", "", "", "", "", ""}, 50},
        {"Boom Bap", 16, 1, {"x.....x...x.....", "....x.......x...", "x.x.x.x.x.x.x.xo", "", "", "", "..........o....o", "", "", "", "", ""}, 58},
        {"Trap Rolls", 32, 1, {"x.....x...x.........x.....x.....", "........x...............x.......", "x.x.x.x.x.x.r.x.x.x.x.x.t.x.rrrr", "", "........x...............x.......", "", "", "", "", "", "", ""}, 50},
        {"Breakbeat", 16, 1, {"x.x.......x.....", "....x..o.o..x..o", "x.x.x.x.x.x.x.x.", ".......x........", "", "", "", "", "", "", "", ""}, 54},
        {"Electro Funk", 16, 1, {"x......x..x.....", "....x.......x...", "xxxxxxxxxxxxxxxx", "", "....x.......x...", "", "", "......x.......x.", "", "", "", ""}, 52},
        {"Industrial Stomp", 16, 1, {"X..xX..xX..xX.xx", "....X.......X...", "x.x.x.x.x.x.x.x.", "", "", "..x...x...x...x.", "", "", "", ".x..?..x..?..x..", "X...............", ""}, 50},
        {"Techno Rumble", 16, 1, {"X...x...X...x...", "", "..x...x...x...x.", "..x...x...x...x.", "....x.......x...", "", "", "", "", "o.:.o.:.o.:.o.:.", "", ""}, 50},
        {"Minimal Pulse", 16, 1, {"x...x...x...x...", "", "..?...x...?...x.", "", "", "", ".......:.......o", "", "....:.......:...", "", "", ""}, 54},
        {"Complex Polymeter", 28, 3, {"x..x..x..x..x..x..x..x...x..", "......x.............x.......", "xo.xo.xoxo.xo.xoxo.xo.xoxo.x", "", "", "x....x....x.....x....x......", "", "..x.....x.....x.....x.....x.", "", "", "", "r.....?......t.....?......."}, 50},
        {"Ambient Drift", 32, 1, {"x...............:.......?.......", "", "?.:.?.:.?.:.?.:.?.:.?.:.?.:.?.:.", "", "", "", "", "", ":...............:...............", "........?...............?.......", "x...............................", ""}, 56},
        {"Reggaeton Dembow", 16, 1, {"x...x...x...x...", "...x..x....x..x.", "x.x.x.x.x.x.x.x.", "", "", "", "...x..x....x..x.", "", "", "", "", ""}, 50},
        {"Drum and Bass", 32, 1, {"x.........x.........x.x.........", "....x.......o..x....x.......o..x", "x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.", "", "", "", "", "", "", "", "", ""}, 52},
    };
    return p;
}
inline const PatternPreset* findPattern(const std::string& n) { for (auto& p : factoryPatterns()) if (n == p.name) return &p; return nullptr; }

inline const std::vector<KitPreset>& factoryKits() {
    static const std::vector<KitPreset> k = {
        {"Classic 808 Kit", {"808 Classic", "808 Snare", "808 Closed Hat", "808 Open Hat", "808 Clap", "808 Low Tom", "808 Rimshot", "808 Cowbell", "808 Mid Tom", "Claves", "Ride Bell", "808 Long Boom"}, "c11_vol=-10 c1_senda=0 c2_senda=0.15 c5_senda=0.2", "Boom Bap"},
        {"909 House Kit", {"909 Classic", "909 Snare", "909 Closed Hat", "909 Open Hat", "909 Clap", "Electro Syn Tom", "808 Rimshot", "Ride Bell", "Dark Shaker", "Woodblock", "Cinematic Impact", "Glitch Click"}, "c5_senda=0.25 c4_sendb=0.1 c11_vol=-12", "Four On The Floor"},
        {"Deep Sub Kit", {"Deep Sub Kick", "Analog Snare", "808 Closed Hat", "808 Open Hat", "Tight Clap", "808 Low Tom", "808 Rimshot", "FM Bell", "Dark Shaker", "Claves", "Ambient Drop", "Deep Dub Kick"}, "c2_senda=0.3 c8_sendb=0.3", "Techno Rumble"},
        {"Distorted Industrial Kit", {"Distorted Industrial", "Distorted Snare", "Digital Hat", "Experimental Open Hat", "Big Room Clap", "Machine Stomp", "Industrial Hit", "Bright Cowbell", "Glitch Click", "Gong Hit", "Cinematic Impact", "Glitch Stutter"}, "m2type=1 m2p1=0.25 m2p2=0.2 m2p5=0.72 c2_fx1type=7 c2_fx1p1=0.45 c2_fx1p2=0.2", "Industrial Stomp"},
        {"Hard Electronic Kit", {"909 Hard Electronic", "909 Snare", "909 Closed Hat", "909 Open Hat", "909 Clap", "Electro Syn Tom", "808 Rimshot", "Bright Cowbell", "Laser Zap", "Ride Bell", "Cinematic Impact", "Glitch Click"}, "c1_fx1type=4 c1_fx1p1=0.8 c3_sendb=0.15", "Techno Rumble"},
        {"Trap 808 Kit", {"Clipped Trap 808", "Tight Clap", "808 Closed Hat", "808 Open Hat", "808 Clap", "808 Mid Tom", "808 Rimshot", "808 Cowbell", "Laser Zap", "Claves", "Cinematic Impact", "808 Long Boom"}, "c2_senda=0.2", "Trap Rolls"},
        {"Cinematic Impact Kit", {"808 Long Boom", "Acoustic-Style Snare", "Dark Shaker", "909 Open Hat", "Big Room Clap", "Acoustic Floor Tom", "Woodblock", "Gong Hit", "808 Low Tom", "FM Bell", "Cinematic Impact", "Ambient Drop"}, "c1_senda=0.15 c2_senda=0.35 c6_senda=0.3 c11_senda=0.4 sap1=0.85 sap2=0.8", "Ambient Drift"},
        {"Experimental FM Kit", {"Experimental FM Kick", "Digital Snare", "Digital Hat", "Experimental Open Hat", "Tight Clap", "Electro Syn Tom", "Glitch Click", "FM Bell", "Laser Zap", "Ride Bell", "Chaos Perc", "Glitch Stutter"}, "c7_sendb=0.3 c11_sendb=0.25 sbp5=0.5", "Complex Polymeter"},
        {"Acoustic-Style Kit", {"Deep Soft Thump", "Acoustic-Style Snare", "909 Closed Hat", "909 Open Hat", "808 Clap", "Acoustic Floor Tom", "808 Rimshot", "Woodblock", "808 Mid Tom", "Claves", "Ride Bell", "Gong Hit"}, "c2_senda=0.25 c6_senda=0.2 sap1=0.35", "Breakbeat"},
        {"Minimal Tech Kit", {"808 Short Tight", "Analog Snare", "808 Closed Hat", "808 Open Hat", "Tight Clap", "808 Mid Tom", "808 Rimshot", "Woodblock", "Glitch Click", "Claves", "Laser Zap", "Chaos Perc"}, "c5_sendb=0.2 c9_senda=0.2", "Minimal Pulse"},
        {"Electro Funk Kit", {"909 Punchy Electronic", "808 Snare", "Digital Hat", "808 Open Hat", "808 Clap", "Electro Syn Tom", "808 Rimshot", "808 Cowbell", "Laser Zap", "Claves", "Cinematic Impact", "Glitch Click"}, "", "Electro Funk"},
        {"Ambient Texture Kit", {"Deep Soft Thump", "Experimental Snare", "Dark Shaker", "Experimental Open Hat", "Big Room Clap", "Electro Syn Tom", "Woodblock", "FM Bell", "Gong Hit", "Ride Bell", "Ambient Drop", "Chaos Perc"}, "c2_senda=0.5 c8_senda=0.5 c10_senda=0.5 sap1=0.9 sap2=0.85 c8_sendb=0.3", "Ambient Drift"},
    };
    return k;
}

inline float normTarget(int engine) {
    switch (engine) { case E_KICK: return -3.f; case E_IMPACT: return -4.f; case E_SNARE: case E_TOM: return -6.f; case E_CLAP: return -7.f; case E_CHAT: case E_OHAT: return -10.f; default: return -8.f; }
}
/// Loudness-robust peak: the largest 1 ms RMS window x sqrt(2) (equals the peak for a sine; stable for noisy hits).
inline float hitPeak(const std::vector<float>& x, double sr) {
    int w = std::max(8, (int)(sr * 0.001)); double best = 0;
    for (size_t a = 0; a + (size_t)w <= x.size(); a += (size_t)w / 2) { double e = 0; for (size_t i = a; i < a + (size_t)w; ++i) e += (double)x[i] * x[i]; best = std::max(best, e / w); }
    return (float)(std::sqrt(best) * 1.41421356);
}
/// Adjust the channel's Voice Level so a full-velocity hit reaches the engine's target level. Returns the measured level (linear).
inline float normaliseChannel(float* v, int ch, double sr = 48000.0) {
    float saved = v[cp(ch, V_LEVEL)]; v[cp(ch, V_LEVEL)] = 0; float syn = v[cp(ch, S_SYNTH)]; v[cp(ch, S_SYNTH)] = 1;
    float pk = 0, spk = 0; // three different noise seeds: level from the average, safety from the worst sample peak
    for (uint64_t seed : {12345ull, 777ull, 4242ull}) { auto hit = renderHit(&v[cp(ch, 0)], sr, (int)(sr * 1.0), 1.f, nullptr, nullptr, -1, seed); pk += hitPeak(hit, sr) / 3.f; for (float x : hit) spk = std::max(spk, std::abs(x)); }
    v[cp(ch, S_SYNTH)] = syn;
    if (pk < 1e-5f) { v[cp(ch, V_LEVEL)] = saved; return pk; }
    float lvl = normTarget((int)v[cp(ch, V_ENGINE)]) - gainToDb(pk);
    lvl = std::min(lvl, -3.f - gainToDb(spk)); // sample peak stays below -3 dBFS (noise peaks vary hit to hit)
    v[cp(ch, V_LEVEL)] = clampf(lvl, -48.f, 6.f);
    return pk;
}
inline bool parseKv(const std::string& s, std::vector<std::pair<std::string, float>>& out) {
    std::istringstream in(s); std::string tok;
    while (in >> tok) { auto e = tok.find('='); if (e == std::string::npos) return false; out.push_back({tok.substr(0, e), (float)std::atof(tok.substr(e + 1).c_str())}); }
    return true;
}
inline bool applySound(float* v, int ch, const SoundPreset& s, bool normalise = true) {
    applyEngineDefaults(v, ch, s.engine);
    std::vector<std::pair<std::string, float>> kv; if (!parseKv(s.kv, kv)) return false;
    for (auto& [k, x] : kv) { int i = chanKeyIndex(k); if (i < 0) return false; v[cp(ch, i)] = clampToDef(cp(ch, i), x); }
    if (s.engine == E_CHAT || s.engine == E_OHAT) { if (s.kv && std::string(s.kv).find("choke=") == std::string::npos) v[cp(ch, V_CHOKE)] = 1; }
    if (normalise) {
        // factory sounds are deterministic, so their normalised level is computed once and cached
        static std::mutex lock; static std::map<std::string, float> cache;
        std::lock_guard<std::mutex> g(lock);
        auto it = cache.find(s.name);
        if (it != cache.end()) v[cp(ch, V_LEVEL)] = it->second; else { normaliseChannel(v, ch); cache[s.name] = v[cp(ch, V_LEVEL)]; }
    }
    return true;
}
inline bool applyPattern(PatternBank& bank, int slot, const PatternPreset& p) {
    bank.clear(slot); bank.setLength(slot, p.length); bank.names[slot] = p.name;
    for (int t = 0; t < kChannels; ++t) {
        std::string row = p.tracks[t] ? p.tracks[t] : "";
        for (int s = 0; s < (int)row.size() && s < kMaxSteps; ++s) {
            Step st; char c = row[(size_t)s]; st.on = 1;
            switch (c) { case 'x': break; case 'X': st.vel = 120; st.accent = 1; break; case 'o': st.vel = 55; break; case 'r': st.ratchet = 2; st.vel = 90; break; case 't': st.ratchet = 3; st.vel = 85; break; case '?': st.prob = 50; st.vel = 90; break; case ':': st.prob = 25; st.vel = 80; break; default: st.on = 0; }
            if (st.on) bank.set(slot, t, s, st);
        }
    }
    return true;
}
/// Resets mixer, FX, sends and master FX to defaults, keeping MIDI notes and transport/sequencer settings.
inline void resetMix(float* v) {
    const auto& r = registry();
    for (int c = 0; c < kChannels; ++c) for (int k = M_VOL; k < kChanParams; ++k) v[cp(c, k)] = r[(size_t)cp(c, k)].def;
    for (int i = kMasterBase; i < kNumParams; ++i) v[i] = r[(size_t)i].def;
    v[gp(G_RETA)] = r[(size_t)gp(G_RETA)].def; v[gp(G_RETB)] = r[(size_t)gp(G_RETB)].def; v[gp(G_MVOL)] = r[(size_t)gp(G_MVOL)].def;
}
inline bool applyMixString(float* v, const std::string& mix) {
    std::vector<std::pair<std::string, float>> kv; if (!parseKv(mix, kv)) return false;
    for (auto& [k, x] : kv) { int i = paramIndex(k); if (i < 0) return false; v[i] = clampToDef(i, x); }
    return true;
}
/// Load a complete kit: 12 sounds + mix/FX + its groove into pattern `slot`.
inline bool applyKit(float* v, PatternBank* bank, int slot, const KitPreset& k) {
    resetMix(v); bool ok = true;
    for (int c = 0; c < kChannels; ++c) { auto* s = findSound(k.sounds[c]); if (!s) { ok = false; continue; } applySound(v, c, *s); }
    ok &= applyMixString(v, k.mix);
    if (bank && k.pattern) { auto* p = findPattern(k.pattern); if (p) { applyPattern(*bank, slot, *p); v[gp(G_SWING)] = p->swing; v[gp(G_RATE)] = (float)p->rate; } else ok = false; }
    return ok;
}
} // namespace bk
