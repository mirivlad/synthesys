#!/usr/bin/env python3
"""Validate HESS-40 numbered pin map in docs; Python stdlib only."""
from pathlib import Path
import re

text = (Path(__file__).resolve().parents[1] / "docs/hardware/04-hess-40pin-connector.md").read_text(encoding="utf-8")
pins = {}
for a, aname, b, bname in re.findall(
    r"^\|\s*(\d+)\s*\|\s*([^|]+)\|\s*[^|]+\|\s*(\d+)\s*\|\s*([^|]+)\|", text, re.M
):
    for number, name in ((int(a), aname.strip()), (int(b), bname.strip())):
        if number in pins:
            raise SystemExit(f"Duplicate pin {number}")
        pins[number] = name
if set(pins) != set(range(1, 41)):
    raise SystemExit(f"Expected pin numbers 1..40; found {sorted(pins)}")
assert all(pins[i] == "+5V" for i in (1, 2, 5, 6))
assert all(pins[i] == "GND" for i in (3, 4, 7, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36))
assert [pins[i] for i in range(37, 41)] == ["CTRL_SER", "CTRL_SCK", "CTRL_LATCH", "BUS_ENABLE"]
print("PASS: 40 HESS pins verified")
