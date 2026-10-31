#!/bin/bash
#
# Builds Ogg.framework and Theora.framework for RSDKv5U.xcodeproj, using only
# the compiler that comes with Xcode (no CMake or Homebrew needed).
#
# Run it from anywhere; it works next to itself, in dependencies/mac:
#   libogg/     libogg's source (1.3.x)
#   libtheora/  libtheora's source (1.1.1 or 1.2.x)
# and writes the frameworks where the Xcode project looks for them:
#   libogg/macosx/Ogg.framework
#   libtheora/macosx/Theora.framework
#
# Both are universal (Apple Silicon and Intel), for macOS 12.0 or later (the
# engine's minimum), and load from the app's Frameworks folder (@rpath), which
# is where the Xcode project embeds them.

set -euo pipefail

readonly HERE="$(cd "$(dirname "$0")" && pwd)"
readonly OGG_SRC="$HERE/libogg"
readonly THEORA_SRC="$HERE/libtheora"
readonly WORK="$HERE/.frameworks-build"
readonly MIN_MACOS="12.0"
readonly ARCH_FLAGS=(-arch arm64 -arch x86_64)

# Fails early with a clear message when something is missing.
require() {
    if [[ ! -e "$1" ]]; then
        echo "error: $2 ($1 not found)" >&2
        exit 1
    fi
}

require "$OGG_SRC/src/framing.c" "put libogg's source in dependencies/mac/libogg"
require "$THEORA_SRC/lib/decode.c" "put libtheora's source in dependencies/mac/libtheora"
if ! xcrun --find clang > /dev/null 2>&1; then
    echo "error: Xcode's compiler was not found; install Xcode or run: xcode-select --install" >&2
    exit 1
fi
readonly CC="$(xcrun --find clang)"
# Called by its full path, clang does not know where the macOS SDK (the
# system headers and libraries) is, so it is passed explicitly.
readonly SDK="$(xcrun --sdk macosx --show-sdk-path)"

# Reads the "Version" a source tree declares in its configure.ac, for Info.plist.
source_version() {
    sed -n 's/^AC_INIT(\[\{0,1\}[A-Za-z]*\]\{0,1\}, *\[\([0-9.]*\)\].*/\1/p' "$1/configure.ac" | head -n 1
}

# Writes a framework's Info.plist.
#   $1 path, $2 bundle name, $3 identifier, $4 version
write_info_plist() {
    cat > "$1" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key><string>English</string>
    <key>CFBundleExecutable</key><string>$2</string>
    <key>CFBundleIdentifier</key><string>$3</string>
    <key>CFBundleInfoDictionaryVersion</key><string>6.0</string>
    <key>CFBundleName</key><string>$2</string>
    <key>CFBundlePackageType</key><string>FMWK</string>
    <key>CFBundleShortVersionString</key><string>$4</string>
    <key>CFBundleVersion</key><string>$4</string>
    <key>LSMinimumSystemVersion</key><string>$MIN_MACOS</string>
</dict>
</plist>
EOF
}

# Lays out an empty framework bundle (Versions/A plus the usual links) and
# prints the path of its Versions/A folder.
#   $1 the .framework path, $2 its name
make_bundle() {
    local framework="$1" name="$2"
    rm -rf "$framework"
    mkdir -p "$framework/Versions/A/Headers" "$framework/Versions/A/Resources"
    ln -s A "$framework/Versions/Current"
    ln -s "Versions/Current/$name" "$framework/$name"
    ln -s Versions/Current/Headers "$framework/Headers"
    ln -s Versions/Current/Resources "$framework/Resources"
    echo "$framework/Versions/A"
}

rm -rf "$WORK"
mkdir -p "$WORK/include/ogg"

# ---------------------
# Ogg.framework
# ---------------------
echo "Building Ogg.framework..."

