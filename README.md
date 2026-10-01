# robohub-stm32

The STM32 platform for the [RoboHub Platform](../robohub-common).

`robohub-common` declares the platform-independent interfaces — the HAL contracts
for GPIO, I2C, SPI, UART, RS485 and Timer, the error codes, the instance
registry, and the platform initialisation contract. It contains no hardware code
and knows nothing about any particular chip. This module is the other half: it
implements those interfaces against STMicroelectronics' STM32 HAL and CMSIS, and
carries a board support package for each supported board.

An application depends on both. It writes against the interfaces, and the two
modules together are what a build links:

```yaml
depends:
  - name: robohub-common
    type: c
    path: ../robohub-common
  - name: robohub-stm32
    type: c
    path: ../robohub-stm32
```

## What is here

| Path | What it is |
|------|------------|
| `include/robohub/platform/` | The platform's own headers — what a board or a driver includes |
| `src/platform/` | Chip-independent platform code: error mapping, clock tree |
| `src/hal/` | The HAL implementations, one file per interface |
| `platform/` | The vendor library integration: CMSIS and HAL bring-up shared by every board of a family |
| `boards/<board>/` | One BSP per development board: its pin map, its clock configuration, its startup |
| `kconfig/` | Kconfig fragments for the symbols this platform owns |
| `targets/` | `rho-target` specs: the MCU families and the boards, with their memory maps and flash commands |

## Families

The families this module covers, and the vendor repository each is built from,
are listed in `module.yml` under `families`. STMicroelectronics publishes the
HAL drivers, the CMSIS device headers and the middleware for a family in one
repository per family, named `STM32Cube<Family>`, so a build fetches the
repository of the family it targets.

The families are grouped into five tiers — high-performance, mainstream,
ultra-low-power, wireless, and MPU. The mainstream tier is where a new board is
most likely to land, because it has the widest board availability.

The MPU tier (STM32MP1, STM32MP2) is catalogued but not yet buildable: those are
Cortex-A parts running Linux or an RTOS, not bare-metal Cortex-M targets, and
they carry no target spec.

## Status

The chip-independent half is implemented and tested: the HAL status mapping is
in `src/platform/stm32_error.c` and its tests run on the host, because the
mapping depends on the vendor's published status values rather than on a
peripheral.

The six HAL implementations, the clock tree, the board support packages and the
target specs are not written yet. They are listed here as the shape of what the
module becomes, so that a driver added later has somewhere to go and a reader
can tell what is missing from what is merely unbuilt for their board.

## Building

This is a C library, not a command, so it is consumed by a project rather than
run. To build and test it on its own:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

`robohub-common`'s headers are needed to compile anything here. CMake finds them
in a sibling checkout; otherwise pass them explicitly:

```sh
cmake -S . -B build -DROBOHUB_COMMON_INCLUDE_DIR=/path/to/robohub-common/include
```

## Licence

MIT.
