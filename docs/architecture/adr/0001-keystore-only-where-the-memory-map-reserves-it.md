---
id: ADR-0001
title: Keep the device key in flash only on boards whose memory map reserves it
status: proposed
date: 2026-11-14
requirements: [STM-0004, STM-BOARD-0002, STM-KEY-0001, STM-KEY-0002, SYS-0022]
supersedes: []
superseded_by: []
---

# Keep the device key in flash only on boards whose memory map reserves it

## Context

A board's key is rotated over its session without flashing it
(robohub:ADR-0010), so the rotated key must survive a reset. An STM32 has no
EEPROM, and its flash is erased a sector at a time, with sector sizes that
differ widely by family: 128 KB at the end of an F4's flash with the small
sectors at the start where the vector table is, 256 KB on an F7, small uniform
pages on L4/L5/U5/H5 whose HALs are not vendored here. Erasing the bank the
code runs from stalls it. Most board specs carry no memory map, so nothing
generates a linker script that could leave room.

## Decision

The flash keystore is implemented for dual-bank STM32H7 parts only: the last
two sectors of bank 2, written while the image runs from bank 1. A board opts in
by shortening `memory.flash.length` in its target spec by those two sectors and
defining `RH_KEYSTORE_STM32_FLASH=1`; today that is the nucleo-h753zi. The
source refuses to compile for another part, and refuses at run time to erase
when the image reaches the keystore. Every other board links robohub-common's
weak "no store" default and answers a REKEY "cannot rotate".

## Consequences

- On every board but the H753ZI a key cannot be rotated without reflashing; the
  gateway is told so rather than losing the key at the next reset.
- Adding a dual-bank H7 board is a target-spec change: a memory map less two
  sectors and the define.
- F2/F4/F7 boards need the image relinked around a hole at the start of flash,
  and L4/L5/U5/H5 boards need their HALs vendored, before they can join; either
  is a new decision.
