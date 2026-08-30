pkgname=wlcrosshair
pkgver=1.0.0
pkgrel=1
pkgdesc="Minimal layer-shell crosshair overlay for KDE Plasma Wayland"
arch=('x86_64')
url="local://wlcrosshair"
license=('MIT')
depends=('qt6-base' 'layer-shell-qt')
makedepends=('cmake')
source=()
sha256sums=()

build() {
  cmake -B "$srcdir/build" -S "$startdir" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr
  cmake --build "$srcdir/build"
}

package() {
  DESTDIR="$pkgdir" cmake --install "$srcdir/build"
  install -Dm644 "$startdir/config/config.ini.example" \
    "$pkgdir/usr/share/doc/wlcrosshair/config.ini.example"
}
