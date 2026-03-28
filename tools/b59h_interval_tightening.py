#!/usr/bin/env python3
"""
B.59h: Interval-Tightening Potential at S=127 (from B.16/B.42a)

At S=511: 0% interval-tightening potential. Every cell had v_min=0, v_max=1.

At S=127: lines are 4x shorter, constraint density 3.1x higher.
Do joint multi-line bounds force any cells beyond single-line rho=0/rho=u?

Method:
  1. IntBound propagation to plateau (= DFS initial propagation)
  2. For each free cell, check all 6 constraint lines:
     - Single-line: can v=0 feasible? can v=1 feasible?
     - Joint multi-line: intersection of all 6 lines' bounds
  3. Report: interval-tightening potential, both-infeasible rate

Usage:
    python3 tools/b59h_interval_tightening.py
    python3 tools/b59h_interval_tightening.py --block 2
"""

import argparse
import collections
import json
import numpy as np
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
S = 127
N = S * S
DIAG_COUNT = 2 * S - 1

LCG_A = 6364136223846793005
LCG_C = 1442695040888963407
LCG_MOD = 1 << 64
SEED1 = int.from_bytes(b"CRSCLTPV", "big")
SEED2 = int.from_bytes(b"CRSCLTPP", "big")


def build_yltp(seed):
    pool = list(range(N)); state = seed
    for i in range(N - 1, 0, -1):
        state = (state * LCG_A + LCG_C) % LCG_MOD
        j = int(state % (i + 1)); pool[i], pool[j] = pool[j], pool[i]
    mem = [0] * N
    for l in range(S):
        for s in range(S):
            mem[pool[l * S + s]] = l
    return mem


YLTP1 = build_yltp(SEED1)
YLTP2 = build_yltp(SEED2)


