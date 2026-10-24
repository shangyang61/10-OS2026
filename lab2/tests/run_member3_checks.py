#!/usr/bin/env python3
"""Member-three integration checks: Buddy page manager plus independent tests.

Builds two isolated copies so the two compile-time profiles never share stale
objects:

  * PMM=buddy   MEMBER3_TESTS=1 : buddy_check, member3 buddy tests, SLUB tests
  * PMM=best_fit MEMBER3_TESTS=1 : member3 SLUB tests on the reference manager

Why two copies: PMM is a compile-time define, and the existing Makefile does
not track command-line defines as dependencies. Reusing one tree would risk
linking objects compiled for the other manager.
"""
from pathlib import Path
import argparse
import datetime
import json
import os
import platform
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
LOGS = ROOT / "logs"

MARKERS = {
    "buddy": [
        "memory management: buddy_pmm_manager",
        "check_alloc_page() succeeded!",
        "member3_buddy_test() succeeded!",
        "member3_slub_test() succeeded!",
    ],
    "best_fit": [
        "memory management: best_fit_pmm_manager",
        "check_alloc_page() succeeded!",
        "member3_slub_test() succeeded!",
    ],
}


def copy_tree(path):
    shutil.copytree(
        ROOT, path,
        ignore=shutil.ignore_patterns(
            "obj", "bin", "logs", ".git", "__pycache__", "cscope*", "tags",
            ".qemu.out", ".gdb.in"),
    )


def run_profile(tree, args, profile, prefix):
    build_cmd = ["make", "-j2", f"PMM={profile}", "MEMBER3_TESTS=1"]
    build = subprocess.run(build_cmd, cwd=tree, stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT, timeout=180)
    (LOGS / f"{prefix}_{profile}_build.log").write_bytes(build.stdout)
    if build.returncode:
        print(build.stdout.decode(errors="replace"))
        raise RuntimeError(f"build failed for {profile}")

    cmd = [args.qemu, "-machine", "virt", "-nographic", "-bios", "default",
           "-device", f"loader,file=bin/ucore.img,addr=0x80200000"]
    proc = subprocess.Popen(cmd, cwd=tree, stdin=subprocess.DEVNULL,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    try:
        out, _ = proc.communicate(timeout=args.timeout)
    except subprocess.TimeoutExpired:
        proc.terminate()
        try:
            out, _ = proc.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
            out, _ = proc.communicate()
    text = out.decode(errors="replace")
    (LOGS / f"{prefix}_{profile}_qemu.log").write_text(text, encoding="utf-8")

    if "panic" in text.lower():
        print(text)
        raise RuntimeError(f"{profile}: kernel panic")
    missing = [m for m in MARKERS[profile] if m not in text]
    if missing:
        print(text)
        raise RuntimeError(f"{profile}: missing markers {missing}")
    print(f"PASS {profile}: all member-three markers present")
    return {"profile": profile, "markers": MARKERS[profile], "result": "PASS"}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--qemu", default=os.environ.get("QEMU",
                        "qemu-system-riscv64"))
    parser.add_argument("--timeout", type=int, default=25)
    parser.add_argument("--log-prefix", default="member3")
    args = parser.parse_args()

    if not shutil.which(args.qemu):
        raise RuntimeError("QEMU not found: " + args.qemu)
    if not shutil.which("riscv64-unknown-elf-gcc"):
        raise RuntimeError("riscv64-unknown-elf-gcc not on PATH")

    LOGS.mkdir(exist_ok=True)
    env = {
        "timestamp_utc": datetime.datetime.now(
            datetime.timezone.utc).isoformat(),
        "system": platform.uname()._asdict(),
        "qemu": shutil.which(args.qemu),
        "compiler": shutil.which("riscv64-unknown-elf-gcc"),
        "profiles": ["buddy", "best_fit"],
    }
    for tool in (env["compiler"], env["qemu"]):
        env[tool] = subprocess.check_output(
            [tool, "--version"], text=True).splitlines()[0]
    (LOGS / f"{args.log_prefix}_environment.json").write_text(
        json.dumps(env, indent=2), encoding="utf-8")

    results = []
    with tempfile.TemporaryDirectory(prefix="lab2_member3_") as tmp:
        for profile in ("buddy", "best_fit"):
            tree = Path(tmp) / profile
            copy_tree(tree)
            results.append(run_profile(tree, args, profile, args.log_prefix))

    (LOGS / f"{args.log_prefix}_results.json").write_text(
        json.dumps(results, indent=2), encoding="utf-8")
    print("All member-three checks passed. Logs:", LOGS)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.SubprocessError) as exc:
        print("FAIL:", exc, file=sys.stderr)
        sys.exit(1)
