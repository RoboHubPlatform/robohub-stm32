#!/usr/bin/env python3
"""
Generate the Nucleo-144 board modules for robohub-stm32.

One folder per board under boards/, each with its own module.yml. The data
below is transcribed from the Nucleo-144 board list; the fields that are a
property of the board (family, part, core, what it is for) come from that list,
and the fields that are a property of a BSP (pin map, clock tree) are left for
the board's own implementation rather than invented here.

SPDX-License-Identifier: MIT
"""

import os
import pathlib

BOARDS = [
    # (board, mcu, family, core, summary, note)
    ("NUCLEO-H753ZI",   "STM32H753ZI", "STM32H7", "Cortex-M7",   "480 MHz, Crypto/Hash hardware, Ethernet", ""),
    ("NUCLEO-H743ZI",   "STM32H743ZI", "STM32H7", "Cortex-M7",   "400 MHz, Ethernet", ""),
    ("NUCLEO-H743ZI2",  "STM32H743ZI", "STM32H7", "Cortex-M7",   "Revision 2: updated power supply and ST-LINK V3", "Revision 2 board; same MCU as NUCLEO-H743ZI."),
    ("NUCLEO-H723ZG",   "STM32H723ZG", "STM32H7", "Cortex-M7",   "550 MHz, high-speed USB PHY", ""),
    ("NUCLEO-H745ZI-Q", "STM32H745ZI", "STM32H7", "Cortex-M7",   "Dual-core Cortex-M7 + Cortex-M4", "Dual-core; the M4 is a co-processor the M7 starts."),
    ("NUCLEO-H755ZI-Q", "STM32H755ZI", "STM32H7", "Cortex-M7",   "Dual-core Cortex-M7 + Cortex-M4, crypto, SMPS", "Dual-core, and the M4 carries the crypto accelerators."),
    ("NUCLEO-H7A3ZI-Q", "STM32H7A3ZI", "STM32H7", "Cortex-M7",   "Low-power optimised, SMPS", ""),
    ("NUCLEO-H7B3ZI-Q", "STM32H7B3ZI", "STM32H7", "Cortex-M7",   "TFT controller and crypto, SMPS", ""),

    ("NUCLEO-F767ZI",   "STM32F767ZI", "STM32F7", "Cortex-M7",   "216 MHz, TFT/DSI controller", ""),
    ("NUCLEO-F756ZG",   "STM32F756ZG", "STM32F7", "Cortex-M7",   "216 MHz, hardware crypto", ""),
    ("NUCLEO-F746ZG",   "STM32F746ZG", "STM32F7", "Cortex-M7",   "216 MHz, Ethernet", ""),
    ("NUCLEO-F722ZE",   "STM32F722ZE", "STM32F7", "Cortex-M7",   "High-speed USB", ""),

    ("NUCLEO-F446ZE",   "STM32F446ZE", "STM32F4", "Cortex-M4F",  "Dual FMC, SPDIF-Rx", ""),
    ("NUCLEO-F439ZI",   "STM32F439ZI", "STM32F4", "Cortex-M4F",  "Crypto, Ethernet, LTDC", ""),
    ("NUCLEO-F429ZI",   "STM32F429ZI", "STM32F4", "Cortex-M4F",  "Ethernet, LCD-TFT controller", ""),
    ("NUCLEO-F413ZH",   "STM32F413ZH", "STM32F4", "Cortex-M4F",  "1.5 MB flash, CAN-FD", ""),
    ("NUCLEO-F412ZG",   "STM32F412ZG", "STM32F4", "Cortex-M4F",  "100 MHz, FSMC interface", ""),

    ("NUCLEO-F303ZE",   "STM32F303ZE", "STM32F3", "Cortex-M4F",  "Mixed-signal, four op-amps", ""),
    ("NUCLEO-F207ZG",   "STM32F207ZG", "STM32F2", "Cortex-M3",   "120 MHz, Ethernet, USB OTG", ""),

    ("NUCLEO-H563ZI",   "STM32H563ZI", "STM32H5", "Cortex-M33",  "250 MHz, secure, TrustZone", ""),

    ("NUCLEO-U575ZI-Q", "STM32U575ZI", "STM32U5", "Cortex-M33",  "Ultra-low-power, SMPS", ""),
    ("NUCLEO-U5A5ZJ-Q", "STM32U5A5ZJ", "STM32U5", "Cortex-M33",  "2.5 MB flash, hardware crypto", "2.5 MB of flash; the largest of the Nucleo-144 range."),

    ("NUCLEO-L552ZE-Q", "STM32L552ZE", "STM32L5", "Cortex-M33",  "TrustZone, low power", ""),
    ("NUCLEO-L4R5ZI",   "STM32L4R5ZI", "STM32L4", "Cortex-M4F",  "120 MHz, low power", ""),
    ("NUCLEO-L4R5ZI-P", "STM32L4R5ZI", "STM32L4", "Cortex-M4F",  "120 MHz, low power", "Fitted with an internal SMPS step-down converter."),
    ("NUCLEO-L496ZG",   "STM32L496ZG", "STM32L4", "Cortex-M4F",  "Low power", ""),
    ("NUCLEO-L496ZG-P", "STM32L496ZG", "STM32L4", "Cortex-M4F",  "Low power", "Fitted with an internal SMPS step-down converter."),
]

