#!/usr/bin/env python3
# WorkshopSync commissioning helper — authored by jayis1.
import argparse
import re

def main() -> None:
    parser = argparse.ArgumentParser(description="Validate a non-secret WorkshopSync node identifier.")
    parser.add_argument("node_id", type=int)
    parser.add_argument("--label", required=True)
    args = parser.parse_args()
    if not 1 <= args.node_id <= 0xFFFFFFFF or not re.fullmatch(r"[a-z0-9-]{3,32}", args.label):
        raise SystemExit("invalid node ID or label")
    print(f"Provisioning record prepared for {args.label} ({args.node_id}); generate keys in the local secure workflow.")

if __name__ == "__main__":
    main()
