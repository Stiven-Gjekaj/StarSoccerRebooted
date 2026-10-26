#!/bin/sh
# Build and run the tests on this computer.
# Usage: scripts/test.sh
set -e

ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build -j8 >/dev/null
ctest --test-dir build --output-on-failure
