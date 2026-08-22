from __future__ import annotations

import argparse
import json
from pathlib import Path

import pandas as pd


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('--csv', required=True)
    parser.add_argument('--out', default='artifacts/nudgebandit.json')
    args = parser.parse_args()

    df = pd.read_csv(args.csv)
    grouped = df.groupby('nudge_type')['reward'].mean().sort_values(ascending=False)
    policy = {
        'best_nudge': grouped.index[0] if not grouped.empty else 'drink-300ml-now',
        'reward_table': grouped.to_dict(),
    }
    Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    Path(args.out).write_text(json.dumps(policy, indent=2), encoding='utf-8')
    print(policy)


if __name__ == '__main__':
    main()
