#!/usr/bin/env bash
set -euo pipefail
project_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ ! -x "$project_root/build/base_vision" ]]; then
    bash "$project_root/scripts/build-linux.sh"
fi
exec "$project_root/build/base_vision" "$@"
