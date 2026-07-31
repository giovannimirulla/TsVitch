cmake_minimum_required(VERSION 3.16)

project(TSVITCH_ANDROID)

set(BUILD_SHARED_LIBS ON CACHE BOOL "")

# SDL sources are in a subfolder named "SDL"
add_subdirectory(SDL)

# Enable Android platform
SET(PLATFORM_ANDROID ON CACHE BOOL "")

# libromfs-generator must be pre-built for the host machine before running this build.
# Run: bash library/borealis/build_libromfs_generator.sh
# Then copy the generated binary to android-project/app/jni/tsvitch/libromfs-generator
SET(LIBROMFS_PREBUILT_GENERATOR "${CMAKE_CURRENT_SOURCE_DIR}/tsvitch/libromfs-generator" CACHE STRING "")

if (NOT EXISTS "${LIBROMFS_PREBUILT_GENERATOR}")
    message(FATAL_ERROR
        "libromfs-generator has not been built or copied.\n"
        "Please run: bash scripts/build_android.sh\n"
        "Or manually: bash library/borealis/build_libromfs_generator.sh\n"
        "Then copy the binary to: ${LIBROMFS_PREBUILT_GENERATOR}"
    )
endif()

# Validate required build parameters
if (NOT DEFINED SERVER_URL OR "${SERVER_URL}" STREQUAL "")
    message(FATAL_ERROR
        "SERVER_URL is required but not defined.\n"
        "Pass it via: -DSERVER_URL=https://your-server.com\n"
        "Or set it in android-project/gradle.properties"
    )
endif()

if (NOT DEFINED SERVER_TOKEN OR "${SERVER_TOKEN}" STREQUAL "")
    message(FATAL_ERROR
        "SERVER_TOKEN is required but not defined.\n"
        "Pass it via: -DSERVER_TOKEN=your-token\n"
        "Or set it in android-project/gradle.properties"
    )
endif()

# Validate that precompiled libmpv .so files are present for the target ABI
if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../libs/${ANDROID_ABI}/libmpv.so")
    message(FATAL_ERROR
        "Precompiled libmpv.so not found for ABI: ${ANDROID_ABI}\n"
        "Expected path: ${CMAKE_CURRENT_SOURCE_DIR}/../libs/${ANDROID_ABI}/libmpv.so\n"
        "Download precompiled .so files from: https://github.com/mpv-android/mpv-android/releases\n"
        "Copy libmpv.so, libavcodec.so, libavformat.so, libavutil.so,\n"
        "libswresample.so, libswscale.so, libavfilter.so to:\n"
        "android-project/app/libs/${ANDROID_ABI}/"
    )
endif()

# OpenSSL for HTTPS support
# Create imported targets BEFORE add_subdirectory(tsvitch) so that cpr/curl can find them
# without needing find_package(OpenSSL) which doesn't work cross-compiled for Android
set(_OPENSSL_LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../libs/${ANDROID_ABI}")
set(_OPENSSL_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/openssl/include")

if (EXISTS "${_OPENSSL_LIBS_DIR}/libssl.so" AND EXISTS "${_OPENSSL_LIBS_DIR}/libcrypto.so")
    message(STATUS "Android: OpenSSL found at ${_OPENSSL_LIBS_DIR}")

    # Set variables that FindOpenSSL.cmake and cpr use
    set(OPENSSL_FOUND TRUE CACHE BOOL "" FORCE)
    set(OPENSSL_INCLUDE_DIR "${_OPENSSL_INCLUDE_DIR}" CACHE PATH "" FORCE)
    set(OPENSSL_CRYPTO_LIBRARY "${_OPENSSL_LIBS_DIR}/libcrypto.so" CACHE FILEPATH "" FORCE)
    set(OPENSSL_SSL_LIBRARY "${_OPENSSL_LIBS_DIR}/libssl.so" CACHE FILEPATH "" FORCE)
    set(OPENSSL_LIBRARIES "${_OPENSSL_LIBS_DIR}/libssl.so;${_OPENSSL_LIBS_DIR}/libcrypto.so" CACHE STRING "" FORCE)
    set(OPENSSL_VERSION "3.3.2" CACHE STRING "" FORCE)

    # Create imported targets so cpr doesn't need to call find_package(OpenSSL)
    add_library(OpenSSL::Crypto SHARED IMPORTED GLOBAL)
    set_target_properties(OpenSSL::Crypto PROPERTIES
        IMPORTED_LOCATION "${_OPENSSL_LIBS_DIR}/libcrypto.so"
        INTERFACE_INCLUDE_DIRECTORIES "${_OPENSSL_INCLUDE_DIR}"
    )
    add_library(OpenSSL::SSL SHARED IMPORTED GLOBAL)
    set_target_properties(OpenSSL::SSL PROPERTIES
        IMPORTED_LOCATION "${_OPENSSL_LIBS_DIR}/libssl.so"
        INTERFACE_INCLUDE_DIRECTORIES "${_OPENSSL_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES "OpenSSL::Crypto"
    )

    # Enable SSL in cpr with OpenSSL backend
    set(CPR_ENABLE_SSL ON CACHE BOOL "" FORCE)
    set(CPR_FORCE_OPENSSL_BACKEND ON CACHE BOOL "" FORCE)
    set(CURL_USE_OPENSSL ON CACHE INTERNAL "" FORCE)
else()
    message(WARNING "Android: OpenSSL not found at ${_OPENSSL_LIBS_DIR}, HTTPS will not work")
    message(WARNING "Run: bash scripts/build_openssl_android.sh")
    set(CPR_ENABLE_SSL OFF CACHE BOOL "" FORCE)
    set(CPR_ENABLE_SSL OFF)
endif()

# Include TsVitch root CMakeLists.txt
# The 'tsvitch' directory is a symlink to the TsVitch project root
# Pass the android-project libs path so the root CMakeLists.txt can find precompiled .so files
set(ANDROID_PROJECT_LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../libs" CACHE PATH "Path to android-project/app/libs/")
set(ANDROID_PROJECT_MPV_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mpv/include" CACHE PATH "Path to mpv headers")
add_subdirectory(tsvitch)
