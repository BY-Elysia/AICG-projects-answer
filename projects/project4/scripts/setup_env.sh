#!/usr/bin/env bash
set -euo pipefail
PROJECT4_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT4_ROOT"
MODE="${1:-gpu}"
case "$MODE" in cpu|gpu) ;; *) echo "Usage: $0 [cpu|gpu]" >&2; exit 2 ;; esac
PYTHON_BIN="${PYTHON_BIN:-python3.10}"
unset PYTHONPATH PYTHONHOME
export PYTHONNOUSERSITE=1
"$PYTHON_BIN" -c 'import sys; assert sys.version_info[:2] == (3,10), "Python 3.10 is required"'
if [[ ! -x .venv/bin/python ]]; then
  "$PYTHON_BIN" -m venv .venv
fi
PYTHON="$PROJECT4_ROOT/.venv/bin/python"
"$PYTHON" -c 'import sys; assert sys.version_info[:2] == (3,10); assert sys.prefix != sys.base_prefix'
PIP=("$PYTHON" -m pip --isolated --cache-dir "$PROJECT4_ROOT/.cache/pip")
"${PIP[@]}" install --index-url https://pypi.org/simple 'pip>=24,<26'
"${PIP[@]}" install --index-url https://pypi.org/simple -e .
if [[ "$MODE" == gpu ]]; then
  "$PYTHON" -c 'import platform; assert platform.system() == "Linux" and platform.machine() == "x86_64", "GPU wheels require Linux x86_64"'
  # Read version pins from pyproject.toml; keep GPU indexes separate from PyPI.
  mapfile -t GPU_PACKAGES < <("$PYTHON" - <<'PY'
from pathlib import Path
import tomli
extra = tomli.loads(Path("pyproject.toml").read_text())["project"]["optional-dependencies"]["gpu"]
for name in ("torch", "gsplat"):
    matches = [value for value in extra if value.startswith(name + "==")]
    assert len(matches) == 1, f"Expected one exact {name} requirement"
    print(matches[0])
PY
  )
  [[ "${#GPU_PACKAGES[@]}" == 2 ]]
  "${PIP[@]}" install --index-url https://download.pytorch.org/whl/cu121 \
    --only-binary=:all: --no-deps "${GPU_PACKAGES[0]}"
  "${PIP[@]}" install --index-url https://docs.gsplat.studio/whl/ \
    --only-binary=:all: --no-deps "${GPU_PACKAGES[1]}"
  # Install all remaining runtime dependencies, including Torch's CUDA libraries.
  "${PIP[@]}" install --index-url https://pypi.org/simple -e '.[gpu]'
fi
"${PIP[@]}" check
echo "Environment ready: $PROJECT4_ROOT/.venv"
if [[ "$MODE" == gpu ]]; then
  echo "Run: ./scripts/run_demo.sh check"
else
  echo "Run: ./scripts/run_demo.sh plan"
  echo "Run: ./scripts/run_demo.sh robot-preview"
fi
