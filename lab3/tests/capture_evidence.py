#!/usr/bin/env python3
"""Capture actual xterm windows on a private Xvfb display; never draw fake output.

Requires Xvfb, xterm and Pillow. --runtime may point at a locally extracted
Debian package root, avoiding changes to the host's desktop or installed tools.
"""
import argparse
from pathlib import Path
import os
import shlex
import shutil
import subprocess
import tempfile
import time
from PIL import ImageGrab

ROOT = Path(__file__).resolve().parents[1]


def capture(xterm, env, name, commands, timeout):
    with tempfile.TemporaryDirectory(prefix="lab3-capture-") as temp:
        ready = Path(temp) / "ready"
        script = "set -o pipefail\ncd " + shlex.quote(str(ROOT)) + "\n"
        script += "printf 'Lab3 evidence | " + name + " | 2026-11-07\\n\\n'\n"
        script += commands + "\nstatus=$?\nprintf '\\nCommand exit status: %s\\n' \"$status\"\n"
        script += "printf '%s' \"$status\" > " + shlex.quote(str(ready)) + "\nread -r hold\n"
        process = subprocess.Popen([xterm, "-geometry", "142x49+0+0", "-fa", "DejaVu Sans Mono",
            "-fs", "12", "-bg", "#10151d", "-fg", "#e6edf3", "-T", "Lab3 " + name,
            "-e", "bash", "-c", script], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            deadline = time.monotonic() + timeout
            while not ready.exists():
                if process.poll() is not None or time.monotonic() > deadline:
                    raise RuntimeError("Terminal did not finish: " + name)
                time.sleep(0.2)
            time.sleep(0.5)
            # XGetImage reads the real terminal framebuffer, with no compositing.
            shot = ImageGrab.grab(xdisplay=env["DISPLAY"])
            path = ROOT / "screenshots" / (name + ".png")
            shot.save(path)
            print(str(path), flush=True)
            if ready.read_text() != "0":
                raise RuntimeError("Captured command failed: " + name)
        finally:
            process.terminate()
            process.wait(timeout=5)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--runtime", type=Path)
    parser.add_argument("--display", default=":92")
    parser.add_argument("--only", choices=("grade", "remaining"))
    args = parser.parse_args()
    env = os.environ.copy()
    if args.runtime:
        env["LD_LIBRARY_PATH"] = str(args.runtime / "usr/lib/x86_64-linux-gnu")
        xvfb, xterm = [str(args.runtime / "usr/bin" / name) for name in ("Xvfb", "xterm")]
    else:
        xvfb, xterm = [shutil.which(name) for name in ("Xvfb", "xterm")]
    env["DISPLAY"] = args.display
    (ROOT / "screenshots").mkdir(exist_ok=True)
    server = subprocess.Popen([xvfb, args.display, "-screen", "0", "1440x1000x24",
        "-nolisten", "tcp", "-ac"], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        time.sleep(0.5)
        if args.only != "remaining":
            capture(xterm, env, "01_official_grade",
                "echo '$ make grade'\nmake grade 2>&1 | tee logs/screenshot_grade.log\n"
                "rc=${PIPESTATUS[0]}\nprintf '\\nOfficial scoring summary:\\n'\n"
                "rg -- '  - .*: PASS|Total Score:' logs/screenshot_grade.log\n(exit \"$rc\")", 80)
        if args.only != "grade":
            capture(xterm, env, "02_timer_shutdown",
                "echo '$ make -j4 && timeout --foreground 25s make qemu'\nmake -j4 > logs/screenshot_build.log 2>&1 && "
                "timeout --foreground 25s make qemu 2>&1 | tee logs/screenshot_qemu.log", 35)
            capture(xterm, env, "03_gdb_trap_return",
                "echo '$ python3 tests/run_gdb.py'\npython3 tests/run_gdb.py", 35)
            capture(xterm, env, "04_extra_test_logs",
                "echo '$ python3 tests/summarize_results.py  # view saved raw test evidence'\n"
                "python3 tests/summarize_results.py", 10)
    finally:
        server.terminate()
        server.wait(timeout=5)


if __name__ == "__main__":
    main()
