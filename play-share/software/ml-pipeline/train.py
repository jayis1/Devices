"""Synthetic-only feature smoke test; no real-world model or accuracy claim."""
import argparse
import json

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--synthetic", action="store_true", required=True)
    args = parser.parse_args()
    rows = [{"day": i, "stock": 20 - i} for i in range(5)]
    slope = rows[-1]["stock"] - rows[0]["stock"]
    result = {"synthetic": args.synthetic, "stock_delta": slope, "reorder_advisory": slope <= -4, "fallback": "threshold rule"}
    print(json.dumps(result, sort_keys=True))

if __name__ == "__main__": main()
