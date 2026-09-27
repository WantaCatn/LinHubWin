#!/bin/sh
set -e
cd "$(dirname "$0")/../.."
BUILD_DIR=${BUILD_DIR:-build-deb}
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build "$BUILD_DIR" -j"$(nproc)"
DESTDIR="${PWD}/packaging/debian/tmp"
rm -rf "$DESTDIR"
cmake --install "$BUILD_DIR" --prefix /usr --destdir "$DESTDIR"

VERSION=$(grep -oP 'project\(LinHub VERSION \K[0-9.]+' CMakeLists.txt)
ARCH=$(dpkg --print-architecture)
PKGDIR="packaging/debian/linhub_${VERSION}_${ARCH}"
rm -rf "$PKGDIR"
mkdir -p "$PKGDIR/DEBIAN"
cp -a "$DESTDIR"/* "$PKGDIR"/
sed "s/^Architecture:.*/Architecture: ${ARCH}/" packaging/debian/control | awk 'BEGIN{p=0} /^Package:/{p=1} p' > "$PKGDIR/DEBIAN/control"
# simpler control
cat > "$PKGDIR/DEBIAN/control" <<EOF
Package: linhub
Version: ${VERSION}
Section: misc
Priority: optional
Architecture: ${ARCH}
Depends: libc6, libqt5core5a | libqt6core6, libqt5widgets5 | libqt6widgets6, libqt5sql5-sqlite | libqt6sql6-sqlite, openssh-client
Maintainer: LinHub <linhub@localhost>
Description: Terminal session manager for Kylin Linux
 Combines MobaXterm-style terminals with Xshell-style session library.
EOF
dpkg-deb --build "$PKGDIR"
echo "built: ${PKGDIR}.deb"
