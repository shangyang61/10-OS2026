#!/usr/bin/env python3
"""Check preserved framework/grade code against the archived official download."""
from hashlib import sha256
import json
from pathlib import Path
import re
import zipfile
from lab2_function_extract import extract_function

ROOT = Path(__file__).resolve().parents[1]


def main():
    checks = []

    def check(label, value):
        checks.append({"check": label, "passed": bool(value)})
        print(("PASS " if value else "FAIL ") + label)

    manifest = json.loads((ROOT / "specs/upstream_manifest.json").read_text())
    archive = ROOT / "reference/lab3_original.zip"
    check("official archive SHA256", sha256(archive.read_bytes()).hexdigest() == manifest["archive_sha256"])
    with zipfile.ZipFile(archive) as z:
        def original(name):
            return z.read("lab3_new/" + name).decode().replace("\r\n", "\n")

        for name in ["tools/inject.py", "tools/grade.sh", "tools/test_lab3.c",
                     "tools/test_lab3.h", "tools/test_lab3_context.S", "kern/trap/trap.h"]:
            check(name + " unchanged", (ROOT / name).read_text() == original(name))
        old = original("kern/trap/trapentry.S")
        new = (ROOT / "kern/trap/trapentry.S").read_text()
        macro = r"\.macro SAVE_ALL\b.*?\.endm"
        check("SAVE_ALL unchanged", re.search(macro, old, re.S).group() == re.search(macro, new, re.S).group())
        old = original("kern/trap/trap.c")
        new = (ROOT / "kern/trap/trap.c").read_text()
        for name in ("print_ticks", "idt_init", "trap_in_kernel", "print_trapframe", "print_regs", "trap_dispatch", "trap"):
            check("trap.c:" + name + " unchanged", extract_function(new, name) == extract_function(old, name))
    for item in json.loads((ROOT / "specs/lab2_protected_functions.json").read_text()):
        text = (ROOT / item["file"]).read_text()
        check("Lab2 " + item["function"] + " preserved",
              sha256(extract_function(text, item["function"]).encode()).hexdigest() == item["sha256"])
    pending = []
    for directory in ("kern", "libs"):
        for path in (ROOT / directory).rglob("*"):
            if path.is_file() and re.search(r"(?m)^\s*(?:#|//|/\*)\s*lab[23][^\n]*YOUR CODE", path.read_text(errors="replace")):
                pending.append(str(path.relative_to(ROOT)))
    check("no unfilled lab2/lab3 YOUR CODE task markers", not pending)
    logdir = ROOT / "logs"
    logdir.mkdir(exist_ok=True)
    (logdir / "contracts.json").write_text(json.dumps(checks, indent=2) + "\n")
    raise SystemExit(0 if all(c["passed"] for c in checks) else 1)


if __name__ == "__main__":
    main()