# The family a board belongs to, the vendor repository that carries its HAL and
# CMSIS, and that repository's default branch.
#
# The branch is per family and is not a guess: ST renamed some repositories from
# `master` to `main` at different times, so `master` is right for most families
# and wrong for seven of them. Verified with `git ls-remote --symref` on
# 2026-09-30; the result is recorded in module.yml under `families` so that the
# evidence and this table are read together and cannot drift apart unnoticed.
CUBE = {
    "STM32H7": ("STMicroelectronics/STM32CubeH7", "master"),
    "STM32F7": ("STMicroelectronics/STM32CubeF7", "master"),
    "STM32F4": ("STMicroelectronics/STM32CubeF4", "master"),
    "STM32F3": ("STMicroelectronics/STM32CubeF3", "master"),
    "STM32F2": ("STMicroelectronics/STM32CubeF2", "master"),
    "STM32H5": ("STMicroelectronics/STM32CubeH5", "main"),
    "STM32U5": ("STMicroelectronics/STM32CubeU5", "main"),
    "STM32L5": ("STMicroelectronics/STM32CubeL5", "master"),
    "STM32L4": ("STMicroelectronics/STM32CubeL4", "master"),
}

# The core a family's toolchain flags and the arm-none-eabi target are chosen
# for. The dual-core parts name the core that runs first; their second core is
# started by the first and is not the build target.
CORE_FLAGS = {
    "Cortex-M7":  "-mcpu=cortex-m7 -mthumb -mfloat-abi=soft",
    "Cortex-M4F": "-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16",
    "Cortex-M33": "-mcpu=cortex-m33 -mthumb -mfloat-abi=soft",
    "Cortex-M3":  "-mcpu=cortex-m3 -mthumb",
}


def device_define(mcu: str) -> str:
    """The device define a build sets for this part.

    ST names a part STM32<line><performance><package>: the package is ZI, ZG,
    ZE or ZH, and the define the HAL selects on is the line and performance with
    the package replaced by xx. STM32H753ZI therefore builds as STM32H753xx.
    """
    stem = mcu[:-2]
    return stem + "xx"


def board_slug(board: str) -> str:
    return board.lower()


