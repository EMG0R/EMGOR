# neptr_engine: pmor_core (verbatim, from ../pmor_core) rendered natively on aarch64 as `pmor-engine`.
# Standalone project: reads the repo, writes only under its own build dir. See README in check.sh header.
cmake_minimum_required(VERSION 3.16)
project(neptr_engine C CXX)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
# identical to the repo's host build: bit-exact, no FMA contraction, no fast-math
set(CMAKE_CXX_FLAGS_RELEASE "-O2")
add_compile_options(-ffp-contract=off -fno-fast-math -Wall)

get_filename_component(REPO ${CMAKE_CURRENT_SOURCE_DIR}/.. ABSOLUTE)
file(GLOB CORE_SRC "${REPO}/pmor_core/*.cpp")
add_library(pmor_core STATIC ${CORE_SRC} ${REPO}/pmor_core/host/platform_host.cpp)
target_include_directories(pmor_core PUBLIC ${REPO}/pmor_core ${REPO} ${REPO}/pmor_core/third_party)
target_compile_definitions(pmor_core PUBLIC PMOR_HOST=1)
set(V ${CMAKE_CURRENT_SOURCE_DIR}/vendor)
set(SRC ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(TST ${CMAKE_CURRENT_SOURCE_DIR}/tests)

find_package(Threads REQUIRED)

# ---- the engine (offline render + CDC host; JACK-less)
add_executable(pmor-engine src/pmor_engine.cpp)
target_include_directories(pmor-engine PRIVATE ${SRC} ${V}/max)
target_link_libraries(pmor-engine pmor_core Threads::Threads)

# ---- simulated PMOR on a pty (the repo's firmware proto core + engines behind a pseudo-terminal)
add_executable(pmor_sim_pty tests/pmor_sim_pty.cpp)
target_include_directories(pmor_sim_pty PRIVATE ${TST})
target_link_libraries(pmor_sim_pty pmor_core util)

add_executable(test_engine_render tests/test_engine_render.cpp)
target_include_directories(test_engine_render PRIVATE ${SRC} ${REPO}/tests ${V}/max)
target_link_libraries(test_engine_render pmor_core)
add_executable(f32stat ${REPO}/tests/f32stat.cpp)
add_library(scenario STATIC ${REPO}/tests/scenario.cpp)
target_include_directories(scenario PUBLIC ${REPO}/tests)
target_link_libraries(scenario pmor_core)
target_link_libraries(pmor-engine scenario)
target_link_libraries(test_engine_render scenario)

add_executable(test_cdc tests/test_cdc.cpp)
target_include_directories(test_cdc PRIVATE ${SRC} ${V}/max)
target_link_libraries(test_cdc pmor_core Threads::Threads)

# ---- vendored Max-side tests (macbert/max sha in vendor/SOURCE.txt), built against THIS pmor_core
foreach(t test_hostclient test_notepad test_cache test_slotload test_compat test_recycle test_savepreset test_boottempo)
  add_executable(${t} ${V}/tests/${t}.cpp)
  target_include_directories(${t} PRIVATE ${V}/tests)
  target_link_libraries(${t} pmor_core)
endforeach()

# ---- JACK / ALSA-seq / OSC skeleton: compiled only on request, never linked into pmor-engine, never run by check.sh
option(NEPTR_ENGINE_JACK "compile the JACK client skeleton (syntax/link check only)" OFF)
if(NEPTR_ENGINE_JACK)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(JACK REQUIRED jack)
  pkg_check_modules(ALSA REQUIRED alsa)
  pkg_check_modules(LO REQUIRED liblo)
  add_executable(pmor-engine-jack src/pmor_engine_jack.cpp)
  target_include_directories(pmor-engine-jack PRIVATE ${SRC} ${V}/max ${JACK_INCLUDE_DIRS} ${ALSA_INCLUDE_DIRS} ${LO_INCLUDE_DIRS})
  target_link_libraries(pmor-engine-jack pmor_core Threads::Threads ${JACK_LIBRARIES} ${ALSA_LIBRARIES} ${LO_LIBRARIES})
endif()
