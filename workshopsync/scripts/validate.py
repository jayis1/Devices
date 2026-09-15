#!/usr/bin/env python3
# Deterministic structural validation — authored by jayis1.
import csv
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
NODES = ["workshop-hub", "tool-dock", "air-sentinel", "ppe-tag", "bench-mat"]
REQUIRED = ["README.md", "docs/system-manifest.json", "docs/architecture.md", "docs/protocol.md", "docs/api.md", "software/dashboard/main.py", "software/ml-pipeline/train_anomaly_baseline.py", "software/mobile-app/App.tsx"]

def run(command: list[str]) -> None:
    subprocess.run(command, cwd=ROOT, check=True, text=True)

def main() -> None:
    failures = []
    for relative in REQUIRED:
        path = ROOT / relative
        if not path.is_file() or path.stat().st_size == 0:
            failures.append(f"missing/empty: {relative}")
    manifest = json.loads((ROOT / "docs/system-manifest.json").read_text())
    mobile_package = json.loads((ROOT / "software/mobile-app/package.json").read_text())
    if manifest.get("author") != "jayis1" or [n["name"] for n in manifest["nodes"]] != NODES:
        failures.append("manifest node set or author mismatch")
    if mobile_package.get("author") != "jayis1" or not mobile_package.get("dependencies"):
        failures.append("mobile package metadata mismatch")
    for node in NODES:
        if not (ROOT / "firmware" / node / "main.c").is_file(): failures.append(f"firmware missing: {node}")
        if not (ROOT / "schematic" / node / "README.md").is_file(): failures.append(f"schematic notes missing: {node}")
    for bom in sorted((ROOT / "hardware/bom").glob("*.csv")):
        rows = list(csv.DictReader(bom.open(encoding="utf-8")))
        if not rows or any(not row.get("Part Number") or not row.get("Reference") for row in rows): failures.append(f"invalid BOM: {bom.name}")
    readme = (ROOT.parent / "README.md").read_text(encoding="utf-8")
    if len(re.findall(r"\| 71 \| WorkshopSync \|", readme)) != 1: failures.append("root README row mismatch")
    corpus = "\n".join(p.read_text(encoding="utf-8", errors="ignore") for p in ROOT.rglob("*") if p.is_file())
    if re.search(r"ghp_[A-Za-z0-9]{20,}|BEGIN (?:RSA |EC )?PRIVATE KEY|AKIA[0-9A-Z]{16}", corpus): failures.append("secret-like material detected")
    if failures: raise SystemExit("\n".join(failures))
    run([sys.executable, "-m", "py_compile", *map(str, ROOT.rglob("*.py"))])
    for source in ROOT.rglob("*.c"):
        if source.name != "test_protocol.c": run(["gcc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-fsyntax-only", str(source)])
    run(["gcc", "-std=c11", "-Wall", "-Wextra", "-Werror", "firmware/common/workshopsync_protocol.c", "firmware/common/test_protocol.c", "-o", "/tmp/workshopsync_protocol_test"])
    run(["/tmp/workshopsync_protocol_test"])
    print("WorkshopSync validation passed: structure, manifest, BOMs, README row, secret scan, Python, C, protocol test")

if __name__ == "__main__": main()
