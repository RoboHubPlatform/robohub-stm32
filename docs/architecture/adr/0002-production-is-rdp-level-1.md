---
id: ADR-0002
title: Production sets read-out protection level 1 and never level 2
status: proposed
date: 2026-11-14
requirements: [STM-0006, STM-PROD-0001, STM-PROD-0002]
supersedes: []
superseded_by: []
---

# Production sets read-out protection level 1 and never level 2

## Context

An STM32's flash, and with it the device keystore, can be read through the
debug port. The option bytes offer read-out protection: level 1 blocks reads
through the debug port and from RAM-loaded code and can be undone at the cost of
a mass erase; level 2 disables the debug port for good and freezes the option
bytes. The parts here have no secure boot comparable to the RP2350's or the
ESP32's that the platform could switch on from a build.

## Decision

`rho flash --production` follows `production/production.json`: write the
image as usual, then set RDP level 1 with STM32CubeProgrammer over SWD. The step
is marked reversible. Level 2 is not in any plan and never runs from this
toolchain; it is documented as a manual step for a board whose firmware can
update itself. `CONFIG_RH_PRODUCTION` changes nothing in an STM32 image.

## Consequences

- A production board's key and image cannot be read over SWD.
- An ordinary `rho flash` through OpenOCD cannot write a level-1 board until it
  is taken back to level 0, which erases it, keystore included.
- STM32CubeProgrammer becomes a tool the production flash needs; it is not in
  `system_deps`.
