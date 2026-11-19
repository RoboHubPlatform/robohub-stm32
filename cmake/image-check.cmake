# What this platform says about a program, once the program target exists: that
# the image it links is one the part can start.
#
# An STM32 starts by reading two words at the base of its flash: the initial stack
# pointer and the address of the reset handler. Both come from the vendor's
# startup assembly (the `g_pfnVectors` table, placed in `.isr_vector`) and the
# linker script (which puts that section at the flash origin and names
# `Reset_Handler` as the entry). A build that loses either still links: with no
# startup file the linker warns "cannot find entry symbol" and defaults the entry
# to 0x8000, and with no linker script it places code where the host's default
# script does. The result is an image of a few bytes that the part cannot run,
# and a smoke test that only asks whether the build succeeded passes for it.
#
# So a build checks the image it produced and fails when it is not bootable:
#
#   - the target spec gave a memory map, so there is a linker script (configure);
#   - `g_pfnVectors` and `Reset_Handler` are defined (the startup file was built);
#   - the ELF entry is `Reset_Handler`;
#   - `.isr_vector` starts at the flash origin the linker script declares;
#   - the second word of the table is `Reset_Handler` with the Thumb bit set;
#   - the first word, the initial stack pointer, is the linker script's `_estack`.
#
# Each board's target spec names this file (`"program"`), and rho-target copies it
# into the generated build as `cmake/program.cmake`, which the build includes after
# creating `${RHO_PROGRAM}`. The same file is the post-build script: run with
# `cmake -P` it does the checks on the ELF it is given.
#
# @req STM-BOARD-0004
#
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.16)

# --------------------------------------------------------------------------- #
#  Script mode: check one ELF                                                 #
# --------------------------------------------------------------------------- #

if(CMAKE_SCRIPT_MODE_FILE)
    foreach(required ELF READELF NM FLASH_ORIGIN)
        if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
            message(FATAL_ERROR "[robohub-stm32] image check: ${required} was not given")
        endif()
    endforeach()

    set(failures "")

    # The symbols, from the symbol table.
    execute_process(COMMAND "${NM}" "${ELF}"
                    OUTPUT_VARIABLE symbols RESULT_VARIABLE nm_result ERROR_VARIABLE nm_error)
    if(NOT nm_result EQUAL 0)
        message(FATAL_ERROR "[robohub-stm32] image check: ${NM} could not read ${ELF}: ${nm_error}")
    endif()
    set(reset "")
    set(vectors "")
    set(estack "")
    if(symbols MATCHES "(^|\n)([0-9a-fA-F]+) [TtWw] Reset_Handler(\n|$)")
        set(reset "${CMAKE_MATCH_2}")
    endif()
    if(symbols MATCHES "(^|\n)([0-9a-fA-F]+) [A-Za-z] g_pfnVectors(\n|$)")
        set(vectors "${CMAKE_MATCH_2}")
    endif()
    if(symbols MATCHES "(^|\n)([0-9a-fA-F]+) [A-Za-z] _estack(\n|$)")
        set(estack "${CMAKE_MATCH_2}")
    endif()
    if(reset STREQUAL "")
        list(APPEND failures "Reset_Handler is not defined: the startup assembly was not built into the image")
    endif()
    if(vectors STREQUAL "")
        list(APPEND failures "g_pfnVectors is not defined: the image has no vector table")
    endif()

    # Hex strings to numbers, with the Thumb bit cleared where it is an address of
    # code. math(EXPR) reads 0x-prefixed values as 64-bit integers.
    function(_rh_hex out value)
        string(REGEX REPLACE "^0[xX]" "" value "${value}")
        if(value STREQUAL "")
            set(${out} "" PARENT_SCOPE)
            return()
        endif()
        math(EXPR number "0x${value}")
        set(${out} "${number}" PARENT_SCOPE)
    endfunction()

    _rh_hex(flash_origin "${FLASH_ORIGIN}")
    _rh_hex(reset_addr "${reset}")
    _rh_hex(vectors_addr "${vectors}")
    _rh_hex(estack_addr "${estack}")
    if(NOT reset_addr STREQUAL "")
        math(EXPR reset_addr "${reset_addr} & ~1")
    endif()

    # The entry point, from the ELF header.
    execute_process(COMMAND "${READELF}" -h "${ELF}"
                    OUTPUT_VARIABLE header RESULT_VARIABLE readelf_result ERROR_VARIABLE readelf_error)
    if(NOT readelf_result EQUAL 0)
        message(FATAL_ERROR "[robohub-stm32] image check: ${READELF} could not read ${ELF}: ${readelf_error}")
    endif()
    if(header MATCHES "Entry point address: +(0x[0-9a-fA-F]+)")
        _rh_hex(entry "${CMAKE_MATCH_1}")
        math(EXPR entry "${entry} & ~1")
        if(NOT reset_addr STREQUAL "" AND NOT entry EQUAL reset_addr)
            math(EXPR entry_hex "${entry}" OUTPUT_FORMAT HEXADECIMAL)
            list(APPEND failures "the entry point is ${entry_hex}, not Reset_Handler")
        endif()
    else()
        list(APPEND failures "the ELF header names no entry point")
    endif()

    if(NOT vectors_addr STREQUAL "" AND NOT vectors_addr EQUAL flash_origin)
        math(EXPR vectors_hex "${vectors_addr}" OUTPUT_FORMAT HEXADECIMAL)
        list(APPEND failures "the vector table is at ${vectors_hex}, not at the flash origin ${FLASH_ORIGIN}")
    endif()

    # The first two words of the table, as the part reads them at reset.
    execute_process(COMMAND "${READELF}" -x .isr_vector "${ELF}"
                    OUTPUT_VARIABLE dump RESULT_VARIABLE dump_result ERROR_VARIABLE dump_error)
    if(dump_result EQUAL 0 AND dump MATCHES "0x[0-9a-fA-F]+ ([0-9a-fA-F]+) ([0-9a-fA-F]+)")
        set(words "${CMAKE_MATCH_1};${CMAKE_MATCH_2}")
        set(values "")
        foreach(word IN LISTS words)
            # readelf prints the bytes in memory order; the part reads them as a
            # little-endian word.
            string(REGEX REPLACE "^(..)(..)(..)(..)$" "\\4\\3\\2\\1" word "${word}")
            math(EXPR value "0x${word}")
            list(APPEND values "${value}")
        endforeach()
        list(GET values 0 initial_sp)
        list(GET values 1 reset_vector)
        if(NOT reset_addr STREQUAL "")
            math(EXPR expected "${reset_addr} | 1")
            if(NOT reset_vector EQUAL expected)
                math(EXPR got "${reset_vector}" OUTPUT_FORMAT HEXADECIMAL)
                list(APPEND failures "the reset vector is ${got}, not Reset_Handler with the Thumb bit set")
            endif()
        endif()
        if(NOT estack_addr STREQUAL "" AND NOT initial_sp EQUAL estack_addr)
            math(EXPR got "${initial_sp}" OUTPUT_FORMAT HEXADECIMAL)
            list(APPEND failures "the initial stack pointer is ${got}, not _estack")
        endif()
    else()
        list(APPEND failures "the image has no .isr_vector section")
    endif()

    if(failures)
        string(REPLACE ";" "\n  - " failures "${failures}")
        # Removed, so the next build links it again and checks it again rather
        # than finding an up-to-date file and calling the build a success.
        file(REMOVE "${ELF}")
        message(FATAL_ERROR
            "[robohub-stm32] ${ELF} is not an image the part can start:\n  - ${failures}\n"
            "A build for a board needs the part's startup assembly (the board preset's "
            "override names it, and the vendor checkout must hold it) and a memory map in "
            "the board's target spec, from which rho-target writes the linker script.")
    endif()
    message(STATUS "[robohub-stm32] image check: entry, vector table and reset vector are where the part looks")
    return()
