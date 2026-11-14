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
| other H7 boards | not yet: the H743ZI/H743ZI2/H7A3/H7B3/H745/H755 specs carry no memory map, so nothing generates a linker script that could reserve the sectors; the H723 is single-bank, so erasing would stall the code running from the same bank |
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
