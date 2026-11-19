#!/usr/bin/env python3
"""The board specs, and the check that refuses an image the part cannot start.

Two halves, both host-only:

- Every board target spec names the image check (cmake/image-check.cmake) as its
  program fragment; every supported board gives a memory map; every memory map's
  flash starts at the STM32 flash base; and every startup or system source a
  board preset names exists in the vendor repository it names, read from that
  repository's own tree (`git cat-file -e`), so a sparse checkout that has not
  materialised a file does not hide a name the vendor never shipped.

- The image check itself, run with `cmake -P` on small ELFs linked here with the
  cross toolchain: one that starts (vector table at the flash origin, reset
  vector and entry at Reset_Handler, initial SP at _estack) passes; one linked
  without a startup file or linker script — what nucleo-f429zi produced, entry
  defaulted to 0x8000 — fails; one whose table is not at the flash origin fails;
  one whose reset vector is not Reset_Handler fails. Skipped, saying so, when
  arm-none-eabi-gcc is not installed.

Exit 0 when everything holds; 1 with the failures listed.
"""

# @verifies STM-BOARD-0004, STM-BOARD-0005

import glob
import json
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CHECK = os.path.join(ROOT, "cmake", "image-check.cmake")
FAILURES = []


def fail(message):
    FAILURES.append(message)


def supported_boards():
    """hardware.supported from module.yml, read without a YAML library."""
    boards, inside = [], False
    with open(os.path.join(ROOT, "module.yml")) as manifest:
        for line in manifest:
            stripped = line.strip()
            if stripped.startswith("supported:"):
                inside = True
                continue
            if inside:
                if stripped.startswith("- "):
                    boards.append(stripped[2:].strip())
                elif stripped and not stripped.startswith("#"):
                    break
    return boards


def vendor_repos(board_dir):
    """name -> path of the vendor repositories a board's module.yml declares."""
    repos, name = {}, None
    with open(os.path.join(board_dir, "module.yml")) as manifest:
        for line in manifest:
            stripped = line.strip()
            if stripped.startswith("- name:"):
                name = stripped.split(":", 1)[1].strip()
            elif stripped.startswith("path:") and name:
                repos[name] = stripped.split(":", 1)[1].strip()
                name = None
    return repos


