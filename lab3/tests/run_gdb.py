#!/usr/bin/env python3
"""Observe one real timer Trap and check the assembly/C return boundary."""
from pathlib import Path
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
qemu = shutil.which("qemu-system-riscv64")
subprocess.run(["make", "-j4"], cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
with (ROOT / "logs/gdb_qemu.log").open("w") as serial:
    process = subprocess.Popen([qemu, "-machine", "virt", "-m", "128M", "-nographic",
        "-bios", "default", "-device", "loader,file=bin/ucore.img,addr=0x80200000",
        "-monitor", "none", "-S", "-gdb", "tcp:127.0.0.1:1235"], cwd=ROOT,
        stdout=serial, stderr=subprocess.STDOUT)
    try:
        time.sleep(0.3)
        with (ROOT / "logs/gdb_trap.log").open("w") as log:
            result = subprocess.run(["gdb-multiarch", "-q", "-batch", "-x", "debug/trap.gdb"],
                cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, timeout=25)
        text = (ROOT / "logs/gdb_trap.log").read_text()
        print(text)
        if result.returncode or "[GDB] Trap frame, timer cause, SP, EPC and SIE restoration: PASS" not in text:
            raise SystemExit(1)
    finally:
        process.terminate()
        process.wait(timeout=5)
