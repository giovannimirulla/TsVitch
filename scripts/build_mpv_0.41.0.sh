#!/bin/bash
# Build mpv 0.41.0 for Nintendo Switch with deko3d support via patch
# Based on switchfin's PKGBUILD approach: applies deko3d.patch then compiles

set -e

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MPVBUILD_DIR="${PROJECT_ROOT}/mpv-build"
DEVKIT_BASE="/opt/devkitpro"
RENDERER="${1:-deko3d}"  # Options: deko3d or opengl

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Building mpv 0.41.0 for Nintendo Switch [Renderer: $RENDERER]"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# Check DevKit installation
if [ ! -d "$DEVKIT_BASE" ]; then
    echo "❌ DevKit Pro not found at $DEVKIT_BASE"
    exit 1
fi

# Setup environment manually (switchvars.sh and devkita64.sh don't exist on all installations)
export DEVKITPRO="${DEVKIT_BASE}"
export DEVKITARM="${DEVKITPRO}/devkitARM"
export DEVKITA64="${DEVKITPRO}/devkitA64"
export PATH="${DEVKITA64}/bin:${PATH}"

# Create build directory
mkdir -p "$MPVBUILD_DIR"
cd "$MPVBUILD_DIR"

# Download mpv 0.41.0 source (if not already present)
if [ ! -d "mpv-0.41.0" ]; then
    echo "📥 Downloading mpv 0.41.0..."
    curl -L -o mpv-0.41.0.tar.gz "https://github.com/mpv-player/mpv/archive/refs/tags/v0.41.0.tar.gz" 2>&1 | tail -5
    tar xzf mpv-0.41.0.tar.gz
fi

cd mpv-0.41.0

# Prepare: Apply deko3d patch if available
echo ""
if [ "$RENDERER" == "deko3d" ]; then
    if [ -f "${PROJECT_ROOT}/scripts/switch/mpv/deko3d.patch" ]; then
        echo "📋 Applying deko3d.patch for mpv 0.41.0..."
        if patch -Np1 --dry-run -i "${PROJECT_ROOT}/scripts/switch/mpv/deko3d.patch" >/dev/null 2>&1; then
            patch -Np1 -i "${PROJECT_ROOT}/scripts/switch/mpv/deko3d.patch" >/dev/null 2>&1
            echo "✅ deko3d.patch applied successfully"
        else
            # If already applied, continue (check for files added by the patch)
            if [ -f "include/mpv/render_dk3d.h" ] || [ -f "video/out/deko3d/ra_dk.c" ]; then
                echo "ℹ️  deko3d.patch appears already applied, continuing"
            else
                echo "❌ Patch failed and not already applied"
                exit 1
            fi
        fi
    else
        echo "❌ deko3d.patch not found at ${PROJECT_ROOT}/scripts/switch/mpv/deko3d.patch"
        exit 1
    fi
fi

# Setup environment variables
export PKG_CONFIG_LIBDIR="${DEVKIT_BASE}/portlibs/switch/lib/pkgconfig:${PKG_CONFIG_LIBDIR}"

# Build configuration based on renderer
if [ "$RENDERER" == "deko3d" ]; then
    BUILD_DIR="build-deko3d"
    CROSSFILE="crossfile-deko3d.txt"
    
    echo "🔧 Configuring for deko3d renderer..."
    echo "   Building in: $BUILD_DIR"
    
    # Use meson-cross.sh like switchfin does
    /opt/devkitpro/meson-cross.sh switch "$CROSSFILE" "$BUILD_DIR" . \
        -Dlibmpv=true \
        -Dcplayer=false \
        -Dtests=false \
        -Dlua=disabled \
        -Ddeko3d=enabled \
        -Dgl=disabled \
        -Degl=disabled \
        -Dlibarchive=disabled
else
    # OpenGL fallback
    BUILD_DIR="build"
    CROSSFILE="crossfile.txt"
    
    echo "🔧 Configuring for OpenGL renderer..."
    echo "   Building in: $BUILD_DIR"
    
    # Use meson-cross.sh for OpenGL variant
    /opt/devkitpro/meson-cross.sh switch "$CROSSFILE" "$BUILD_DIR" . \
        -Dlibmpv=true \
        -Dcplayer=false \
        -Dtests=false \
        -Dlua=disabled \
        -Ddeko3d=disabled \
        -Dplain-gl=enabled \
        -Dlibarchive=disabled
fi

# Compile
echo ""
echo "🔨 Compiling mpv 0.41.0 [$RENDERER]..."
meson compile -C "$BUILD_DIR" 2>&1 | tail -20

# Success message
echo ""
echo "✅ Build complete! Output in: $BUILD_DIR"
echo "   Next step: Package generation in main build script"

# Install to system and staging dir, then create package archive for distribution
echo ""
echo "📦 Installing to system prefix..."
meson install -C "$BUILD_DIR" >/dev/null 2>&1 || true

