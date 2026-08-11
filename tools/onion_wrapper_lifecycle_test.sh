#!/bin/sh
set -eu

WRAPPER_PATH="${1:-$(dirname "$0")/templates/onion-sprout-launch.sh}"
CASE_ROOT="${TMPDIR:-/tmp}/sprout-wrapper-lifecycle-$$"
DUMMY_PID=""
DUMMY_PID_2=""
WRAPPER_PID=""

cleanup() {
  if [ -n "$WRAPPER_PID" ]; then
    kill -TERM "$WRAPPER_PID" 2>/dev/null || true
    wait "$WRAPPER_PID" 2>/dev/null || true
  fi
  if [ -n "$DUMMY_PID" ]; then
    kill -CONT "$DUMMY_PID" 2>/dev/null || true
    kill -TERM "$DUMMY_PID" 2>/dev/null || true
    wait "$DUMMY_PID" 2>/dev/null || true
  fi
  if [ -n "$DUMMY_PID_2" ]; then
    kill -CONT "$DUMMY_PID_2" 2>/dev/null || true
    kill -TERM "$DUMMY_PID_2" 2>/dev/null || true
    wait "$DUMMY_PID_2" 2>/dev/null || true
  fi
  rm -rf "$CASE_ROOT"
}
trap cleanup EXIT INT TERM

fail() {
  printf 'wrapper lifecycle failure: %s\n' "$1" >&2
  exit 1
}

wait_for_file() {
  target="$1"
  attempts=0
  while [ ! -e "$target" ] && [ "$attempts" -lt 100 ]; do
    sleep 0.05
    attempts=$((attempts + 1))
  done
  [ -e "$target" ] || fail "timed out waiting for $target"
}

start_dummy_launcher() {
  HEARTBEAT="$TEST_ROOT/onion-heartbeat"
  : >"$HEARTBEAT"
  while :; do
    printf '.\n' >>"$HEARTBEAT"
    sleep 0.05
  done &
  DUMMY_PID=$!
}

start_second_dummy_launcher() {
  HEARTBEAT_2="$TEST_ROOT/onion-heartbeat-2"
  : >"$HEARTBEAT_2"
  while :; do
    printf '.
' >>"$HEARTBEAT_2"
    sleep 0.05
  done &
  DUMMY_PID_2=$!
}

heartbeat_count() {
  wc -l <"$HEARTBEAT"
}

assert_dummy_resumed() {
  kill -0 "$DUMMY_PID" 2>/dev/null || fail "dummy Onion launcher exited unexpectedly"
  before="$(heartbeat_count)"
  sleep 0.2
  after="$(heartbeat_count)"
  [ "$after" -gt "$before" ] || fail "Onion launcher remained stopped after wrapper cleanup"
}

assert_dummy_stopped() {
  kill -0 "$DUMMY_PID" 2>/dev/null || fail "dummy Onion launcher exited unexpectedly"
  sleep 0.1
  before="$(heartbeat_count)"
  sleep 0.2
  after="$(heartbeat_count)"
  [ "$after" -eq "$before" ] || fail "Onion launcher was not stopped while Sprout owned the display"
}

assert_second_dummy_resumed() {
  kill -0 "$DUMMY_PID_2" 2>/dev/null || fail "second framebuffer owner exited unexpectedly"
  before="$(wc -l <"$HEARTBEAT_2")"
  sleep 0.2
  after="$(wc -l <"$HEARTBEAT_2")"
  [ "$after" -gt "$before" ] || fail "second framebuffer owner remained stopped after wrapper cleanup"
}

assert_second_dummy_stopped() {
  kill -0 "$DUMMY_PID_2" 2>/dev/null || fail "second framebuffer owner exited unexpectedly"
  sleep 0.1
  before="$(wc -l <"$HEARTBEAT_2")"
  sleep 0.2
  after="$(wc -l <"$HEARTBEAT_2")"
  [ "$after" -eq "$before" ] || fail "second framebuffer owner was not stopped while Sprout owned the display"
}

prepare_case() {
  name="$1"
  TEST_ROOT="$CASE_ROOT/$name"
  APP_ROOT="$TEST_ROOT/app"
  DATA_ROOT="$TEST_ROOT/data"
  SD_ROOT="$TEST_ROOT/sd"
  RUNTIME_ROOT="$TEST_ROOT/runtime"
  LOCK_DIR="$TEST_ROOT/lock"
  EXIT_MARKER="$TEST_ROOT/authorized-exit"
  mkdir -p "$APP_ROOT/bin" "$APP_ROOT/games" "$APP_ROOT/config" \
    "$DATA_ROOT" "$SD_ROOT" "$RUNTIME_ROOT/script"
  : >"$APP_ROOT/config/household-seed.json"
  : >"$APP_ROOT/bin/sprout-runtime"
  cat >"$RUNTIME_ROOT/script/stop_audioserver.sh" <<'EOF'
printf 'audio-stopped\n' >"$TEST_AUDIO_MARKER"
EOF
  cat >"$APP_ROOT/bin/sprout-launcher" <<'EOF'
#!/bin/sh
set -u
{
  printf 'video=%s\n' "${SDL_VIDEODRIVER:-}"
  printf 'audio=%s\n' "${SDL_AUDIODRIVER:-}"
  printf 'egl=%s\n' "${EGL_VIDEODRIVER:-}"
  printf 'contained=%s\n' "${SPROUT_CONTAINED:-}"
  printf 'runtime_root=%s\n' "${SPROUT_ONION_RUNTIME_ROOT:-}"
  printf 'args=%s\n' "$*"
} >>"$TEST_ENV_LOG"
printf 'launch\n' >>"$TEST_CALL_LOG"
case "$TEST_MODE" in
  handoff) exit 75 ;;
  authorized) : >"$SPROUT_EXIT_MARKER"; exit 0 ;;
  error) exit 9 ;;
  *) exit 64 ;;
