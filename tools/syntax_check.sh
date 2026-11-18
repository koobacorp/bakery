#!/bin/bash
# Local (Linux) compile-only check of plugin sources against JUCE headers. Not a build; CI on macOS is the real build.
J=${JUCE_DIR:-/workspace/doctor/JUCE}/modules
D=""; for m in core events data_structures graphics gui_basics gui_extra audio_basics audio_devices audio_formats audio_processors audio_utils dsp; do D="$D -DJUCE_MODULE_AVAILABLE_juce_$m=1"; done
for f in "$@"; do
  echo "== $f"
  python3 -m ziglang c++ -std=c++20 -fsyntax-only -ferror-limit=0 -Wall -Wextra -Wno-unused-parameter -I$J -Isrc/engine -Isrc/plugin -I/tmp/bkstub -DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 -DJucePlugin_Build_Standalone=0 $D "$f" 2>&1 | grep -E "error:|warning:" -A2 | grep -v "unused during\|juce_AudioPluginInstance.h:173" | grep -v "^--$" | head -${LIMIT:-80}
done
