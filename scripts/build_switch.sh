#!/bin/bash
# TsVitch Nintendo Switch Build Script
# 
# Supports multiple rendering backends and mpv versions:
#   - deko3d (default): Primary GPU-accelerated rendering with OpenGL fallback
#   - opengl: OpenGL-only rendering (legacy, for compatibility)
#
# Usage:
#   ./scripts/build_switch.sh [OPTIONS]
#
# Options:
#   --renderer deko3d|opengl    Renderer backend (default: deko3d)
#   --mpv-version 0.36.0|0.41.0 MPV version to use (default: 0.41.0)
#
# Examples:
#   ./scripts/build_switch.sh                                  # deko3d + mpv 0.41.0
#   ./scripts/build_switch.sh --renderer opengl                # OpenGL + mpv 0.41.0
#   ./scripts/build_switch.sh --mpv-version 0.36.0             # deko3d + mpv 0.36.0
#   ./scripts/build_switch.sh --renderer opengl --mpv-version 0.36.0
#

set -e

# Default values
RENDERER="deko3d"
MPV_VERSION="0.41.0"
BUILD_DIR="cmake-build-switch"
USE_DEKO3D="ON"

# Add Homebrew paths to PATH for meson/ninja (macOS only)
if [ -d "/opt/homebrew/bin" ]; then
    export PATH="/opt/homebrew/bin:$PATH"
elif [ -d "/usr/local/bin" ]; then
    export PATH="/usr/local/bin:$PATH"
fi

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --renderer)
            RENDERER="$2"
            shift 2
            case "$RENDERER" in
                deko3d|opengl)
                    ;;
                *)
                    echo "❌ Invalid renderer: $RENDERER"
                    echo "Valid options: deko3d, opengl"
                    exit 1
                    ;;
            esac
            ;;
        --mpv-version)
            MPV_VERSION="$2"
            shift 2
            case "$MPV_VERSION" in
                0.36.0|0.41.0)
                    ;;
                *)
                    echo "❌ Invalid mpv version: $MPV_VERSION"
                    echo "Valid options: 0.36.0, 0.41.0"
                    exit 1
                    ;;
            esac
            ;;
        *)
            echo "❌ Unknown option: $1"
            echo "Usage: $0 [--renderer deko3d|opengl] [--mpv-version 0.36.0|0.41.0]"
            exit 1
            ;;
    esac
done

# Configure based on renderer choice
if [ "$RENDERER" = "opengl" ]; then
    BUILD_DIR="cmake-build-switch-opengl"
    USE_DEKO3D="OFF"
fi

# cd to TsVitch
cd "$(dirname $0)/.."
git config --global --add safe.directory `pwd`

# Ensure pkg-config can see Switch portlibs early (for libmpv detection)
export PKG_CONFIG_PATH="/opt/devkitpro/portlibs/switch/lib/pkgconfig:${PKG_CONFIG_PATH}"

