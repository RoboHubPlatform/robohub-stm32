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
| `targets/` | `rho-target` specs: the MCU families, with the toolchain and flags they share |
| `tests/` | The project's tests, declared in `tests.yml` rather than discovered |
| `examples/` | Two hello-worlds, one written in Rho and one in C, so the toolchain can be seen to treat a program rather than a language |
| `vendor/` | The vendor's HAL and CMSIS, fetched by a build. Nothing here is checked in |

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

A board is *supported* when a build for it works: a target spec the build can read,
a vendor tree that can be fetched, drivers that compile for it. That is a narrower
thing than having a board directory, and the difference is why
`hardware.supported` in `module.yml` is a list rather than the `boards/` directory.

**Supported — builds and links:**

| Board | Part |
|---|---|
| `nucleo-f412zg` | STM32F412ZG |
| `nucleo-f429zi` | STM32F429ZI |
| `nucleo-f446ze` | STM32F446ZE |
| `nucleo-h753zi` | STM32H753ZI |

**Present but not yet building.** The other boards carry a BSP, a preset and a
target spec, and some will build once their family's vendor tree is fetched and a
memory map is written. A board in `boards/` that is absent from
`hardware.supported` is not claimed to work; it is *recorded*, which is a
different statement and is the one that is true of it.

`nucleo-f746zg` and `nucleo-h743zi` are the nearest: both have complete target
specs and both compile until the linker wants a memory map.

## Tests

The project's tests are declared in `tests/tests.yml` and run with the same runner
that runs everything else:

```sh
rho test                     # everything the project declares
rho test --tag unit          # the host unit tests
rho test --tag smoke         # every example, for every supported board
rho test --list              # what is declared, and what is not yet runnable
```

| Suite | What it is |
|---|---|
| `error` | A GoogleTest suite over the HAL status mapping — the one part of this platform a host can run, because it depends on the vendor's published status values rather than on a peripheral |
| `sw-smoke` | Every example, for every board in `hardware.supported`, as a matrix. A cell builds; a board that cannot be built on this machine reports itself skipped with the reason, which is the difference between a smoke suite that can be trusted and one that cannot |
| `hw-smoke` | Declared, not written. Flashing each example and reading its console needs a board on a bench and somebody watching it, and a hardware test that runs and finds nothing is worse than one that is honestly absent |

The matrix reads its boards from the contract rather than listing them, so a board
added to `hardware.supported` is tested by the next run without anything else being
edited — and a board that stops building cannot go on being claimed by a test that
no longer runs it.

## Building

This is a C library, not a command, so it is consumed by a project rather than
run. Built through the toolchain, which fetches the vendor tree and generates
what the vendor asks for and does not ship:

```sh
rho build -target nucleo-h753zi examples/rho/hello_world
rho test                        # the tests this project declares
```

Or with CMake on its own, which is what a person does when they are not going to
use the toolchain:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

CMake alone does not fetch the vendor tree. The toolchain does that, and derives
the HAL configuration header from the vendor's own template; the two together are
why a checkout containing nothing but sources still builds.

`robohub-common`'s headers are needed to compile anything here. CMake finds them
in a sibling checkout; otherwise pass them explicitly:

```sh
cmake -S . -B build -DROBOHUB_COMMON_INCLUDE_DIR=/path/to/robohub-common/include
```

## Licence

MIT.
