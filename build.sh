#!/usr/bin/env bash
set -euo pipefail
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j"$(nproc)"
echo "Compiling shader -> shaders/sha256t.spv"
command -v glslc >/dev/null || { echo "Please install glslc (glslang-tools)"; exit 1; }
mkdir -p shaders
glslc ../shaders/sha256t.comp -O -o shaders/sha256t.spv
echo "Done. Quick start: cd build && ./janusv"