# Ensure deko3d header is present in system prefix if using deko3d
if [ "$RENDERER" == "deko3d" ]; then
    SYS_HDR_DIR="${DEVKIT_BASE}/portlibs/switch/include/mpv"
    mkdir -p "$SYS_HDR_DIR"
    # Ensure deko3d-specific header is installed
    if [ -f "include/mpv/render_dk3d.h" ]; then
        cp -f "include/mpv/render_dk3d.h" "$SYS_HDR_DIR/"
    fi
    # Ensure patched render.h (with MPV_RENDER_PARAM_DEKO3D_FBO) is installed
    if [ -f "include/mpv/render.h" ]; then
        cp -f "include/mpv/render.h" "$SYS_HDR_DIR/"
    fi
    
    # Fix mpv.pc: Remove EGL/glapi from Libs.private for deko3d build
    # deko3d does NOT use EGL; removing it prevents linker errors from nouveau_* symbols
    SYS_PC_FILE="${DEVKIT_BASE}/portlibs/switch/lib/pkgconfig/mpv.pc"
    if [ -f "$SYS_PC_FILE" ]; then
        echo "🔧 Patching mpv.pc to exclude EGL/glapi (deko3d build)..."
        # Replace: Libs.private: -lEGL -lglapi -l... with Libs.private: -lavfilter -l...
        sed -i 's|^Libs\.private: -lEGL -lglapi -l|Libs.private: -l|' "$SYS_PC_FILE"
    fi
fi

echo "📦 Installing to staging directory..."
DESTDIR="${MPVBUILD_DIR}/pkgdir"
rm -rf "$DESTDIR"
meson install -C "$BUILD_DIR" --destdir "$DESTDIR" >/dev/null 2>&1 || true

# Ensure deko3d header is present in staging dir as well
if [ "$RENDERER" == "deko3d" ]; then
    STAGE_HDR_DIR="${DESTDIR}${DEVKIT_BASE}/portlibs/switch/include/mpv"
    mkdir -p "$STAGE_HDR_DIR"
    # Ensure deko3d-specific header is present in staging
    if [ -f "include/mpv/render_dk3d.h" ]; then
        cp -f "include/mpv/render_dk3d.h" "$STAGE_HDR_DIR/"
    fi
    # Ensure patched render.h is present in staging
    if [ -f "include/mpv/render.h" ]; then
        cp -f "include/mpv/render.h" "$STAGE_HDR_DIR/"
    fi
    
    # Fix mpv.pc in staging: Remove EGL/glapi from Libs.private for deko3d build
    STAGE_PC_FILE="${DESTDIR}${DEVKIT_BASE}/portlibs/switch/lib/pkgconfig/mpv.pc"
    if [ -f "$STAGE_PC_FILE" ]; then
        echo "🔧 Patching mpv.pc in staging to exclude EGL/glapi (deko3d build)..."
        sed -i 's|^Libs\.private: -lEGL -lglapi -l|Libs.private: -l|' "$STAGE_PC_FILE"
    fi
fi

echo "📦 Creating package archive..."
mkdir -p "${PROJECT_ROOT}/packages"

# Prefer .zst; fallback to .xz or .gz if compressors unavailable
PACKAGE_DIR="${PROJECT_ROOT}/packages"
PACKAGE_BASE="switch-libmpv-0.41.0-1-any.pkg.tar"
if command -v zstd >/dev/null 2>&1; then
    PACKAGE_NAME="${PACKAGE_BASE}.zst"
    COMPRESSOR="-I 'zstd -19 -T0'"
elif command -v xz >/dev/null 2>&1; then
    PACKAGE_NAME="${PACKAGE_BASE}.xz"
    COMPRESSOR="-I 'xz -9 -T0'"
else
    PACKAGE_NAME="${PACKAGE_BASE}.gz"
    COMPRESSOR="-z"
fi
PACKAGE_PATH="${PACKAGE_DIR}/${PACKAGE_NAME}"

set +e
if [ "$COMPRESSOR" = "-z" ]; then
    tar -C "$DESTDIR" -cf - \
        opt/devkitpro/portlibs/switch/include/mpv \
        opt/devkitpro/portlibs/switch/lib/libmpv.a \
        opt/devkitpro/portlibs/switch/lib/pkgconfig/mpv.pc | gzip -9 > "$PACKAGE_PATH"
else
    # shellcheck disable=SC2086
    tar -C "$DESTDIR" $COMPRESSOR -cf "$PACKAGE_PATH" \
        opt/devkitpro/portlibs/switch/include/mpv \
        opt/devkitpro/portlibs/switch/lib/libmpv.a \
        opt/devkitpro/portlibs/switch/lib/pkgconfig/mpv.pc
fi
TAR_STATUS=$?
set -e

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
if [ $TAR_STATUS -eq 0 ]; then
    echo "✅ Build completed successfully!"
    echo ""
    echo "📍 Location: $PACKAGE_PATH"
    echo "📦 Package:  $PACKAGE_NAME"
    echo ""
    echo "Next steps:"
    echo "  1. Install locally: dkp-pacman -U $PACKAGE_PATH"
    echo "  2. Re-run scripts/build_switch.sh to link against 0.41.0"
else
    echo "⚠️  Packaging failed (compressor missing), but mpv 0.41.0 is installed in /opt/devkitpro/portlibs/switch."
    echo "   You can proceed with the build; packaging is optional."
fi
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
