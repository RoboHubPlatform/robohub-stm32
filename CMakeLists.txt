#
# robohub-stm32 — the STM32 platform for the RoboHub Platform.
#
# This builds the implementations of the RoboHub HAL interfaces against the
# STM32 HAL and CMSIS. It is a static library: a board build links it, and the
# linker discards the parts of it a project does not use.
#
# The HAL sources themselves are not compiled here. They are compiled against a
# chip, and which chip is a property of the board being built, so a board
# support package under boards/ names the sources and the device define that
# belong to it. Building every driver for every chip would compile each one
# against a header set it does not belong to.
#
# @project RoboHub Platform
# @license MIT
#

cmake_minimum_required(VERSION 3.16)

project(robohub-stm32 LANGUAGES C VERSION 0.1.0)

include(CTest)

# ---------------------------------------------------------------------------
#  robohub-common
# ---------------------------------------------------------------------------
#
# The interfaces this platform implements are declared by robohub-common, so
# its headers are needed to compile anything here. A build supplies them either
# as an installed package or, in a checkout of the ecosystem, as a sibling
# directory. Which one is not this module's decision, so both are offered and
# the first that exists is used; a build with neither is told what is missing
# rather than failing on a hundred unknown types.

set(ROBOHUB_COMMON_INCLUDE_DIR "" CACHE PATH
    "Path to robohub-common's include directory")

if(NOT ROBOHUB_COMMON_INCLUDE_DIR)
    find_path(ROBOHUB_COMMON_INCLUDE_DIR
        NAMES error.h
        HINTS
            ${CMAKE_CURRENT_SOURCE_DIR}/../robohub-common/include
            ${CMAKE_CURRENT_SOURCE_DIR}/../../robohub-common/include
    )
endif()

if(NOT ROBOHUB_COMMON_INCLUDE_DIR)
    message(FATAL_ERROR
        "robohub-common's headers were not found. Pass -DROBOHUB_COMMON_INCLUDE_DIR=<path>, "
        "or check out robohub-common beside this module.")
endif()

message(STATUS "robohub-common headers: ${ROBOHUB_COMMON_INCLUDE_DIR}")

# ---------------------------------------------------------------------------
#  Sources
# ---------------------------------------------------------------------------
#
# The chip-independent half: error mapping and clock tree setup. These compile
# for any STM32 and are what the rest of the module is written against.

set(ROBOHUB_STM32_CORE_SOURCES
    src/platform/stm32_error.c
)

# ---------------------------------------------------------------------------
#  Configuration
# ---------------------------------------------------------------------------
#
# The values a project declared, resolved by rho-kconfig into
# generated/autoconf.h. The include is added whether or not the header exists
# yet, because a build that has not been configured should fail on the missing
# configuration rather than on a missing directory.

if(EXISTS ${CMAKE_BINARY_DIR}/generated/autoconf.h)
    add_compile_definitions(HAVE_ROBOHUB_AUTOCONF=1)
endif()

include_directories(${CMAKE_BINARY_DIR}/generated)

# ---------------------------------------------------------------------------
#  Library
# ---------------------------------------------------------------------------

add_library(robohub-stm32 STATIC ${ROBOHUB_STM32_CORE_SOURCES})

target_include_directories(robohub-stm32 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/robohub
    ${CMAKE_CURRENT_SOURCE_DIR}/include/robohub/hal
    ${CMAKE_CURRENT_SOURCE_DIR}/include/robohub/platform
    ${CMAKE_CURRENT_SOURCE_DIR}/include/robohub/drivers
    ${CMAKE_CURRENT_SOURCE_DIR}/platform
    ${ROBOHUB_COMMON_INCLUDE_DIR}
    ${CMAKE_BINARY_DIR}/generated
    $<INSTALL_INTERFACE:include>
)

target_compile_features(robohub-stm32 PUBLIC c_std_11)

# ---------------------------------------------------------------------------
#  Tests
# ---------------------------------------------------------------------------
#
# The chip-independent half is testable on the host, which is the only way these
# run without a board on the bench.

if(BUILD_TESTING)
    add_executable(robohub-stm32-test-error tests/test_error.c)
    target_link_libraries(robohub-stm32-test-error PRIVATE robohub-stm32)
    add_test(NAME robohub-stm32-test-error COMMAND robohub-stm32-test-error)
endif()

# ---------------------------------------------------------------------------
#  Install
# ---------------------------------------------------------------------------

install(FILES module.yml DESTINATION share/robohub-stm32)
install(DIRECTORY include/ DESTINATION include)
install(TARGETS robohub-stm32
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)
