#!/usr/bin/env python3
"""B.63k: Payload trade-off sweep — run K0-K6 across all blocks of the MP4 test file."""

import subprocess
import json
import sys
import time
import re

BINARY = "./build/arm64-release/combinatorSolver"
INPUT = "docs/testData/useless-machine.mp4"
CONFIGS = ["b63k0", "b63k1", "b63k2", "b63k3", "b63k4", "b63k5", "b63k6"]
MAX_BLOCKS = 1331
OUTPUT = "tools/b63k_results.json"
TIMEOUT = 120  # seconds per block

results = []

for cfg in CONFIGS:
    print(f"\n=== Config {cfg} ===", flush=True)
    cfg_results = []

    for blk in range(MAX_BLOCKS):
        try:
            t0 = time.time()
            out = subprocess.run(
                [BINARY, "-in", INPUT, "-block", str(blk), "-config", cfg],
                capture_output=True, text=True, timeout=TIMEOUT
            )
            elapsed = time.time() - t0
            stderr = out.stderr

            det_pct = 0.0
            det_cells = 0
            rank = 0
            ge_cells = 0
            ib_cells = 0
            bh = False

            for line in stderr.split('\n'):
                m = re.search(r'Determined:\s+(\d+)\s+/\s+\d+\s+\(([0-9.]+)%\)', line)
                if m:
                    det_cells = int(m.group(1))
                    det_pct = float(m.group(2))
                if 'Rank:' in line:
                    m2 = re.search(r'Rank:\s+(\d+)', line)
                    if m2:
                        rank = int(m2.group(1))
                if 'GaussElim:' in line:
                    m3 = re.search(r'GaussElim:\s+(\d+)', line)
                    if m3:
                        ge_cells = int(m3.group(1))
                if 'IntBound:' in line:
                    m4 = re.search(r'IntBound:\s+(\d+)', line)
                    if m4:
                        ib_cells = int(m4.group(1))
                if 'BH verified: true' in line:
                    bh = True

            entry = {
                "config": cfg,
                "block": blk,
                "det_cells": det_cells,
                "det_pct": det_pct,
                "rank": rank,
                "ge_cells": ge_cells,
                "ib_cells": ib_cells,
                "bh": bh,
                "time_s": round(elapsed, 2)
            }
            cfg_results.append(entry)

            if blk % 100 == 0:
                print(f"  {cfg} block {blk}: {det_pct:.1f}% rank={rank} bh={bh} ({elapsed:.1f}s)", flush=True)

        except subprocess.TimeoutExpired:
            cfg_results.append({
                "config": cfg, "block": blk, "det_cells": 0, "det_pct": 0,
                "rank": 0, "ge_cells": 0, "ib_cells": 0, "bh": False, "time_s": TIMEOUT
            })
            if blk % 100 == 0:
                print(f"  {cfg} block {blk}: TIMEOUT", flush=True)

    results.extend(cfg_results)

    # Per-config summary
    full_solves = sum(1 for r in cfg_results if r["bh"])
    avg_det = sum(r["det_pct"] for r in cfg_results) / len(cfg_results) if cfg_results else 0
    avg_rank = sum(r["rank"] for r in cfg_results) / len(cfg_results) if cfg_results else 0
    print(f"\n  {cfg} summary: {full_solves}/{len(cfg_results)} BH-verified, avg det={avg_det:.1f}%, avg rank={avg_rank:.0f}")

# Write all results
with open(OUTPUT, "w") as f:
    json.dump(results, f, indent=1)

print(f"\nResults written to {OUTPUT}")

# Final comparison table
print("\n=== COMPARISON TABLE ===")
print(f"{'Config':<10} {'BH Solves':>10} {'Avg Det%':>10} {'Avg Rank':>10} {'50% Det%':>10}")
for cfg in CONFIGS:
    cr = [r for r in results if r["config"] == cfg]
    full = sum(1 for r in cr if r["bh"])
    avg_d = sum(r["det_pct"] for r in cr) / len(cr) if cr else 0
    avg_r = sum(r["rank"] for r in cr) / len(cr) if cr else 0
    # 50% density blocks: blocks 5+
    d50 = [r for r in cr if r["block"] >= 5]
    avg_50 = sum(r["det_pct"] for r in d50) / len(d50) if d50 else 0
    print(f"{cfg:<10} {full:>10} {avg_d:>10.1f} {avg_r:>10.0f} {avg_50:>10.1f}")
