#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
work="$(mktemp -d "${TMPDIR:-/tmp}/xmms-sanitizers.XXXXXXXX")"
trap 'rm -rf -- "$work"' EXIT
cd "$root"
read -ra flags <<< "$(pkg-config --cflags gtk+-3.0)"
read -ra libs <<< "$(pkg-config --libs gtk+-3.0)"
common=(-g -O1 -fno-omit-frame-pointer '-fsanitize=address,undefined' -I. "${flags[@]}")
for name in bmp config ui_scale playback; do
    source=()
    case "$name" in
        bmp) source+=(xmms/bmp.c) ;;
        config) source+=(libxmms/configfile.c) ;;
    esac
    "${CC:-cc}" "${common[@]}" "tests/test_$name.c" "${source[@]}" \
        "${libs[@]}" -o "$work/test_$name"
    ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 "$work/test_$name"
done
if pkg-config --exists libpulse; then
    read -ra pulse_flags <<< "$(pkg-config --cflags --libs libpulse)"
    "${CC:-cc}" "${common[@]}" -Wno-deprecated-declarations \
        tests/test_pulse.c Output/pulse/audio.c "${libs[@]}" "${pulse_flags[@]}" \
        -o "$work/test_pulse"
    status=0
    ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
        XMMS_TEST_BUILDDIR="$work" bash tests/run-pulse-test.sh || status=$?
    if [[ $status != 0 && $status != 77 ]]; then
        exit "$status"
    fi
fi
