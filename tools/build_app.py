#!/usr/bin/env python3
"""
Build a RoboHub application for a target, from what the manifests say.

Nothing here is typed by hand. The include paths, the sources, the defines and
the toolchain flags are read from the manifests of the application, of every
platform and interface module it depends on, and of the target spec — the same
declarations the toolchain reads, read by the same rules, so a build here and a
build through the toolchain cannot differ.

Every source is named. A directory is never scanned: a folder holding a module
this build should not include is the reason, and a file that appeared in it
yesterday should not be in today's firmware because it is in the same directory
as one that is.

SPDX-License-Identifier: MIT
"""

import argparse
import json
import os
import re
import subprocess
import sys


def load_manifest(path):
    """Read the fields of a manifest that describe a build.

    A small reader rather than a general YAML one: the build needs four fields
    from the build section and the dependencies, and a build that understood
    every part of the file would be a second implementation of the manifest
    format, free to disagree with the toolchain's.
    """
    if not os.path.isfile(path):
        return None
    with open(path) as handle:
        text = handle.read()

    out = {"include_dirs": [], "sources": [], "source_dirs": [], "defines": [],
           "depends": [], "name": None, "toolchain": None,
           }

    def section(name):
        match = re.search(r"^%s:\s*$" % re.escape(name), text, re.M)
        if not match:
            return None
        start = match.end()
        lines = []
        for line in text[start:].split("\n"):
            if line.strip() and not line.startswith((" ", "\t", "-")):
                break
            lines.append(line)
        return "\n".join(lines)

    build = section("build")
    if build:
        for key in ("include_dirs", "sources", "source_dirs", "defines"):
            block = re.search(r"^  %s:\s*$" % key, build, re.M)
            if not block:
                continue
            for line in build[block.end():].split("\n"):
                if line.strip() and not line.startswith("    - "):
                    break
                item = line.strip()[2:].strip()
                if item:
                    out[key].append(item)
        tool = re.search(r"^  toolchain:\s*(\S+)\s*$", build, re.M)
        if tool:
            out["toolchain"] = tool.group(1)
        # The board's sources, keyed by the target they belong to.
        boards = re.search(r"^  board_sources:\s*$", build, re.M)
        if boards:
            target = None
            for line in build[boards.end():].split("\n"):
                if not line.strip():
                    continue
                if not line.startswith("    "):
                    break
                if line.startswith("    - "):
                    out["board_sources"].setdefault(target or "", []).append(
                        line.strip()[2:].strip())
                elif line.rstrip().endswith(":") and line.startswith("    ") \
                        and not line.startswith("      "):
                    target = line.strip()[:-1].strip()

    deps = section("depends")
    if deps:
        for block in re.finditer(
                r"- name:\s*(\S+).*?(?=\n  - name:|\Z)", deps, re.S):
            name = block.group(1)
            path = re.search(r"^\s+path:\s*(\S+)", block.group(0), re.M)
            out["depends"].append({"name": name,
                                   "path": path.group(1) if path else None})
    return out


def expand(paths, root):
    """Turn declared paths into absolute ones, relative to the module that
    declared them — never to the directory the build happens to run in."""
    resolved = []
    for path in paths:
        if path.startswith("${"):
            continue  # a CMake substitution, which this build does not evaluate
        full = path if os.path.isabs(path) else os.path.normpath(os.path.join(root, path))
        if os.path.isdir(full):
            resolved.append(full)
        elif os.path.isfile(full):
            resolved.append(full)
    return resolved


def collect_sources(manifests):
    """Every source, from the explicit lists only.

    A declared source directory contributes its C files, because a module that
    names a directory has said the files in it are its sources. A module that
    does not name a directory contributes only what it listed, so a file that
    happens to sit beside a listed one is not compiled.
    """
    sources = []
    seen = set()
    for manifest in manifests:
        root = manifest["root"]
        for path in manifest["sources"]:
            full = os.path.normpath(os.path.join(root, path))
            if full.endswith(".c") and os.path.isfile(full) and full not in seen:
                seen.add(full)
                sources.append(full)
        for directory in manifest["source_dirs"]:
            full = os.path.normpath(os.path.join(root, directory))
            if not os.path.isdir(full):
                continue
            for entry in sorted(os.listdir(full)):
                if entry.endswith(".c"):
                    candidate = os.path.join(full, entry)
                    if candidate not in seen:
                        seen.add(candidate)
                        sources.append(candidate)
    return sources


