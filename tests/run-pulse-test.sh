#!/usr/bin/env bash
set -euo pipefail
bin="${XMMS_TEST_PULSE_BIN:-pulseaudio}"
if ! command -v "$bin" >/dev/null; then
    echo 'SKIP: pulseaudio is required for the isolated null-sink tests.'
    exit 77
fi
work="$(mktemp -d "${TMPDIR:-/tmp}/xmms-pulse-test.XXXXXXXX")"
server_pid=
cleanup() {
    if [[ -n $server_pid ]]; then
        kill "$server_pid" 2>/dev/null || true
        wait "$server_pid" 2>/dev/null || true
    fi
    rm -rf -- "$work"
}
trap cleanup EXIT
mkdir -m700 "$work/runtime" "$work/state" "$work/config"
args=()
if [[ -n ${XMMS_TEST_PULSE_MODULES:-} ]]; then
    args+=("--dl-search-path=$XMMS_TEST_PULSE_MODULES")
fi
env PULSE_RUNTIME_PATH="$work/runtime" PULSE_STATE_PATH="$work/state" \
    DBUS_SESSION_BUS_ADDRESS="unix:path=$work/no-bus" \
    XDG_CONFIG_HOME="$work/config" "$bin" --daemonize=no --use-pid-file=no \
    --exit-idle-time=-1 --disable-shm --disallow-exit --log-level=warn \
    "${args[@]}" -n \
    --load="module-native-protocol-unix socket=$work/runtime/native auth-anonymous=1 auth-cookie-enabled=0" \
    --load='module-null-sink sink_name=xmms_test rate=44100 channels=2' \
    >"$work/server.log" 2>&1 &
server_pid=$!
for ((i=0; i<100; i++)); do
    [[ -S $work/runtime/native ]] && break
    if ! kill -0 "$server_pid" 2>/dev/null; then
        cat "$work/server.log" >&2
        exit 1
    fi
    sleep 0.05
done
[[ -S $work/runtime/native ]] || { cat "$work/server.log" >&2; exit 1; }
test_bin="${XMMS_TEST_BUILDDIR:-$(dirname "$0")}/test_pulse"
if ! env XMMS_TEST_PULSE_SERVER="unix:$work/runtime/native" "$test_bin"; then
    cat "$work/server.log" >&2
    exit 1
fi
