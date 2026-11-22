cmake_minimum_required(VERSION 3.18)
project(csound-nam CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# NeuralAmpModelerCore source tree
# ---------------------------------------------------------------------------
set(NAM_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/NeuralAmpModelerCore"
    CACHE PATH "Path to NeuralAmpModelerCore source tree")

if(NOT EXISTS "${NAM_CORE_DIR}/NAM/dsp.h")
    message(FATAL_ERROR
        "NeuralAmpModelerCore not found at ${NAM_CORE_DIR}.\n"
        "Run: cd libs && git clone https://github.com/sdatkinson/NeuralAmpModelerCore.git\n"
        "Then: cd NeuralAmpModelerCore && git submodule update --init Dependencies/eigen Dependencies/AudioDSPTools")
endif()

# Glob NAM source files (same as tools/CMakeLists.txt does)
file(GLOB NAM_SOURCES_TOP "${NAM_CORE_DIR}/NAM/*.cpp")
file(GLOB NAM_SOURCES_SUB "${NAM_CORE_DIR}/NAM/*/*.cpp")
set(NAM_SOURCES ${NAM_SOURCES_TOP} ${NAM_SOURCES_SUB})

set(NAM_DEPS_PATH "${NAM_CORE_DIR}/Dependencies")

if(NOT EXISTS "${NAM_DEPS_PATH}/eigen/Eigen/Dense")
    message(FATAL_ERROR
        "Eigen submodule missing.\n"
        "Run inside libs/NeuralAmpModelerCore:\n"
        "  git submodule update --init Dependencies/eigen")
endif()

# ---------------------------------------------------------------------------
# CSound headers
# ---------------------------------------------------------------------------
# Standard install (Linux apt / homebrew): headers live under a csound/ subdir.
find_path(CSOUND_INCLUDE_DIR
    NAMES csound/csdl.h
    PATHS
        /usr/local/include
        /usr/include
        /opt/homebrew/include
        /opt/homebrew/opt/csound/include
    DOC "CSound include directory")

# macOS Framework install: headers live flat inside the framework Headers dir.
if(NOT CSOUND_INCLUDE_DIR)
    set(_CS_FW_HEADERS
        "/Library/Frameworks/CsoundLib64.framework/Versions/6.0/Headers")
    if(EXISTS "${_CS_FW_HEADERS}/csdl.h")
        set(CSOUND_INCLUDE_DIR "${_CS_FW_HEADERS}"
            CACHE PATH "CSound include directory" FORCE)
        message(STATUS "CSound found via macOS Framework: ${_CS_FW_HEADERS}")
        # plugin.h in CSound 6.18 has a typo (opaddr → opadr) that breaks
        # compilation with AppleClang 17+.  Force csdl.h path by poisoning
        # the plugin.h include so the fallback in csound_nam_opcode.cpp fires.
        add_compile_definitions(CSOUND_USE_CSDL_H=1)
    endif()
endif()

if(NOT CSOUND_INCLUDE_DIR)
    message(FATAL_ERROR
        "CSound headers not found.\n"
        "Pi:  sudo apt install libcsound64-dev\n"
        "Mac: reinstall CsoundLib64 package from csound.com")
endif()
message(STATUS "CSound headers: ${CSOUND_INCLUDE_DIR}")

# ---------------------------------------------------------------------------
# Plugin target — compile NAM sources + our opcode directly into the .so
# ---------------------------------------------------------------------------
add_library(csound-nam SHARED
    src/csound_nam_opcode.cpp
    src/nam_loader.cpp
    ${NAM_SOURCES})

target_include_directories(csound-nam PRIVATE
    ${CSOUND_INCLUDE_DIR}
    ${NAM_CORE_DIR}
    "${NAM_DEPS_PATH}/eigen"
    "${NAM_DEPS_PATH}/nlohmann"
    src)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(csound-nam PRIVATE pthread)
endif()

# macOS: use libc++ to match what NeuralAmpModelerCore expects on Darwin
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_compile_options(csound-nam PRIVATE -stdlib=libc++)
    target_link_options(csound-nam PRIVATE -stdlib=libc++)
endif()

# A2 fast-path WaveNet opt-in (mirrors NeuralAmpModelerCore default)
target_compile_definitions(csound-nam PRIVATE NAM_ENABLE_A2_FAST)

# Don't link against libcsound — opcodes are loaded at runtime as plugins
set_target_properties(csound-nam PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    PREFIX ""
    SUFFIX ".so"
    POSITION_INDEPENDENT_CODE ON)
message(STATUS "CXX_STANDARD forced to 20 on target")

# ---------------------------------------------------------------------------
# Architecture-specific optimisation
# ---------------------------------------------------------------------------
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    target_compile_options(csound-nam PRIVATE -O3 -march=armv8-a+simd)
    message(STATUS "ARM64 detected — enabling NEON SIMD")
else()
    target_compile_options(csound-nam PRIVATE -O3)
endif()

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
# CSound only auto-loads plugins from its packaged dir (on Debian/RPi:
# /usr/lib/<triplet>/csound/plugins64-6.0). Installing anywhere else means
# the .so silently never loads — detect the real dir, fall back to
# /usr/local only if none exists (e.g. macOS dev builds).
file(GLOB _CSOUND_SYS_PLUGIN_DIRS "/usr/lib/*/csound/plugins64-*")
if(_CSOUND_SYS_PLUGIN_DIRS)
    list(GET _CSOUND_SYS_PLUGIN_DIRS 0 _CSOUND_DETECTED_DIR)
else()
    set(_CSOUND_DETECTED_DIR "/usr/local/lib/csound/plugins64")
endif()
set(CSOUND_PLUGINS_DIR "${_CSOUND_DETECTED_DIR}"
    CACHE PATH "CSound plugin install directory")

install(TARGETS csound-nam DESTINATION "${CSOUND_PLUGINS_DIR}")
message(STATUS "Plugin will install to: ${CSOUND_PLUGINS_DIR}")
