#!/bin/bash
set -e

# ======== Build OpenSSL for Android ========
# Compiles OpenSSL 3.x for Android arm64-v8a using NDK r22+
# Output: android-project/app/libs/arm64-v8a/libssl.so + libcrypto.so
#         android-project/app/jni/openssl/include/openssl/...
#
# Usage:
#   bash scripts/build_openssl_android.sh
#
# Prerequisites:
#   - Android NDK r22+ at ~/Library/Android/sdk/ndk/22.1.7171670
#   - Internet access to download OpenSSL source

OPENSSL_VERSION="3.3.2"
OPENSSL_URL="https://www.openssl.org/source/openssl-${OPENSSL_VERSION}.tar.gz"

PROJECT_PATH="$(cd "$(dirname "$0")/.." && pwd)"
NDK_PATH="${ANDROID_SDK_ROOT:-$HOME/Library/Android/sdk}/ndk/22.1.7171670"
TOOLCHAIN="${NDK_PATH}/toolchains/llvm/prebuilt/darwin-x86_64"
BUILD_DIR="/tmp/openssl-android-build"
OUTPUT_LIBS="${PROJECT_PATH}/android-project/app/libs"
OUTPUT_INCLUDE="${PROJECT_PATH}/android-project/app/jni/openssl/include"

ABI="arm64-v8a"
ANDROID_API=21
TARGET="aarch64-linux-android"
CLANG="${TOOLCHAIN}/bin/${TARGET}${ANDROID_API}-clang"

echo "========================================"
echo "Building OpenSSL ${OPENSSL_VERSION} for Android ${ABI}"
echo "NDK: ${NDK_PATH}"
echo "========================================"

# Verify NDK exists
if [ ! -f "${CLANG}" ]; then
    echo "ERROR: NDK clang not found at ${CLANG}"
    echo "Make sure NDK r22.1.7171670 is installed."
    exit 1
fi

# Download OpenSSL source
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

if [ ! -f "openssl-${OPENSSL_VERSION}.tar.gz" ]; then
    echo "Downloading OpenSSL ${OPENSSL_VERSION}..."
    curl -L -o "openssl-${OPENSSL_VERSION}.tar.gz" "${OPENSSL_URL}"
fi

if [ ! -d "openssl-${OPENSSL_VERSION}" ]; then
    echo "Extracting OpenSSL..."
    tar xzf "openssl-${OPENSSL_VERSION}.tar.gz"
fi

cd "openssl-${OPENSSL_VERSION}"

# Configure for arm64-v8a
echo "Configuring OpenSSL for ${ABI}..."

export ANDROID_NDK_ROOT="${NDK_PATH}"
export PATH="${TOOLCHAIN}/bin:${PATH}"

./Configure android-arm64 \
    -D__ANDROID_API__=${ANDROID_API} \
    --prefix="${BUILD_DIR}/install-${ABI}" \
    --openssldir="${BUILD_DIR}/install-${ABI}/ssl" \
    shared \
    no-tests \
    no-ui-console \
    no-asm \
    -fPIC

echo "Building OpenSSL..."
make -j$(sysctl -n hw.ncpu) build_sw

echo "Installing OpenSSL..."
make install_sw

# Copy .so files to android-project/app/libs/arm64-v8a/
echo "Copying libraries..."
mkdir -p "${OUTPUT_LIBS}/${ABI}"
cp "${BUILD_DIR}/install-${ABI}/lib/libssl.so.3" "${OUTPUT_LIBS}/${ABI}/libssl.so" 2>/dev/null || \
cp "${BUILD_DIR}/install-${ABI}/lib/libssl.so" "${OUTPUT_LIBS}/${ABI}/libssl.so"
cp "${BUILD_DIR}/install-${ABI}/lib/libcrypto.so.3" "${OUTPUT_LIBS}/${ABI}/libcrypto.so" 2>/dev/null || \
cp "${BUILD_DIR}/install-${ABI}/lib/libcrypto.so" "${OUTPUT_LIBS}/${ABI}/libcrypto.so"

# Copy headers
echo "Copying headers..."
mkdir -p "${OUTPUT_INCLUDE}"
cp -r "${BUILD_DIR}/install-${ABI}/include/openssl" "${OUTPUT_INCLUDE}/"

echo ""
echo "========================================"
echo "OpenSSL built successfully!"
echo "Libraries: ${OUTPUT_LIBS}/${ABI}/libssl.so + libcrypto.so"
echo "Headers:   ${OUTPUT_INCLUDE}/openssl/"
echo "========================================"
