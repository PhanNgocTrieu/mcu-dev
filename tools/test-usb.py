#!/usr/bin/env python3
"""Drive usb-man over the unix socket: plug/unplug and print state lines."""
from __future__ import annotations

import argparse
import os
import socket
import sys
import time


def talk(runtime: str, command: str) -> None:
    path = os.path.join(runtime, "usb-manager.sock")
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.settimeout(3.0)
    sock.connect(path)
    sock.sendall((command + "\n").encode())
    buf = b""
    while True:
        chunk = sock.recv(4096)
        if not chunk:
            break
        buf += chunk
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            text = line.decode(errors="replace")
            print(text)
            if text.startswith("ok") or text.startswith("err"):
                sock.close()
                return
    sock.close()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--runtime", default=os.environ.get("HUPI_RUNTIME", "/tmp/hupi-sim"))
    ap.add_argument("command", nargs="+", help="e.g. sim plug android")
    args = ap.parse_args()
    talk(args.runtime, " ".join(args.command))
    return 0


if __name__ == "__main__":
    sys.exit(main())
