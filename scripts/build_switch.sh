#!/bin/bash
# TsVitch Nintendo Switch Build Script
# 
# Supports multiple rendering backends:
#   - deko3d (default): Primary GPU-accelerated rendering with OpenGL fallback
#   - opengl: OpenGL-only rendering (legacy, for compatibility)
#
# Usage:
#   ./scripts/build_switch.sh [--renderer deko3d|opengl]
#
# Options:
#   --renderer deko3d    Build with deko3d primary + OpenGL fallback (default)
#   --renderer opengl    Build with OpenGL only (legacy mode)
#
# Examples:
#   ./scripts/build_switch.sh                      # Uses deko3d (default)
#   ./scripts/build_switch.sh --renderer deko3d    # Explicit deko3d
#   ./scripts/build_switch.sh --renderer opengl    # OpenGL only
#

set -e

# Default to deko3d
RENDERER="deko3d"
BUILD_DIR="cmake-build-switch"
USE_DEKO3D="ON"

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
        *)
            echo "❌ Unknown option: $1"
            echo "Usage: $0 [--renderer deko3d|opengl]"
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

BASE_URL="https://github.com/xfangfang/wiliwili/releases/download/v0.1.0/"

# Select packages based on renderer
if [ "$RENDERER" = "deko3d" ]; then
    PKGS=(
        "deko3d-8939ff80f94d061dbc7d107e08b8e3be53e2938b-1-any.pkg.tar.zst"
        "libuam-f8c9eef01ffe06334d530393d636d69e2b52744b-1-any.pkg.tar.zst"
        "switch-libass-0.17.1-1-any.pkg.tar.zst"
        "switch-ffmpeg-7.1-1-any.pkg.tar.zst"
        "switch-libmpv_deko3d-0.36.0-2-any.pkg.tar.zst"
        "switch-nspmini-48d4fc2-1-any.pkg.tar.xz"
        "hacBrewPack-3.05-1-any.pkg.tar.zst"
    )
else
    PKGS=(
        "switch-libass-0.17.1-1-any.pkg.tar.zst"
        "switch-ffmpeg-6.1-5-any.pkg.tar.zst"
        "switch-libmpv-0.36.0-2-any.pkg.tar.zst"
        "switch-nspmini-48d4fc2-1-any.pkg.tar.xz"
        "hacBrewPack-3.05-1-any.pkg.tar.zst"
    )
fi

# Download and install packages
for PKG in "${PKGS[@]}"; do
    [ -f "${PKG}" ] || curl -LO ${BASE_URL}${PKG}
    dkp-pacman -U --noconfirm ${PKG}
done

# Validate required environment variables
if [ -z "${GA_ID}" ] || [ -z "${GA_KEY}" ]; then
    echo "❌ GA_ID or GA_KEY not found in environment"
    exit 1
fi

if [ -z "${SERVER_URL}" ]; then
    echo "❌ SERVER_URL not found in environment"
    exit 1
fi

if [ -z "${SERVER_TOKEN}" ]; then
    echo "❌ SERVER_TOKEN not found in environment"
    exit 1
fi

if [ -z "${M3U8_URL}" ]; then
    echo "❌ M3U8_URL not found in environment"
    exit 1
fi

# GITHUB_TOKEN is optional but pass it if available
GITHUB_TOKEN_FLAG=""
if [ -n "${GITHUB_TOKEN}" ]; then
    GITHUB_TOKEN_FLAG="-DGITHUB_TOKEN=\"${GITHUB_TOKEN}\""
fi

# Disable unity build by default for stability on Switch
# Can be re-enabled with ENABLE_UNITY_BUILD=true environment variable
UNITY_BUILD_FLAG="-DBRLS_UNITY_BUILD=OFF"
if [ "${ENABLE_UNITY_BUILD}" = "true" ]; then
    UNITY_BUILD_FLAG="-DBRLS_UNITY_BUILD=ON"
fi

# Display build configuration
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Building TsVitch for Nintendo Switch"
if [ "$RENDERER" = "deko3d" ]; then
    echo "  Renderer:            deko3d (primary) + OpenGL (fallback)"
else
    echo "  Renderer:            OpenGL (legacy mode)"
fi
echo "  Build Directory:     $BUILD_DIR"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

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