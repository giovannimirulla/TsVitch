#!/bin/bash
set -e

# ======== TsVitch Android Build Script ========
# Builds TsVitch for Android (arm64-v8a, armeabi-v7a)
#
# Prerequisites:
#   - Android Studio with NDK r22+ installed
#   - CMake 3.16+ (via Android Studio SDK Manager)
#   - Java 11+
#   - Precompiled libmpv .so files in android-project/app/libs/
#     (see android-project/app/libs/README.md for instructions)
#
# Usage:
#   export SERVER_URL="https://your-server.com"
#   export SERVER_TOKEN="your-token"
#   bash scripts/build_android.sh
#
# Optional environment variables:
#   ANDROID_SDK_ROOT  - Path to Android SDK (default: ~/Library/Android/sdk on macOS)
#   JAVA_HOME         - Path to JDK (default: Android Studio bundled JDK on macOS)
#   BUILD_TYPE        - "Debug" or "Release" (default: Release)

# ======== Validate required parameters ========

if [ -z "${SERVER_URL}" ]; then
    echo "ERROR: SERVER_URL is required but not set."
    echo "Usage: export SERVER_URL=https://your-server.com && bash scripts/build_android.sh"
    exit 1
fi

if [ -z "${SERVER_TOKEN}" ]; then
    echo "ERROR: SERVER_TOKEN is required but not set."
    echo "Usage: export SERVER_TOKEN=your-token && bash scripts/build_android.sh"
    exit 1
fi

# ======== Configuration ========

PROJECT_PATH="$(cd "$(dirname "$0")/.." && pwd)"
BOREALIS_PATH="${PROJECT_PATH}/library/borealis"
LIBROMFS_PATH="${BOREALIS_PATH}/library/lib/extern/libromfs/generator"
BUILD_DIR="${PROJECT_PATH}/build_libromfs_generator_android"
ANDROID_PROJECT_PATH="${PROJECT_PATH}/android-project"
JNI_TSVITCH_DIR="${ANDROID_PROJECT_PATH}/app/jni/tsvitch"
BUILD_TYPE="${BUILD_TYPE:-Release}"

echo "========================================"
echo "TsVitch Android Build"
echo "========================================"
echo "PROJECT_PATH:    ${PROJECT_PATH}"
echo "SERVER_URL:      ${SERVER_URL}"
echo "BUILD_TYPE:      ${BUILD_TYPE}"
echo "========================================"

# ======== Set default Android SDK/JDK paths (macOS) ========

if [ -z "${ANDROID_SDK_ROOT}" ]; then
    if [ -d "${HOME}/Library/Android/sdk" ]; then
        export ANDROID_SDK_ROOT="${HOME}/Library/Android/sdk"
        echo "Using ANDROID_SDK_ROOT: ${ANDROID_SDK_ROOT}"
    else
        echo "WARNING: ANDROID_SDK_ROOT not set. Set it manually if the build fails."
    fi
fi

if [ -z "${JAVA_HOME}" ]; then
    # Try Android Studio bundled JDK on macOS
    AS_JBR="/Applications/Android Studio.app/Contents/jbr/Contents/Home"
    if [ -d "${AS_JBR}" ]; then
        export JAVA_HOME="${AS_JBR}"
        echo "Using JAVA_HOME: ${JAVA_HOME}"
    else
        echo "WARNING: JAVA_HOME not set. Set it manually if the build fails."
    fi
fi

# ======== Validate precompiled libmpv .so files ========

echo ""
echo "Checking precompiled libmpv libraries..."
MISSING_LIBS=0
for ABI in arm64-v8a armeabi-v7a; do
    LIBS_DIR="${ANDROID_PROJECT_PATH}/app/libs/${ABI}"
    for LIB in libmpv.so libavcodec.so libavformat.so libavutil.so libswresample.so libswscale.so libavfilter.so; do
        if [ ! -f "${LIBS_DIR}/${LIB}" ]; then
            echo "  MISSING: ${LIBS_DIR}/${LIB}"
            MISSING_LIBS=1
        fi
    done
done

if [ "${MISSING_LIBS}" -eq 1 ]; then
    echo ""
    echo "ERROR: Precompiled libmpv .so files are missing."
    echo "Download them from: https://github.com/mpv-android/mpv-android/releases"
    echo "See: android-project/app/libs/README.md for detailed instructions."
    exit 1
fi
echo "  All required .so files found."

# ======== Create/update JNI symlinks ========

echo ""
echo "Setting up JNI symlinks..."
JNI_DIR="${ANDROID_PROJECT_PATH}/app/jni"

