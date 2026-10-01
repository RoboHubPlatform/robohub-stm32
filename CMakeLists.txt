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

# Where those headers are is robohub-common's to say, and it says so in its own
# manifest. `deps.cmake` is written from those declarations by rho-target, so a
# build that consumed this project as a dependency already has the answer and
# nothing here has to guess at a directory layout.
#
# A person building this module on its own has no such file, and passes the path:
# that is a deliberate override of what the dependency declares, and it wins.
if(NOT ROBOHUB_COMMON_INCLUDE_DIR AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps.cmake")
    include("${CMAKE_CURRENT_SOURCE_DIR}/deps.cmake")
endif()

if(NOT ROBOHUB_COMMON_INCLUDE_DIR)
    message(FATAL_ERROR
        "robohub-common's headers were not found.\n"
        "  - build this project as a Rho dependency, so rho-target writes deps.cmake from\n"
        "    the manifests, or\n"
        "  - pass -DROBOHUB_COMMON_INCLUDE_DIR=<path>.")
endif()

message(STATUS "robohub-common headers: ${ROBOHUB_COMMON_INCLUDE_DIR}")

# ---------------------------------------------------------------------------
#  Sources
# ---------------------------------------------------------------------------
#
# The chip-independent half: error mapping and clock tree setup. These compile
# for any STM32 and are what the rest of the module is written against.

# Every source of this module. It is a library, and a library that compiles one
# file of itself is a library whose other files are somebody else's problem: the
# consumer links it, the symbols are missing, and the report names them rather
# than the list that omitted them.
set(ROBOHUB_STM32_CORE_SOURCES
    src/platform/stm32_error.c
    src/platform/stm32_console.c
    src/platform/stm32_fault.c
    src/platform/stm32_systick.c
    src/platform/stm32_platform.c
    src/hal/stm32_gpio.c
    src/hal/stm32_i2c.c
    src/hal/stm32_rs485.c
    src/hal/stm32_spi.c
    src/hal/stm32_timer.c
    src/hal/stm32_uart.c
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

# The interfaces this module implements live in robohub-common, so this library
# cannot be linked without it. PUBLIC, not PRIVATE: a consumer of this one needs
# the interfaces too, and a consumer that had to know that is a contract every
# project would have to be told separately. Naming it once here is what makes the
# chain compose on its own.
#
# The target is the namespaced one because that is what this module's manifest
# exports; `deps.cmake` bridges it to the target robohub-common's own build
# defines.
target_link_libraries(robohub-stm32 PUBLIC rho::robohub-common)

# This module's own compile definitions, as its manifest declares them.
#
# The driver sources test which family they are building for, and a test with no
# macro is a test that is false — so a driver that supports both the current and
# the older vendor naming compiles the older one, and fails on a constant the
# pinned HAL does not have.
#
# PUBLIC rather than PRIVATE: a consumer compiles against these headers too, and
# the vendor's umbrella header reads the same macros. A macro that reached only
# this library would make the two disagree about what is enabled.
target_compile_definitions(robohub-stm32 PUBLIC
    USE_HAL_DRIVER
    HAL_MODULE_ENABLED
    HAL_GPIO_MODULE_ENABLED
    HAL_RCC_MODULE_ENABLED
    HAL_CORTEX_MODULE_ENABLED
    HAL_UART_MODULE_ENABLED
    HAL_DMA_MODULE_ENABLED
    HAL_FLASH_MODULE_ENABLED
    HAL_PWR_MODULE_ENABLED
)

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

# These run on the host, so they are only registered for a hosted build. A
# bare-metal build links a C library that has no `_kill` or `_getpid` for a host
# test to resolve against, and registering one there failed the build at the link
# step naming a symbol from a libc that does not exist on the part — a symptom
# that said nothing about the decision that caused it.
#
# The flags come from rho-target, which read the `testing:` section of this
# module's manifest. Building this project on its own with plain cmake defines
# neither, and the tests are then not registered: a test is something a build is
# told to run, not something every build inherits.
if(RHO_BUILD_HOST_TESTS AND NOT RHO_TARGET_IS_BAREMETAL)
    message(STATUS "[robohub-stm32] registering host unit tests")
    enable_testing()
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
