#!/usr/bin/env python3
"""Run real RV64 kernels in isolated trees and retain unmodified console output."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
LOGS = ROOT / "logs"


def invoke(command, cwd, log, timeout=40):
    started = time.monotonic()
    with log.open("w") as stream:
        result = subprocess.run(command, cwd=cwd, stdout=stream,
                                stderr=subprocess.STDOUT, timeout=timeout)
    return result.returncode, round(time.monotonic() - started, 3)


def inject_probes(tree):
    target = tree / "kern/checklab3"
    target.mkdir()
    for name in ("trap_probe.c", "trap_probe_registers.S"):
        shutil.copyfile(ROOT / "tests" / name, target / name)
    makefile = tree / "Makefile"
    text = makefile.read_text()
    anchor = "KCFLAGS\t\t+="
    position = text.index(anchor)
    text = text[:position] + "KINCLUDE += kern/checklab3\nKSRCDIR += kern/checklab3\n\n" + text[position:]
    makefile.write_text(text)
    init = tree / "kern/init/init.c"
    text = init.read_text().replace("#include <dtb.h>", "#include <dtb.h>\nvoid lab3_probe_exceptions(void);\nvoid lab3_probe_sync(void);")
    position = text.rindex("    idt_init();")
    position = text.index("\n", position) + 1
    text = text[:position] + "    lab3_probe_exceptions();\n" + text[position:]
    text = text.replace("    intr_enable();  // enable irq interrupt", "    intr_enable();  // enable irq interrupt\n    lab3_probe_sync();")
    init.write_text(text)


def run_case(name, qemu, memory, options, probes):
    with tempfile.TemporaryDirectory(prefix="lab3-" + name + "-") as temp:
        tree = Path(temp) / "lab3"
        shutil.copytree(ROOT, tree, ignore=shutil.ignore_patterns(
            "obj", "bin", "logs", "screenshots", "reference", "__pycache__", ".qemu*"))
        if probes:
            inject_probes(tree)
        build_rc, _ = invoke(["make", "-j4", *options], tree, LOGS / (name + "_build.log"))
        if build_rc:
            raise RuntimeError(name + ": build failed; see logs")
        boot = ["-kernel", "bin/ucore.img"] if "PORTABLE_BOOT=1" in options else [
            "-device", "loader,file=bin/ucore.img,addr=0x80200000"]
        command = [qemu, "-machine", "virt", "-m", memory, "-smp", "1", "-nographic",
                   "-bios", "default", *boot, "-monitor", "none", "-no-reboot"]
        log = LOGS / (name + "_qemu.log")
        rc, elapsed = invoke(command, tree, log, timeout=25)
        text = log.read_text(errors="replace")
        checks = {
            "normal_qemu_exit": rc == 0,
            "pmm_self_check": "check_alloc_page() succeeded!" in text,
            "ten_tick_groups": text.splitlines().count("100 ticks") == 10,
            "shutdown_at_1000": text.count("Shutdown after 1000 ticks") == 1,
            "no_panic": "kernel panic" not in text.lower() and "assertion failed" not in text.lower(),
        }
        if probes:
            checks.update({
                "all_gpr_sp_return": "[extra] all GPRs, SP and S-mode return: PASS" in text,
                "breakpoint_16": "[extra] 16-bit breakpoint return: PASS" in text,
                "breakpoint_32": "[extra] 32-bit breakpoint return: PASS" in text,
                "illegal_16": "[extra] 16-bit illegal instruction return: PASS" in text,
                "illegal_32": "[extra] 32-bit illegal instruction return: PASS" in text,
                "nested_interrupt_state": "[extra] nested interrupt state and page wrappers: PASS" in text,
            })
        if "PMM=buddy" in options:
            checks["buddy_selected"] = "memory management: buddy_pmm_manager" in text
            checks["buddy_tests"] = "member3_buddy_test() succeeded!" in text and sum("member3 T" in line and ": PASS" in line for line in text.splitlines()) == 6
            checks["slub_tests"] = "member3_slub_test() succeeded!" in text and sum("member3 SLUB T" in line and ": PASS" in line for line in text.splitlines()) == 8
        result = {"case": name, "memory": memory, "options": options, "qemu_command": command,
                  "returncode": rc, "elapsed_seconds": elapsed, "checks": checks,
                  "passed": all(checks.values()), "log_sha256": hashlib.sha256(log.read_bytes()).hexdigest()}
        print(json.dumps(result, ensure_ascii=False), flush=True)
        return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--qemu", default=shutil.which("qemu-system-riscv64"))
    parser.add_argument("--modern-qemu", default="/usr/bin/qemu-system-riscv64")
    args = parser.parse_args()
    LOGS.mkdir(exist_ok=True)
    environment = {"platform": platform.platform(), "date": "2026-11-07",
                   "qemu": subprocess.check_output([args.qemu, "--version"], text=True).splitlines()[0],
                   "gcc": subprocess.check_output(["riscv64-unknown-elf-gcc", "--version"], text=True).splitlines()[0],
                   "gdb": subprocess.check_output(["gdb-multiarch", "--version"], text=True).splitlines()[0]}
    (LOGS / "environment.json").write_text(json.dumps(environment, indent=2) + "\n")
    cases = [("course_128M", args.qemu, "128M", [], True),
             ("course_64M", args.qemu, "64M", [], True),
             ("course_256M", args.qemu, "256M", [], False),
             ("buddy_128M", args.qemu, "128M", ["PMM=buddy", "MEMBER3_TESTS=1"], True),
             ("portable_128M", args.modern_qemu, "128M", ["PORTABLE_BOOT=1"], True)]
    results = []
    for case in cases:
        try:
            results.append(run_case(*case))
        except Exception as error:
            results.append({"case": case[0], "passed": False, "error": str(error)})
            print(str(error), flush=True)
    (LOGS / "checks.json").write_text(json.dumps(results, indent=2, ensure_ascii=False) + "\n")
    print("Integration cases: %d/%d PASS" % (sum(r["passed"] for r in results), len(results)), flush=True)
    raise SystemExit(0 if all(r["passed"] for r in results) else 1)


if __name__ == "__main__":
    main()
