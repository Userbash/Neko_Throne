#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build}"
APP="${APP:-$BUILD_DIR/Neko_Throne}"
CORE="${CORE:-$BUILD_DIR/NekoCore}"
APPDATA_DIR="${APPDATA_DIR:-$(mktemp -d /tmp/neko_audit_appdata.XXXXXX)}"
LOG_ROOT="${LOG_ROOT:-$BUILD_DIR/audit_logs}"
STAMP="$(date +%Y%m%d_%H%M%S)"
RUN_DIR="$LOG_ROOT/$STAMP"
mkdir -p "$RUN_DIR"

APP_STDOUT="$RUN_DIR/app.stdout.log"
X11_LOG="$RUN_DIR/x11.log"
STRACE_LOG="$RUN_DIR/strace.log"
PRECHECK_LOG="$RUN_DIR/precheck.log"
EVENT_LOG="$RUN_DIR/events.log"
SUMMARY_LOG="$RUN_DIR/summary.log"

log() {
  printf '%s %s\n' "$(date +%H:%M:%S)" "$*" | tee -a "$EVENT_LOG"
}

fail() {
  printf 'ERROR: %s\n' "$*" | tee -a "$SUMMARY_LOG" >&2
  exit 1
}

command -v xvfb-run >/dev/null 2>&1 || fail "xvfb-run not found"
command -v xdotool >/dev/null 2>&1 || fail "xdotool not found"
command -v strace >/dev/null 2>&1 || fail "strace not found"
test -x "$APP" || fail "App not found: $APP"

{
  echo "ROOT_DIR=$ROOT_DIR"
  echo "BUILD_DIR=$BUILD_DIR"
  echo "APP=$APP"
  echo "CORE=$CORE"
  echo "APPDATA_DIR=$APPDATA_DIR"
  echo "UID_GID=$(id)"
  echo "--- /dev/net/tun ---"
  ls -l /dev/net/tun || true
  stat -c '%a %U %G %t:%T %n' /dev/net/tun 2>/dev/null || true
  echo "--- getcap ---"
  command -v getcap >/dev/null 2>&1 && getcap "$APP" "$CORE" 2>/dev/null || true
  echo "--- capsh ---"
  command -v capsh >/dev/null 2>&1 && capsh --print || true
  echo "--- x11 tools ---"
  command -v xev >/dev/null 2>&1 && echo "xev=yes" || echo "xev=no"
  command -v xprop >/dev/null 2>&1 && echo "xprop=yes" || echo "xprop=no"
  command -v xwininfo >/dev/null 2>&1 && echo "xwininfo=yes" || echo "xwininfo=no"
} | tee "$PRECHECK_LOG"

export APP BUILD_DIR APPDATA_DIR APP_STDOUT X11_LOG STRACE_LOG EVENT_LOG
xvfb-run -a bash <<'INNER' || true
set -euo pipefail
export QT_LOGGING_RULES="*.debug=true;qt.qpa.*=true"
export NEKO_THRONE_DISABLE_SINGLE_INSTANCE=1
cd "$BUILD_DIR"

timeout 45s strace -ff -tt -s 160 -e trace=network,file,desc,ioctl -o "$STRACE_LOG" \
  "$APP" -platform xcb -appdata "$APPDATA_DIR" >"$APP_STDOUT" 2>&1 &
app_pid=$!
echo "APP_PID=$app_pid" | tee -a "$EVENT_LOG"

pick_main_window() {
  local candidates cand geo width height
  candidates=$(xdotool search --onlyvisible --name "Neko Throne" 2>/dev/null || true)
  for cand in $candidates; do
    geo=$(xwininfo -id "$cand" 2>/dev/null || true)
    width=$(printf '%s\n' "$geo" | awk '/Width:/ {print $2}')
    height=$(printf '%s\n' "$geo" | awk '/Height:/ {print $2}')
    if [ "${width:-0}" -ge 400 ] && [ "${height:-0}" -ge 300 ]; then
      printf '%s\n' "$cand"
      return 0
    fi
  done
  return 1
}

log_action() {
  printf '%s %s\n' "$(date +%H:%M:%S)" "$*" | tee -a "$EVENT_LOG"
}

