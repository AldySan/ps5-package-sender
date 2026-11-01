#!/usr/bin/env bash
# Bundles the Linux build into a portable directory:
#   dist/ps5-package-sender-linux-x64/
#     run.sh, bin/, lib/, plugins/, qml/
# Usage: scripts/deploy-linux.sh [build-dir] [dist-dir]
set -euo pipefail

BUILD_DIR="${1:-build}"
DIST_ROOT="${2:-dist}"
APP=ps5-package-sender
OUT="$DIST_ROOT/$APP-linux-x64"

if [ ! -x "$BUILD_DIR/bin/$APP" ]; then
    echo "binary $BUILD_DIR/bin/$APP not found" >&2
    exit 1
fi

rm -rf "$OUT"
mkdir -p "$OUT/bin" "$OUT/lib" "$OUT/plugins" "$OUT/qml"

cp -L "$BUILD_DIR/bin/$APP" "$OUT/bin/$APP"

# System libraries we never bundle (glibc territory).
SKIP_RE='libc\.so|libm\.so|ld-linux|libpthread|libdl|librt|libresolv|libnsl'

copy_deps() {
    local target="$1"
    ldd "$target" 2>/dev/null | awk '/=> \//{print $3}' | sort -u | while read -r lib; do
        local base
        base="$(basename "$lib")"
        if echo "$base" | grep -Eq "$SKIP_RE"; then
            continue
        fi
        if [ ! -e "$OUT/lib/$base" ]; then
            cp -L "$lib" "$OUT/lib/$base"
        fi
    done
}

copy_deps "$OUT/bin/$APP"
# Second pass catches dependencies of the Qt libraries themselves,
# third pass is a safety net for deeper chains.
for _ in 1 2; do
    for lib in "$OUT"/lib/*.so*; do
        [ -e "$lib" ] && copy_deps "$lib"
    done
done

QT_LIBDIR="$(dirname "$(ldd "$OUT/bin/$APP" | awk '/libQt6Core\.so/{print $3; exit}')")"
QT_PREFIX="$(dirname "$QT_LIBDIR")"

for group in platforms imageformats iconengines sqldrivers networkinformation xcbglintegrations; do
    if [ -d "$QT_PREFIX/plugins/$group" ]; then
        mkdir -p "$OUT/plugins/$group"
        cp -L "$QT_PREFIX/plugins/$group/"*.so* "$OUT/plugins/$group/" 2>/dev/null || true
    fi
done


for module in Qt QtCore QtQml QtQuick QtNetwork; do
    if [ -d "$QT_PREFIX/qml/$module" ]; then
        cp -aL "$QT_PREFIX/qml/$module" "$OUT/qml/"
    fi
done
if [ -d "$QT_PREFIX/qml/QML" ]; then
    cp -aL "$QT_PREFIX/qml/QML" "$OUT/qml/"
fi


# Plugins and QML modules pull in extra system libraries (libxcb-cursor
# and friends), so walk their dependencies too.
find "$OUT/plugins" "$OUT/qml" -name '*.so' -o -name '*.so.*' 2>/dev/null | while read -r so; do
    [ -e "$so" ] && copy_deps "$so"
done

cat > "$OUT/bin/qt.conf" << 'EOF'
[Paths]
Prefix=..
Plugins=plugins
Qml2Imports=qml
EOF

cat > "$OUT/run.sh" << 'EOF'
#!/usr/bin/env bash
DIR="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
export LD_LIBRARY_PATH="$DIR/lib:$LD_LIBRARY_PATH"
export QT_PLUGIN_PATH="$DIR/plugins"
export QML2_IMPORT_PATH="$DIR/qml"
export QT_QUICK_CONTROLS_STYLE=Basic
exec "$DIR/bin/ps5-package-sender" "$@"
EOF
chmod +x "$OUT/run.sh"

mkdir -p "$OUT/data-placeholder"
rmdir "$OUT/data-placeholder" 2>/dev/null || true

echo "portable tree ready: $OUT"
du -sh "$OUT"