# What libogg's configure would write: macOS has <stdint.h> and its types.
sed -e 's/@INCLUDE_INTTYPES_H@/1/' -e 's/@INCLUDE_STDINT_H@/1/' -e 's/@INCLUDE_SYS_TYPES_H@/1/' \
    -e 's/@SIZE16@/int16_t/' -e 's/@USIZE16@/uint16_t/' \
    -e 's/@SIZE32@/int32_t/' -e 's/@USIZE32@/uint32_t/' \
    -e 's/@SIZE64@/int64_t/' -e 's/@USIZE64@/uint64_t/' \
    "$OGG_SRC/include/ogg/config_types.h.in" > "$WORK/include/ogg/config_types.h"

readonly OGG_FRAMEWORK="$OGG_SRC/macosx/Ogg.framework"
mkdir -p "$OGG_SRC/macosx"
OGG_VERSIONED="$(make_bundle "$OGG_FRAMEWORK" Ogg)"
readonly OGG_VERSIONED

"$CC" -dynamiclib "${ARCH_FLAGS[@]}" -isysroot "$SDK" -mmacosx-version-min="$MIN_MACOS" -O2 \
    -I"$WORK/include" -I"$OGG_SRC/include" \
    "$OGG_SRC/src/bitwise.c" "$OGG_SRC/src/framing.c" \
    -install_name @rpath/Ogg.framework/Versions/A/Ogg \
    -o "$OGG_VERSIONED/Ogg"

cp "$OGG_SRC/include/ogg/ogg.h" "$OGG_SRC/include/ogg/os_types.h" "$WORK/include/ogg/config_types.h" "$OGG_VERSIONED/Headers/"
write_info_plist "$OGG_VERSIONED/Resources/Info.plist" Ogg org.xiph.ogg "$(source_version "$OGG_SRC")"

# ---------------------
# Theora.framework
# ---------------------
echo "Building Theora.framework..."

# Every C file directly in lib/ (plain C; the assembly lives in lib/x86 and
# lib/arm, and stays off), except lib/encoder_disabled.c: its stubs repeat
# the encoder's functions, which a static library tolerates but a framework
# does not. Taking the folder as it is covers both 1.1.1 and 1.2.x, whose
# file lists differ (1.2 moved cpu.c into lib/x86).
theora_sources=()
for source in "$THEORA_SRC"/lib/*.c; do
    [[ "$(basename "$source")" == encoder_disabled.c ]] && continue
    theora_sources+=("$source")
done

readonly THEORA_FRAMEWORK="$THEORA_SRC/macosx/Theora.framework"
THEORA_VERSIONED="$(make_bundle "$THEORA_FRAMEWORK" Theora)"
readonly THEORA_VERSIONED

# -w: libtheora's older C would otherwise flood the output with warnings.
"$CC" -dynamiclib "${ARCH_FLAGS[@]}" -isysroot "$SDK" -mmacosx-version-min="$MIN_MACOS" -O2 -fsigned-char -w \
    -I"$THEORA_SRC/include" -I"$WORK/include" -I"$OGG_SRC/include" \
    "${theora_sources[@]}" \
    -F"$OGG_SRC/macosx" -framework Ogg \
    -install_name @rpath/Theora.framework/Versions/A/Theora \
    -o "$THEORA_VERSIONED/Theora"

cp "$THEORA_SRC"/include/theora/{codec,theora,theoradec,theoraenc}.h "$THEORA_VERSIONED/Headers/"
write_info_plist "$THEORA_VERSIONED/Resources/Info.plist" Theora org.xiph.theora "$(source_version "$THEORA_SRC")"

# ---------------------
# Sign and check
# ---------------------
# Ad-hoc signatures, so they load on Apple Silicon (Xcode re-signs them when
# it embeds them in the app).
codesign --force --sign - "$OGG_FRAMEWORK" "$THEORA_FRAMEWORK" > /dev/null

rm -rf "$WORK"

echo
echo "Done:"
for binary in "$OGG_FRAMEWORK/Ogg" "$THEORA_FRAMEWORK/Theora"; do
    echo "  $binary"
    echo "    architectures: $(lipo -archs "$binary")"
    echo "    loads as:      $(otool -D "$binary" | tail -n 1)"
done
echo "  Theora links to: $(otool -L "$THEORA_FRAMEWORK/Theora" | grep Ogg.framework | awk '{print $1}')"
