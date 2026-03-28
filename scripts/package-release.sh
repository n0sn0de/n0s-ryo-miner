#!/usr/bin/env bash
# package-release.sh — Build and package a local release binary
#
# Usage:
#   ./scripts/package-release.sh [--version VERSION] [--distro CODENAME]
#
# Builds an optimized, generic (portable) binary and packages it into
# a tarball with LICENSE, NOTICE, and a README.
#
# Options:
#   --version VERSION   Version string (default: reads from version.cpp)
#   --distro CODENAME   Ubuntu codename for filename (default: auto-detect)
#   --help              Show this help

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

VERSION=""
DISTRO=""

# ── Parse args ─────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        --version) VERSION="$2"; shift 2 ;;
        --distro)  DISTRO="$2"; shift 2 ;;
        --help|-h)
            head -14 "$0" | tail -12
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            exit 1
            ;;
    esac
done

# ── Auto-detect version from source ───────────
if [[ -z "$VERSION" ]]; then
    VERSION=$(grep -oP 'N0S_CNGPU_VERSION\s+"[^"]*"' "$PROJECT_ROOT/xmrstak/version.cpp" \
              | grep -oP '"[^"]*"' | tr -d '"' || echo "0.0.0")
    echo "Auto-detected version: $VERSION"
fi

# ── Auto-detect distro ────────────────────────
if [[ -z "$DISTRO" ]]; then
    if [[ -f /etc/os-release ]]; then
        DISTRO=$(. /etc/os-release && echo "${VERSION_CODENAME:-unknown}")
    else
        DISTRO="unknown"
    fi
    echo "Auto-detected distro: $DISTRO"
fi

ARCH=$(uname -m)
case "$ARCH" in
    x86_64)  ARCH="amd64" ;;
    aarch64) ARCH="arm64" ;;
esac

PKG_NAME="n0s-cngpu-${VERSION}-ubuntu-${DISTRO}-${ARCH}"
BUILD_DIR="$PROJECT_ROOT/build-release"
DIST_DIR="$PROJECT_ROOT/dist"

echo "═══════════════════════════════════════════"
echo "  n0s-cngpu release build"
echo "  Version: $VERSION"
echo "  Distro:  $DISTRO"
echo "  Arch:    $ARCH"
echo "  Output:  $DIST_DIR/$PKG_NAME.tar.gz"
echo "═══════════════════════════════════════════"

# ── Build ──────────────────────────────────────
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake "$PROJECT_ROOT" \
    -DCUDA_ENABLE=OFF \
    -DOpenCL_ENABLE=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    -DXMR-STAK_COMPILE=generic \
    -DMICROHTTPD_ENABLE=ON

make -j"$(nproc)"

# ── Verify ─────────────────────────────────────
BINARY="$BUILD_DIR/bin/n0s-cngpu"
if [[ ! -f "$BINARY" ]]; then
    echo "ERROR: Binary not found at $BINARY" >&2
    exit 1
fi

echo ""
echo "Binary info:"
file "$BINARY"
"$BINARY" --version || true
echo ""

# ── Package ────────────────────────────────────
rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR/$PKG_NAME"

cp "$BINARY" "$DIST_DIR/$PKG_NAME/"
cp "$PROJECT_ROOT/LICENSE" "$DIST_DIR/$PKG_NAME/"
cp "$PROJECT_ROOT/NOTICE" "$DIST_DIR/$PKG_NAME/" 2>/dev/null || true

cat > "$DIST_DIR/$PKG_NAME/README.txt" <<EOF
n0s-cngpu $VERSION — CryptoNight-GPU Miner
Built for Ubuntu $DISTRO ($ARCH)

Usage:
  ./n0s-cngpu --help
  ./n0s-cngpu -o pool.ryo-currency.com:3333 -u YOUR_WALLET -p x

Configuration:
  On first run, n0s-cngpu creates config.txt, pools.txt, and
  cpu.txt in the current directory. Edit pools.txt to set your
  pool and wallet address.

License: GPLv3 (see LICENSE)
Source:  https://github.com/n0sn0de/xmr-stak
EOF

cd "$DIST_DIR"
tar czf "${PKG_NAME}.tar.gz" "$PKG_NAME"
sha256sum "${PKG_NAME}.tar.gz" > "${PKG_NAME}.tar.gz.sha256"

echo ""
echo "═══════════════════════════════════════════"
echo "  ✅ Release package created"
echo "  $DIST_DIR/${PKG_NAME}.tar.gz"
echo "  $(cat "${PKG_NAME}.tar.gz.sha256")"
echo "═══════════════════════════════════════════"
