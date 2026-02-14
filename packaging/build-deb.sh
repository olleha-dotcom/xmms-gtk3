#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

PKG_NAME="xmms-gtk3"
PKG_VERSION="1.5.1"
PKG_REVISION="1ubuntu24.04"
ARCH="$(dpkg --print-architecture)"
MAINTAINER="Olle Hallnas <olle@example.invalid>"
DESCRIPTION="XMMS (GTK3 port)"

BUILD_ROOT="${ROOT_DIR}/.pkgbuild"
STAGE_DIR="${BUILD_ROOT}/${PKG_NAME}_${PKG_VERSION}-${PKG_REVISION}_${ARCH}"
INSTALL_ROOT="${STAGE_DIR}"
DEBIAN_DIR="${INSTALL_ROOT}/DEBIAN"
OUT_DEB="${ROOT_DIR}/${PKG_NAME}_${PKG_VERSION}-${PKG_REVISION}_${ARCH}.deb"
OUT_DEB_TMP="/tmp/${PKG_NAME}_${PKG_VERSION}-${PKG_REVISION}_${ARCH}.deb"

rm -rf "$BUILD_ROOT"
mkdir -p "$DEBIAN_DIR"

# Install project into staging.
make install DESTDIR="$INSTALL_ROOT"

cat > "${DEBIAN_DIR}/control" <<CONTROL
Package: ${PKG_NAME}
Version: ${PKG_VERSION}-${PKG_REVISION}
Section: sound
Priority: optional
Architecture: ${ARCH}
Maintainer: ${MAINTAINER}
Depends: libc6, libglib2.0-0, libgtk-3-0, libgdk-pixbuf-2.0-0, libpango-1.0-0, libatk1.0-0, libx11-6
Description: ${DESCRIPTION}
 Classic XMMS media player with GTK3 compatibility updates.
CONTROL

cat > "${DEBIAN_DIR}/postinst" <<'POSTINST'
#!/bin/sh
set -e
if [ "$1" = "configure" ] || [ "$1" = "triggered" ]; then
	ldconfig
fi
exit 0
POSTINST

cat > "${DEBIAN_DIR}/postrm" <<'POSTRM'
#!/bin/sh
set -e
if [ "$1" = "remove" ] || [ "$1" = "purge" ]; then
	ldconfig
fi
exit 0
POSTRM

chmod 0755 "$DEBIAN_DIR"
chmod 0755 "${DEBIAN_DIR}/postinst" "${DEBIAN_DIR}/postrm"
find "$INSTALL_ROOT" -type d -print0 | xargs -0 chmod 0755

# Build .deb
rm -f "$OUT_DEB"
dpkg-deb --build "$INSTALL_ROOT" "$OUT_DEB"
cp -f "$OUT_DEB" "$OUT_DEB_TMP"

echo "Built: $OUT_DEB"
echo "Copy:  $OUT_DEB_TMP"
