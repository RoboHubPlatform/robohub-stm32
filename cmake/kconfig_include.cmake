# ---------------------------------------------------------------------------
#  Kconfig Include Helper for RoboHub STM32
# ---------------------------------------------------------------------------
#
# Included by a board support package, or by anything else that depends on this
# platform and needs the generated header on its include path. A board that
# does its own configuration includes this rather than repeating the path, so
# that where the header lands is decided in one place.

if(NOT DEFINED KCONFIG_HEADER)
    message(FATAL_ERROR
        "KCONFIG_HEADER is not defined. Include Kconfig.cmake before this, "
        "or set it to the header robohub-stm32 generated.")
endif()

include_directories(${CMAKE_BINARY_DIR}/generated)

# Expose the header's own directory, so a dependent module can include the
# configuration without knowing where the build put it.
set(ROBOHUB_STM32_KCONFIG_INCLUDE_DIR ${CMAKE_BINARY_DIR}/generated PARENT_SCOPE)
set(ROBOHUB_STM32_KCONFIG_HEADER ${KCONFIG_HEADER} PARENT_SCOPE)
