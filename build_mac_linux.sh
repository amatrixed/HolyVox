#!/usr/bin/env bash
# macOS: needs Xcode command line tools + cmake + git.  Linux: see README for apt packages.
set -e
EXTRA=""
if [[ "$(uname)" == "Darwin" ]]; then EXTRA='-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'; fi
cmake -B build -DCMAKE_BUILD_TYPE=Release "$EXTRA"
cmake --build build --config Release --parallel
OUT="build/HolyVox_artefacts/Release/VST3/HolyVox.vst3"
echo "Done: $OUT"
if [[ "$(uname)" == "Darwin" ]]; then
  mkdir -p ~/Library/Audio/Plug-Ins/VST3 && cp -R "$OUT" ~/Library/Audio/Plug-Ins/VST3/ && echo "Installed in ~/Library/Audio/Plug-Ins/VST3"
else
  mkdir -p ~/.vst3 && cp -R "$OUT" ~/.vst3/ && echo "Installed in ~/.vst3"
fi
