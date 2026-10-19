#!/usr/bin/env bash
# Build the static, universal Slint that `./build.sh static` links on a Mac, so
# "OpenSWOS Setup.app" runs on Apple silicon and Intel Macs alike (the published
# Slint SDK is shared and arm64 only). Once per Mac; the result lives OUTSIDE
# this repo, beside it, where build.sh looks:
#
#   ../../../slint/install/macos-universal-static   (with OpenSWOS checked out
#                                                    at ~/Programming/OpenSWOS:
#                                                    ~/Programming/slint/...)
#
# Needs Rust (rustup) and CMake. Slint 1.17.1 is built for arm64 and x86_64
# from its release tag, static, without the interpreter, testing or Skia
# (StorageAgent's docs/SLINT_STATIC.md), and the two libslint_cpp.a are joined
# with lipo; the headers and the slint-compiler are the arm64 build's. A cold
# build is a few minutes per architecture.
#
# The system frameworks the static library needs are named in CMakeLists.txt
# (rustc --print native-static-libs, printed at the end here): rerun this and
# compare when Slint is updated.
set -euo pipefail
cd "$(dirname "$0")"

VERSION=1.17.1
BASE="$(cd ../../.. && pwd)"
SRC="$BASE/slint-v$VERSION"
INSTALL="$BASE/slint/install"
OPTS=(-DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF
      -DSLINT_FEATURE_INTERPRETER=OFF -DSLINT_FEATURE_TESTING=OFF
      -DSLINT_FEATURE_RENDERER_SKIA=OFF -DSLINT_BUILD_EXAMPLES=OFF
      -DSLINT_BUILD_TESTING=OFF -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0)

rustup target add aarch64-apple-darwin x86_64-apple-darwin
[ -d "$SRC" ] || git clone -q --depth 1 --branch "v$VERSION" https://github.com/slint-ui/slint.git "$SRC"

build() {   # arch rust-target
  echo "== Slint $VERSION, $1"
  cmake -S "$SRC" -B "$SRC/build-static-$1" "${OPTS[@]}" -DCMAKE_OSX_ARCHITECTURES="$1" \
        -DRust_CARGO_TARGET="$2" -DCMAKE_INSTALL_PREFIX="$INSTALL/macos-$1-static" >/dev/null
  cmake --build "$SRC/build-static-$1" --parallel
  cmake --install "$SRC/build-static-$1" >/dev/null
}
build arm64 aarch64-apple-darwin
build x86_64 x86_64-apple-darwin

echo "== universal"
U="$INSTALL/macos-universal-static"
rm -rf "$U"
cp -R "$INSTALL/macos-arm64-static" "$U"
lipo -create -output "$U/lib/libslint_cpp.a" \
     "$INSTALL/macos-arm64-static/lib/libslint_cpp.a" "$INSTALL/macos-x86_64-static/lib/libslint_cpp.a"
lipo -info "$U/lib/libslint_cpp.a"

echo "== the system libraries it needs (compare with CMakeLists.txt)"
( cd "$SRC" && cargo rustc --release -p slint-cpp --crate-type staticlib --no-default-features \
      --features backend-winit,renderer-femtovg,accessibility -- --print native-static-libs 2>&1 |
  grep native-static-libs | tail -1 )
echo "Done: $U"
