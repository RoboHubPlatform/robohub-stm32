/*
 * @file rho-config.cmake
 * @brief Kconfig bridge for a Rho project.
 *
 * @project main
 * @version 0.1
 * @date 2026
 *
 * SPDX-License-Identifier: MIT
 */

# The configuration a project declared is resolved by the rho-kconfig
# component, which is run here rather than at generation time: a value may
# change without regenerating the project.

# The tool is the rho that invoked this build when there is one, and
# whatever rho is on PATH otherwise.
if(DEFINED ENV{RHO_BIN} AND NOT "$ENV{RHO_BIN}" STREQUAL "")
  set(RHO_CONFIG_TOOL "$ENV{RHO_BIN}")
else()
  set(RHO_CONFIG_TOOL rho)
endif()

set(RHO_CONFIG_DIALECT "linux")
set(RHO_CONFIG_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(RHO_CONFIG_OUT ${CMAKE_CURRENT_SOURCE_DIR})
set(RHO_CONFIG_FRAGMENTS
  "kconfig/Kconfig"
)
set(RHO_CONFIG_VALUES ".config")

add_custom_command(
  OUTPUT ${RHO_CONFIG_VALUES}
  COMMAND ${RHO_CONFIG_TOOL} kconfig merge
          --force
          --dialect ${RHO_CONFIG_DIALECT}
          --root ${RHO_CONFIG_DIR}
          --out ${RHO_CONFIG_OUT}
          ${RHO_CONFIG_FRAGMENTS}
  COMMENT "Resolving the Kconfig bridge with rho-kconfig"
  VERBATIM
)