# Remove old symlinks if they exist
rm -f "${JNI_DIR}/SDL" "${JNI_DIR}/tsvitch"

# Create relative symlinks (portable across machines)
# Paths are relative from android-project/app/jni/
ln -sf "../../../library/borealis/library/lib/extern/SDL" "${JNI_DIR}/SDL"
ln -sf "../../.." "${JNI_DIR}/tsvitch"

echo "  SDL -> ../../../library/borealis/library/lib/extern/SDL"
echo "  tsvitch -> ../../.."

# ======== Build libromfs-generator for host ========

echo ""
echo "Building libromfs-generator for host..."

cmake -B "${BUILD_DIR}" "${LIBROMFS_PATH}" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}"

GENERATOR_BINARY="${BUILD_DIR}/libromfs-generator"
if [ ! -f "${GENERATOR_BINARY}" ]; then
    echo "ERROR: libromfs-generator build failed. Binary not found at: ${GENERATOR_BINARY}"
    exit 1
fi

# Copy libromfs-generator to the JNI tsvitch directory
echo "Copying libromfs-generator to ${JNI_TSVITCH_DIR}/"
cp "${GENERATOR_BINARY}" "${JNI_TSVITCH_DIR}/libromfs-generator"
chmod +x "${JNI_TSVITCH_DIR}/libromfs-generator"

# Clean up build directory
rm -rf "${BUILD_DIR}"
echo "libromfs-generator built and copied successfully."

# ======== Write gradle.properties with build parameters ========

echo ""
echo "Writing build parameters to gradle.properties..."
GRADLE_PROPS="${ANDROID_PROJECT_PATH}/gradle.properties"

# Preserve existing content and update/add SERVER_URL and SERVER_TOKEN
if grep -q "^SERVER_URL=" "${GRADLE_PROPS}" 2>/dev/null; then
    sed -i.bak "s|^SERVER_URL=.*|SERVER_URL=${SERVER_URL}|" "${GRADLE_PROPS}"
else
    echo "SERVER_URL=${SERVER_URL}" >> "${GRADLE_PROPS}"
fi

if grep -q "^SERVER_TOKEN=" "${GRADLE_PROPS}" 2>/dev/null; then
    sed -i.bak "s|^SERVER_TOKEN=.*|SERVER_TOKEN=${SERVER_TOKEN}|" "${GRADLE_PROPS}"
else
    echo "SERVER_TOKEN=${SERVER_TOKEN}" >> "${GRADLE_PROPS}"
fi

if grep -q "^GA_ID=" "${GRADLE_PROPS}" 2>/dev/null; then
    sed -i.bak "s|^GA_ID=.*|GA_ID=${GA_ID:-}|" "${GRADLE_PROPS}"
else
    echo "GA_ID=${GA_ID:-}" >> "${GRADLE_PROPS}"
fi

if grep -q "^GA_KEY=" "${GRADLE_PROPS}" 2>/dev/null; then
    sed -i.bak "s|^GA_KEY=.*|GA_KEY=${GA_KEY:-}|" "${GRADLE_PROPS}"
else
    echo "GA_KEY=${GA_KEY:-}" >> "${GRADLE_PROPS}"
fi

if grep -q "^M3U8_URL=" "${GRADLE_PROPS}" 2>/dev/null; then
    sed -i.bak "s|^M3U8_URL=.*|M3U8_URL=${M3U8_URL:-}|" "${GRADLE_PROPS}"
else
    echo "M3U8_URL=${M3U8_URL:-}" >> "${GRADLE_PROPS}"
fi

# Clean up backup files created by sed on macOS
rm -f "${GRADLE_PROPS}.bak"

echo "gradle.properties updated."

# ======== Build APK ========

echo ""
echo "Building TsVitch APK (${BUILD_TYPE})..."

cd "${ANDROID_PROJECT_PATH}"

if [ "${BUILD_TYPE}" = "Debug" ]; then
    ./gradlew assembleDebug
    APK_PATH="${ANDROID_PROJECT_PATH}/app/build/outputs/apk/debug/app-debug.apk"
else
    ./gradlew assembleRelease
    APK_PATH="${ANDROID_PROJECT_PATH}/app/build/outputs/apk/release/app-release.apk"
fi

echo ""
echo "========================================"
echo "Build complete!"
echo "APK: ${APK_PATH}"
echo ""
echo "To install on a connected device:"
if [ "${BUILD_TYPE}" = "Debug" ]; then
    echo "  ./gradlew installDebug"
else
    echo "  adb install ${APK_PATH}"
fi
echo "========================================"