def write_config(app_root, fragments, out_dir):
    """Resolve the application's configuration into a header the sources use.

    The application says which peripherals it needs in its `project.conf`; the
    fragments say which symbols exist and what they default to; the generator
    merges the two. What comes out is a header of `#define`s, and a driver that
    guards itself on its own symbol is compiled or not according to it.

    That is what makes "this build does not need I2C" something a file says
    rather than something somebody removed from a list — the driver stays in the
    module and out of the firmware.
    """
    generator = os.path.join(ROOT_HINT, "tools", "kconfig", "kconfig_gen.py")
    if not os.path.isfile(generator):
        return None
    conf = os.path.join(app_root, "project.conf")
    inputs = [f for f in fragments if os.path.isfile(f)]
    if not inputs:
        return None
    out = os.path.join(out_dir, "generated")
    os.makedirs(out, exist_ok=True)
    header = os.path.join(out, "kconfig.h")

    command = ["python3", generator, "--config", conf] + inputs + [header] \
        if os.path.isfile(conf) else ["python3", generator] + inputs + [header]
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:
        print(result.stderr.strip()[:500], file=sys.stderr)
        return None
    return header


def write_hal_config(spec, vendor_roots, out_dir):
    """Write the HAL configuration header the vendor's headers need.

    The vendor ships a template with every clock macro the HAL expects, and a
    hand-written one is how a build ends up missing HSI_VALUE and failing on a
    line in the middle of the vendor's clock code that says nothing about what
    was forgotten. So the template is the base, and only what this part actually
    differs on is changed.

    It is written into the build, not kept in the application: it is derived from
    the part and from the vendor's own file, so a checkout of the application
    carries neither.
    """
    for root in vendor_roots:
        template = os.path.join(root, "hal", "Inc",
                                "stm32%sxx_hal_conf_template.h"
                                % spec.get("core", "cortex-m7").replace("cortex-m", ""))
        # The template is named for the family, and the family is what the spec
        # declares: a part's define says which part, and the part's family is not
        # derivable from it — STM32H753xx is in STM32H7, and reducing one to the
        # other gives "h753", which is not a family anything ships.
        family = spec.get("family", "").replace("STM32", "").lower()
        # Each vendor root is the repository it names, so the HAL's template is
        # directly under it. The `hal/Inc/` shape is tried too because a build may
        # be handed a parent directory rather than the repository.
        candidates = [template]
        if family:
            candidates.insert(0, os.path.join(
                root, "Inc", "stm32%sxx_hal_conf_template.h" % family))
        for candidate in candidates:
            if not os.path.isfile(candidate):
                continue
            with open(candidate) as handle:
                text = handle.read()
            # The oscillator the board is fitted with is the one thing every
            # project gets wrong, because the template states the vendor's
            # reference value and the board has something else.
            clock = next((d for d in spec.get("defines", []) if d.startswith("HSE_VALUE")), None)
            if clock:
                import re as _re
                text = _re.sub(r"#define HSE_VALUE\s+\d+U",
                               "#define HSE_VALUE " + clock.split("=")[1], text)
            text = text.replace("stm32%sxx_hal_conf_TEMPLATE_H"
                                 % family.upper(), "STM32%sxx_HAL_CONF_H" % family.upper())
            destination = os.path.join(out_dir, "stm32%sxx_hal_conf.h" % family)
            with open(destination, "w") as handle:
                handle.write(text)
            return destination
    return None


def generate_target_files(specs_dir, target, out_dir):
    """Ask rho-target for the files the target contributes: the toolchain, the
    flags and the memory map, and the linker script generated from that map.

    They are generated here rather than expected to be lying around, because a
    linker script from a previous target is a program linked against the wrong
    memory map — and it is wrong in a way that shows up as a board that boots and
    then does nothing.
    """
    result = subprocess.run(["rho", "target", "generate", out_dir,
                             "--target", target, "--specs", specs_dir],
                            capture_output=True, text=True)
    if result.returncode != 0:
        print(result.stderr.strip()[:800], file=sys.stderr)
        return None
    return os.path.join(out_dir, "cmake", "linker.ld")


