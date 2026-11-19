# RoboHub STM32 Platform

The STM32 platform for the RoboHub ecosystem: the interfaces declared by
`robohub-common`, implemented on STM32 microcontrollers, with board support
packages for the Nucleo boards.

## The platform

A Rho program on this platform runs on an STM32 part. The platform provides
the interfaces, the HAL, and the board support package that registers the
board's peripherals.

## Building

```sh
cd examples/c/hello_world
rho build -target nucleo-f412zg .
```

## Boards, memory maps and the image check

A board builds only if its target spec (`boards/<board>/target.json`) gives a
memory map, from which rho-target writes the linker script, and its preset names
the part's startup assembly and system file in the vendor's CMSIS device
repository. Each memory map is the flash and RAM of the vendor's STM32Cube linker
script for that part (the path is in the flash region's comment); none is
written from memory.

| Memory map | Boards |
|---|---|
| from the vendor's linker script | F412ZG, F413ZH, F429ZI, F446ZE, H723ZG, H743ZI, H743ZI2 (the same STM32H743ZI), H745ZI-Q (the Cortex-M7's half: bank 1), H7A3ZI-Q |
| written before this rule, shortened for the keystore | H753ZI (STM32CubeH7 has no linker script for it; its regions agree with the CMSIS `stm32h753xx.h` bases) |
| none — the vendor checkout states nothing for the part | F439ZI, H755ZI-Q, H7B3ZI-Q (STM32CubeH7/F4 have no project for these parts), and every F2, F3, F7, L4, L5, U5 and H5 board (their Cube trees are not vendored) |

F207ZG, F303ZE and H563ZI have no target spec at all, for the same reason.

Every board's spec names `cmake/image-check.cmake` as its program fragment, so a
build checks the image it links and fails, removing it, unless the part can
start it: the ELF entry is `Reset_Handler`, `g_pfnVectors` is in `.isr_vector`
at the linker script's flash origin, the table's reset vector is
`Reset_Handler | 1` and its initial stack pointer is `_estack`. A spec without a
memory map is refused when the build is configured. This is what `sw-smoke`
relies on: before it, nucleo-f429zi linked a few bytes at `0x8000` ("cannot find
entry symbol _start") and passed. `tests/board_specs_test.py` checks the specs,
the preset sources against the vendor's tree, and the check itself.

The vendor's CMSIS device checkout is sparse, and it is cloned with the paths
the first board built named. A board of the same family whose startup file was
not in that set builds without it — which the image check now refuses. Add its
files to the checkout with
`git -C vendor/cmsis_device_<family> sparse-checkout add Source/Templates/gcc/startup_<part>.s Source/Templates/system_stm32<family>xx.c`.

## Flashing

```sh
rho flash -target nucleo-f412zg build/output/main
```

## The examples

`examples/c/hello_world` is the smallest program that runs on real hardware.

`examples/rho/hello_world` is that program in Rho, written against the
contracts and nothing else.

## The device keystore

The rotated device key (`robohub/keystore.h`) is kept in flash only on boards
whose memory map reserves room for it, because an STM32's erase unit is a
sector and sectors differ widely by family:

| Board | Keystore |
|---|---|
| NUCLEO-H753ZI | bank 2 sectors 6 and 7 (`0x081C0000`-`0x081FFFFF`, 2 × 128 KB); `memory.flash.length` is shortened to `0x1C0000` so the linker never places code there, and `RH_KEYSTORE_STM32_FLASH=1` turns the implementation on |
| other H7 boards | not yet: the H743ZI/H743ZI2/H7A3/H745 specs give the vendor's whole flash (bank 1 only on the H745, whose bank 2 is the Cortex-M4's) and reserve nothing; the H7B3/H755 specs carry no memory map; the H723 is single-bank, so erasing would stall the code running from the same bank |
| F2/F4/F7 boards | not yet: the end-of-flash sectors are 128 KB (256 KB on F7), and the small 16/32 KB sectors are at the start where the vector table is; using them needs the image relinked around a hole |
| L4/L5/U5/H5 boards | not yet: small uniform pages make this easy, but those HALs are not vendored here, so it could not be built and checked |

Every board without it links robohub-common's weak default: no store, the
compiled-in key is used, and a REKEY is answered "cannot rotate".

`src/platform/stm32_keystore.c` erases one sector with `HAL_FLASHEx_Erase` and
programs whole flash words (`FLASH_TYPEPROGRAM_FLASHWORD`), writing bank 2 while
the image runs from bank 1 (read-while-write, so nothing stalls). Before it
touches flash it checks the image's end (`_sidata` + the size of `.data`)
against the keystore's start and refuses if they overlap, so a board that set
the define without shortening its memory map cannot erase its own code. To add
a dual-bank H7 board: give its target spec a memory map, take two sectors off
`memory.flash.length`, and add `RH_KEYSTORE_STM32_FLASH=1` to its defines.

## Production

`rho flash --production` (plan: `production/production.json`) writes the image
and then sets **RDP level 1** through STM32CubeProgrammer
(`STM32_Programmer_CLI -c port=SWD -ob RDP=0xBB`; `STM32_PROGRAMMER_PORT`
overrides the port). Level 1 stops the flash being read through the debug port.
It is reversible, but going back to level 0 (`-ob RDP=0xAA`) mass-erases the
whole flash, keystore included, and until then OpenOCD cannot write the board.
`CONFIG_RH_PRODUCTION` changes nothing in an STM32 image.

**RDP level 2 is not offered** and never runs from this toolchain. It is
irreversible: the debug port is disabled for good, the option bytes can never
change again, and the board can only be updated by its own firmware. Set it by
hand (`-ob RDP=0xCC`), deliberately, only once the firmware can update itself.