def load_csm(data, block_idx):
    csm = np.zeros((S, S), dtype=np.uint8)
    start = block_idx * N
    for i in range(N):
        sb = (start + i) // 8
        if sb < len(data) and (data[sb] >> (7 - (start + i) % 8)) & 1:
            csm[i // S, i % S] = 1
    return csm


def build_and_propagate(csm):
    """Build constraints, propagate, return (determined, free, rho, u, members, cell_lines)."""
    targets = []
    members = []
    for r in range(S):
        targets.append(int(np.sum(csm[r, :]))); members.append([r * S + c for c in range(S)])
    for c in range(S):
        targets.append(int(np.sum(csm[:, c]))); members.append([r * S + c for r in range(S)])
    for d in range(DIAG_COUNT):
        off = d - (S - 1); m = []; s = 0
        for r in range(S):
            cv = r + off
            if 0 <= cv < S: s += int(csm[r, cv]); m.append(r * S + cv)
        targets.append(s); members.append(m)
    for x in range(DIAG_COUNT):
        m = []; s = 0
        for r in range(S):
            cv = x - r
            if 0 <= cv < S: s += int(csm[r, cv]); m.append(r * S + cv)
        targets.append(s); members.append(m)
    for mt in [YLTP1, YLTP2]:
        ls = [0] * S; lm = [[] for _ in range(S)]
        for f in range(N): ls[mt[f]] += int(csm[f // S, f % S]); lm[mt[f]].append(f)
        for k in range(S): targets.append(ls[k]); members.append(lm[k])

    cell_lines = [[] for _ in range(N)]
    for i, m in enumerate(members):
        for j in m: cell_lines[j].append(i)

    rho = list(targets); u = [len(m) for m in members]
    det = {}; free = set(range(N))
    q = collections.deque(range(len(targets))); inq = set(q)
    while q:
        i = q.popleft(); inq.discard(i)
        if u[i] == 0: continue
        if rho[i] < 0 or rho[i] > u[i]: break
        fv = None
        if rho[i] == 0: fv = 0
        elif rho[i] == u[i]: fv = 1
        if fv is None: continue
        for j in members[i]:
            if j not in free: continue
            det[j] = fv; free.discard(j)
            for li in cell_lines[j]:
                u[li] -= 1; rho[li] -= fv
                if li not in inq: q.append(li); inq.add(li)

    return det, free, rho, u, members, cell_lines


def analyze_interval_tightening(free, rho, u, members, cell_lines):
    """For each free cell, check if joint multi-line analysis forces it.

    For cell j on lines L1..L6:
      - v=0 feasible on line Li iff rho[Li] >= 0 and rho[Li] <= u[Li]-1
        (removing j with value 0: new_rho = rho, new_u = u-1)
      - v=1 feasible on line Li iff rho[Li]-1 >= 0 and rho[Li]-1 <= u[Li]-1
        (removing j with value 1: new_rho = rho-1, new_u = u-1)
      - v=0 jointly infeasible if ANY line makes v=0 infeasible
      - v=1 jointly infeasible if ANY line makes v=1 infeasible
    """
    n_free = len(free)
    pref_infeasible = 0      # preferred value (0) is infeasible
    alt_infeasible = 0       # alternate value (1) is infeasible
    both_infeasible = 0      # both values infeasible (contradiction)
    interval_forced = 0      # exactly one value feasible (forced by interval)
    both_feasible = 0        # neither forced

    # Also track per-line statistics at plateau
    rho_over_u = []  # rho/u ratio for active lines

    for j in sorted(free):
        v0_feasible = True
        v1_feasible = True

        for li in cell_lines[j]:
            if u[li] == 0:
                continue  # j shouldn't be on a fully-determined line

            # Check v=0: rho stays same, u decreases by 1
            new_rho_0 = rho[li]
            new_u_0 = u[li] - 1
            if new_rho_0 < 0 or new_rho_0 > new_u_0:
                v0_feasible = False

            # Check v=1: rho decreases by 1, u decreases by 1
            new_rho_1 = rho[li] - 1
            new_u_1 = u[li] - 1
            if new_rho_1 < 0 or new_rho_1 > new_u_1:
                v1_feasible = False

        if not v0_feasible and not v1_feasible:
            both_infeasible += 1
        elif not v0_feasible:
            pref_infeasible += 1
            interval_forced += 1
        elif not v1_feasible:
            alt_infeasible += 1
            interval_forced += 1
        else:
            both_feasible += 1

    # Per-line rho/u statistics at plateau
    for i in range(len(rho)):
        if u[i] > 0:
            rho_over_u.append(rho[i] / u[i])

    return {
        "n_free": n_free,
        "pref_infeasible": pref_infeasible,
        "alt_infeasible": alt_infeasible,
        "both_infeasible": both_infeasible,
        "interval_forced": interval_forced,
        "both_feasible": both_feasible,
        "pref_infeasible_pct": round(pref_infeasible / max(n_free, 1) * 100, 1),
        "alt_infeasible_pct": round(alt_infeasible / max(n_free, 1) * 100, 1),
        "both_infeasible_pct": round(both_infeasible / max(n_free, 1) * 100, 1),
        "interval_forced_pct": round(interval_forced / max(n_free, 1) * 100, 1),
        "both_feasible_pct": round(both_feasible / max(n_free, 1) * 100, 1),
        "rho_over_u_mean": round(np.mean(rho_over_u), 3) if rho_over_u else 0,
        "rho_over_u_min": round(min(rho_over_u), 3) if rho_over_u else 0,
        "rho_over_u_max": round(max(rho_over_u), 3) if rho_over_u else 0,
    }


def main():
    parser = argparse.ArgumentParser(description="B.59h: Interval-tightening at S=127")
    parser.add_argument("--block", type=int, default=0)
    parser.add_argument("--max-blocks", type=int, default=None)
    parser.add_argument("--out", default=str(REPO_ROOT / "tools" / "b59h_results.json"))
    args = parser.parse_args()

    mp4_path = REPO_ROOT / "docs" / "testData" / "useless-machine.mp4"
    data = mp4_path.read_bytes()
    total_blocks = (len(data) * 8 + N - 1) // N

    if args.max_blocks:
        blocks = list(range(min(args.max_blocks, total_blocks)))
    else:
        blocks = [args.block]

    print("B.59h: Interval-Tightening Potential at S=127")
    print(f"  S={S}, N={N}, blocks={len(blocks)}")
    print(f"  B.42a baseline (S=511): pref_inf=56.6%, both_inf=43.2%, interval_forced=0%")
    print()

    results = []
    for bidx in blocks:
        csm = load_csm(data, bidx)
        density = np.sum(csm) / N
        det, free, rho, u, members, cl = build_and_propagate(csm)

        t0 = time.time()
        analysis = analyze_interval_tightening(free, rho, u, members, cl)
        elapsed = time.time() - t0

        print(f"  Block {bidx:4d}: density={density:.3f} det={len(det):5d} ({len(det)/N*100:.1f}%) "
              f"pref_inf={analysis['pref_infeasible_pct']:5.1f}% "
              f"alt_inf={analysis['alt_infeasible_pct']:5.1f}% "
              f"both_inf={analysis['both_infeasible_pct']:5.1f}% "
              f"interval_forced={analysis['interval_forced_pct']:5.1f}% "
              f"rho/u={analysis['rho_over_u_mean']:.3f} "
              f"({elapsed:.1f}s)")

        analysis["block"] = bidx
        analysis["density"] = round(density, 4)
        analysis["determined"] = len(det)
        results.append(analysis)

    # Summary
    if len(results) > 1:
        print(f"\n{'='*80}")
        print(f"Summary — {len(results)} blocks")
        avg_forced = np.mean([r["interval_forced_pct"] for r in results])
        avg_both = np.mean([r["both_infeasible_pct"] for r in results])
        print(f"  Avg interval_forced: {avg_forced:.1f}%")
        print(f"  Avg both_infeasible: {avg_both:.1f}%")

    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps({"experiment": "B.59h", "results": results}, indent=2))
    print(f"\n  Results: {out_path}")


if __name__ == "__main__":
    main()
