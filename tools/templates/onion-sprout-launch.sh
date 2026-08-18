#!/bin/sh
set -u

APP_ROOT="${SPROUT_APP_ROOT:-/mnt/SDCARD/App/Sprout}"
DATA_ROOT="${SPROUT_DATA_ROOT:-/mnt/SDCARD/Saves/CurrentProfile/sprout}"
SD_ROOT="${SPROUT_SD_ROOT:-/mnt/SDCARD}"
ONION_RUNTIME_ROOT="${SPROUT_ONION_RUNTIME_ROOT:-/mnt/SDCARD/.tmp_update}"
LOG_ROOT="$DATA_ROOT/logs"
STAMP="$(date +%Y%m%d-%H%M%S)-$$"
LOG_FILE="$LOG_ROOT/launcher-$STAMP.log"
# mkdir is atomic on the Miyoo's tmpfs. A second supervisor must never reach
# SDL while the active supervisor owns the application session.
SPROUT_LOCK_DIR="${SPROUT_LOCK_DIR:-/tmp/sprout-launcher.lock}"

mkdir -p "$DATA_ROOT" "$LOG_ROOT" "$DATA_ROOT/tmp" "$DATA_ROOT/home"
printf '%s wrapper-start pid=%s\n' "$(date +%Y-%m-%dT%H:%M:%S)" "$$" >"$LOG_FILE"
LOCK_ACQUIRED=0
if mkdir "$SPROUT_LOCK_DIR" 2>/dev/null; then
  LOCK_ACQUIRED=1
else
  STALE_OWNER="$(cat "$SPROUT_LOCK_DIR/pid" 2>/dev/null || true)"
  case "$STALE_OWNER" in
    ''|*[!0-9]*) ;;
    *)
      if ! kill -0 "$STALE_OWNER" 2>/dev/null; then
        rm -f "$SPROUT_LOCK_DIR/pid" 2>/dev/null || true
        rmdir "$SPROUT_LOCK_DIR" 2>/dev/null || true
      fi
      ;;
  esac
  if mkdir "$SPROUT_LOCK_DIR" 2>/dev/null; then
    LOCK_ACQUIRED=1
  fi
fi
if [ "$LOCK_ACQUIRED" -ne 1 ]; then
  # Onion may race its idle router against the already-running App command.
  # Returning immediately makes that router queue another copy every second;
  # wait behind the real owner instead, without ever touching fb0 ourselves.
  ACTIVE_OWNER="$(cat "$SPROUT_LOCK_DIR/pid" 2>/dev/null || true)"
  printf '%s duplicate-launch-refused owner=%s\n' \
    "$(date +%Y-%m-%dT%H:%M:%S)" \
    "${ACTIVE_OWNER:-unknown}" >>"$LOG_FILE"
  case "$ACTIVE_OWNER" in
    ''|*[!0-9]*) exit 0 ;;
  esac
  while kill -0 "$ACTIVE_OWNER" 2>/dev/null; do
    sleep 1
  done
  exit 0
fi
printf '%s\n' "$$" >"$SPROUT_LOCK_DIR/pid"
export HOME="$DATA_ROOT/home"
export TMPDIR="$DATA_ROOT/tmp"
export LD_LIBRARY_PATH="$APP_ROOT/lib:/lib:/config/lib:$ONION_RUNTIME_ROOT/lib:$ONION_RUNTIME_ROOT/lib/parasyte:$SD_ROOT/miyoo/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export SDL_VIDEODRIVER=mmiyoo
export SPROUT_DIRECT_FRAMEBUFFER=/dev/fb0
export SPROUT_CONTAINED=1
export SPROUT_INPUT_PROBE=1
export SPROUT_ONION_RUNTIME_ROOT="$ONION_RUNTIME_ROOT"
export SPROUT_EXIT_MARKER="${SPROUT_EXIT_MARKER:-/tmp/sprout-authorized-exit-$$}"
export SDL_AUDIODRIVER=mmiyoo
export EGL_VIDEODRIVER=mmiyoo

