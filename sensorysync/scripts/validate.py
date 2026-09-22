#!/usr/bin/env python3
# Deterministic validation authored by jayis1.
import csv, json, re, subprocess, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
NODES=["sensory-hub","room-beacon","comfort-band","ambient-controller","quiet-pod"]
REQ=["README.md","docs/system-manifest.json","docs/architecture.md","docs/protocol.md","docs/api.md","docs/deployment.md","software/dashboard/main.py","software/ml-pipeline/train.py","software/mobile-app/App.js"]
def main():
 failures=[]
 for p in REQ:
  if not (ROOT/p).is_file() or not (ROOT/p).read_text().strip(): failures.append(f"missing: {p}")
 manifest=json.loads((ROOT/"docs/system-manifest.json").read_text())
 if manifest.get("author")!="jayis1" or [n["name"] for n in manifest["nodes"]]!=NODES: failures.append("manifest mismatch")
 for n in NODES:
  for p in (ROOT/"firmware"/n/"main.c",ROOT/"schematic"/n/"README.md",ROOT/"hardware"/"bom"/f"{n}.csv"):
   if not p.is_file(): failures.append(f"missing node artifact: {p}")
 for p in (ROOT/"hardware"/"bom").glob("*.csv"):
  if not list(csv.DictReader(p.open())): failures.append(f"empty BOM: {p.name}")
 corpus="\n".join(p.read_text(errors="ignore") for p in ROOT.rglob("*") if p.is_file())
 if re.search(r"ghp_[A-Za-z0-9]{20,}|BEGIN (?:RSA |EC )?PRIVATE KEY|AKIA[0-9A-Z]{16}",corpus): failures.append("secret-like material")
 if failures: raise SystemExit("\n".join(failures))
 subprocess.run([sys.executable,"-m","py_compile",*map(str,ROOT.rglob("*.py"))],check=True)
 for p in ROOT.rglob("*.c"): subprocess.run(["gcc","-std=c11","-Wall","-Wextra","-Werror","-fsyntax-only",str(p)],check=True)
 subprocess.run(["gcc","-std=c11","-Wall","-Wextra","-Werror","-Ifirmware/common","firmware/common/sensorysync_protocol.c","firmware/common/test_protocol.c","-o","/tmp/sensorysync_protocol_test"],cwd=ROOT,check=True)
 subprocess.run(["/tmp/sensorysync_protocol_test"],check=True)
 print("SensorySync validation passed")
if __name__=="__main__": main()
