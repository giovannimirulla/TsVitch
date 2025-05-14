cmake_minimum_required(VERSION 3.13)

# --------------------------------------------------------------------------- #
#  Répertoires Borealis et dépendances                                        #
# --------------------------------------------------------------------------- #
set(BOREALIS_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/library/borealis)
set(BOREALIS_LIBRARY ${BOREALIS_DIR}/library)

# --------------------------------------------------------------------------- #
#  Options plates-formes / build                                              #
# --------------------------------------------------------------------------- #
include(${BOREALIS_LIBRARY}/cmake/commonOption.cmake)

# Nintendo Switch
cmake_dependent_option(BUILTIN_NSP "Built in NSP forwarder" OFF "PLATFORM_SWITCH" OFF)

# macOS (app bundle)
cmake_dependent_option(MAC_IntelChip_Old "Build for x86_64 (10.11)" OFF "APPLE;NOT PLATFORM_IOS" OFF)
cmake_dependent_option(MAC_IntelChip     "Build for x86_64 (10.15)" OFF "APPLE;NOT PLATFORM_IOS" OFF)
cmake_dependent_option(MAC_AppleSilicon  "Build for arm64 (11.0)"   OFF "APPLE;NOT PLATFORM_IOS" OFF)
cmake_dependent_option(MAC_Universal     "Universal (x86_64+arm64)" OFF "APPLE;NOT PLATFORM_IOS" OFF)
cmake_dependent_option(MAC_DOWNLOAD_DYLIB "Use precompiled dylibs"  OFF "APPLE;NOT PLATFORM_IOS" OFF)

# Linux install path :
cmake_dependent_option(INSTALL "Install to system path" OFF "UNIX;NOT APPLE" OFF)

# Dev options
option(DEBUG_SANITIZER "Turn on sanitizers (debug build only)" OFF)

# Google Analytics helper
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/GoogleAnalytics.cmake)

# --------------------------------------------------------------------------- #
#  Dépendances (peuvent provenir du système)                                  #
# --------------------------------------------------------------------------- #
option(USE_SHARED_LIB  "Use shared libs from system"    OFF)
option(USE_SYSTEM_CURL "" ${USE_SHARED_LIB})
option(USE_SYSTEM_QRCODEGEN "" ${USE_SHARED_LIB})
option(USE_SYSTEM_PYSTRING ""  ${USE_SHARED_LIB})
option(USE_SYSTEM_OPENCC  ""   ${USE_SHARED_LIB})
option(USE_SYSTEM_FMT ""       ${USE_SHARED_LIB})
option(USE_SYSTEM_TINYXML2 ""  ${USE_SHARED_LIB})
option(USE_SYSTEM_SDL2 ""      ${USE_SHARED_LIB})
option(USE_SYSTEM_GLFW "Use system GLFW (not recommended)" OFF)

option(DISABLE_OPENCC "Disable Simplified/Traditional conversion" OFF)
option(DISABLE_WEBP   "Disable WebP" OFF)

# mpv related
option(MPV_SW_RENDER "Use CPU to draw videos" OFF)
if (MPV_SW_RENDER)
    list(APPEND APP_PLATFORM_OPTION -DMPV_SW_RENDER)
endif()

option(MPV_NO_FB "Use system framebuffer (no UI masking)" OFF)
if (MPV_NO_FB)
    list(APPEND APP_PLATFORM_OPTION -DMPV_NO_FB)
endif()

cmake_dependent_option(MPV_BUNDLE_DLL "Bundle mpv.dll" OFF "USE_LIBROMFS;WIN32" OFF)
if (MPV_BUNDLE_DLL)
    list(APPEND APP_PLATFORM_OPTION -DMPV_BUNDLE_DLL)
    list(APPEND APP_PLATFORM_LIB MemoryModule)
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/extra.cmake)

# --------------------------------------------------------------------------- #
#  Toolchain / Git info / defines                                             #
# --------------------------------------------------------------------------- #
include(${BOREALIS_LIBRARY}/cmake/toolchain.cmake)

