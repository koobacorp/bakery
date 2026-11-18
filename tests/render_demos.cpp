// Renders the demo WAVs into dist/ using the real engine (same code the plugin runs).
#include "TestUtil.h"
using namespace bk;
int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : "dist";
    const int sr = 48000;
    // 1) kick showcase: one hit each, 0.9 s apart
    {
        const char* kicks[] = {"808 Classic", "808 Long Boom", "909 Classic", "909 Punchy Electronic", "Deep Sub Kick", "808 Short Tight", "Distorted Industrial", "909 Hard Electronic", "Experimental FM Kick", "Cinematic Impact"};
        auto m = tu::machine(sr); tu::clearAll(*m); std::vector<float> L, R; int slot = (int)(0.9 * sr);
        for (auto* k : kicks) {
            applySound(m->params, 0, *findSound(k)); if (findSound(k)->engine == E_IMPACT) slot = (int)(2.5 * sr);
            auto r = tu::render(*m, sr, slot / (double)sr, 120, 512, false, 0, 0, [](int64_t pos, std::vector<MidiEvent>& ev) { if (pos == 0) ev.push_back({0, 0x99, 36, 120}); });
            L.insert(L.end(), r.L.begin(), r.L.end()); R.insert(R.end(), r.R.begin(), r.R.end());
        }
        tu::writeWav(dir + "/BAKERY-demo-kicks.wav", L, R, sr);
    }
    auto kitDemo = [&](const char* kit, double bpm, double bars, const char* file) {
        auto m = tu::machine(sr); for (auto& k : factoryKits()) if (std::string(k.name) == kit) applyKit(m->params, &m->bank, 0, k);
        double sec = bars * 4 * 60 / bpm * (m->bank.length(0) > 16 ? 1 : 1); auto r = tu::render(*m, sr, sec + 1.0, bpm, 512);
        tu::writeWav(dir + "/" + file, r.L, r.R, sr); std::printf("%s: peak %.3f rms %.4f\n", file, tu::peak(r.L), tu::rms(r.L));
    };
    kitDemo("909 House Kit", 120, 8, "BAKERY-demo-909-house-120bpm.wav");
    kitDemo("Hard Electronic Kit", 140, 8, "BAKERY-demo-hard-electronic-140bpm.wav");
    kitDemo("Distorted Industrial Kit", 140, 8, "BAKERY-demo-industrial-140bpm.wav");
    kitDemo("Classic 808 Kit", 92, 4, "BAKERY-demo-808-boombap-92bpm.wav");
    kitDemo("Trap 808 Kit", 140, 4, "BAKERY-demo-trap-140bpm.wav");
    // random kits: 4 modes, 2 bars each
    {
        std::vector<float> L, R; Rng rng(2026);
        for (int mode : {R_CLASSIC, R_INDUSTRIAL, R_COMPLEX, R_AMBIENT}) {
            auto m = tu::machine(sr); Locks lk; randomKit(m->params, mode, 0.6f, rng, lk); randomPattern(m->bank, 0, mode, 0.6f, rng, lk); m->params[sp(0, 6)] = 1;
            auto r = tu::render(*m, sr, 4.0 * (m->bank.length(0) / 16.0), 120, 512); L.insert(L.end(), r.L.begin(), r.L.end()); R.insert(R.end(), r.R.begin(), r.R.end());
        }
        tu::writeWav(dir + "/BAKERY-demo-random-kits-120bpm.wav", L, R, sr);
    }
    std::puts("demos written");
}
