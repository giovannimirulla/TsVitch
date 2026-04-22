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

# Disable SSL for cpr on Android (no OpenSSL/MbedTLS available in NDK by default)
# Using set() before add_subdirectory ensures it takes precedence over option() in cpr
# The -D flag from Gradle also passes it, but we set it here as a normal variable too
set(CPR_ENABLE_SSL OFF CACHE BOOL "" FORCE)
# Also set as a normal variable so it shadows the cache when cpr reads it
set(CPR_ENABLE_SSL OFF)

# Include TsVitch root CMakeLists.txt
# The 'tsvitch' directory is a symlink to the TsVitch project root
# Pass the android-project libs path so the root CMakeLists.txt can find precompiled .so files
set(ANDROID_PROJECT_LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../libs" CACHE PATH "Path to android-project/app/libs/")
set(ANDROID_PROJECT_MPV_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mpv/include" CACHE PATH "Path to mpv headers")
add_subdirectory(tsvitch)