def vendor_sources(resolution, vendor_roots):
    """The C sources the preset's repositories contribute, named by the preset.

    A directory is not scanned. The preset says which paths of each repository
    take part — the CMSIS device repository's `Source/Templates/gcc`, which is
    where the startup assembly is — and a repository folder holds hundreds of
    files that belong to other parts and other configurations.
    """
    sources = []
    # A board narrows what it inherits, so a repository's sources are the ones the
    # preset actually names: the family's, unless the board overrode them, in
    # which case the board's. Both come from the same resolution, so the build and
    # the reader of the preset cannot disagree about what was fetched.
    override = resolution.get("override") or {}
    for repository in resolution.get("repositories", []):
        # The repository name matches on both sides; the path lists do not, so
        # they are looked up in the shape each was serialised in.
        narrowed = override.get(repository.get("Name")) or {}
        patterns = narrowed.get("src") or repository.get("Src") or []
        for pattern in patterns:
            for root in vendor_roots:
                candidate = os.path.join(root, pattern)
                if os.path.isfile(candidate):
                    sources.append(candidate)
                elif os.path.isdir(candidate):
                    for entry in sorted(os.listdir(candidate)):
                        # `*_template.c` are alternatives a project picks between
                        # — the RTC and the timer timebases, for one — and each
                        # defines the same functions. Compiling them all is several
                        # definitions of HAL_SuspendTick, which is a link failure
                        # that says nothing about which of them was meant. They are
                        # chosen by a project that needs a different timebase, by
                        # naming that one file.
                        if entry.endswith(("_template.c",)):
                            continue
                        if entry.endswith((".c", ".s", ".S")):
                            sources.append(os.path.join(candidate, entry))
                    break
    return sources


def load_spec(path, name):
    """The target's spec: the compiler, the flags, the defines, and how it is
    programmed. The spec is named by the target flag and lives in the platform
    that owns the hardware."""
    for candidate in (os.path.join(path, name + ".json"),):
        if os.path.isfile(candidate):
            with open(candidate) as handle:
                return json.load(handle)
    raise SystemExit("no target spec named %q in %s" % (name, path))


