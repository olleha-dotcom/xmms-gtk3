#!/usr/bin/env bash
set -euo pipefail
export LC_ALL=C TZ=UTC
umask 022

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PKG_NAME=xmms-gtk3
PKG_VERSION="$(cat "$ROOT_DIR/VERSION")"
PKG_REVISION="${PKG_REVISION:-2ubuntu24.04}"
OUT_DIR="${OUT_DIR:-$ROOT_DIR}"
JOBS="${JOBS:-$(nproc)}"

die() { printf 'build-deb: %s\n' "$*" >&2; exit 1; }
[[ $# == 0 ]] || die 'Use PKG_REVISION, OUT_DIR, JOBS and SOURCE_DATE_EPOCH environment variables; no arguments are accepted.'
[[ $PKG_VERSION =~ ^[0-9]+(\.[0-9]+)*$ ]] || die 'Invalid upstream VERSION.'
[[ $PKG_REVISION =~ ^[A-Za-z0-9.+]+$ ]] || die 'Invalid Debian revision.'
[[ $JOBS =~ ^[1-9][0-9]*$ ]] || die 'JOBS must be a positive integer.'
for tool in autoreconf automake libtoolize pkg-config make cc dpkg dpkg-deb \
            dpkg-architecture dpkg-buildflags dpkg-shlibdeps readelf strip \
            tar desktop-file-validate; do
    command -v "$tool" >/dev/null || die "Missing build dependency: $tool (see README)."
done

ARCH="$(dpkg --print-architecture)"
MULTIARCH="$(dpkg-architecture -qDEB_HOST_MULTIARCH)"
[[ $(dpkg-architecture -qDEB_HOST_ARCH) == "$ARCH" ]] || die 'Cross-packaging is not supported.'
mkdir -p "$OUT_DIR"
OUT_DIR="$(cd "$OUT_DIR" && pwd)"
OUT_DEB="$OUT_DIR/${PKG_NAME}_${PKG_VERSION}-${PKG_REVISION}_${ARCH}.deb"
[[ ! -e $OUT_DEB && ! -L $OUT_DEB ]] || die "Release already exists: $OUT_DEB. Choose a different OUT_DIR or PKG_REVISION."

pkg-config --exists 'gtk+-3.0 >= 3.24' 'glib-2.0 >= 2.56' \
    'libpulse >= 12' alsa vorbisfile || die 'Install the Ubuntu build dependencies listed in README.'

if [[ -z ${SOURCE_DATE_EPOCH:-} ]]; then
    SOURCE_DATE_EPOCH="$(git -C "$ROOT_DIR" log -1 --format=%ct -- . 2>/dev/null || true)"
    SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-$(stat -c %Y "$ROOT_DIR/VERSION")}"
fi
[[ $SOURCE_DATE_EPOCH =~ ^[0-9]+$ ]] || die 'SOURCE_DATE_EPOCH must be an integer timestamp.'
export SOURCE_DATE_EPOCH

# Keep all generated state outside the user's tree, even for an in-tree build.
BUILD_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/xmms-deb.XXXXXXXX")"
trap 'rm -rf -- "$BUILD_ROOT"' EXIT
BUILD_ROOT="$(cd "$BUILD_ROOT" && pwd -P)"
case "$BUILD_ROOT/" in
    "$(cd "$ROOT_DIR" && pwd -P)/"*) die 'TMPDIR must be outside the source tree.' ;;
esac
SOURCE_DIR="$BUILD_ROOT/source"
STAGE_DIR="$BUILD_ROOT/stage"
mkdir -p "$SOURCE_DIR" "$STAGE_DIR/DEBIAN"
tar -C "$ROOT_DIR" --exclude-from="$ROOT_DIR/packaging/source-excludes" \
    -cf - . | tar -C "$SOURCE_DIR" -xf -

