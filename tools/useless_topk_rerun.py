#!/usr/bin/env python3
"""
Re-run top-K seeds from completion_stats.log/CSV with heavier effort.

Usage:
  python3 tools/useless_topk_rerun.py [--k 10]
                                     [--by row_avg_pct|valid_prefix]
                                     [--csv build/uselessTest/completion_stats.csv]

Selects the top K seeds by the chosen metric and re-runs make test/uselessMachine
for those seeds. Environment variables are used to pass the seed.
"""
from __future__ import annotations

import csv
import os
import subprocess
import sys
from typing import List


def pick_top(csv_path: str, k: int, by: str) -> List[int]:
    rows = []
    with open(csv_path, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for r in reader:
            seed = int(r.get('rng_seed_belief', '-1'))
            if seed < 0:
                continue
            metric = float(r.get(by, '0'))
            rows.append((seed, metric))
    rows.sort(key=lambda t: t[1], reverse=True)
    return [s for s, _ in rows[:k]]


def main(argv: List[str]) -> int:
    args = argv[1:]
    k = 10
    by = 'row_avg_pct'
    csv_path = os.path.join('build', 'uselessTest', 'completion_stats.csv')
    i = 0
    while i < len(args):
        a = args[i]
        if a == '--k' and i + 1 < len(args):
            k = int(args[i + 1])
            i += 2
            continue
        if a == '--by' and i + 1 < len(args):
            by = args[i + 1]
            i += 2
            continue
        if a == '--csv' and i + 1 < len(args):
            csv_path = args[i + 1]
            i += 2
            continue
        i += 1

    if not os.path.exists(csv_path):
        print(f"error: csv not found: {csv_path}", file=sys.stderr)
        return 2

    seeds = pick_top(csv_path, k, by)
    print(f"Re-running top {len(seeds)} seeds by {by}: {seeds}")
    for s in seeds:
        env = os.environ.copy()
        env['CRSCE_SOLVER_SEED'] = str(s)
        # Optionally set exploration toggles if desired in the future
        print(f"[rerun] seed={s}")
        subprocess.run(['make', 'test/uselessMachine'], env=env, check=False)
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))