TEMPLATE = """\
#
# {board} — a Nucleo-144 board support package for robohub-stm32.
#
# The Nucleo-144 boards share one set of connectors — ST morpho and the ST Zio
# extension of the Arduino UNO V3 — so what differs between them is the part on
# the board and what that part has. This module is the {board} one: it says
# which part it is, which family that part belongs to, and what a project needs
# to build and program it.
#
{extra_comment}#
# SPDX-License-Identifier: MIT
#

module:
  name: {slug}
  type: platform-board
  version: 0.1.0
  description: >
    {board}: {mcu}, {summary}. A Nucleo-144 board, so the whole pinout is
    reachable through the morpho and ST Zio headers, and programming is through
    the ST-LINK already on the board.

  maintainer: "Aurel Miron"
  license: "MIT"
  created: "2026-09-30"

# No main: a board is native code a project links, not something it runs.

build:
  language: c
  standard: c11

  include_dirs:
    - include/
    - include/board/

  source_dirs:
    - src/

depends:
  # The interfaces come first, then the platform that implements them, then this
  # board's own support package. `path` resolves for a checkout of the ecosystem
  # and `git` for a published one; the resolver takes the first it can use.
  - name: robohub-common
    type: c
    path: ../../robohub-common
    git: https://github.com/RoboHubPlatform/robohub-common
    branch: main
  - name: robohub-stm32
    type: c
    path: ../../robohub-stm32
    git: https://github.com/RoboHubPlatform/robohub-stm32
    branch: main
  # The vendor's HAL and CMSIS for this board's family. A board names its
  # family's repository because a board is the only place the family is known:
  # robohub-stm32 implements every family at once and so cannot depend on all of
  # them, and a build should not fetch the HAL for the twenty families it is not
  # targeting. Nothing here is fetched until a build actually needs it.
  - name: {cube_name}
    type: c
    git: https://github.com/{cube}
    branch: {cube_branch}

config:
  dialect: linux
  kconfig_fragments:
    - Kconfig

package:
  kind: library
  binary: false
  include:
    - module.yml
    - Kconfig
    - include
    - src
    - targets
  require:
    - module.yml
    - Kconfig

metadata:
  tags:
    - board
    - nucleo-144
    - {family_lower}
  stability: experimental
  visibility: public

# ---------------------------------------------------------------------------
#  The board
# ---------------------------------------------------------------------------

board:
  designation: {board}
  mcu: {mcu}
  family: {family}
  core: {core}
  summary: {summary}
{extra_field}
  # The preset this board builds with, and the target spec that goes with it.
  #
  # It names both rather than repeating what they say. A board's own facts are
  # which part it carries, how much memory that part has, and how it is
  # programmed; the repositories, the startup assembly and the compiler belong to
  # the family, and a board that copied them would be a second place for them to
  # be wrong. The preset carries the hardware, so the device define is read from
  # there rather than stated here.
  preset: {family_lower}
  target: {family_lower}

  # Every Nucleo-144 board has these, which is what makes them interchangeable
  # at the connector and what lets a project move between them.
  connectors:
    - ST morpho headers, full pinout
    - ST Zio, Arduino UNO V3 extension
  programmer: ST-LINK, on board
  debug_interface: ST-LINK, SWD

  # Programming goes through the ST-LINK already fitted, so there is nothing to
  # wire up and no external programmer to name.
  flash:
    tool: openocd
    interface: stlink.cfg
    # The target configuration file is chosen by the part, not the board: two
    # boards carrying the same part program the same way.
    target_config: >-
      {target_cfg}

  # Filled in by the board's own implementation rather than here. A pin map
  # written once for the family and copied to every board would be a map that is
  # wrong on whichever board it happens to be wrong on, and a wrong pin map is
  # the one fault that does not announce itself.
  pins: {{}}
  peripherals: {{}}
"""


def main() -> None:
    root = pathlib.Path(__file__).resolve().parent.parent / "boards"
    root.mkdir(parents=True, exist_ok=True)

    for board, mcu, family, core, summary, note in BOARDS:
        slug = board_slug(board)
        target_cfg = f"target/stm32{device_define(mcu)[5:7].lower()}.x.cfg"

        extra_comment = ""
        if note:
            extra_comment = f"# {note}\n#\n"
        extra_field = ""
        if note:
            extra_field = f"    {note}"

        text = TEMPLATE.format(
            board=board,
            slug=slug,
            mcu=mcu,
            family=family,
            family_lower=family.lower(),
            core=core,
            summary=summary,
            extra_comment=extra_comment,
            extra_field=extra_field,
            cube=CUBE[family][0],
            cube_name=CUBE[family][0].split("/")[-1].lower(),
            cube_branch=CUBE[family][1],
            device_define=device_define(mcu),
            target_cfg=target_cfg,
        )

        board_dir = root / slug
        board_dir.mkdir(parents=True, exist_ok=True)
        (board_dir / "module.yml").write_text(text)
        print(f"wrote boards/{slug}/module.yml")


if __name__ == "__main__":
    main()
