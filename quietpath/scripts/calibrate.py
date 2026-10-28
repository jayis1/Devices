#!/usr/bin/env python3
"""Record calibration metadata template; measurements require a reference meter."""
import json
from pathlib import Path
record = {"node_id": int(input("Node ID: ")), "reference_db": float(input("Reference level dB: ")), "raw_db": float(input("Sentinel estimate dB: ")), "method": "field calibration; verify acoustic calibrator procedure"}
record["offset_db"] = record["reference_db"] - record["raw_db"]
out = Path(f"calibration-{record['node_id']}.json")
out.write_text(json.dumps(record, indent=2) + "\n")
print(f"saved {out}; retain calibration context and uncertainty")