# Ensure build prerequisites are installed
ensure_prerequisites() {
    # Install pip3 if missing (needed for Meson)
    if ! command -v pip3 &> /dev/null; then
        if command -v pacman &> /dev/null; then
            pacman -Sy --noconfirm python-pip 2>/dev/null || true
        elif command -v apt-get &> /dev/null; then
            apt-get update && apt-get install -y python3-pip 2>/dev/null || true
        fi
    fi
    
    # Auto-install Meson in Docker/CI environments
    if ! command -v meson &> /dev/null; then
        if command -v pip3 &> /dev/null; then
            echo "📦 Installing Meson via pip3..."
            pip3 install --break-system-packages meson 2>/dev/null || pip3 install meson 2>/dev/null || true
            # Add user bin to PATH for pip installs
            export PATH="$HOME/.local/bin:$PATH"
        elif command -v pacman &> /dev/null; then
            echo "📦 Installing Meson via pacman..."
            pacman -Sy --noconfirm meson 2>/dev/null || true
        fi
    fi
    
    # Auto-install Ninja in Docker/CI environments
    if ! command -v ninja &> /dev/null; then
        if command -v pacman &> /dev/null; then
            echo "📦 Installing Ninja via pacman..."
            pacman -Sy --noconfirm ninja 2>/dev/null || true
        elif command -v apt-get &> /dev/null; then
            apt-get update && apt-get install -y ninja-build 2>/dev/null || true
        fi
    fi
    
    # Upgrade Meson if version too old (mpv 0.41.0 requires >= 1.3.0)
    if command -v meson &> /dev/null; then
        local meson_version=$(meson --version 2>/dev/null | head -1)
        local major=$(echo "$meson_version" | cut -d. -f1)
        local minor=$(echo "$meson_version" | cut -d. -f2)
        
        if [ -z "$major" ] || [ "$major" -lt 1 ] || ([ "$major" -eq 1 ] && [ "$minor" -lt 3 ]); then
            if command -v pip3 &> /dev/null; then
                echo "📦 Upgrading Meson to >= 1.3.0..."
                pip3 install --break-system-packages --upgrade meson 2>/dev/null || pip3 install --upgrade meson 2>/dev/null || true
            fi
        fi
    fi
}

# Check if prerequisites for mpv 0.41.0 build are available
has_build_prerequisites() {
    local missing=""
    
    command -v meson &>/dev/null || missing="${missing}Meson "
    command -v ninja &>/dev/null || missing="${missing}Ninja "
    [ -d "/opt/devkitpro" ] || missing="${missing}DevKit-Pro "
    
    if [ -n "$missing" ]; then
        return 1
    fi
    return 0
}


# Check if libmpv is installed and version >= the specified version
mpv_installed_ge() {
    local want_version="$1"
    # Prefer target pkg-config for Switch if available
    local PC="aarch64-none-elf-pkg-config"
    if ! command -v "$PC" &>/dev/null; then
        PC="pkg-config"
    fi
    if ! command -v "$PC" &>/dev/null; then
        return 1
    fi
    local pc_name=""
    if "$PC" --exists libmpv; then
        pc_name="libmpv"
    elif "$PC" --exists mpv; then
        pc_name="mpv"
    else
        return 1
    fi
    local have_version=$("$PC" --modversion "$pc_name" 2>/dev/null | tr -d '\r')
    if [ -z "$have_version" ]; then
        return 1
    fi
    # Use sort -V to compare versions
    if [ "$(printf '%s\n' "$want_version" "$have_version" | sort -V | tail -n1)" = "$have_version" ]; then
        return 0
    fi
    return 1
}