git_info(GIT_TAG_VERSION GIT_TAG_SHORT)
list(APPEND APP_PLATFORM_OPTION
     -DBUILD_TAG_VERSION=${GIT_TAG_VERSION}
     -DBUILD_TAG_SHORT=${GIT_TAG_SHORT})

# nanovg optimisation
add_definitions(-DFONS_HASH_LUT_SIZE=4096)

# --------------------------------------------------------------------------- #
#  Project                                                                    #
# --------------------------------------------------------------------------- #
project(tsvitch)
set(VERSION_MAJOR "0")
set(VERSION_MINOR "1")
set(VERSION_REVISION "1")
if (NOT VERSION_BUILD)
    set(VERSION_BUILD "0")
endif()

set(PROJECT_AUTHOR "giovannimirulla")
set(PACKAGE_NAME   com.giovannimirulla.tsvitch)
set(VITA_TITLEID   "TSVITCH0")
set(PSN_TITLE_ID   "TVSI00000")

set(VITA_VERSION   "00.10")
set(PROJECT_ICON   ${CMAKE_CURRENT_SOURCE_DIR}/resources/icon/icon.jpg)
set(PROJECT_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/resources)

set(APP_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_REVISION}")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

# --------------------------------------------------------------------------- #
#  Threads + JSON (NOUVEAU)                                                   #
# --------------------------------------------------------------------------- #
find_package(Threads REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)   # ← historique / favoris

list(APPEND APP_PLATFORM_LIB ${CMAKE_THREAD_LIBS_INIT})

# --------------------------------------------------------------------------- #
#  libmpv / libwebp selon plateforme                                          #
# --------------------------------------------------------------------------- #
if (MAC_DOWNLOAD_DYLIB)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/macos.cmake)
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR}/deps/lib/cmake)

elseif (PLATFORM_DESKTOP)
    find_package(MPV REQUIRED)
    message(STATUS "Found libmpv: ${MPV_VERSION} ${MPV_INCLUDE_DIR} ${MPV_LIBRARY}")
    list(APPEND APP_PLATFORM_INCLUDE ${MPV_INCLUDE_DIR})
    if (NOT MPV_BUNDLE_DLL)
        list(APPEND APP_PLATFORM_LIB ${MPV_LIBRARY})
    endif()

    find_package(WebP)
    if (WebP_FOUND AND NOT DISABLE_WEBP)
        message(STATUS "Found WebP: ${WebP_INCLUDE_DIRS} ${WebP_LIBRARY}")
        list(APPEND APP_PLATFORM_OPTION -DUSE_WEBP)
        list(APPEND APP_PLATFORM_INCLUDE ${WebP_INCLUDE_DIRS})
        list(APPEND APP_PLATFORM_LIB ${WebP_LIBRARY})
    endif()

    if (MINGW)
        set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++ -static"
            CACHE STRING "" FORCE)
    endif()

elseif (APP_PLATFORM_CUSTOM_LIBS)
    message(STATUS "Using custom libraries for mpv/webp")

else()
    # Cross-compile (Switch, etc.)
    find_package(PkgConfig REQUIRED)

    pkg_search_module(MPV REQUIRED mpv)
    message(STATUS "Found libmpv: ${MPV_VERSION} ${MPV_INCLUDE_DIRS}")
    list(APPEND APP_PLATFORM_INCLUDE ${MPV_INCLUDE_DIRS})
    list(APPEND APP_PLATFORM_LIB ${MPV_STATIC_LIBRARIES})
    link_directories(${MPV_LIBRARY_DIRS})
    if (PLATFORM_IOS)
        list(APPEND APP_PLATFORM_LIB
            "-framework CoreMedia -framework CoreText -framework VideoToolbox")
    endif()

    pkg_search_module(LIBWEBP QUIET libwebp)
    if (LIBWEBP_FOUND AND NOT DISABLE_WEBP)
        message(STATUS "Found libwebp: ${LIBWEBP_INCLUDE_DIRS}")
        list(APPEND APP_PLATFORM_INCLUDE ${LIBWEBP_INCLUDE_DIRS})
        list(APPEND APP_PLATFORM_LIB ${LIBWEBP_LIBRARIES})
        list(APPEND APP_PLATFORM_OPTION -DUSE_WEBP)
        link_directories(${LIBWEBP_LIBRARY_DIRS})
    endif()
