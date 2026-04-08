#!/bin/bash
set -e

# ═══════════════════════════════════════════════════════════════════════════════
# Throne Linux Deployment Script — Portable & Atomic
# ═══════════════════════════════════════════════════════════════════════════════

# Clean up any leftover AppImages in root that might confuse linuxdeploy
rm -f linuxdeploy*.AppImage*

if [[ $(uname -m) == 'aarch64' || $(uname -m) == 'arm64' ]]; then
  ARCH="arm64"
  ARCH1="aarch64"
else
  ARCH="amd64"
  ARCH1="x86_64"
fi

# Load environment (BUILD, DEPLOYMENT, etc.)
if [ -f "script/env_deploy.sh" ]; then
    source script/env_deploy.sh
else
    # Fallback defaults if env_deploy.sh is missing
    BUILD="build"
    DEPLOYMENT="deployment"
    SRC_ROOT="."
fi

DEST=$DEPLOYMENT/linux-$ARCH
rm -rf "$DEST"
mkdir -p "$DEST"

echo "[1/4] Copying core artifacts..."
# Copy GUI binary
cp "$BUILD/Neko_Throne" "$DEST/"

# Copy Core backend (if exists)
if [ -f "$DEST/NekoCore" ]; then echo "Core already in place"; elif [ -f "deployment/linux-$ARCH/NekoCore" ]; then
    cp "deployment/linux-$ARCH/NekoCore" "$DEST/"
elif [ -f "$BUILD/NekoCore" ]; then
    cp "$BUILD/NekoCore" "$DEST/"
fi

# Copy translations
if [ -d "$BUILD/lang" ]; then
    mkdir -p "$DEST/lang"
    cp "$BUILD/lang"/*.qm "$DEST/lang/" 2>/dev/null || true
fi

# Copy resources
cp "$SRC_ROOT/res/public/Throne.png" "$DEST/"

echo "[2/4] Bundling libraries with linuxdeploy..."

# Ensure tools are available
mkdir -p tools-bin tools-storage
if [ ! -d "linuxdeploy-root" ]; then
    if [ ! -f "tools-storage/linuxdeploy-$ARCH1.AppImage" ]; then
        wget -q "https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20250213-2/linuxdeploy-$ARCH1.AppImage" -O "tools-storage/linuxdeploy-$ARCH1.AppImage"
        chmod +x "tools-storage/linuxdeploy-$ARCH1.AppImage"
    fi
    ./tools-storage/linuxdeploy-$ARCH1.AppImage --appimage-extract
    mv squashfs-root linuxdeploy-root
fi
if [ ! -d "linuxdeploy-plugin-qt-root" ]; then
    if [ ! -f "tools-storage/linuxdeploy-plugin-qt-$ARCH1.AppImage" ]; then
        wget -q "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/1-alpha-20250213-1/linuxdeploy-plugin-qt-$ARCH1.AppImage" -O "tools-storage/linuxdeploy-plugin-qt-$ARCH1.AppImage"
        chmod +x "tools-storage/linuxdeploy-plugin-qt-$ARCH1.AppImage"
    fi
    ./tools-storage/linuxdeploy-plugin-qt-$ARCH1.AppImage --appimage-extract
    mv squashfs-root linuxdeploy-plugin-qt-root
fi

# Make plugin and patchelf available in PATH
cat > tools-bin/linuxdeploy-plugin-qt <<EOF
#!/bin/bash
exec "$PWD/linuxdeploy-plugin-qt-root/AppRun" "\$@"
EOF
chmod +x tools-bin/linuxdeploy-plugin-qt
export PATH="$PWD/tools-bin:$PWD/linuxdeploy-root/usr/bin:$PATH"

# Run linuxdeploy to populate AppDir structure
export EXTRA_QT_PLUGINS="iconengines;wayland-shell-integration;wayland-decoration-client"
export EXTRA_PLATFORM_PLUGINS="libqwayland.so"
# Disable internal strip as it might be too old for modern Arch binaries (.relr.dyn)
export NO_STRIP=1

# We use the extracted linuxdeploy
./linuxdeploy-root/AppRun --appdir "$DEST" \
    --executable "$DEST/Neko_Throne" \
    --desktop-file "$SRC_ROOT/res/public/Throne.desktop" \
    --icon-file "$DEST/Throne.png" \
    --plugin qt

# Manual strip is skipped to avoid page-alignment issues (ELF load command address/offset not page-aligned)
echo "Skipping manual strip to ensure binary integrity..."

echo "[3/4] Enhancing portability (RPATH & missing libs)..."

# linuxdeploy puts libs in usr/lib. We want the binary to find them regardless of install path.
# Set RPATH to $ORIGIN/usr/lib (for binary in root)
patchelf --set-rpath '$ORIGIN/usr/lib' "$DEST/Neko_Throne"

# Also set RPATH for plugins to find the bundled Qt libs
find "$DEST/usr/plugins" -type f -name "*.so" -exec patchelf --set-rpath '$ORIGIN/../../lib' {} \; 2>/dev/null || true

# Copy additional required libs that linuxdeploy might miss or that are needed for Qt 6.11
if [ -n "$QT_ROOT" ]; then
    cp "$QT_ROOT/lib"/libQt6*.so.6 "$DEST/usr/lib/" 2>/dev/null || true
    cp "$QT_ROOT/lib"/libicu*.so.* "$DEST/usr/lib/" 2>/dev/null || true
fi

# Handle debug info
if [ -f "$DEST/Neko_Throne" ]; then
    objcopy --only-keep-debug "$DEST/Neko_Throne" "$DEST/Neko_Throne.debug"
    # strip --strip-debug --strip-unneeded "$DEST/Neko_Throne"
    objcopy --add-gnu-debuglink="$DEST/Neko_Throne.debug" "$DEST/Neko_Throne"
fi

echo "[4/4] Creating wrapper script..."
# Create a wrapper script for systems where RPATH might fail
cat > "$DEST/Throne.sh" <<'EOF'
#!/bin/bash
HERE="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="$HERE/usr/lib:$LD_LIBRARY_PATH"
export QT_PLUGIN_PATH="$HERE/usr/plugins"
export QML2_IMPORT_PATH="$HERE/usr/qml"

# Enable Core Dumps for debugging
ulimit -c unlimited
# Set core dump pattern to the current directory if possible
# (Requires /proc/sys/kernel/core_pattern to be just "core", but we try our best)

echo "--- Launching Throne (Segfault Debug Mode) ---"
exec "$HERE/Neko_Throne" "$@"
EOF
chmod +x "$DEST/Throne.sh"

echo "Deployment finished! Portable files are in $DEST"
