#!/bin/sh
set -eu
cd "$(dirname "$0")"
make check
version=1.0.0
mkdir -p dist
out="$(pwd)/dist"
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
install -Dm755 orbit-timer "$stage/usr/bin/orbit-timer"
install -Dm644 orbit-timer.desktop "$stage/usr/share/applications/orbit-timer.desktop"
cat > "$stage/.PKGINFO" <<META
pkgname = orbit-timer
pkgbase = orbit-timer
pkgver = $version-1
pkgdesc = Offline circular countdown timer
url = https://github.com/fluffydisk/orbit-timer
builddate = $(date +%s)
packager = Orbit Timer
size = $(wc -c < orbit-timer)
arch = x86_64
depend = glibc
depend = libx11
depend = cairo
depend = xorg-xwayland
META
tar --owner=0 --group=0 -C "$stage" -cf - .PKGINFO usr | zstd -q -f -o "$out/orbit-timer-$version-1-x86_64.pkg.tar.zst"
rm "$stage/.PKGINFO"
mkdir "$stage/DEBIAN"
cat > "$stage/DEBIAN/control" <<META
Package: orbit-timer
Version: $version
Architecture: amd64
Maintainer: Orbit Timer <fluffydisk@users.noreply.github.com>
Section: utils
Priority: optional
Depends: libc6 (>= 2.34), libx11-6, libcairo2
Recommends: xwayland
Description: Offline circular countdown timer
 Start, pause, resume and reset a countdown in a desktop window.
META
if command -v dpkg-deb >/dev/null; then
    dpkg-deb --root-owner-group --build "$stage" "$out/orbit-timer_${version}_amd64.deb"
else
    printf '2.0\n' > "$stage/debian-binary"
    tar --owner=0 --group=0 -C "$stage/DEBIAN" -czf "$stage/control.tar.gz" ./control
    tar --owner=0 --group=0 -C "$stage" -czf "$stage/data.tar.gz" usr
    (cd "$stage" && ar cr "$out/orbit-timer_${version}_amd64.deb" debian-binary control.tar.gz data.tar.gz)
fi
install -m755 orbit-timer "$out/orbit-timer-x86_64"
(cd "$out" && sha256sum orbit-timer* > SHA256SUMS.txt)