endif()

# --------------------------------------------------------------------------- #
#  Build mode: register the check on the program                              #
# --------------------------------------------------------------------------- #

if(NOT DEFINED RHO_PROGRAM OR NOT TARGET ${RHO_PROGRAM})
    return()
endif()

# No memory map, no linker script: refused at configure time, because what would
# be linked is placed by the host's default script and cannot start on the part.
if(NOT DEFINED RHO_LINKER_SCRIPT OR NOT EXISTS "${RHO_LINKER_SCRIPT}")
    message(FATAL_ERROR
        "[robohub-stm32] the target spec for this board has no memory map, so there is no "
        "linker script and the image would not start on the part. Add `memory` (flash and "
        "ram, from the part's reference manual or the vendor's linker script) to the "
        "board's target.json.")
endif()

file(READ "${RHO_LINKER_SCRIPT}" ROBOHUB_STM32_LINKER_SCRIPT_TEXT)
if(NOT ROBOHUB_STM32_LINKER_SCRIPT_TEXT MATCHES "FLASH[^:\n]*:[^\n]*ORIGIN *= *(0x[0-9a-fA-F]+)")
    message(FATAL_ERROR "[robohub-stm32] ${RHO_LINKER_SCRIPT} declares no FLASH region, so nothing says where the vector table belongs")
endif()
set(ROBOHUB_STM32_FLASH_ORIGIN "${CMAKE_MATCH_1}")

# The binutils of the cross toolchain: what CMake found, or the ones beside the
# compiler.
foreach(tool READELF NM)
    if(NOT CMAKE_${tool})
        string(TOLOWER "${tool}" lower)
        string(REGEX REPLACE "gcc(\\.exe)?$" "${lower}\\1" CMAKE_${tool} "${CMAKE_C_COMPILER}")
    endif()
endforeach()

add_custom_command(TARGET ${RHO_PROGRAM} POST_BUILD
    COMMAND ${CMAKE_COMMAND}
            -DELF=$<TARGET_FILE:${RHO_PROGRAM}>
            -DREADELF=${CMAKE_READELF}
            -DNM=${CMAKE_NM}
            -DFLASH_ORIGIN=${ROBOHUB_STM32_FLASH_ORIGIN}
            -P ${CMAKE_CURRENT_LIST_FILE}
    COMMENT "Checking that ${RHO_PROGRAM} is an image the part can start"
    VERBATIM)
