# Changelog

## [0.3.1] - 2026-11-21

A health check for the board (STM-0009, robohub ADR-0025). `examples/c/health_check` walks a Nucleo board through three boots:

- a task that hangs, which the watchdog resets;
- a HardFault, which the handler records before the running watchdog resets the board, so the third boot knows it by its record and its reason reads watchdog;
- the tasks keeping their time with the watchdog fed.

The keystore in flash holds key A across the first reset and key B across the second. The check prints `health-check: passed`, and `health_check.py` builds it, flashes it and reads the console. It is built with the tests, and the run on a board is declared skipped until one is on the bench. The check's keys have their own scope and epoch 0, so no session uses them, but they replace a key the board was rotated to.

## [0.3.0] - 2026-11-20

The health contracts on the STM32 F4 and H7 (STM-0008, robohub-common 1.3.0): the independent watchdog (1 ms to 32 768 ms from the LSI, the smallest prescaler that reaches the period; it cannot be stopped), the RCC reset flags read once and cleared (a pin flag counts only when nothing else is set), and the first eight RTC backup registers for the fault record and boot count. The fault handler writes the record before its report; it then stops as before, and a running watchdog resets the board.

## [0.2.1] - 2026-11-19

Board images that could not boot no longer build. nucleo-f429zi linked with "cannot find entry symbol _start; defaulting to 0x8000" — no startup file, no linker script, an image of eight bytes — and `sw-smoke` passed it; nucleo-f412zg and nucleo-f446ze were as empty (no startup file; f412zg no memory map either). Causes: every F4 preset named `system_stm32f4xx_singlecore.c`, which the F4 device repository does not have (it is `system_stm32f4xx.c`), f412zg named `startup_stm32f412xx.s` (it is `startup_stm32f412zx.s`), the sparse vendor checkout held no other F4 startup file, and those specs gave no memory map. Every board spec now names `cmake/image-check.cmake`, which refuses a spec without a memory map and fails the build, deleting the image, unless the entry, the vector table at the flash origin, the reset vector and the initial stack pointer are where the part reads them (STM-BOARD-0004). Memory maps added from the vendor's STM32Cube linker scripts for F412ZG, F413ZH, F429ZI, H723ZG, H743ZI, H743ZI2, H745ZI-Q (bank 1, the Cortex-M7's) and H7A3ZI-Q (STM-BOARD-0005). F446ZE's spec claimed 64 KB of core-coupled memory the part does not have; removed, and it links with nano/nosys like the other boards. H745ZI-Q and H7A3ZI-Q were set for the LDO while the vendor's projects for those boards use the direct SMPS supply — a mismatch that stops the part or locks its supply configuration; now `USE_PWR_DIRECT_SMPS_SUPPLY`, and H745ZI-Q defines `CORE_CM7`, which its device header requires. New host suite `board-specs`. F439ZI, H755ZI-Q, H7B3ZI-Q and the F2/F3/F7/L4/L5/U5/H5 boards still have no memory map, and F207ZG, F303ZE and H563ZI no target spec: the vendor material here states nothing for those parts.

## [0.2.0] - 2026-11-14

The board key is kept in reserved flash on the nucleo-h753zi; other boards answer a key rotation with "cannot rotate". A production plan sets read-out protection level 1 (reversible; level 2 is documented and never run). Host tests build robohub-common's tests.
