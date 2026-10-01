# ---------------------------------------------------------------------------
#  Kconfig Integration for RoboHub STM32
# ---------------------------------------------------------------------------
#
# The fragments a project declared, merged into a header the drivers include.
# robohub-common does the same for the interfaces it declares; this is the
# platform's half, and it is a separate header because the two halves answer
# different questions — the common one says which interfaces exist, this one says
# which of this platform's drivers a board actually builds.
#
# The generator is the one robohub-common ships, copied rather than rewritten:
# two generators that produce the same header in slightly different ways are a
# configuration that reads differently depending on which module wrote it.

find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(KCONFIG_GEN ${CMAKE_CURRENT_LIST_DIR}/../tools/kconfig/kconfig_gen.py)

set(KCONFIG_INPUTS
    ${CMAKE_CURRENT_LIST_DIR}/../Kconfig
    ${CMAKE_CURRENT_LIST_DIR}/../kconfig/hal/Kconfig.gpio
    ${CMAKE_CURRENT_LIST_DIR}/../kconfig/hal/Kconfig.i2c
    ${CMAKE_CURRENT_LIST_DIR}/../kconfig/hal/Kconfig.spi
    ${CMAKE_CURRENT_LIST_DIR}/../kconfig/hal/Kconfig.uart
    ${CMAKE_CURRENT_LIST_DIR}/../kconfig/hal/Kconfig.rs485
    ${CMAKE_CURRENT_LIST_DIR}/../kconfig/hal/Kconfig.timer
)

set(ROBOHUB_KCONFIG_FILE "" CACHE FILEPATH
    "Application Kconfig override file, such as project.conf")

set(KCONFIG_HEADER ${CMAKE_BINARY_DIR}/generated/kconfig.h)

set(KCONFIG_ARGS ${KCONFIG_INPUTS} ${KCONFIG_HEADER})
set(KCONFIG_CONFIG_DEPENDS)
if(ROBOHUB_KCONFIG_FILE)
    list(PREPEND KCONFIG_ARGS --config ${ROBOHUB_KCONFIG_FILE})
    list(APPEND KCONFIG_CONFIG_DEPENDS ${ROBOHUB_KCONFIG_FILE})
endif()

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/generated)

add_custom_command(
    OUTPUT ${KCONFIG_HEADER}
    COMMAND ${Python3_EXECUTABLE} ${KCONFIG_GEN} ${KCONFIG_ARGS}
    DEPENDS ${KCONFIG_INPUTS} ${KCONFIG_GEN} ${KCONFIG_CONFIG_DEPENDS}
    COMMENT "Generating kconfig.h for robohub-stm32"
    VERBATIM
)

add_custom_target(kconfig_stm32 DEPENDS ${KCONFIG_HEADER})
