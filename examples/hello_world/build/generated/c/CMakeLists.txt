cmake_minimum_required(VERSION 3.16)

# The target this project builds for, when one was generated.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cmake/toolchain.cmake)
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/toolchain.cmake)
endif()

project(main C)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cmake/target.cmake)
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/target.cmake)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/rho-config.cmake)

add_executable(main main.c app.c
    runtime.c
    ${RHO_WORKSPACE}/robohub-stm32/src/platform/stm32_error.c
    ${RHO_WORKSPACE}/robohub-stm32/src/platform/stm32_console.c
    ${RHO_WORKSPACE}/robohub-stm32/src/platform/stm32_platform.c
    ${RHO_WORKSPACE}/robohub-stm32/src/hal/stm32_gpio.c
    ${RHO_WORKSPACE}/robohub-stm32/src/hal/stm32_i2c.c
    ${RHO_WORKSPACE}/robohub-stm32/src/hal/stm32_rs485.c
    ${RHO_WORKSPACE}/robohub-stm32/src/hal/stm32_spi.c
    ${RHO_WORKSPACE}/robohub-stm32/src/hal/stm32_timer.c
    ${RHO_WORKSPACE}/robohub-stm32/src/hal/stm32_uart.c
    ${RHO_WORKSPACE}/robohub-common/src/error.c
    ${RHO_WORKSPACE}/robohub-common/src/platform/platform.c
    ${RHO_WORKSPACE}/robohub-common/src/platform/registry.c
)

set_source_files_properties(${RHO_CONFIG_VALUES} PROPERTIES GENERATED TRUE)
add_dependencies(main ${CMAKE_CURRENT_SOURCE_DIR}/${RHO_CONFIG_VALUES})
set_property(SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/main.c ${CMAKE_CURRENT_SOURCE_DIR}/app.c
             ${CMAKE_CURRENT_SOURCE_DIR}/runtime.c
             APPEND PROPERTY OBJECT_DEPENDS ${RHO_CONFIG_VALUES})

target_include_directories(main PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${RHO_WORKSPACE}/robohub-stm32/include
    ${RHO_WORKSPACE}/robohub-stm32/include/robohub
    ${RHO_WORKSPACE}/robohub-stm32/include/robohub/hal
    ${RHO_WORKSPACE}/robohub-stm32/include/robohub/platform
    ${RHO_WORKSPACE}/robohub-stm32/include/robohub/drivers
    ${RHO_WORKSPACE}/robohub-stm32/platform
    ${RHO_WORKSPACE}/robohub-stm32/${CMAKE_BINARY_DIR}/generated
    ${RHO_WORKSPACE}/robohub-common/include
    ${RHO_WORKSPACE}/robohub-common/include/robohub
    ${RHO_WORKSPACE}/robohub-common/include/robohub/hal
    ${RHO_WORKSPACE}/robohub-common/include/robohub/drivers
    ${RHO_WORKSPACE}/robohub-common/${CMAKE_BINARY_DIR}/generated
    ${RHO_WORKSPACE}/robohub-common/include
    ${RHO_WORKSPACE}/robohub-common/include/robohub/hal
)
target_include_directories(main PRIVATE ${RHO_CONFIG_OUT}/include/generated)
target_link_libraries(main PRIVATE robohub_platform)
