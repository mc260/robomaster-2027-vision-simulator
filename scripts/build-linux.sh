#!/usr/bin/env bash
set -euo pipefail
project_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cmake -S "$project_root" -B "$project_root/build" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON "$@"
cmake --build "$project_root/build" --parallel 4
ctest --test-dir "$project_root/build" --output-on-failure
