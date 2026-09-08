#!/bin/sh
# Run only after the parent build finishes, e.g. from the build/tests directory:
# XMMS_GUI_BUILD_READY=1 sh /source/tests/run-gui-test.sh \
#   ../xmms/xmms ./test_gui ./test_home.so
# Driver/preload compilation commands are in test_gui.c and test_home.c.
# Missing tools/build opt-in: Automake skip (77). Assertions/startup failures: 1.
set -eu

skip() { echo "SKIP: $*" >&2; exit 77; }
[ "${XMMS_GUI_BUILD_READY:-0}" = 1 ] || skip "parent build not confirmed; set XMMS_GUI_BUILD_READY=1 after building"
[ "$#" -le 3 ] || skip "usage: run-gui-test.sh [xmms-binary test_gui test_home.so]"
absolute() { case $1 in /*) printf '%s\n' "$1";; *) printf '%s/%s\n' "$PWD" "$1";; esac; }
app=$(absolute "${1:-../xmms/xmms}")
driver=$(absolute "${2:-./test_gui}")
preload=$(absolute "${3:-./test_home.so}")
if [ ! -x "$app" ] || [ ! -x "$driver" ] || [ ! -r "$preload" ]; then
	skip "build XMMS, test_gui and test_home.so first"
fi
if [ -u "$app" ] || [ -g "$app" ]; then
	skip "set-id application cannot safely use the test preload"
fi
xvfb=${XVFB:-}
if [ -z "$xvfb" ]; then
	xvfb=$(command -v Xvfb || true)
	[ -n "$xvfb" ] || xvfb=/tmp/xmms-test-tools/root/usr/bin/Xvfb
fi
[ -x "$xvfb" ] || skip "Xvfb unavailable (set XVFB to its binary)"

umask 077
scratch=$(mktemp -d /tmp/xmms-gui.XXXXXX) || skip "cannot create private test directory"
app_pid=
xvfb_pid=
cleanup()
{
	status=$?
	trap - EXIT HUP INT TERM
	if [ -n "$app_pid" ]; then kill -KILL "$app_pid" 2>/dev/null || true; wait "$app_pid" 2>/dev/null || true; fi
	if [ -n "$xvfb_pid" ]; then kill "$xvfb_pid" 2>/dev/null || true; wait "$xvfb_pid" 2>/dev/null || true; fi
	if [ "$status" -ne 0 ]; then
		for log in "$scratch"/*.log; do [ ! -f "$log" ] || { echo "=== $log ===" >&2; tail -60 "$log" >&2; }; done
	fi
	if [ "${XMMS_GUI_KEEP_TMP:-0}" = 1 ]; then echo "GUI artifacts: $scratch";
	else rm -rf -- "$scratch"; fi
	exit "$status"
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM

# Never connect to the caller's display, session bus, audio server or XDG home.
# -displayfd chooses an unused server number atomically; no existing X server
# is reused. No WM is needed: geometry assertions are on X window device pixels.
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY SESSION_MANAGER DBUS_SESSION_BUS_ADDRESS
unset GTK_MODULES GTK_PATH GTK_IM_MODULE LD_PRELOAD GDK_DPI_SCALE
"$xvfb" -displayfd 3 -screen 0 4096x2400x24 -nolisten tcp -noreset -ac \
	+extension XTEST 3>"$scratch/display" >"$scratch/xvfb.log" 2>&1 &
xvfb_pid=$!
n=0
while [ ! -s "$scratch/display" ]; do
	kill -0 "$xvfb_pid" 2>/dev/null || skip "private Xvfb failed to start"
	n=$((n + 1)); [ "$n" -lt 100 ] || skip "private Xvfb startup timed out"
	sleep 0.05
done
number=$(cat "$scratch/display")
case $number in ''|*[!0-9]*) skip "invalid private Xvfb display number";; esac
DISPLAY=:$number
XMMS_TEST_DISPLAY=$DISPLAY
XMMS_GUI_TEST_ACTIVE=1
GDK_BACKEND=x11
GSETTINGS_BACKEND=memory
NO_AT_BRIDGE=1
GTK_USE_PORTAL=0
LC_ALL=C
export DISPLAY XMMS_TEST_DISPLAY XMMS_GUI_TEST_ACTIVE GDK_BACKEND
export GSETTINGS_BACKEND NO_AT_BRIDGE GTK_USE_PORTAL LC_ALL

for scale in 1 2; do
	for linked in 0 1; do
		case_dir=$scratch/s$scale-l$linked
		mkdir -p "$case_dir/home/.xmms/Plugins" "$case_dir/tmp" "$case_dir/config" \
			"$case_dir/cache" "$case_dir/data" "$case_dir/runtime"
		XMMS_TEST_HOME=$case_dir/home
		TMPDIR=$case_dir/tmp
		XDG_CONFIG_HOME=$case_dir/config
		XDG_CACHE_HOME=$case_dir/cache
		XDG_DATA_HOME=$case_dir/data
		XDG_RUNTIME_DIR=$case_dir/runtime
		PULSE_SERVER=unix:$case_dir/no-pulse
		DBUS_SESSION_BUS_ADDRESS=unix:path=$case_dir/no-bus
		GDK_SCALE=$scale
		export XMMS_TEST_HOME TMPDIR XDG_CONFIG_HOME XDG_CACHE_HOME XDG_DATA_HOME
		export XDG_RUNTIME_DIR PULSE_SERVER DBUS_SESSION_BUS_ADDRESS GDK_SCALE
		cat >"$XMMS_TEST_HOME/.xmms/config" <<EOF
[xmms]
player_visible=TRUE
equalizer_visible=TRUE
playlist_visible=TRUE
player_shaded=FALSE
equalizer_shaded=FALSE
playlist_shaded=FALSE
player_x=20
player_y=20
equalizer_x=20
equalizer_y=300
playlist_x=700
playlist_y=20
playlist_width=300
playlist_height=232
doublesize=FALSE
eq_doublesize_linked=$([ "$linked" = 1 ] && echo TRUE || echo FALSE)
equalizer_active=FALSE
equalizer_autoload=FALSE
save_window_positions=TRUE
show_wm_decorations=FALSE
snap_windows=FALSE
easy_move=FALSE
resume_playback_on_startup=FALSE
resume_playback_was_playing=FALSE
get_info_on_load=FALSE
use_eplugins=FALSE
enabled_gplugins=
enabled_eplugins=
enabled_vplugins=
disabled_iplugins=libwav.so,libmpg123.so,libmikmod.so,libcdaudio.so,libtonegen.so,libvorbis.so
output_plugin=__xmms_gui_test_no_audio__.so

[PulseAudio]
server=unix:$case_dir/no-pulse
EOF
		# The preload also blocks plugin enumeration: disabled input plugins still
		# get init() calls, and output plugins have no generic disableplugin key.
		if env LD_PRELOAD="$preload" "$driver" --check-home; then :;
		else
			status=$?
			[ "$status" -ne 127 ] || skip "GUI driver shared-library dependencies unavailable"
			exit "$status"
		fi
		env LD_PRELOAD="$preload" "$driver" --probe
		(cd "$case_dir"; exec env LD_PRELOAD="$preload" "$app") >"$scratch/app-s$scale-l$linked.log" 2>&1 &
		app_pid=$!
		env LD_PRELOAD="$preload" "$driver" "$scale" "$linked"
		n=0
		while kill -0 "$app_pid" 2>/dev/null; do
			n=$((n + 1)); [ "$n" -lt 100 ] || { echo "GUI FAIL: XMMS did not quit" >&2; exit 1; }
			sleep 0.05
		done
		if wait "$app_pid"; then app_pid=;
		else app_pid=; echo "GUI FAIL: XMMS exited unsuccessfully" >&2; exit 1; fi
		env LD_PRELOAD="$preload" "$driver" --verify-config
	done
done
echo "GUI smoke tests passed (GDK_SCALE=1/2, EQ linked/unlinked)."