click_main() {
  local x="$1"
  local y="$2"
  log_action "click-main ($x,$y)"
  xdotool mousemove --window "$wid" "$x" "$y" click 1
  sleep 1
}

key_main() {
  log_action "key-main $*"
  xdotool key --window "$wid" --clearmodifiers --delay 120 "$@"
  sleep 1
}

key_dialog() {
  local target="$1"
  shift
  log_action "key-dialog $*"
  xdotool key --window "$target" --clearmodifiers --delay 120 "$@"
  sleep 1
}

type_dialog() {
  local target="$1"
  shift
  log_action "type-dialog [$*]"
  xdotool type --window "$target" --delay 60 "$*"
  sleep 1
}

wid=""
for _ in 1 2 3 4 5 6 7 8 9 10 11 12; do
  wid=$(pick_main_window || true)
  [ -n "$wid" ] && break
  sleep 1
done
[ -n "$wid" ] || { echo 'main window not found' | tee -a "$EVENT_LOG"; kill -TERM "$app_pid"; wait "$app_pid" || true; exit 1; }

echo "MAIN_WID=$wid" | tee -a "$EVENT_LOG"
xdotool getwindowname "$wid" | tee -a "$EVENT_LOG"
xwininfo -id "$wid" | tee -a "$EVENT_LOG"
if command -v xprop >/dev/null 2>&1; then
  xprop -id "$wid" | sed -n '1,80p' >"$X11_LOG" 2>&1 || true
else
  : >"$X11_LOG"
fi

click_main 220 120
key_main ctrl+n

dwid=""
for _ in 1 2 3 4 5 6; do
  dwid=$(xdotool search --onlyvisible --name "Edit" 2>/dev/null | head -n1 || true)
  [ -n "$dwid" ] && break
  sleep 1
done

if [ -z "$dwid" ]; then
  log_action 'fallback click toolButton_server'
  xdotool mousemove 150 32 click 1
  sleep 1
  log_action 'fallback click menu new profile'
  xdotool mousemove 175 90 click 1
  sleep 2
  for _ in 1 2 3 4 5 6; do
    dwid=$(xdotool search --onlyvisible --name "Edit" 2>/dev/null | head -n1 || true)
    [ -n "$dwid" ] && break
    sleep 1
  done
fi

echo "DIALOG_WID=${dwid:-}" | tee -a "$EVENT_LOG"
if [ -n "$dwid" ]; then
  xwininfo -id "$dwid" | tee -a "$EVENT_LOG"
  key_dialog "$dwid" Tab
  type_dialog "$dwid" AuditProfile
  key_dialog "$dwid" Tab
  type_dialog "$dwid" 127.0.0.1
  key_dialog "$dwid" Tab
  type_dialog "$dwid" 1080
  key_dialog "$dwid" Return
  sleep 2
fi

click_main 250 220
click_main 250 220
click_main 70 242
sleep 2

kill -0 "$app_pid"
echo 'APP_ALIVE=1' | tee -a "$EVENT_LOG"
kill -TERM "$app_pid" 2>/dev/null || true
wait "$app_pid" || true
INNER

{
  echo "RUN_DIR=$RUN_DIR"
  echo "--- created appdata files ---"
  find "$APPDATA_DIR" -maxdepth 3 -type f | sort
  echo "--- profile files ---"
  find "$APPDATA_DIR/config/profiles" -maxdepth 1 -type f -print -exec sed -n '1,220p' {} \; 2>/dev/null || true
  echo "--- configs.json ---"
  sed -n '1,220p' "$APPDATA_DIR/config/configs.json" 2>/dev/null || true
  echo "--- tun-related strace ---"
  grep -nE 'TUNSETIFF|/dev/net/tun|EPERM|ENOENT|socket\(' "$STRACE_LOG"* 2>/dev/null || true
  echo "--- app stdout tail ---"
  tail -n 160 "$APP_STDOUT" 2>/dev/null || true
  echo "--- x11 tail ---"
  tail -n 80 "$X11_LOG" 2>/dev/null || true
} | tee "$SUMMARY_LOG"

echo "Audit complete. Logs: $RUN_DIR"
