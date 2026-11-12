cmake_minimum_required(VERSION 3.0)

set(C74_MIN_API_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../min-api)
include(${C74_MIN_API_DIR}/script/min-pretarget.cmake)

set(PMOR_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../../PMOR_V3)

include_directories(
	"${C74_INCLUDES}"
	"${PMOR_ROOT}/pmor_core"
	"${PMOR_ROOT}/max"
)

add_library(
	${PROJECT_NAME}
	MODULE
	${PROJECT_NAME}.cpp
	${PMOR_ROOT}/pmor_core/proto_util.cpp
)

include(${C74_MIN_API_DIR}/script/min-posttarget.cmake)

if (APPLE)
	target_link_libraries(${PROJECT_NAME} PUBLIC "-framework IOKit" "-framework CoreFoundation")
endif ()
