cmake_minimum_required(VERSION 3.16)
project(neptr_video CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# The Pi is fill-rate bound, not CPU bound, but the per-frame CPU work (uniform
# upload, chain walk) sits on the critical path to the page flip. -O2 always.
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

# ---------------------------------------------------------------------------
# Sources shared by every platform. Everything above the platform seam.
# ---------------------------------------------------------------------------
set(NV_CORE_SOURCES
  src/gfx/shader.cpp
  src/gfx/fbo.cpp
  src/gfx/quad.cpp
  src/core/kernels.cpp
  src/chain/effects.cpp
  src/chain/compiler.cpp
  src/chain/ops.cpp
  src/io/profile.cpp
  src/palette/palette.cpp
  src/layers/particles.cpp
  src/layers/ink.cpp
  src/ui/font.cpp
  src/ui/panel.cpp
  src/home/home.cpp
  src/home/nav.cpp
  src/home/launcher.cpp
  src/control/osc.cpp
  src/control/mapping.cpp
  src/control/receiver.cpp
  src/renderer.cpp
  src/governor.cpp
  src/state.cpp
  src/mod.cpp
  src/patch.cpp
  src/preset.cpp
  src/async_compiler.cpp
  src/shader_source.cpp
)

# ---------------------------------------------------------------------------
# Platform backend selection
# ---------------------------------------------------------------------------
if(APPLE)
  # Development target. GLFW is vendored rather than installed so the build
  # never mutates the host system and stays reproducible.
  include(FetchContent)
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(glfw)

  set(NV_PLATFORM_SOURCES src/platform/platform_glfw.cpp)
  set(NV_PLATFORM_LIBS glfw "-framework OpenGL")
  set(NV_PLATFORM_DEFS NV_PLATFORM_GLFW=1 GL_SILENCE_DEPRECATION=1)
else()
  # Pi / any KMS machine. Direct scanout: libdrm + GBM + EGL + GLESv2.
  # No compositor in the path, we own the page flip.
  find_path(DRM_INCLUDE_DIR drm.h PATH_SUFFIXES libdrm drm)
  find_library(DRM_LIB drm)
  find_library(GBM_LIB gbm)
  find_library(EGL_LIB EGL)
  find_library(GLES_LIB GLESv2)

  if(NOT DRM_LIB OR NOT GBM_LIB OR NOT EGL_LIB OR NOT GLES_LIB)
    message(FATAL_ERROR
      "KMS backend needs libdrm, gbm, EGL and GLESv2.\n"
      "  sudo apt install libdrm-dev libgbm-dev libegl1-mesa-dev libgles2-mesa-dev")
  endif()

  set(NV_PLATFORM_SOURCES src/platform/platform_drm.cpp
                          src/platform/evdev_touch.cpp)
  set(NV_PLATFORM_LIBS ${DRM_LIB} ${GBM_LIB} ${EGL_LIB} ${GLES_LIB})
  set(NV_PLATFORM_DEFS NV_PLATFORM_DRM=1)
  include_directories(${DRM_INCLUDE_DIR})
endif()

# ---------------------------------------------------------------------------
add_executable(neptr-video src/main.cpp ${NV_CORE_SOURCES} ${NV_PLATFORM_SOURCES})
target_include_directories(neptr-video PRIVATE src)
target_compile_definitions(neptr-video PRIVATE ${NV_PLATFORM_DEFS})
target_link_libraries(neptr-video PRIVATE ${NV_PLATFORM_LIBS})

# ---------------------------------------------------------------------------
# Tests. The chain compiler's pass-fusion grouping is pure logic with no GL
# dependency, which is exactly why it is the piece worth testing hardest.
# ---------------------------------------------------------------------------
enable_testing()
add_executable(test_compiler
  tests/test_compiler.cpp
  src/chain/compiler.cpp
  src/chain/effects.cpp
  src/shader_source.cpp)
target_include_directories(test_compiler PRIVATE src tests)
target_compile_definitions(test_compiler PRIVATE NV_NO_GL=1)
add_test(NAME compiler COMMAND test_compiler)

add_executable(test_palette
  tests/test_palette.cpp
  src/palette/palette.cpp)
target_include_directories(test_palette PRIVATE src tests)
target_compile_definitions(test_palette PRIVATE NV_NO_GL=1)
add_test(NAME palette COMMAND test_palette)

# The parameter bank and the patch rebuild are the other piece of pure logic
# that everything else depends on being right.
add_executable(test_patch
  tests/test_patch.cpp
  src/patch.cpp
  src/state.cpp
  src/mod.cpp
  src/control/mapping.cpp
  src/core/kernels.cpp
  src/chain/effects.cpp
  src/chain/ops.cpp)
target_include_directories(test_patch PRIVATE src tests)
target_compile_definitions(test_patch PRIVATE NV_NO_GL=1)
add_test(NAME patch COMMAND test_patch)

# OSC parsing runs on bytes off a socket; the mapping table is what every
# external control passes through. Both fail silently when wrong.
add_executable(test_control
  tests/test_control.cpp
  src/control/osc.cpp
  src/control/mapping.cpp
  src/control/receiver.cpp
  src/state.cpp
  src/mod.cpp
  src/chain/ops.cpp)
target_include_directories(test_control PRIVATE src tests)
target_compile_definitions(test_control PRIVATE NV_NO_GL=1)
add_test(NAME control COMMAND test_control)

# A preset's whole value is that it still loads later, on the other machine.
add_executable(test_preset
  tests/test_preset.cpp
  src/preset.cpp
  src/patch.cpp
  src/state.cpp
  src/mod.cpp
  src/control/mapping.cpp
  src/core/kernels.cpp
  src/chain/effects.cpp
  src/palette/palette.cpp
  src/chain/ops.cpp)
target_include_directories(test_preset PRIVATE src tests)
target_compile_definitions(test_preset PRIVATE NV_NO_GL=1)
add_test(NAME preset COMMAND test_preset)

# The governor's failure mode is silent and permanent: a wrong threshold
# degrades every frame forever and the engine still appears to work.
# HOME's three moving parts all fail SILENTLY: a menu that moves two rows, a
# joystick that walks the cursor as it springs back, a hold that fires twice.
# None of them throws, and none can be stepped through on the device -- the
# display is held by the process being debugged.
add_executable(test_home
  tests/test_home.cpp
  src/home/home.cpp
  src/home/nav.cpp)
target_include_directories(test_home PRIVATE src tests)
target_compile_definitions(test_home PRIVATE NV_NO_GL=1)
add_test(NAME home COMMAND test_home)

# The picture is rotated in a shader and the touch input in a Linux-only evdev
# reader. If those two ever disagree, taps land somewhere else and it reads as
# broken input rather than a sideways screen -- this is the only place the pair
# can be checked against each other.
add_executable(test_rotate tests/test_rotate.cpp)
target_include_directories(test_rotate PRIVATE src tests)
target_compile_definitions(test_rotate PRIVATE NV_NO_GL=1)
add_test(NAME rotate COMMAND test_rotate)

# What a particle dies of is a hazard model, and a hazard model gets its
# framerate dependence wrong silently -- the swarm just "feels different on
# the other machine". The half-life is the whole contract, so it is checked by
# simulation at three framerates rather than by inspecting a formula.
add_executable(test_lifecycle tests/test_lifecycle.cpp
  src/state.cpp
  src/mod.cpp
  src/control/mapping.cpp
  src/chain/ops.cpp)
target_include_directories(test_lifecycle PRIVATE src tests)
target_compile_definitions(test_lifecycle PRIVATE NV_NO_GL=1)
add_test(NAME lifecycle COMMAND test_lifecycle)

# Slew, LFOs and macros. All three fail in ways that read as "feel" rather
# than as faults: a frame-rate-dependent glide is fine on the machine you
# wrote it on, a triangle that is really a sawtooth is "a bit odd", and a macro
# that fires every frame silently becomes the only control on the instrument.
# None of it touches GL, a socket or a clock, so all of it is simulated here.
add_executable(test_mod
  tests/test_mod.cpp
  src/mod.cpp
  src/state.cpp
  src/preset.cpp
  src/control/mapping.cpp
  src/core/kernels.cpp
  src/chain/effects.cpp
  src/palette/palette.cpp
  src/chain/ops.cpp)
target_include_directories(test_mod PRIVATE src tests)
target_compile_definitions(test_mod PRIVATE NV_NO_GL=1)
add_test(NAME mod COMMAND test_mod)

# SPEED's curve and the motion clock. Both are silent failures: an inexact
# inverse makes every preset land a hair off where it was saved, and a motion
# phase that is not exactly periodic in its wrap makes the picture jump once
# every few minutes -- unreproducible by hand, and the exact complaint that
# started this work.
add_executable(test_motion
  tests/test_motion.cpp
  src/state.cpp
  src/mod.cpp
  src/control/mapping.cpp
  src/chain/ops.cpp)
target_include_directories(test_motion PRIVATE src tests)
target_compile_definitions(test_motion PRIVATE NV_NO_GL=1)
add_test(NAME motion COMMAND test_motion)

add_executable(test_journey tests/test_journey.cpp)
target_include_directories(test_journey PRIVATE src tests)
target_compile_definitions(test_journey PRIVATE NV_NO_GL=1)
add_test(NAME journey COMMAND test_journey)

add_executable(test_focus tests/test_focus.cpp)
target_include_directories(test_focus PRIVATE src tests)
target_compile_definitions(test_focus PRIVATE NV_NO_GL=1)
add_test(NAME focus COMMAND test_focus)

add_executable(test_governor tests/test_governor.cpp src/governor.cpp)
target_include_directories(test_governor PRIVATE src tests)
target_compile_definitions(test_governor PRIVATE NV_NO_GL=1)
add_test(NAME governor COMMAND test_governor)

# GAMES mode is a shell script, so its suite is one too -- the logic worth
# pinning there is "may this mode start at all", and the failure it guards
# against is the script never returning with the display already handed away.
# Nothing to compile, nothing device-specific: it builds a fixture tree in a
# temp dir and checks the refusal codes.
add_test(NAME mode_games
         COMMAND /bin/sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_mode_games.sh)

# GAMES gets its controls off the DEMIURGE bus, so a virtual button on the
# phone plays a game exactly as a footswitch does. Everything between the
# datagram and the frame the arcade consumes is pure Python and belongs under
# ctest with the rest -- the alternative is that it is only ever exercised on
# the device, with the display handed away and nothing to read.
#
# What this can NOT cover, and must not appear to: the pad itself is a Linux
# uinput device and the supervisor drives RetroArch. The seam is the frame
# dict, and the frame dict is what this suite checks -- against the arcade's
# real parse_frame() when DEMIURGE_OS is beside this tree (NV_GAMES_DIR points
# it elsewhere), so the contract is verified against the actual consumer.
find_program(NV_PYTHON3 python3)
if(NV_PYTHON3)
  add_test(NAME control_bus
           COMMAND ${NV_PYTHON3} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_control_bus.py)

  # "Does every generator actually move" and "is reverse really reverse" are
  # claims about PIXELS, and this project has been burned before by an answer
  # read off the source. Both suites drive the real binary, capture real
  # frames and compare them; neither can pass by inspection.
  #
  # They need a GL context, so they are opt-in on a headless box: set
  # NV_HEADLESS=1 in the environment (or -DNV_SKIP_GL_TESTS=ON) to leave them
  # out rather than watch them fail for the wrong reason.
  if(NOT NV_SKIP_GL_TESTS)
    add_test(NAME motion_bank
             COMMAND ${NV_PYTHON3} ${CMAKE_CURRENT_SOURCE_DIR}/tests/motion_probe.py
                     --build ${CMAKE_CURRENT_BINARY_DIR})
    add_test(NAME motion_time
             COMMAND ${NV_PYTHON3} ${CMAKE_CURRENT_SOURCE_DIR}/tests/motion_proofs.py
                     --build ${CMAKE_CURRENT_BINARY_DIR})
    # Does the picture ADVANCE, or only move? motion_bank proves nothing is
    # static, which the original closed motion path passed while going
    # nowhere. This one measures recurrence, and carries its own control.
    # --span 600, not the probe's default 2400.
    #
    # On the DRM backend every shot is a full-panel PPM (NV_GEOM is ignored)
    # and the probe's wall-clock cap cuts the run at ~frame 859, so a
    # 2400-frame span captured 35 of 96 samples -- 36% coverage against the
    # 80% the probe now demands. That gate is right and stays: recurrence is a
    # MINIMUM over earlier frames, so missing frames can only flatter the
    # result. But as registered it made ctest permanently red on the Pi for an
    # invocation mismatch rather than a defect. A 600-frame span fits inside
    # the cap on both backends and still covers several dives.
    add_test(NAME journey_advance
             COMMAND ${NV_PYTHON3} ${CMAKE_CURRENT_SOURCE_DIR}/tests/journey_probe.py
                     --build ${CMAKE_CURRENT_BINARY_DIR} --span 600)
    # The bank probe starts thirty processes, each compiling a shader.
    add_test(NAME list_bus
             COMMAND ${NV_PYTHON3} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_list_bus.py
                     $<TARGET_FILE:neptr-video>)
    set_tests_properties(motion_bank PROPERTIES TIMEOUT 900)
    set_tests_properties(journey_advance PROPERTIES TIMEOUT 300)
    set_tests_properties(motion_time PROPERTIES TIMEOUT 900)
  endif()
else()
  message(WARNING "python3 not found -- the control_bus suite will not run")
endif()

add_executable(test_ops tests/test_ops.cpp src/chain/ops.cpp)
target_include_directories(test_ops PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_test(NAME ops COMMAND test_ops)

add_executable(test_io tests/test_io.cpp src/io/profile.cpp)
target_include_directories(test_io PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src tests)
add_test(NAME io COMMAND test_io)
