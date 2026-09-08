#!/usr/bin/env bash
set -euo pipefail
export LC_ALL=C

[[ $# == 1 ]] || { echo "Usage: $0 STAGING_DIRECTORY" >&2; exit 1; }
STAGE_DIR="$(cd "$1" && pwd)"
[[ -x $STAGE_DIR/usr/bin/xmms ]] || { echo 'Staged xmms executable is missing.' >&2; exit 1; }
WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/xmms-shlibdeps.XXXXXXXX")"
trap 'rm -rf -- "$WORK_DIR"' EXIT
mkdir "$WORK_DIR/debian"
cat > "$WORK_DIR/debian/control" <<'CONTROL'
Source: xmms-gtk3
Section: sound
Priority: optional
Maintainer: Olle Hallnas <olle@olleha.com>

Package: xmms-gtk3
Architecture: any
Description: XMMS GTK3 player
CONTROL
: > "$WORK_DIR/debian/shlibs.local"

elf_args=()
lib_args=()
while IFS= read -r -d '' binary; do
    if ! readelf --file-header "$binary" >/dev/null 2>&1; then
        continue
    fi
    # Include modules loaded by dlopen, not just the main executable's NEEDED list.
    elf_args+=("-e$binary")
    lib_args+=("-l$(dirname "$binary")")
    soname="$(readelf --dynamic "$binary" | sed -n 's/.*(SONAME).*\[\([^]]*\)\].*/\1/p')"
    if [[ $soname =~ ^libxmms\.so\.(.+)$ ]]; then
        # Resolve the bundled libxmms explicitly; -x removes only this self-dependency.
        printf 'libxmms %s xmms-gtk3\n' "${BASH_REMATCH[1]}" >> "$WORK_DIR/debian/shlibs.local"
    fi
done < <(find "$STAGE_DIR/usr" -type f -print0 | sort -z)
[[ -s $WORK_DIR/debian/shlibs.local ]] || { echo 'Bundled libxmms is missing.' >&2; exit 1; }

cd "$WORK_DIR"
dpkg-shlibdeps -O -S"$STAGE_DIR" -xxmms-gtk3 \
    -L"$WORK_DIR/debian/shlibs.local" "${lib_args[@]}" "${elf_args[@]}" \
    | sed -n 's/^shlibs:Depends=//p'
