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
    # A sibling checkout is the ordinary arrangement: the interfaces and the
    # platform that implements them are two repositories, versioned apart, and in
    # a working checkout of the ecosystem they sit beside each other. Offering it
    # here means building this module on its own needs no flags at all, which is
    # the difference between a build a person can run and one they have to be told
    # how to run.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../robohub-common/include")
        set(ROBOHUB_COMMON_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../robohub-common/include")
        message(STATUS "robohub-common: using the sibling checkout")
    endif()
endif()

if(NOT ROBOHUB_COMMON_INCLUDE_DIR)
    message(FATAL_ERROR
        "robohub-common's headers were not found.\n"
        "  - check out robohub-common beside this repository, or\n"
        "  - build this project as a Rho dependency, so rho-target writes deps.cmake from\n"
        "    the manifests, or\n"
        "  - pass -DROBOHUB_COMMON_INCLUDE_DIR=<path>.")
endif()

message(STATUS "robohub-common headers: ${ROBOHUB_COMMON_INCLUDE_DIR}")

# Where those headers came from is a repository, and the repository is what this
# library links against.
#
# `deps.cmake`, written by rho-target from the manifests, defines the
# `rho::robohub-common` target and this is not needed. A person building this
# module on its own passes the include directory instead, and that satisfies the
# check above without defining anything — so the build got as far as
# `target_link_libraries` and stopped with a message about a target that could not
# be found, which said nothing about the thing the reader had to do.
#
# So the headers are turned back into the repository they came from, and its build
# is run. robohub-common is a library with sources of its own, so there is nothing
# to link against until it is built; the include path alone was never going to be
# enough, and the header check passing is what hid that.
get_filename_component(ROBOHUB_COMMON_ROOT "${ROBOHUB_COMMON_INCLUDE_DIR}" DIRECTORY)
# Only for a build that did not come through the dependency resolver. One that did
# has already had robohub-common added by the generated project — after this file
# is read, which is why `TARGET robohub-common` cannot answer this — and adding it
# again fails on a name already taken.
# Only when this module is the project being built.
#
# In a build that came through the resolver, this file is read as a subdirectory of
# a generated project that adds robohub-common itself — after this one, which is
# why `TARGET robohub-common` cannot answer the question. The generated project's
# CMakeLists lives one directory up, and there is no variable set before this file
# runs that says so. What does say so is this being the top-level project, which is
# exactly the case this fallback is for: somebody building this module on its own.
#
# Guessing at anything else — whether deps.cmake exists in a directory this file
# cannot see — would have this module add a dependency a generated project is
# already going to add, and CMake stops on the name being taken.
if(PROJECT_IS_TOP_LEVEL
   AND NOT TARGET robohub-common AND NOT TARGET rho::robohub-common)
    if(EXISTS "${ROBOHUB_COMMON_ROOT}/CMakeLists.txt")
        message(STATUS "[robohub-stm32] building robohub-common from ${ROBOHUB_COMMON_ROOT}")
        add_subdirectory("${ROBOHUB_COMMON_ROOT}" "${CMAKE_BINARY_DIR}/_robohub-common" EXCLUDE_FROM_ALL)
    else()
        message(FATAL_ERROR
            "robohub-common's headers were found at ${ROBOHUB_COMMON_INCLUDE_DIR}, but\n"
            "  there is no build to link against beside them.\n"
            "  - point ROBOHUB_COMMON_INCLUDE_DIR at the `include` directory of a\n"
            "    robohub-common checkout, or\n"
            "  - build this project as a Rho dependency, so rho-target writes deps.cmake\n"
            "    with the target in it.")
    endif()
endif()
# The alias is what this module links by, and `deps-aliases.cmake` writes it after
# every dependency has been added — which is after this file is read. So in a Rho
# build the alias does not exist yet at this point, and asking about it alone would
# have this module add robohub-common a second time and fail on a name already
# taken. The alias is therefore made here when the target is there to alias.
if(TARGET robohub-common AND NOT TARGET rho::robohub-common)
    add_library(rho::robohub-common ALIAS robohub-common)
endif()

# ---------------------------------------------------------------------------
#  Sources
# ---------------------------------------------------------------------------
#
# The chip-independent half: error mapping and clock tree setup. These compile
# for any STM32 and are what the rest of the module is written against.

# Whether this is a host build, decided once.
#
# A bare-metal build links a C library with no `_kill` or `_getpid` for a host test
# to resolve against, and registering one there failed the build at the link step
# naming a symbol from a libc that does not exist on the part — a symptom that said
# nothing about the decision that caused it. So the same two facts decide both
# whether this library is compiled for a part and whether there are any tests to
# register, and they are read in one place because a build that decided them
# differently in two places would compile a host library with a part's sources in
# it.
#
# The flags come from rho-target, which read the `testing:` section of this module's
# manifest. Building this project on its own with plain cmake defines neither, and
# then this is a bare-metal build — which is what it is: a library for a part, and
# the odd one out is somebody who asks for host tests explicitly.
set(ROBOHUB_STM32_HOSTED OFF)
if(PROJECT_IS_TOP_LEVEL AND RHO_BUILD_HOST_TESTS AND NOT RHO_TARGET_IS_BAREMETAL)
    set(ROBOHUB_STM32_HOSTED ON)
endif()

# Every source of this module. It is a library, and a library that compiles one
# file of itself is a library whose other files are somebody else's problem: the
# consumer links it, the symbols are missing, and the report names them rather
# than the list that omitted them.
# Which drivers this build gets is the project's configuration, not a list here.
#
# `guarded_sources` in this module's manifest says which symbol governs which file;
# `robo-config.cmake` says which of those symbols the project set. Listing the
# files unconditionally instead would mean a driver a program never asked for was
# compiled into its image — switched off, and present, which is the worst of
# both: the flash spent and nothing to show for it.
# The resolved configuration lives in the generated project, which is where the
# build that produced it put it. This directory is a subdirectory of that one, so
# the path is handed down rather than guessed from here.
set(ROBOHUB_STM32_CONFIG_KNOWN OFF)
if(RHO_ROBO_CONFIG AND EXISTS "${RHO_ROBO_CONFIG}")
    include("${RHO_ROBO_CONFIG}")
    set(ROBOHUB_STM32_CONFIG_KNOWN ON)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/robo-config.cmake")
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/robo-config.cmake")
    set(ROBOHUB_STM32_CONFIG_KNOWN ON)
endif()

# Always compiled: the platform itself — the clock, the fault handler, the
# console, and the tick every driver's delay is counted in. None of it is a
# peripheral a program chooses.
#
# Four of the five are bare metal, and one is not. The error mapping is the whole
# of this module that a host can run, because it is a function of the vendor's
# published status values rather than of a peripheral; the other four reach for
# `bsp.h` and the vendor's HAL, which exist only once somebody has said which
# board this is for. A host build therefore takes the mapping and leaves the rest,
# which is the same distinction the tests are built on.
set(ROBOHUB_STM32_CHIP_INDEPENDENT_SOURCES
    src/platform/stm32_error.c
)

set(ROBOHUB_STM32_BAREMETAL_SOURCES
    src/platform/stm32_console.c
    src/platform/stm32_fault.c
    src/platform/stm32_systick.c
    src/platform/stm32_platform.c
)

# And the driver for each symbol the project set. A symbol nobody set contributes
# nothing, so the image holds the drivers it asked for and no others.
#
# A hosted build stops at the chip-independent half. Not because the drivers are
# too much for a host — they are C, and would compile — but because they are
# written against a peripheral's registers through a HAL that only exists once a
# board has been named, so there is nothing to compile them against here.
set(ROBOHUB_STM32_CORE_SOURCES ${ROBOHUB_STM32_CHIP_INDEPENDENT_SOURCES})

if(NOT ROBOHUB_STM32_HOSTED)
    list(APPEND ROBOHUB_STM32_CORE_SOURCES ${ROBOHUB_STM32_BAREMETAL_SOURCES})

    foreach(driver
            "GPIO:src/hal/stm32_gpio.c"
            "I2C:src/hal/stm32_i2c.c"
            "RS485:src/hal/stm32_rs485.c"
            "SPI:src/hal/stm32_spi.c"
            "TIMER:src/hal/stm32_timer.c"
            "UART:src/hal/stm32_uart.c")
        string(REPLACE ":" ";" parts "${driver}")
        list(GET parts 0 symbol)
        list(GET parts 1 source)
        # With no configuration to consult, every driver is built: this module is
        # being compiled on its own rather than for a program, and the answer to
        # "which of these does that program want" has not been asked yet. Guessing
        # "none" would compile a module that cannot work.
        if(NOT ROBOHUB_STM32_CONFIG_KNOWN
           OR (DEFINED RHO_CONFIG_RH_HAL_${symbol} AND RHO_CONFIG_RH_HAL_${symbol}))
            list(APPEND ROBOHUB_STM32_CORE_SOURCES ${source})
        endif()
    endforeach()
endif()

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

# These run on the host, so they are registered only when this is a host build —
# the same decision that chose which sources the library was compiled from, and
# read from the same place. A test is something a build is told to run, not
# something every build inherits.
if(ROBOHUB_STM32_HOSTED)
    message(STATUS "[robohub-stm32] registering host unit tests")
    enable_testing()
    add_executable(robohub-stm32-test-error tests/test_error.c)
    target_link_libraries(robohub-stm32-test-error PRIVATE robohub-stm32)
    add_test(NAME robohub-stm32-test-error COMMAND robohub-stm32-test-error)

    # The GoogleTest suites, which are the ones `tests/tests.yml` declares.
    #
    # GoogleTest is built from source rather than found installed, for the reason
    # every other dependency here is: a test framework on a machine is whichever
    # version the machine happened to get, and a suite written against one of them
    # is a suite that only builds on some machines. The sources come from the
    # dependency declared in this module's manifest, so the revision is one line
    # somebody can read rather than a version somewhere in a package manager.
    # Where GoogleTest is, from whoever fetched it.
    #
    # rho-test-gtest fetches it and sets GTEST_ROOT, so a project does not fetch a
    # test framework itself and does not carry a path it did not write. The default
    # is where this module expects it, for somebody running the build by hand.
    if(DEFINED ENV{GTEST_ROOT})
        set(ROBOHUB_GOOGLETEST_DIR "$ENV{GTEST_ROOT}")
    endif()
    set(ROBOHUB_GOOGLETEST_DIR "${ROBOHUB_GOOGLETEST_DIR}"
        CACHE PATH "Path to the googletest checkout" FORCE)
    if(EXISTS "${ROBOHUB_GOOGLETEST_DIR}/CMakeLists.txt")
        # GoogleTest is C++, and this project is C — `project(... LANGUAGES C)`
        # because a driver is C and a program that only ever calls one has no
        # reason to pay for a C++ toolchain. The language is enabled here rather
        # than in that line, so a checkout without the test framework does not
        # demand a C++ compiler it will never use.
        enable_language(CXX)
        # Only the pieces needed, and only the ones asked for: this build is the
        # host's, not the part's, so the vendored samples and the extras are
        # compiled for nothing.
        set(gtest_force_shared_crt ON  CACHE BOOL "" FORCE)
        set(BUILD_GMOCK      OFF CACHE BOOL "" FORCE)
        set(INSTALL_GTEST   OFF CACHE BOOL "" FORCE)
        add_subdirectory("${ROBOHUB_GOOGLETEST_DIR}" "${CMAKE_BINARY_DIR}/_googletest" EXCLUDE_FROM_ALL)

        # One binary for every GoogleTest suite in this project.
        #
        # One rather than one per suite because GoogleTest registers its tests
        # globally, so two binaries cannot share a suite without each of them
        # running the other's tests as well. `tests/tests.yml` names this binary,
        # and `rho test` runs it with `--gtest_filter` when it wants one suite.
        #
        # C++, which is not a choice the tests could make: GoogleTest is a C++
        # library and its own headers include `<cstddef>`, so a `.c` file that
        # included it would not compile at all. What is still C is the code under
        # test — the headers declare it `extern "C"`, which is what they are for.
        file(GLOB ROBOHUB_GTEST_SOURCES
             "${CMAKE_CURRENT_SOURCE_DIR}/tests/gtest/*.cpp"
             "${CMAKE_CURRENT_SOURCE_DIR}/tests/gtest/*.cc")
        if(ROBOHUB_GTEST_SOURCES)
            add_executable(robohub-stm32-gtest ${ROBOHUB_GTEST_SOURCES})
            target_link_libraries(robohub-stm32-gtest
                PRIVATE robohub-stm32 gtest gtest_main)
            add_test(NAME robohub-stm32-gtest COMMAND robohub-stm32-gtest)
            message(STATUS "[robohub-stm32] GoogleTest suites: ${ROBOHUB_GTEST_SOURCES}")
        else()
            message(STATUS "[robohub-stm32] no GoogleTest suites in tests/gtest/")
        endif()
    else()
        # Said plainly rather than failing the build. Whether GoogleTest is present
        # is a question about this checkout, not about whether this platform is
        # correct, and a platform that could not be built because an optional test
        # framework had not been fetched would be worse than one that says so.
        message(STATUS
            "[robohub-stm32] googletest not found at ${ROBOHUB_GOOGLETEST_DIR}: "
            "the GoogleTest suites are not built. It is a dependency of this module; "
            "fetch it there.")
    endif()
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