endif()

list(APPEND APP_PLATFORM_OPTION
    -DBUILD_PACKAGE_NAME=${PACKAGE_NAME}
    -DBUILD_VERSION_MAJOR=${VERSION_MAJOR}
    -DBUILD_VERSION_MINOR=${VERSION_MINOR}
    -DBUILD_VERSION_REVISION=${VERSION_REVISION})

# NSP Switch
if (BUILTIN_NSP)
    list(APPEND APP_PLATFORM_LIB nsp)
    list(APPEND APP_PLATFORM_OPTION -DBUILTIN_NSP)
    add_subdirectory(scripts/switch-forwarder)
endif()

# libromfs option
if (USE_LIBROMFS)
    add_libromfs(${PROJECT_NAME} ${PROJECT_RESOURCES})
endif()

# --------------------------------------------------------------------------- #
#  Ressources                                                                  #
# --------------------------------------------------------------------------- #
if (CUSTOM_RESOURCES_DIR)
    set(BRLS_RESOURCES_DIR ${CUSTOM_RESOURCES_DIR})
elseif (INSTALL)
    set(BRLS_RESOURCES_DIR ${CMAKE_INSTALL_PREFIX}/share/tsvitch)
else()
    set(BRLS_RESOURCES_DIR ".")
endif()

# --------------------------------------------------------------------------- #
#  Sources                                                                    #
# --------------------------------------------------------------------------- #
file(GLOB_RECURSE MAIN_SRC tsvitch/source/*.cpp)       # ← prend aussi core/*.cpp

if (WIN32)
    configure_file("${CMAKE_SOURCE_DIR}/tsvitch/app_win32.rc.in"
                   "${CMAKE_BINARY_DIR}/app_win32.rc")
    list(APPEND MAIN_SRC ${CMAKE_BINARY_DIR}/app_win32.rc)
elseif (PLATFORM_SWITCH)
    list(APPEND MAIN_SRC ${BOREALIS_LIBRARY}/lib/platforms/switch/switch_wrapper.c)
elseif (PLATFORM_PS4)
    list(APPEND MAIN_SRC ${BOREALIS_LIBRARY}/lib/platforms/ps4/crashlog.c)
endif()

# --------------------------------------------------------------------------- #
#  Sous-dossiers tiers                                                        #
# --------------------------------------------------------------------------- #
add_subdirectory(library)

# --------------------------------------------------------------------------- #
#  Cible principale                                                            #
# --------------------------------------------------------------------------- #
program_target(${PROJECT_NAME} "${MAIN_SRC}")

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 17
    UNITY_BUILD  ${BRLS_UNITY_BUILD})

set_source_files_properties(
    tsvitch/source/utils/config_helper.cpp
    tsvitch/source/activity/setting_activity.cpp
    tsvitch/source/activity/dlna_activity.cpp
    PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)

# --------------------------------------------------------------------------- #
#  Post-build desktop / bundle / switch etc. (inchangés)                      #
# --------------------------------------------------------------------------- #
# …  (tout le bloc d’origine « building release file » reste identique)  …

# --------------------------------------------------------------------------- #
#  Include / compile / link                                                   #
# --------------------------------------------------------------------------- #
target_include_directories(${PROJECT_NAME} PRIVATE
    tsvitch/include
    tsvitch/include/api
    ${APP_PLATFORM_INCLUDE})

target_compile_options(${PROJECT_NAME} PRIVATE
    -ffunction-sections -fdata-sections -Wunused-variable
    ${APP_PLATFORM_OPTION})

target_link_libraries(${PROJECT_NAME} PRIVATE
    wiliwiliLibExtra
    borealis
    lunasvg
    pystring
    z
    nlohmann_json::nlohmann_json          # ← NOUVEAU
    ${APP_PLATFORM_LIB})

target_link_options(${PROJECT_NAME} PRIVATE ${APP_PLATFORM_LINK_OPTION})