def check_specs():
    supported = set(supported_boards())
    if not supported:
        fail("module.yml: hardware.supported names no boards")
    for spec_path in sorted(glob.glob(os.path.join(ROOT, "boards", "*", "target.json"))):
        board = os.path.basename(os.path.dirname(spec_path))
        with open(spec_path) as handle:
            spec = json.load(handle)
        if spec.get("program") != "../../cmake/image-check.cmake":
            fail(f"{board}: target.json does not name ../../cmake/image-check.cmake as its program")
        memory = spec.get("memory")
        if board in supported and not memory:
            fail(f"{board}: supported, but its target spec gives no memory map")
        if memory:
            flash, ram = memory.get("flash", {}), memory.get("ram", {})
            if flash.get("origin", "").lower() != "0x08000000":
                fail(f"{board}: flash origin {flash.get('origin')!r} is not the STM32 flash base 0x08000000")
            for region in ("flash", "ram"):
                try:
                    if int(memory[region]["length"], 16) <= 0:
                        raise ValueError
                except (KeyError, ValueError):
                    fail(f"{board}: memory.{region} has no length")
            if ram and not ram.get("origin", "").lower().startswith("0x2"):
                fail(f"{board}: RAM origin {ram.get('origin')!r} is not in the SRAM/DTCM space")

    vendor = os.path.join(ROOT, "vendor")
    for preset_path in sorted(glob.glob(os.path.join(ROOT, "boards", "*", "presets", "*.json"))):
        board_dir = os.path.dirname(os.path.dirname(preset_path))
        board = os.path.basename(board_dir)
        with open(preset_path) as handle:
            preset = json.load(handle)
        repos = vendor_repos(board_dir)
        for dependency, override in preset.get("override", {}).items():
            repo = os.path.join(vendor, repos.get(dependency, "-"))
            if not os.path.isdir(os.path.join(repo, ".git")) and not os.path.isfile(os.path.join(repo, ".git")):
                continue  # this family's vendor tree is not fetched here
            for source in override.get("src", []):
                found = subprocess.run(["git", "-C", repo, "cat-file", "-e", f"HEAD:{source}"],
                                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode == 0
                if not found:
                    fail(f"{board}: preset names {dependency}:{source}, which the vendor repository does not have")


STARTUP = """
    .syntax unified
    .thumb
    .section .isr_vector,"a",%progbits
    .global g_pfnVectors
g_pfnVectors:
    .word _estack
    .word {reset}
    .text
    .global Reset_Handler
    .type Reset_Handler, %function
    .thumb_func
Reset_Handler:
    b .
"""

LINKER = """
MEMORY
{{
    FLASH (rx): ORIGIN = {flash}, LENGTH = 0x10000
    RAM (rw): ORIGIN = 0x20000000, LENGTH = 0x1000
}}
_estack = ORIGIN(RAM) + LENGTH(RAM);
ENTRY(Reset_Handler)
SECTIONS
{{
    .isr_vector : {{ KEEP(*(.isr_vector)) }} > FLASH
    .text : {{ *(.text*) }} > FLASH
}}
"""


def run_check(elf, origin="0x08000000"):
    result = subprocess.run(
        ["cmake", f"-DELF={elf}", "-DREADELF=arm-none-eabi-readelf", "-DNM=arm-none-eabi-nm",
         f"-DFLASH_ORIGIN={origin}", "-P", CHECK],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    return result.returncode == 0, result.stdout


def link(work, name, reset="Reset_Handler", flash="0x08000000", script=True, startup=True):
    sources, args = [], []
    if startup:
        asm = os.path.join(work, name + ".s")
        with open(asm, "w") as handle:
            handle.write(STARTUP.format(reset=reset))
        sources.append(asm)
    else:
        c = os.path.join(work, name + ".c")
        with open(c, "w") as handle:
            handle.write("int main(void) { return 0; }\n")
        sources.append(c)
    if script:
        ld = os.path.join(work, name + ".ld")
        with open(ld, "w") as handle:
            handle.write(LINKER.format(flash=flash))
        args += ["-T", ld]
    elf = os.path.join(work, name + ".elf")
    subprocess.run(["arm-none-eabi-gcc", "-mcpu=cortex-m4", "-mthumb", "-nostartfiles", "-nostdlib",
                    *args, *sources, "-o", elf],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return elf


def check_image_check():
    if not shutil.which("arm-none-eabi-gcc") or not shutil.which("cmake"):
        print("skip: the image check needs arm-none-eabi-gcc and cmake on PATH")
        return
    with tempfile.TemporaryDirectory() as work:
        good = link(work, "good")
        ok, output = run_check(good)
        if not ok:
            fail("image check refused an image that starts:\n" + output)

        cases = [
            ("no startup and no linker script (entry defaulted to 0x8000)",
             link(work, "nostartup", startup=False, script=False), "0x08000000", "Reset_Handler is not defined"),
            ("vector table not at the flash origin",
             link(work, "moved", flash="0x08004000"), "0x08000000", "not at the flash origin"),
            ("reset vector not Reset_Handler",
             link(work, "badvector", reset="0"), "0x08000000", "reset vector"),
        ]
        for what, elf, origin, expected in cases:
            ok, output = run_check(elf, origin)
            if ok:
                fail(f"image check passed an image that cannot start: {what}")
            elif expected not in output:
                fail(f"image check refused {what}, but not for the reason expected ({expected!r}):\n{output}")
            elif os.path.exists(elf):
                fail(f"image check refused {what} but left the image in place for the next build to skip")


def main():
    check_specs()
    check_image_check()
    if FAILURES:
        for message in FAILURES:
            print("FAIL:", message)
        return 1
    print("ok: board specs and the image check")
    return 0


if __name__ == "__main__":
    sys.exit(main())
