#!/usr/bin/env bash
set -euo pipefail
PROJECT4_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
PYTHON="${DEMO_PYTHON:-$PROJECT4_ROOT/.venv/bin/python}"
if [[ ! -x "$PYTHON" ]]; then
  echo "Create the project environment first: ./scripts/setup_env.sh gpu" >&2
  exit 1
fi
unset PYTHONHOME LD_PRELOAD LD_LIBRARY_PATH
export PYTHONPATH="$PROJECT4_ROOT/src"
export PYTHONNOUSERSITE=1
export PYTHONDONTWRITEBYTECODE=1
export TORCH_EXTENSIONS_DIR="$PROJECT4_ROOT/.cache/torch_extensions"
export XDG_CACHE_HOME="$PROJECT4_ROOT/.cache"
export PIP_CACHE_DIR="$PROJECT4_ROOT/.cache/pip"
cd "$PROJECT4_ROOT"
exec "$PYTHON" -m robotnav.demo "$@"