esac
EOF
  chmod +x "$APP_ROOT/bin/sprout-launcher"
  TEST_AUDIO_MARKER="$TEST_ROOT/audio.marker"
  TEST_ENV_LOG="$TEST_ROOT/environment.log"
  TEST_CALL_LOG="$TEST_ROOT/calls.log"
  export SPROUT_APP_ROOT="$APP_ROOT"
  export SPROUT_DATA_ROOT="$DATA_ROOT"
  export SPROUT_SD_ROOT="$SD_ROOT"
  export SPROUT_ONION_RUNTIME_ROOT="$RUNTIME_ROOT"
  export SPROUT_LOCK_DIR="$LOCK_DIR"
  export SPROUT_EXIT_MARKER="$EXIT_MARKER"
  export SPROUT_STOP_AUDIOSERVER_SCRIPT="$RUNTIME_ROOT/script/stop_audioserver.sh"
  export SPROUT_AUTO_LAUNCH=1
  export TEST_AUDIO_MARKER TEST_ENV_LOG TEST_CALL_LOG
}

run_handoff_case() {
  prepare_case handoff
  start_dummy_launcher
  export SPROUT_ONION_L_PID="$DUMMY_PID"
  export TEST_MODE=handoff
  set +e
  sh "$WRAPPER_PATH"
  status=$?
  set -e
  [ "$status" -eq 75 ] || fail "handoff exit was $status, expected 75"
  [ ! -e "$LOCK_DIR" ] || fail "handoff left the single-owner lock behind"
  assert_dummy_resumed
  [ -f "$TEST_AUDIO_MARKER" ] || fail "known-good Onion audio handoff was not sourced"
  grep -q '^video=mmiyoo$' "$TEST_ENV_LOG" || fail "mmiyoo video environment missing"
  grep -q '^audio=mmiyoo$' "$TEST_ENV_LOG" || fail "mmiyoo audio environment missing"
  grep -q '^egl=mmiyoo$' "$TEST_ENV_LOG" || fail "mmiyoo EGL environment missing"
  grep -q '^contained=1$' "$TEST_ENV_LOG" || fail "contained environment missing"
  grep -q "^runtime_root=$RUNTIME_ROOT$" "$TEST_ENV_LOG" || fail "runtime root was not propagated"
  grep -q -- "--sd-root $SD_ROOT" "$TEST_ENV_LOG" || fail "SD root argument was not propagated"
  kill -TERM "$DUMMY_PID"
  wait "$DUMMY_PID" 2>/dev/null || true
  DUMMY_PID=""
}

run_error_signal_case() {
  prepare_case error-signal
  start_dummy_launcher
  start_second_dummy_launcher
  export SPROUT_ONION_L_PID="$DUMMY_PID"
  export SPROUT_ONION_FB_OWNER_PIDS="$DUMMY_PID $DUMMY_PID_2"
  export TEST_MODE=error
  sh "$WRAPPER_PATH" &
  WRAPPER_PID=$!
  wait_for_file "$LOCK_DIR/pid"
  wait_for_file "$TEST_CALL_LOG"
  attempts=0
  while [ "$(wc -l <"$TEST_CALL_LOG")" -lt 2 ] && [ "$attempts" -lt 100 ]; do
    sleep 0.05
    attempts=$((attempts + 1))
  done
  [ "$(wc -l <"$TEST_CALL_LOG")" -ge 2 ] || fail "launcher error was not supervised and restarted"
  assert_dummy_stopped
  assert_second_dummy_stopped
  kill -TERM "$WRAPPER_PID"
  wait "$WRAPPER_PID" 2>/dev/null || true
  WRAPPER_PID=""
  [ ! -e "$LOCK_DIR" ] || fail "TERM left the single-owner lock behind"
  assert_dummy_resumed
  assert_second_dummy_resumed
  kill -TERM "$DUMMY_PID"
  wait "$DUMMY_PID" 2>/dev/null || true
  DUMMY_PID=""
  kill -TERM "$DUMMY_PID_2"
  wait "$DUMMY_PID_2" 2>/dev/null || true
  DUMMY_PID_2=""
}

run_handoff_case
run_error_signal_case
printf 'Onion wrapper lifecycle contract passed\n'
