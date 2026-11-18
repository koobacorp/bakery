# Building BAKERY

The official build runs on GitHub Actions (`.github/workflows/macos.yml`, runner `macos-15`, Xcode).

**Requirements**
- JUCE 8.0.4. CMake uses a local `JUCE/` folder if there is one; otherwise it fetches JUCE with FetchContent.
- CMake 3.22 or newer
- Xcode 15 or newer (or the Command Line Tools)

**What it produces**
- Universal binaries for arm64 and x86_64
- macOS 11 or newer
- Ad-hoc signed `BAKERY.vst3` and `BAKERY.component`

## Build on a Mac

These commands do not install anything into the plug-in folders.

```bash
git clone https://github.com/koobacorp/bakery.git && cd bakery      # private repo
git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git JUCE
cmake -S . -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build --config Release --parallel
```

Outputs:
- `build/Bakery_artefacts/Release/VST3/BAKERY.vst3`
- `build/Bakery_artefacts/Release/AU/BAKERY.component`

## Tests

```bash
./build/Release/bakery_engine_tests            # full engine suite incl. 10-minute long run (add --quick to skip it)
(mkdir -p shots && cd shots && ../build/bakery_plugin_tests_artefacts/Release/bakery_plugin_tests)   # JUCE tests + UI PNGs
./build/Release/bakery_render_demos demos      # demo WAVs
```

## Sign, validate, package (same steps as CI)

```bash
for b in build/Bakery_artefacts/Release/VST3/BAKERY.vst3 build/Bakery_artefacts/Release/AU/BAKERY.component; do
  strip -x "$b/Contents/MacOS/BAKERY"; codesign --force --deep --sign - --timestamp=none "$b"; codesign --verify --deep --strict "$b"
  lipo "$b/Contents/MacOS/BAKERY" -verify_arch arm64 x86_64
done
# pluginval (download from github.com/Tracktion/pluginval/releases)
pluginval.app/Contents/MacOS/pluginval --strictness-level 10 --validate-in-process build/Bakery_artefacts/Release/VST3/BAKERY.vst3
# auval needs the component installed; on your own Mac use scripts/install_mac.sh, then:
auval -strict -v aumu Bkry Koob
```

## Engine only, without JUCE (any OS)

The DSP engine is header-only C++20 and can be tested anywhere:

```bash
c++ -std=c++20 -O2 -Isrc/engine tests/test_engine.cpp -o engine_tests && ./engine_tests --quick
c++ -std=c++20 -O2 -Isrc/engine tests/render_demos.cpp -o render_demos && ./render_demos dist
```

During development this ran on Linux using `python3 -m ziglang c++`. `tools/syntax_check.sh` compile-checks the JUCE sources against the JUCE headers on Linux. It is only a syntax check; the real build is CI.

## Plugin identity

| Field | Value |
|---|---|
| Name | BAKERY |
| Bundle id | com.koobacorp.bakery |
| Manufacturer code | Koob |
| Plugin code | Bkry |
| AU type | aumu (MusicDevice) |
| VST3 category | Instrument\|Drum |
| Version | 1.0.0 |