# ROOT_HINT is where this script lives, so the generator beside it is found
# without the caller having to say where the module is.
ROOT_HINT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", required=True, help="the application directory")
    parser.add_argument("--target", required=True, help="the target to build for")
    parser.add_argument("--specs", required=True, help="where the target specs are")
    parser.add_argument("--vendor", required=True,
                        help="comma-separated roots of the fetched vendor sources")
    parser.add_argument("--out", default="build", help="where to write the firmware")
    args = parser.parse_args()

    app_root = os.path.abspath(args.app)
    app = load_manifest(os.path.join(app_root, "module.yml"))
    if app is None:
        raise SystemExit("no module.yml in %s" % app_root)
    app["root"] = app_root

    # The application's dependencies, then their dependencies: every module whose
    # sources take part in this build, in the order they are named, so that a
    # module's own sources precede the ones that use them.
    manifests = [app]
    queue = [app]
    while queue:
        current = queue.pop(0)
        for dep in current["depends"]:
            if not dep["path"]:
                continue
            root = dep["path"] if os.path.isabs(dep["path"]) \
                else os.path.normpath(os.path.join(current["root"], dep["path"]))
            manifest = load_manifest(os.path.join(root, "module.yml"))
            if manifest is None:
                print("  note: %s has no module.yml; skipped" % dep["name"], file=sys.stderr)
                continue
            manifest["root"] = root
            if any(m["root"] == root for m in manifests):
                continue
            manifests.append(manifest)
            queue.append(manifest)

    # The preset, which names the repositories the C side is fetched from and the
    # target that builds it. Resolving it here is what makes "the HAL and CMSIS
    # for the family this program named" a fact rather than a flag.
    resolution = {}
    preset_json = subprocess.run(
        ["rho", "cimport", "resolve", "--preset", args.target,
         "--project", args.app], capture_output=True, text=True)
    if preset_json.returncode == 0:
        resolution = json.loads(preset_json.stdout)
        print("preset:     %s (%s, %s)" % (resolution.get("preset"),
              resolution.get("mcu", "?"), resolution.get("core", "?")))
    else:
        print("warning: the preset could not be resolved; building what is on disk",
              file=sys.stderr)

    vendor_roots = [os.path.abspath(v) for v in args.vendor.split(",")]

    spec = load_spec(args.specs, args.target)
    toolchain = spec.get("toolchain", {})
    compiler = toolchain.get("compiler", "arm-none-eabi-gcc")
    core = spec.get("core", "cortex-m7")

    # What the modules contribute. A Rho application has no C of its own: its C is
    # generated from its sources and belongs to the build, so a file named here
    # would be a build looking for something a Rho project never had.
    sources = collect_sources(manifests)

    # The board's support package, found from the board.
    #
    # The target named the board, and the board's facts — which UART is the
    # console, which pin an LED is on — live in its own directory. So a build that
    # is for a board picks that board's package up and a build that is not does
    # not, with nothing declaring which is which: the preset said, and the build
    # followed. Declaring it as well would be a second place for the board a build
    # is for and the board it compiles to disagree.
    # The vendor's own sources, named by the preset.
    sources += vendor_sources(resolution, vendor_roots)

    # The application's own sources. A Rho program has none of its own: its C is
    # generated. Anything a hand-written application carries, it declares, and it
    # is compiled like every other module's.
    for source in sorted(os.listdir(os.path.join(args.app, "src"))) \
            if os.path.isdir(os.path.join(args.app, "src")) else []:
        if source.endswith(".c"):
            full = os.path.join(args.app, "src", source)
            if full not in sources:
                sources.append(full)

    board_dirs = []
    for manifest in manifests:
        candidate = os.path.join(manifest["root"], "boards", args.target, "bsp")
        if os.path.isdir(candidate):
            board_dirs.append(candidate)
            for entry in sorted(os.listdir(candidate)):
                if entry.endswith(".c"):
                    full = os.path.join(candidate, entry)
                    if full not in sources:
                        sources.append(full)
    if board_dirs:
        print("board bsp:  %s" % ", ".join(
            os.path.relpath(d, os.getcwd()) for d in board_dirs))

    # Include paths: the vendor roots first, then each module's, in declaration
    # order, so that a module's own headers win over another's.
    includes = []
    # Each vendor root is the repository it names, and the three repositories have
    # different layouts: the Cube repository keeps CMSIS under Drivers, the device
    # repository under Include and Source/Templates, the HAL repository at the top.
    # Every layout that is there is offered, rather than one guessed per vendor —
    # a guess that is right for one repository is a missing include for another.
    for root in vendor_roots:
        for sub in ("Drivers/CMSIS/Core/Include", "Drivers/CMSIS/Include",
                    "Source/Templates", "Include", "Inc", "Src"):
            candidate = os.path.join(root, sub)
            if os.path.isdir(candidate):
                includes.append(candidate)
    # The build's own directory: the generated configuration header lives in it.
    includes.insert(0, os.path.abspath(args.out))
    for manifest in manifests:
        includes += expand(manifest["include_dirs"], manifest["root"])
    # The board's own directory, so its header is reached as "bsp.h" rather than
    # by the path to it — the same name a program would use.
    includes += board_dirs
    # Deduplicated, keeping order.
    includes = list(dict.fromkeys(includes))

    defines = list(spec.get("defines", []))
    for manifest in manifests:
        defines += manifest["defines"]

    # The family, as one symbol. The spec says which family the part is in, and
    # the sources guard on it: the vendor's device headers name the part and
    # never the family, so a guard written against a family macro the headers do
    # not define never matches and every family takes the fallback silently.
    family = spec.get("family", "").replace("STM32", "").upper()
    if family:
        defines.append("ROBOHUB_STM32_FAMILY_%s=1" % family)
        print("family:     %s" % family)

    flags = ["-mcpu=" + core, "-mthumb", "-ffunction-sections", "-fdata-sections"]
    flags += toolchain.get("flags", [])
    flags += spec.get("flags", [])

    print("target:   %s (%s, %s)" % (args.target, spec.get("mcu", "?"), core))
    print("compiler: %s" % compiler)
    print("modules:  %s" % ", ".join(os.path.basename(m["root"]) for m in manifests))
    print("includes: %d" % len(includes))
    print("sources:  %d" % len(sources))

    os.makedirs(args.out, exist_ok=True)

    # The target's own files, from the component that owns target specs.
    generate_target_files(args.specs, args.target, args.out)

    # The configuration: the fragments every module in the graph contributes,
    # merged with what this application asks for.
    fragments = []
    for manifest in manifests:
        for fragment in manifest.get("kconfig_fragments", []):
            path = fragment if os.path.isabs(fragment) \
                else os.path.join(manifest["root"], fragment)
            fragments.append(path)
    config = write_config(args.app, fragments, args.out)
    if config:
        print("config:     %s" % os.path.relpath(config, os.getcwd()))

    # The HAL configuration header, written from the vendor's own template into
    # the build. It is on the include path because the build directory is.
    conf = write_hal_config(spec, vendor_roots, args.out)
    if conf:
        print("hal conf: %s" % os.path.relpath(conf, os.getcwd()))

    objects = []
    # The configuration is included first in every source, so a driver can guard
    # itself on its own symbol without including anything to find out whether it
    # was asked for.
    common = ["-c"]
    if config:
        common += ["-include", config]
    common += ["-I" + i for i in includes] + \
              ["-D" + d for d in defines] + flags + ["-O1", "-Wall"]
    for source in sources:
        obj = os.path.join(args.out,
                           os.path.basename(source).replace("/", "_") + ".o")
        if not os.path.isfile(source):
            print("  missing source: %s" % source, file=sys.stderr)
            continue
        if source.endswith((".s", ".S")):
            # Assembly is not compiled, and the flags that describe how to compile
            # it do not apply to it. The startup assembly is the whole of what
            # gives a bare-metal program an entry point.
            assemble = ["-c", "-x", "assembler-with-cpp"]
            assemble += [f for f in flags if f.startswith(("-mcpu", "-mthumb", "-mfloat", "-mfpu"))]
            assemble += ["-I" + i for i in includes] + ["-D" + d for d in defines]
            command = [compiler] + assemble + [source, "-o", obj]
        else:
            command = [compiler] + common + [source, "-o", obj]
        result = subprocess.run(command, capture_output=True, text=True)
        if result.returncode != 0:
            sys.stderr.write(result.stderr[:3000])
            raise SystemExit("compiling %s failed" % source)
        objects.append(obj)
        for warning in result.stderr.split("\n"):
            if "warning:" in warning:
                print("  " + warning.strip())

    # The linker script, from the component that owns target specs. Generating it
    # here rather than as a separate step means a build cannot be run against a
    # stale script from a previous target: the map and the code are produced by
    # the same command from the same spec.
    linker = os.path.join(args.out, "cmake", "linker.ld")
    if not os.path.isfile(linker):
        print("no linker script was generated for %s" % args.target, file=sys.stderr)
        return 1
    elf = os.path.join(args.out, "firmware.elf")
    link = [compiler, "-T" + linker, "--specs=nano.specs", "--specs=nosys.specs",
            "-Wl,--gc-sections", "-o", elf] + objects
    result = subprocess.run(link, capture_output=True, text=True)
    noise = re.compile(r"not implemented and will always fail|warning: _")
    for line in result.stderr.split("\n"):
        if line.strip() and not noise.search(line):
            print("  " + line.strip())
    if result.returncode != 0 or not os.path.isfile(elf):
        raise SystemExit("linking failed")

    binary = os.path.join(args.out, "firmware.bin")
    subprocess.run([toolchain.get("objcopy", "arm-none-eabi-objcopy"),
                    "-O", "binary", elf, binary], check=True)
    size = subprocess.run([toolchain.get("size", "arm-none-eabi-size"), elf],
                          capture_output=True, text=True)
    print(size.stdout.strip())
    print("\nfirmware: %s" % binary)
    print("flash:    %s" % spec.get("flash", {}).get("command", "(none declared)"))


if __name__ == "__main__":
    main()
