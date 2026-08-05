#!/bin/sh
set -u

APP_ROOT="/mnt/SDCARD/App/Sprout"
DATA_ROOT="/mnt/SDCARD/Saves/CurrentProfile/sprout"
LOG_ROOT="$DATA_ROOT/logs"
STAMP="$(date +%Y%m%d-%H%M%S)-$$"
LOG_FILE="$LOG_ROOT/launcher-$STAMP.log"

mkdir -p "$DATA_ROOT" "$LOG_ROOT" "$DATA_ROOT/tmp" "$DATA_ROOT/home"
export HOME="$DATA_ROOT/home"
export TMPDIR="$DATA_ROOT/tmp"
export LD_LIBRARY_PATH="$APP_ROOT/lib:/mnt/SDCARD/.tmp_update/lib/parasyte:/mnt/SDCARD/miyoo/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

"$APP_ROOT/bin/sprout-launcher" \
  --data-dir "$DATA_ROOT" \
  --sd-root /mnt/SDCARD \
  --arcade-root "$APP_ROOT/games" \
  --runtime "$APP_ROOT/bin/sprout-runtime" \
  --household-seed "$APP_ROOT/config/household-seed.json" \
  >>"$LOG_FILE" 2>&1
STATUS=$?
printf '\nsprout-launcher-exit-code: %s\n' "$STATUS" >>"$LOG_FILE"
sync
exit "$STATUS"
