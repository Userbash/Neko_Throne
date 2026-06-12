#!/bin/sh
set -eu

ARTIFACT_DIR=${ARTIFACT_DIR:-/tmp/Neko_Throne/artifacts/linux-amd64}
APP=${APP:-$ARTIFACT_DIR/Neko_Throne}
LOG_FILE=${LOG_FILE:-$ARTIFACT_DIR/headless_smoke_test.log}

export HOME=${HOME:-/tmp/neko_throne_smoke_home}
export XDG_CONFIG_HOME=${XDG_CONFIG_HOME:-$HOME/.config}
export XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}
export XDG_CACHE_HOME=${XDG_CACHE_HOME:-$HOME/.cache}
export QT_QPA_PLATFORM=${QT_QPA_PLATFORM:-offscreen}
export NEKO_THRONE_DISABLE_SINGLE_INSTANCE=1
export QT_DEBUG_PLUGINS=${QT_DEBUG_PLUGINS:-0}

mkdir -p "$HOME" "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_CACHE_HOME"

if [ ! -x "$APP" ]; then
  echo "missing app: $APP" >&2
  exit 1
fi

rm -f "$LOG_FILE"
set +e
timeout 25s "$APP" --headless-smoke --version >"$LOG_FILE" 2>&1
status=$?
set -e

cat "$LOG_FILE"

if [ "$status" -ne 0 ] && [ "$status" -ne 124 ]; then
  echo "headless smoke test failed: $status" >&2
  exit "$status"
fi

if grep -qi "segmentation fault\|SIGSEGV\|core dumped\|Another instance is already running" "$LOG_FILE"; then
  echo "unexpected runtime error in smoke log" >&2
  exit 1
fi

echo "headless smoke test ok"
