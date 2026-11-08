#!/usr/bin/env python3
"""Display stored integration results and relevant raw kernel log lines."""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
results = json.loads((ROOT / "logs/checks.json").read_text())
for case in results:
    print("%s: %s" % (case["case"], "PASS" if case["passed"] else "FAIL"))
print("\nRaw course_128M_qemu.log:")
for line in (ROOT / "logs/course_128M_qemu.log").read_text().splitlines():
    if "[extra]" in line or "instruction at" in line or "Breakpoint at" in line or "Shutdown after" in line:
        print(line)
print("\nRaw buddy_128M_qemu.log:")
for line in (ROOT / "logs/buddy_128M_qemu.log").read_text().splitlines():
    if "member3 T" in line or "member3 SLUB T" in line or "succeeded!" in line:
        print(line)
print("\nSaved integration cases: %d/%d PASS" % (sum(c["passed"] for c in results), len(results)))