ONION_FB_OWNER_PIDS="${SPROUT_ONION_FB_OWNER_PIDS:-}"
if [ -z "$ONION_FB_OWNER_PIDS" ] && [ -n "${SPROUT_ONION_L_PID:-}" ]; then
  # Compatibility injection for lifecycle tests and older launch callers.
  ONION_FB_OWNER_PIDS="$SPROUT_ONION_L_PID"
fi
if [ -z "$ONION_FB_OWNER_PIDS" ]; then
  for OWNER_FD in /proc/[0-9]*/fd/*; do
    [ "$(readlink "$OWNER_FD" 2>/dev/null || true)" = "/dev/fb0" ] || continue
    OWNER_PID="${OWNER_FD#/proc/}"
    OWNER_PID="${OWNER_PID%%/*}"
    [ "$OWNER_PID" = "$$" ] && continue
    case " $ONION_FB_OWNER_PIDS " in
      *" $OWNER_PID "*) ;;
      *) ONION_FB_OWNER_PIDS="$ONION_FB_OWNER_PIDS $OWNER_PID" ;;
    esac
  done
fi
PAUSED_ONION_FB_OWNER_PIDS=""
release_sprout_lock() {
  rm -f "$SPROUT_LOCK_DIR/pid" 2>/dev/null || true
  rmdir "$SPROUT_LOCK_DIR" 2>/dev/null || true
}
pause_onion_fb_owners() {
  for OWNER_PID in $ONION_FB_OWNER_PIDS; do
    kill -0 "$OWNER_PID" 2>/dev/null || continue
    OWNER_STATE="$(awk '{print $3}' "/proc/$OWNER_PID/stat" 2>/dev/null || true)"
    [ "$OWNER_STATE" = "T" ] && continue
    if kill -STOP "$OWNER_PID" 2>/dev/null; then
      PAUSED_ONION_FB_OWNER_PIDS="$PAUSED_ONION_FB_OWNER_PIDS $OWNER_PID"
    fi
  done
}
resume_onion_fb_owners() {
  STATUS=$?
  trap - EXIT INT TERM
  release_sprout_lock
  for OWNER_PID in $PAUSED_ONION_FB_OWNER_PIDS; do
    if kill -0 "$OWNER_PID" 2>/dev/null; then
      kill -CONT "$OWNER_PID" 2>/dev/null || true
    fi
  done
  exit "$STATUS"
}
trap resume_onion_fb_owners EXIT INT TERM
pause_onion_fb_owners

# Keep disp_init alive. Sprout owns fb0 while Onion's launcher is paused.
STOP_AUDIOSERVER_SCRIPT="${SPROUT_STOP_AUDIOSERVER_SCRIPT:-$ONION_RUNTIME_ROOT/script/stop_audioserver.sh}"
if [ -r "$STOP_AUDIOSERVER_SCRIPT" ]; then
  . "$STOP_AUDIOSERVER_SCRIPT"
fi
printf '%s launcher-supervisor-start\n' "$(date +%Y-%m-%dT%H:%M:%S)" >>"$LOG_FILE"
rm -f "$SPROUT_EXIT_MARKER"
if [ "${SPROUT_AUTO_LAUNCH:-0}" != "1" ]; then
  rm -f /tmp/sprout-stock-onion-session
fi

while :; do
  "$APP_ROOT/bin/sprout-launcher" \
    --data-dir "$DATA_ROOT" \
    --sd-root "$SD_ROOT" \
    --arcade-root "$APP_ROOT/games" \
    --runtime "$APP_ROOT/bin/sprout-runtime" \
    --household-seed "$APP_ROOT/config/household-seed.json" \
    >>"$LOG_FILE" 2>&1
  STATUS=$?
  printf '\nsprout-launcher-exit-code: %s\n' "$STATUS" >>"$LOG_FILE"

  if [ -f "$SPROUT_EXIT_MARKER" ]; then
    rm -f "$SPROUT_EXIT_MARKER"
    : >/tmp/sprout-stock-onion-session
    exit 0
  fi
  if [ "$STATUS" -eq 75 ]; then
    exit 75
  fi

  printf '%s unapproved-exit-restarting status=%s\n' \
    "$(date +%Y-%m-%dT%H:%M:%S)" "$STATUS" >>"$LOG_FILE"
  sleep 1
done
