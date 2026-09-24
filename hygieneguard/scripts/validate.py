#!/usr/bin/env python3
"""Portable HygieneGuard content validator. Author: jayis1."""
import csv, json, subprocess
from pathlib import Path
root = Path(__file__).resolve().parents[1]
required = ["README.md", "schematic", "firmware", "hardware/bom", "software/dashboard", "software/ml-pipeline", "software/mobile-app", "docs", "scripts"]
for name in required:
    assert (root / name).exists(), f"missing {name}"
for bom in sorted((root / "hardware/bom").glob("*.csv")):
    rows = list(csv.reader(bom.open()))
    assert len(rows) > 1 and rows[0][:4] == ["Component", "Quantity", "Description", "Manufacturer Part Number"], f"bad BOM {bom}"
json.load((root / "software/mobile-app/package.json").open())
subprocess.run(["python3", "-m", "py_compile", *map(str, root.glob("software/**/*.py"))], check=True)
c_files = list(root.glob("firmware/**/*.c"))
subprocess.run(["gcc", "-std=c11", "-Wall", "-Werror", "-fsyntax-only", *map(str, c_files)], check=True)
for heading in ["Architecture", "Hardware Nodes and BOM", "Firmware", "Cloud Backend", "ML Pipeline", "Mobile App", "Deployment", "Build and validate"]:
    assert heading in (root / "README.md").read_text(), f"missing heading {heading}"
print(f"validated {len(c_files)} C files and {len(list((root / 'hardware/bom').glob('*.csv')))} BOMs")
