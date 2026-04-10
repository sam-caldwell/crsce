#!/usr/bin/env python3
"""B.64c: Empirical candidate enumeration across CRC widths.

Runs b63c (CRC-32) and b63n (CRC-64) on all blocks of the MP4 test file.
Each block runs until BH-verified solve or 1-hour timeout.
Records: candidates searched, solution time, DI ordinal.
"""

import subprocess
import json
import time
import re
import sys

BINARY = "./build/arm64-release/combinatorSolver"
INPUT = "docs/testData/useless-machine.mp4"
MAX_BLOCKS = 1331
TIMEOUT_SEC = 3600  # 1 hour per block

# Configs to test: (name, config_flag)
CONFIGS = [
    ("CRC-32", "b63c"),
    ("CRC-64", "b63n"),
]

results = []

for cfg_name, cfg_flag in CONFIGS:
    print(f"\n{'='*80}", flush=True)
    print(f"  {cfg_name} ({cfg_flag})", flush=True)
    print(f"{'='*80}", flush=True)

    for blk in range(MAX_BLOCKS):
        t0 = time.time()
        try:
            out = subprocess.run(
                [BINARY, "-in", INPUT, "-block", str(blk), "-config", cfg_flag],
                capture_output=True, text=True, timeout=TIMEOUT_SEC
            )
            elapsed = time.time() - t0
            stderr = out.stderr

            # Parse results
            det_pct = 0.0
            rank = 0
            bh = False
            correct = False
            ge_cells = 0

            for line in stderr.split('\n'):
                m = re.search(r'Determined:\s+(\d+)\s+/\s+\d+\s+\(([0-9.]+)%\)', line)
                if m: det_pct = float(m.group(2))
                m2 = re.search(r'Rank:\s+(\d+)', line)
                if m2: rank = int(m2.group(1))
                m3 = re.search(r'GaussElim:\s+(\d+)', line)
                if m3: ge_cells = int(m3.group(1))
                if 'BH verified: true' in line: bh = True
                if 'Correct:     true' in line: correct = True

            # Count BH failures and candidates
            bh_fails = stderr.count('BH FAIL')
            candidates = bh_fails + (1 if bh else 0)

            # Estimate density from GaussElim pattern
            density = 0.5  # default assumption for 50% blocks

            entry = {
                "config": cfg_name,
                "block": blk,
                "det_pct": det_pct,
                "rank": rank,
                "ge_cells": ge_cells,
                "candidates": candidates,
                "bh_fails": bh_fails,
                "solved": bh,
                "correct": correct,
                "time_s": round(elapsed, 2),
                "timeout": False
            }
            results.append(entry)

            if blk % 50 == 0 or bh:
                status = "SOLVED" if bh else f"{candidates} candidates"
                print(f"  blk={blk:>5} det={det_pct:>5.1f}% rank={rank:>6} {status} {elapsed:.1f}s", flush=True)

        except subprocess.TimeoutExpired:
            results.append({
                "config": cfg_name, "block": blk, "det_pct": 0, "rank": 0,
                "ge_cells": 0, "candidates": -1, "bh_fails": -1, "solved": False,
                "correct": False, "time_s": TIMEOUT_SEC, "timeout": True
            })
            if blk % 50 == 0:
                print(f"  blk={blk:>5} TIMEOUT (1hr)", flush=True)

    # Per-config summary
    cfg_results = [r for r in results if r["config"] == cfg_name]
    solved = sum(1 for r in cfg_results if r["solved"])
    algebraic = sum(1 for r in cfg_results if r["solved"] and r["candidates"] <= 1)
    timeouts = sum(1 for r in cfg_results if r["timeout"])
    solved_candidates = [r["candidates"] for r in cfg_results if r["solved"] and r["candidates"] > 0]

    print(f"\n  {cfg_name} SUMMARY:")
    print(f"    Blocks: {len(cfg_results)}")
    print(f"    Solved: {solved} ({solved*100/len(cfg_results):.1f}%)")
    print(f"    Algebraic (0 DFS): {algebraic}")
    print(f"    Timeouts: {timeouts}")
    if solved_candidates:
        print(f"    Candidates to solve (DFS blocks): min={min(solved_candidates)} mean={sum(solved_candidates)/len(solved_candidates):.0f} max={max(solved_candidates)}")

# Save results
output_file = "tools/b64c_results.json"
with open(output_file, "w") as f:
    json.dump(results, f, indent=1)
print(f"\nResults written to {output_file}")

# Final comparison
print(f"\n{'='*80}")
print(f"  COMPARISON")
print(f"{'='*80}")
print(f"{'Config':<10} {'Solved':>8} {'Algebraic':>10} {'Timeout':>8} {'MeanCand':>10} {'MaxCand':>10}")
for cfg_name, _ in CONFIGS:
    cr = [r for r in results if r["config"] == cfg_name]
    solved = sum(1 for r in cr if r["solved"])
    alg = sum(1 for r in cr if r["solved"] and r["candidates"] <= 1)
    to = sum(1 for r in cr if r["timeout"])
    sc = [r["candidates"] for r in cr if r["solved"] and r["candidates"] > 0]
    mc = f"{sum(sc)/len(sc):.0f}" if sc else "—"
    mx = str(max(sc)) if sc else "—"
    print(f"{cfg_name:<10} {solved:>8} {alg:>10} {to:>8} {mc:>10} {mx:>10}")
