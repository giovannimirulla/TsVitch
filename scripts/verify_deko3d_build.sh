#!/bin/bash
# Verification script to ensure deko3d is used and OpenGL is disabled for Switch

set -e

echo "🔍 Checking TsVitch Switch Build Configuration..."
echo ""

BUILD_DIR="${1:-cmake-build-switch}"

if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    echo "❌ Error: $BUILD_DIR/CMakeCache.txt not found"
    echo "   Run build_switch_deko3d.sh first"
    exit 1
fi

echo "📋 Configuration Check:"
echo ""

# Check for deko3d
if grep -q "USE_DEKO3D:BOOL=ON" "$BUILD_DIR/CMakeCache.txt"; then
    echo "✅ USE_DEKO3D is enabled"
else
    echo "❌ USE_DEKO3D is NOT enabled"
    exit 1
fi

# Check for Switch platform
if grep -q "PLATFORM_SWITCH:BOOL=ON" "$BUILD_DIR/CMakeCache.txt"; then
    echo "✅ PLATFORM_SWITCH is enabled"
else
    echo "❌ PLATFORM_SWITCH is NOT enabled"
    exit 1
fi

echo ""
echo "📦 Dependency Check:"
echo ""

# Check for deko3d installation
if dkp-pacman -Q deko3d &>/dev/null; then
    deko3d_version=$(dkp-pacman -Q deko3d | awk '{print $2}')
    echo "✅ deko3d installed (version: $deko3d_version)"
else
    echo "❌ deko3d not installed"
    exit 1
fi

# Check for libmpv_deko3d
if dkp-pacman -Q switch-libmpv_deko3d &>/dev/null; then
    libmpv_version=$(dkp-pacman -Q switch-libmpv_deko3d | awk '{print $2}')
    echo "✅ switch-libmpv_deko3d installed (version: $libmpv_version)"
else
    echo "❌ switch-libmpv_deko3d NOT installed"
    echo "   Using standard switch-libmpv instead - this may not work correctly!"
    if dkp-pacman -Q switch-libmpv &>/dev/null; then
        echo "   ⚠️  Found switch-libmpv (standard, not deko3d)"
    fi
fi

echo ""
echo "🔧 Source Code Check:"
echo ""

# Check that get_proc_address was removed
if grep -q "get_proc_address" tsvitch/source/view/mpv_core.cpp; then
    echo "⚠️  get_proc_address still referenced in source"
else
    echo "✅ get_proc_address successfully removed"
fi

# Check that opengl-glfinish was removed for Switch
if grep -A2 "if defined(__SWITCH__)" tsvitch/source/view/mpv_core.cpp | grep -q "opengl-glfinish"; then
    echo "❌ opengl-glfinish option still present for Switch"
    exit 1
else
    echo "✅ opengl-glfinish option removed for Switch"
fi

# Check that deko3d rendering is intact
if grep -q "BOREALIS_USE_DEKO3D" tsvitch/source/view/mpv_core.cpp; then
    echo "✅ deko3d rendering code is present"
else
    echo "❌ deko3d rendering code missing"
    exit 1
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "✅ All checks passed! Switch build is correctly configured for deko3d"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "Next steps:"
echo "  1. Run: ./scripts/build_switch_deko3d.sh"
echo "  2. Output file: cmake-build-switch/TsVitch.nro"
echo ""
