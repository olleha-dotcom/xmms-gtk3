#!/usr/bin/env bash
set -euo pipefail
export LC_ALL=C

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
die() { printf 'check-deb: %s\n' "$*" >&2; exit 1; }
[[ $# == 1 ]] || die "Usage: $0 PACKAGE.deb"
DEB="$(realpath "$1")"
WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/xmms-check-deb.XXXXXXXX")"
trap 'rm -rf -- "$WORK_DIR"' EXIT

for archive in fsys ctrl; do
    dpkg-deb "--${archive}-tarfile" "$DEB" | tar --numeric-owner -tvf - \
        > "$WORK_DIR/$archive.list"
    awk '$2 != "0/0" { bad = 1; print > "/dev/stderr" } END { exit (bad || NR == 0) }' \
        "$WORK_DIR/$archive.list" || die 'Archive entries must all be owned by root:root (0:0).'
done
[[ $(dpkg-deb -f "$DEB" Package) == xmms-gtk3 ]] || die 'Unexpected package name.'
[[ $(dpkg-deb -f "$DEB" Version) == "$(cat "$ROOT_DIR/VERSION")-"* ]] || die 'Package version does not match VERSION.'
[[ $(dpkg-deb -f "$DEB" Architecture) == "$(dpkg --print-architecture)" ]] || die 'Verify on the build architecture.'
MULTIARCH="$(dpkg-architecture -qDEB_HOST_MULTIARCH)"
STAGE_DIR="$WORK_DIR/stage"
dpkg-deb --extract "$DEB" "$STAGE_DIR"
[[ ! -e $STAGE_DIR/usr/local && ! -L $STAGE_DIR/usr/local ]] || die '/usr/local must not be packaged.'
[[ -z $(find "$STAGE_DIR" -mindepth 1 -maxdepth 1 ! -name usr -print) ]] || die 'All package data must be under /usr.'
[[ -x $STAGE_DIR/usr/bin/xmms ]] || die 'Missing xmms executable.'
[[ -s $STAGE_DIR/usr/lib/$MULTIARCH/xmms/Output/libpulseout.so ]] || die 'Missing multiarch PulseAudio plugin.'
[[ -s $STAGE_DIR/usr/share/pixmaps/xmms.png ]] || die 'Missing desktop icon.'
DESKTOP="$STAGE_DIR/usr/share/applications/xmms.desktop"
[[ -s $DESKTOP ]] || die 'Missing desktop entry.'
desktop-file-validate "$DESKTOP"
grep -Fx 'Exec=xmms %U' "$DESKTOP" >/dev/null || die 'Unexpected desktop command.'
grep -Fx 'Icon=xmms' "$DESKTOP" >/dev/null || die 'Unexpected desktop icon name.'

while IFS= read -r -d '' binary; do
    if ! readelf --file-header "$binary" >/dev/null 2>&1; then
        continue
    fi
    case "$binary" in
        "$STAGE_DIR/usr/bin/"* | "$STAGE_DIR/usr/lib/$MULTIARCH/"*) ;;
        *) die "ELF file outside the multiarch install layout: $binary" ;;
    esac
    rpath="$(readelf --dynamic "$binary" | sed -n '/(RPATH)\|(RUNPATH)/p')"
    [[ -z $rpath ]] || die "Unexpected runtime search path in $binary: $rpath"
done < <(find "$STAGE_DIR/usr" -type f -print0 | sort -z)

ACTUAL="$(dpkg-deb -f "$DEB" Depends)"
EXPECTED="$(bash "$ROOT_DIR/packaging/runtime-deps.sh" "$STAGE_DIR")"
[[ -n $EXPECTED && $ACTUAL == "$EXPECTED" ]] || die "Depends mismatch. Expected: $EXPECTED; found: $ACTUAL"
pulse_dependency='(^|, )libpulse0([ ,(]|$)'
bundled_dependency='(^|, )(libxmms[^ ,]*|xmms-gtk3)([ ,(]|$)'
[[ $ACTUAL =~ $pulse_dependency ]] || die 'Missing plugin dependency libpulse0.'
[[ ! $ACTUAL =~ $bundled_dependency ]] || die 'Bundled libxmms must not create an external or self-dependency.'
printf 'Verified root:root ownership, multiarch layout, desktop/icon and all ELF dependencies.\nDepends: %s\n' "$ACTUAL"