# Try to build or use mpv 0.41.0, fallback to 0.36.0 if needed
try_mpv_0_41() {
    # If libmpv >= 0.41.0 already installed, nothing to do
    if mpv_installed_ge "0.41.0"; then
        # When using deko3d, ensure both the deko header and patched render.h are present
        if [ "$RENDERER" = "deko3d" ]; then
            NEED_FIX=0
            if [ ! -f "/opt/devkitpro/portlibs/switch/include/mpv/render_dk3d.h" ]; then
                NEED_FIX=1
            fi
            if [ -f "/opt/devkitpro/portlibs/switch/include/mpv/render.h" ]; then
                if ! grep -q "MPV_RENDER_PARAM_DEKO3D_FBO" \
                    "/opt/devkitpro/portlibs/switch/include/mpv/render.h"; then
                    NEED_FIX=1
                fi
            else
                NEED_FIX=1
            fi
            if [ "$NEED_FIX" -eq 1 ]; then
                echo "ℹ️  libmpv >= 0.41.0 detected, but deko3d headers are incomplete; fixing install..."
                # Try to copy from mpv-build source if available (supports Docker mounts)
                if [ -f "/data/mpv-build/mpv-0.41.0/include/mpv/render_dk3d.h" ]; then
                    echo "📋 Copying deko3d headers from local mpv-build source..."
                    mkdir -p "/opt/devkitpro/portlibs/switch/include/mpv"
                    # Copy both render_dk3d.h AND the patched render.h
                    cp -f "/data/mpv-build/mpv-0.41.0/include/mpv/render_dk3d.h" \
                        "/opt/devkitpro/portlibs/switch/include/mpv/"
                    cp -f "/data/mpv-build/mpv-0.41.0/include/mpv/render.h" \
                        "/opt/devkitpro/portlibs/switch/include/mpv/"
                    echo "✅ Copied render_dk3d.h and patched render.h"
                    return 0
                fi
                ensure_prerequisites
                bash "scripts/build_mpv_0.41.0.sh" "$RENDERER" || true
            else
                echo "ℹ️  Detected system libmpv >= 0.41.0 with deko3d headers; skipping build"
            fi
        else
            echo "ℹ️  Detected system libmpv >= 0.41.0; skipping build"
        fi
        return 0
    fi
    
    echo "⚠️  mpv 0.41.0 package not found, checking build prerequisites..."
    ensure_prerequisites
    
    if ! has_build_prerequisites; then
        local missing=""
        command -v meson &>/dev/null || missing="${missing}meson "
        command -v ninja &>/dev/null || missing="${missing}ninja "
        [ -d "/opt/devkitpro" ] || missing="${missing}devkitpro "
        echo "❌ Missing build prerequisites: $missing"
        echo "   Install via: pacman -S meson ninja"
        return 1
    fi
    
    # Ensure build-time deps for deko3d are installed before building mpv
    if [ "$RENDERER" = "deko3d" ]; then
        # Install deko3d + libuam required by mpv's deko3d backend
        for dep in \
            "deko3d-8939ff80f94d061dbc7d107e08b8e3be53e2938b-1-any.pkg.tar.zst" \
            "libuam-f8c9eef01ffe06334d530393d636d69e2b52744b-1-any.pkg.tar.zst"; do
            if [ -f "$dep" ]; then
                echo "📦 Preinstalling build dependency: $dep"
                dkp-pacman -U --noconfirm "$dep" >/dev/null || true
            fi
        done
    fi

    echo "🔨 Building mpv 0.41.0 with $RENDERER renderer (30-45 min)..."
    if bash "scripts/build_mpv_0.41.0.sh" "$RENDERER"; then
        echo "✅ mpv 0.41.0 built successfully!"
        # Verify installation succeeded
        if mpv_installed_ge "0.41.0"; then
            return 0
        else
            echo "⚠️  mpv 0.41.0 build finished but not detected via pkg-config"
            return 1
        fi
    else
        echo "⚠️  Build failed - falling back to 0.36.0"
        return 1
    fi
}

# Select packages based on renderer and mpv version
select_packages() {
    local renderer="$1"
    local mpv_version="$2"
    
    # Base packages (always needed)
    local base_pkgs=(
        "switch-libass-0.17.1-1-any.pkg.tar.zst"
        "switch-nspmini-48d4fc2-1-any.pkg.tar.xz"
        "hacBrewPack-3.05-1-any.pkg.tar.zst"
    )
    
    if [ "$renderer" = "deko3d" ]; then
        # deko3d requires additional libraries
        printf '%s\n' \
            "deko3d-8939ff80f94d061dbc7d107e08b8e3be53e2938b-1-any.pkg.tar.zst" \
            "libuam-f8c9eef01ffe06334d530393d636d69e2b52744b-1-any.pkg.tar.zst" \
            "switch-ffmpeg-7.1-1-any.pkg.tar.zst" \
            "${base_pkgs[@]}"
        
        # deko3d-specific mpv packages
        if [ "$mpv_version" = "0.41.0" ]; then
            # For mpv 0.41.0 we build and install via Meson; do not install a fake pacman package
            # If a real pacman package exists locally with any compression, you could list it here.
            :
        else
            echo "switch-libmpv_deko3d-0.36.0-2-any.pkg.tar.zst"
        fi
    else
        # OpenGL: standard ffmpeg, no deko3d deps
        printf '%s\n' \
            "switch-ffmpeg-6.1-5-any.pkg.tar.zst" \
            "${base_pkgs[@]}" \
            "switch-libmpv-0.36.0-2-any.pkg.tar.zst"
    fi
}