(
    cd "$SOURCE_DIR"
    # Do not inherit make overrides, configure site/cache settings or plugin lists.
    unset MAKEFLAGS MFLAGS MAKEOVERRIDES CONFIG_SHELL
    unset INPUT_PLUGINS OUTPUT_PLUGINS EFFECT_PLUGINS GENERAL_PLUGINS VISUALIZATION_PLUGINS
    export CONFIG_SITE=/dev/null
    CFLAGS="$(dpkg-buildflags --get CFLAGS) -ffile-prefix-map=$BUILD_ROOT=. -fdebug-prefix-map=$BUILD_ROOT=."
    CPPFLAGS="$(dpkg-buildflags --get CPPFLAGS)"
    LDFLAGS="$(dpkg-buildflags --get LDFLAGS)"
    export CFLAGS CPPFLAGS LDFLAGS
    autoreconf --force --install
    ./configure --prefix=/usr --libdir="/usr/lib/$MULTIARCH" \
        --disable-static --disable-esd --disable-oss --cache-file=/dev/null
    make -j"$JOBS"
    make install DESTDIR="$STAGE_DIR"
)

install -Dm0644 "$SOURCE_DIR/packaging/xmms.desktop" "$STAGE_DIR/usr/share/applications/xmms.desktop"
install -Dm0644 "$SOURCE_DIR/xmms/xmms.png" "$STAGE_DIR/usr/share/pixmaps/xmms.png"
for doc in README AUTHORS COPYING LICENSE; do
    install -Dm0644 "$SOURCE_DIR/$doc" "$STAGE_DIR/usr/share/doc/$PKG_NAME/$doc"
done

# Libtool archives contain build-machine paths and are not needed at runtime.
find "$STAGE_DIR/usr/lib" -type f \( -name '*.la' -o -name '*.a' \) -delete
while IFS= read -r -d '' binary; do
    if readelf --file-header "$binary" >/dev/null 2>&1; then
        strip --strip-unneeded "$binary"
    fi
done < <(find "$STAGE_DIR/usr" -type f -print0 | sort -z)

DEPENDS="$(bash "$SOURCE_DIR/packaging/runtime-deps.sh" "$STAGE_DIR")"
[[ -n $DEPENDS ]] || die 'dpkg-shlibdeps produced an empty Depends field.'
INSTALLED_SIZE="$(du -sk "$STAGE_DIR/usr" | cut -f1)"
cat > "$STAGE_DIR/DEBIAN/control" <<CONTROL
Package: $PKG_NAME
Version: $PKG_VERSION-$PKG_REVISION
Section: sound
Priority: optional
Architecture: $ARCH
Maintainer: Olle Hallnas <olle@olleha.com>
Installed-Size: $INSTALLED_SIZE
Depends: $DEPENDS
Recommends: unzip
Description: XMMS media player with GTK3 and PulseAudio support
 Classic skinnable XMMS player with GTK3 compatibility and native
 PulseAudio output, also usable with PipeWire's PulseAudio server.
CONTROL
install -m0755 "$SOURCE_DIR/packaging/postinst" "$STAGE_DIR/DEBIAN/postinst"
install -m0755 "$SOURCE_DIR/packaging/postrm" "$STAGE_DIR/DEBIAN/postrm"
(
    cd "$STAGE_DIR"
    find usr -type f -print0 | sort -z | xargs -0 md5sum > DEBIAN/md5sums
)
find "$STAGE_DIR" -type d -exec chmod 0755 {} +
find "$STAGE_DIR" -exec touch -h -d "@$SOURCE_DATE_EPOCH" {} +

# Verify before publishing the file, and never replace an existing release.
TEMP_DEB="$BUILD_ROOT/package.deb"
dpkg-deb --root-owner-group --threads-max=1 -Zxz --build "$STAGE_DIR" "$TEMP_DEB"
bash "$SOURCE_DIR/packaging/check-deb.sh" "$TEMP_DEB"
(set -o noclobber; cat "$TEMP_DEB" > "$OUT_DEB") || die "Could not create release: $OUT_DEB"
printf 'Built: %s\n' "$OUT_DEB"
