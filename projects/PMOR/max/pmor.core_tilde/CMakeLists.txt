cmake_minimum_required(VERSION 3.0)

set(C74_MIN_API_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../min-api)
include(${C74_MIN_API_DIR}/script/min-pretarget.cmake)

# PMOR core (hardware-free). Same sources the firmware and the host tests use.
set(PMOR_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../../PMOR_V3)
file(GLOB PMOR_CORE_SRC "${PMOR_ROOT}/pmor_core/*.cpp")
list(APPEND PMOR_CORE_SRC "${PMOR_ROOT}/pmor_core/host/platform_host.cpp")

include_directories(
	"${C74_INCLUDES}"
	"${PMOR_ROOT}"
	"${PMOR_ROOT}/pmor_core"
	"${PMOR_ROOT}/pmor_core/third_party"
	"${PMOR_ROOT}/max"
)

add_library(
	${PROJECT_NAME}
	MODULE
	${PROJECT_NAME}.cpp
	${PMOR_CORE_SRC}
)

# bit-exact with firmware/host tests: no contraction, no fast-math
target_compile_definitions(${PROJECT_NAME} PRIVATE PMOR_HOST=1)
target_compile_options(${PROJECT_NAME} PRIVATE -ffp-contract=off -fno-fast-math)

include(${C74_MIN_API_DIR}/script/min-posttarget.cmake)
