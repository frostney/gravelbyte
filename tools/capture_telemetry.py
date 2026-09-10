#!/usr/bin/env python3
"""Capture PicoSystem USB telemetry using only the Python standard library."""
import argparse
import array
import fcntl
import glob
import os
from pathlib import Path
import select
import termios
import time
import tty

parser = argparse.ArgumentParser()
parser.add_argument("output", type=Path)
parser.add_argument("--seconds", type=float, default=110)
parser.add_argument("--port")
args = parser.parse_args()
# USB serial disappears briefly when picotool returns the device to firmware.
connect_deadline = time.monotonic() + 8
ports = ([args.port] if Path(args.port).exists() else []) if args.port else glob.glob("/dev/cu.usbmodem*")
while not ports and time.monotonic() < connect_deadline:
    time.sleep(0.1)
    ports = ([args.port] if Path(args.port).exists() else []) if args.port else glob.glob("/dev/cu.usbmodem*")
if len(ports) != 1:
    parser.error("Expected one USB serial port; use --port to select the PicoSystem")
args.output.parent.mkdir(parents=True, exist_ok=True)
fd = os.open(ports[0], os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
original = termios.tcgetattr(fd)
try:
    tty.setraw(fd)
    attrs = termios.tcgetattr(fd)
    attrs[4] = attrs[5] = termios.B115200
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    fcntl.ioctl(fd, termios.TIOCMBIS, array.array("i", [termios.TIOCM_DTR]))
    end = time.monotonic() + args.seconds
    with args.output.open("w") as log:
        while time.monotonic() < end:
            ready, _, _ = select.select([fd], [], [], min(1, max(0, end-time.monotonic())))
            if ready:
                try:
                    data = os.read(fd, 4096).decode("utf-8", errors="replace")
                except BlockingIOError:
                    continue
                if data:
                    log.write(data)
                    log.flush()
                    print(data, end="", flush=True)
finally:
    termios.tcsetattr(fd, termios.TCSANOW, original)
    os.close(fd)