# Download and install package
install_package() {
    local pkg="$1"
    if [ -f "$pkg" ]; then
        echo "📦 Using local: $pkg"
    else
        echo "📥 Downloading: $pkg"
        curl -LO "${BASE_URL}${pkg}" 2>/dev/null || {
            echo "❌ Failed to download $pkg"
            return 1
        }
    fi
    
    dkp-pacman -U --noconfirm "$pkg" || {
        echo "❌ Failed to install $pkg"
        return 1
    }
}

# Main build flow
BASE_URL="https://github.com/xfangfang/wiliwili/releases/download/v0.1.0/"

# Attempt to use mpv 0.41.0 if requested (must succeed for deko3d)
if [ "$MPV_VERSION" = "0.41.0" ]; then
    if ! try_mpv_0_41; then
        if [ "$RENDERER" = "deko3d" ]; then
            echo "❌ FATAL: deko3d requires mpv 0.41.0, but build or install failed"
            echo "   Cannot fallback to mpv 0.36.0 with deko3d"
            exit 1
        else
            # OpenGL can use 0.36.0
            MPV_VERSION="0.36.0"
            echo "⏹️  Falling back to mpv 0.36.0 for OpenGL rendering"
        fi
    fi
fi

# Display build configuration
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "TsVitch Nintendo Switch Build Configuration"
echo "  Renderer:            $RENDERER"
echo "  MPV Version:         $MPV_VERSION"
echo "  Build Directory:     $BUILD_DIR"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Validate required environment variables
for var in GA_ID GA_KEY SERVER_URL SERVER_TOKEN M3U8_URL; do
    if [ -z "${!var}" ]; then
        echo "❌ Missing environment variable: $var"
        exit 1
    fi
done

# Build environment flags
GITHUB_TOKEN_FLAG=""
[ -n "$GITHUB_TOKEN" ] && GITHUB_TOKEN_FLAG="-DGITHUB_TOKEN=\"${GITHUB_TOKEN}\""

UNITY_BUILD_FLAG="-DBRLS_UNITY_BUILD=OFF"
[ "${ENABLE_UNITY_BUILD}" = "true" ] && UNITY_BUILD_FLAG="-DBRLS_UNITY_BUILD=ON"

# Download and install packages
echo "📦 Installing dependencies..."
while IFS= read -r pkg; do
    install_package "$pkg" || exit 1
done < <(select_packages "$RENDERER" "$MPV_VERSION")

# Build TsVitch
echo ""
echo "🔨 Building TsVitch..."
# Ensure pkg-config sees portlibs (needed for libuam detection)
export PKG_CONFIG_PATH="/opt/devkitpro/portlibs/switch/lib/pkgconfig:${PKG_CONFIG_PATH}"
cmake -B ${BUILD_DIR} \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILTIN_NSP=ON \
  -DPLATFORM_SWITCH=ON \
  -DUSE_DEKO3D=${USE_DEKO3D} \
  ${UNITY_BUILD_FLAG} \
  -DCMAKE_UNITY_BUILD_BATCH_SIZE=16 \
  -DANALYTICS=ON \
  -DANALYTICS_ID="${GA_ID}" \
  -DANALYTICS_KEY="${GA_KEY}" \
  -DSERVER_URL="${SERVER_URL}" \
  -DSERVER_TOKEN="${SERVER_TOKEN}" \
  -DM3U8_URL="${M3U8_URL}" \
  ${GITHUB_TOKEN_FLAG}

make -C ${BUILD_DIR} TsVitch.nro -j$(nproc)