#!/usr/bin/env python3
"""On-target health check for a Nucleo board (STM-0008, STM-0009).

Run with the board on USB (its ST-LINK's serial port at STM32_PORT,
/dev/ttyACM0 by default; under WSL, attached with usbipd), for the board in
STM32_TARGET (nucleo-f446ze by default):

    python3 health_check.py

Builds this example, flashes it, and reads the board's serial console across its
three boots — a watchdog reset, a recorded fault, then a fed watchdog, with the
keystore holding a key across each reset — until it prints `health-check:
passed` or a failure. The keystore then holds the check's keys: flash the board
from Mesh afterwards.
"""
import os
import subprocess
import sys
import time

import serial

HERE = os.path.dirname(os.path.abspath(__file__))
STM32 = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
WORKSPACE = os.environ.get("ROBOHUB_WORKSPACE", os.path.dirname(STM32))
PORT = os.environ.get("STM32_PORT", "/dev/ttyACM0")
TARGET = os.environ.get("STM32_TARGET", "nucleo-f446ze")


def main():
    env = dict(os.environ, ROBOHUB_WORKSPACE=WORKSPACE)
    for step in (["rho", "build", "-target", TARGET, "."],
                 ["rho", "flash", "-target", TARGET, "build/output/health_check"]):
        done = subprocess.run(step, cwd=HERE, env=env)
        if done.returncode != 0:
            print("FAILED:", " ".join(step))
            return 1
    end = time.time() + 120
    port, line = None, b""
    while time.time() < end:
        if port is None:
            try:
                port = serial.Serial(PORT, 115200, timeout=0.2)
            except (serial.SerialException, OSError):
                port = None
            if port is None:
                time.sleep(0.3)
                continue
        try:
            data = port.read(256)
        except (serial.SerialException, OSError):
            port = None
            continue
        for byte in data:
            if byte == 0x0A:
                text = line.decode("utf-8", "replace").rstrip()
                print(text, flush=True)
                if text.startswith("health-check: passed"):
                    print("OK: the health contracts and the keystore held on the board")
                    return 0
                if "FAILED" in text:
                    return 1
                line = b""
            else:
                line += bytes([byte])
    print("FAILED: no verdict within 120 s")
    return 1


if __name__ == "__main__":
    sys.exit(main())
