#!/usr/bin/env python3
"""
B.33a — Minimum Constraint-Preserving Swap Size.

Investigates the minimum support of a balanced sign vector D ∈ {-1,0,+1}^N
with A·D = 0 for the CRSCE constraint families (row, col, diag, anti, LTP1–LTP6).

KEY INSIGHT (discovered during implementation):
  Constraint-preserving swaps cannot be restricted to small submatrices because
  LTP lines span the entire 511×511 matrix (each line contains exactly 511 cells
  uniformly distributed across all rows/cols).  A valid swap must involve cells
  from many different rows/cols — it cannot be "local."

METHODS:
  1. Geometric-only (4-family) minimum: construct incrementally from 6-cell hexagonal.
     The hexagonal preserves row+col+diag; we extend to also preserve anti-diagonal.
     Reports the minimum number of additional cells needed.

  2. Full-system (10-family) estimation: run 2 independent Phase-1 DFS solves
     (using random branching in the meeting band), compute the cell difference,
     report its support size.  This is an UPPER BOUND on the minimum swap.

  3. LTP sensitivity: quantify how LTP families constrain the swap vocabulary.
     For each additional LTP sub-table, measure the increase in minimum repair cells.

Usage:
    python3 tools/b33a_min_swap.py [options]

    --init PATH    Load LTPB file (default: B.26c FY seeds)
    --seed N       NumPy RNG seed (default: 42)
    --geo-trials N Geometric-only BFS trials (default: 500)
    --geo-max N    Max cells for geometric BFS (default: 300)
    --ltp-stages   Test LTP sensitivity (0,2,4,6 sub-tables)
"""

import argparse
import pathlib
import struct
import sys
from collections import defaultdict

try:
    import numpy as np
except ImportError:
    sys.exit("ERROR: numpy required.  Run: pip install numpy")

S = 511; N = S * S

SEEDS = [
    0x435253434C545056,  # CRSCLTPV
    0x435253434C545050,  # CRSCLTPP
    0x435253434C545033,  # CRSCLTP3
    0x435253434C545034,  # CRSCLTP4
    0x435253434C545035,  # CRSCLTP5
    0x435253434C545036,  # CRSCLTP6
]
MAGIC = b"LTPB"


def lcg_next(state: int) -> int:
    return (state * 6364136223846793005 + 1442695040888963407) & 0xFFFFFFFFFFFFFFFF


def build_all_sub_tables() -> list:
    pool = list(range(N))
    assignments = []
    for seed in SEEDS:
        state = seed
        for i in range(N - 1, 0, -1):
            state = lcg_next(state)
            j = int(state % (i + 1))
            pool[i], pool[j] = pool[j], pool[i]
        a = np.empty(N, dtype=np.uint16)
        for line in range(S):
            base = line * S
            for pos in range(S):
                a[pool[base + pos]] = line
        assignments.append(a)
    return assignments


def load_ltpb(path: pathlib.Path) -> list:
    data = path.read_bytes()
    if len(data) < 16:
        sys.exit(f"ERROR: {path} too small")
    magic, _, s_field, num_sub = struct.unpack_from("<4sIII", data, 0)
    if magic != MAGIC or s_field != S:
        sys.exit(f"ERROR: invalid LTPB in {path}")
    assignments, offset = [], 16
    for _ in range(min(num_sub, 6)):
        assignments.append(np.frombuffer(data, dtype="<u2", count=N, offset=offset).copy())
        offset += N * 2
    if len(assignments) < 6:
        fy = build_all_sub_tables()
        while len(assignments) < 6:
            assignments.append(fy[len(assignments)])
    return assignments


def cell_keys(flat: int, assignments: list, num_ltp: int) -> list:
    """Return family keys for flat index using up to num_ltp LTP sub-tables."""
    r = flat // S; c = flat % S
    keys = [(0, r), (1, c), (2, c - r + S - 1), (3, r + c)]
    for k in range(num_ltp):
        keys.append((4 + k, int(assignments[k][flat])))
    return keys


def build_line_to_cells(assignments: list, num_ltp: int) -> dict:
    """Return dict (fam,lid) → list of flat indices."""
    ltc = defaultdict(list)
    for flat in range(N):
        for key in cell_keys(flat, assignments, num_ltp):
            ltc[key].append(flat)
    return ltc


def compute_violations(cells: dict, assignments: list, num_ltp: int) -> dict:
    """Return {(fam,lid): excess} for all non-zero line sums."""
    viol = {}
    for flat, sign in cells.items():
        for key in cell_keys(flat, assignments, num_ltp):
            viol[key] = viol.get(key, 0) + sign
    return {k: v for k, v in viol.items() if v != 0}


# ── Method 1: geometric-only minimum via score-guided BFS ────────────────────

def geometric_repair(r1, c1, r2, c2, assignments, num_ltp,
                     line_to_cells, rng, max_cells=300):
    """
    Start from rectangle (r1,c1,r1,c2,r2,c1,r2,c2) with signs (+1,-1,-1,+1).
    Repair violations by adding cells.

    Score heuristic: for each (candidate, sign) pair:
      score = Σ { +10 if cancels a violation, +3 if reduces |excess|, -2 if creates new }

    Returns swap_size on convergence, None on failure.
    """
    cells = {
        r1 * S + c1: +1, r1 * S + c2: -1,
        r2 * S + c1: -1, r2 * S + c2: +1,
    }

    def violations():
        return compute_violations(cells, assignments, num_ltp)

    viol = violations()
    SAMPLE = 15

    while viol and len(cells) < max_cells:
        best_flat = best_sign = None; best_score = -9999

        vkeys = list(viol.keys())
        if len(vkeys) > SAMPLE:
            vkeys = [vkeys[i] for i in rng.choice(len(vkeys), SAMPLE, replace=False)]

        for key in vkeys:
            if key not in viol:
                continue
            excess = viol[key]
            cands = line_to_cells.get(key, [])
            cands_fresh = [c for c in cands if c not in cells]
            if not cands_fresh:
                continue
            samp = cands_fresh if len(cands_fresh) <= SAMPLE else \
                [cands_fresh[i] for i in rng.choice(len(cands_fresh), SAMPLE, replace=False)]

            for c in samp:
                for s in (-1, +1):
                    score = 0
                    for k2 in cell_keys(c, assignments, num_ltp):
                        cur = viol.get(k2, 0)
                        new = cur + s
                        if cur != 0 and new == 0:
                            score += 10
                        elif cur != 0 and abs(new) < abs(cur):
                            score += 3
                        elif cur == 0:
                            score -= 2
                        else:
                            score -= 5
                    if score > best_score:
                        best_score = score; best_flat = c; best_sign = s

        if best_flat is None:
            return None
        cells[best_flat] = best_sign
        viol = violations()

    return len(cells) if not viol else None


# ── Method 2: full-system estimation via two DFS completions ─────────────────

def full_system_estimate(assignments, rng):
    """
    Estimate full-system swap size by taking two independently-seeded
    propagation-only solutions (meeting-band cells assigned randomly).

    A complete cross-sum-valid matrix can be built by:
    1. Setting meeting-band rows 178–510 randomly (uniform over feasible cells)
    2. Adjusting each row's assignment to fix its row-sum (trivial)
    3. Column/diag/anti/LTP sums will generally not be satisfied → this is NOT valid.

    This method is infeasible without the C++ solver.  We report N/A and suggest
    running `make test/uselessMachine` twice with different seeds.
    """
    print("  (Full-system swap estimation requires C++ solver — skipping)")
    print("  To estimate: run `make test/uselessMachine` twice with different DI")
    print("  values and count differing cells.  Expected: 100,000–200,000 cells.")
    return None


# ── LTP sensitivity analysis ─────────────────────────────────────────────────

def ltp_sensitivity(assignments, rng, geo_max, n_trials, ltc_cache):
    """
    For num_ltp = 0, 1, 2, 3, 4, 5, 6:
      Run n_trials geometric_repair trials and report min,mean swap sizes.
    """
    print("\n--- LTP sensitivity analysis ---")
    print(f"  (Showing how minimum swap grows as LTP sub-tables are added)")
    print(f"  {'Sub-tables':>12}  {'Converged':>10}  {'Min':>6}  {'Mean':>8}")

    results = {}
    for num_ltp in [0, 1, 2, 3, 4, 5, 6]:
        ltc = ltc_cache.get(num_ltp)
        if ltc is None:
            ltc = build_line_to_cells(assignments, num_ltp)
            ltc_cache[num_ltp] = ltc

        sizes = []
        for _ in range(n_trials):
            # Use non-degenerate rectangle (away from matrix edges)
            while True:
                r1, r2 = rng.integers(50, S-50, size=2)
                c1, c2 = rng.integers(50, S-50, size=2)
                if r1 != r2 and c1 != c2:
                    break
            sz = geometric_repair(int(r1), int(c1), int(r2), int(c2),
                                  assignments, num_ltp, ltc, rng, geo_max)
            if sz is not None:
                sizes.append(sz)

        if sizes:
            print(f"  {num_ltp:12d}  {len(sizes):>8}/{n_trials}  "
                  f"{min(sizes):>6d}  {sum(sizes)/len(sizes):>8.0f}", flush=True)
            results[num_ltp] = (min(sizes), sum(sizes) / len(sizes) if sizes else None)
        else:
            print(f"  {num_ltp:12d}  {0:>8}/{n_trials}  "
                  f"{'N/A':>6}  {'N/A':>8}", flush=True)
            results[num_ltp] = (None, None)
    return results


# ── Main ──────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--init",       type=pathlib.Path, default=None)
    parser.add_argument("--seed",       type=int, default=42)
    parser.add_argument("--geo-trials", type=int, default=500)
    parser.add_argument("--geo-max",    type=int, default=300)
    parser.add_argument("--ltp-stages", action="store_true")
    args = parser.parse_args()

    # ── Load / build LTP assignments ─────────────────────────────────────────
    if args.init is not None:
        print(f"Loading LTPB from {args.init} ...", flush=True)
        assignments = load_ltpb(args.init)
    else:
        print("Building FY assignments from B.26c seeds ...", flush=True)
        assignments = build_all_sub_tables()
    print(f"  {len(assignments)} sub-tables loaded.", flush=True)

    rng = np.random.default_rng(args.seed)
    ltc_cache = {}

    # ── Method 1a: geometric-only (0 LTP sub-tables) ─────────────────────────
    print(f"\n--- Method 1a: Geometric-only (4 families) "
          f"({args.geo_trials} trials, max_cells={args.geo_max}) ---", flush=True)

    ltc_geo = build_line_to_cells(assignments, 0)
    ltc_cache[0] = ltc_geo
    geo_sizes = []
    geo_fail  = 0

    for trial in range(args.geo_trials):
        while True:
            r1, r2 = rng.integers(50, S-50, size=2)
            c1, c2 = rng.integers(50, S-50, size=2)
            if r1 != r2 and c1 != c2:
                break
        sz = geometric_repair(int(r1), int(c1), int(r2), int(c2),
                              assignments, 0, ltc_geo, rng, args.geo_max)
        if sz is None:
            geo_fail += 1
        else:
            geo_sizes.append(sz)
        if (trial + 1) % 100 == 0:
            if geo_sizes:
                print(f"  [{trial+1}/{args.geo_trials}]  success={len(geo_sizes)}  "
                      f"min={min(geo_sizes)}  mean={sum(geo_sizes)/len(geo_sizes):.0f}",
                      flush=True)
            else:
                print(f"  [{trial+1}/{args.geo_trials}]  all failed so far", flush=True)

    print(f"\nGeometric-only results ({args.geo_trials} trials):")
    print(f"  Converged: {len(geo_sizes)} ({100*len(geo_sizes)/args.geo_trials:.0f}%)")
    if geo_sizes:
        arr = sorted(geo_sizes)
        print(f"  Min swap size:  {arr[0]}")
        print(f"  5th pct:        {arr[max(0,int(0.05*len(arr)))]}")
        print(f"  Median:         {arr[len(arr)//2]}")
        print(f"  Mean:           {sum(arr)/len(arr):.0f}")
        print(f"  95th pct:       {arr[min(len(arr)-1,int(0.95*len(arr)))]}")

    # ── Method 1b: full system (6 LTP sub-tables) ─────────────────────────────
    print(f"\n--- Method 1b: Full system (10 families, {args.geo_trials} trials) ---",
          flush=True)

    ltc_full = build_line_to_cells(assignments, 6)
    ltc_cache[6] = ltc_full
    full_sizes = []
    full_fail  = 0

    for trial in range(args.geo_trials):
        while True:
            r1, r2 = rng.integers(50, S-50, size=2)
            c1, c2 = rng.integers(50, S-50, size=2)
            if r1 != r2 and c1 != c2:
                break
        sz = geometric_repair(int(r1), int(c1), int(r2), int(c2),
                              assignments, 6, ltc_full, rng, args.geo_max)
        if sz is None:
            full_fail += 1
        else:
            full_sizes.append(sz)
        if (trial + 1) % 100 == 0:
            if full_sizes:
                print(f"  [{trial+1}/{args.geo_trials}]  success={len(full_sizes)}  "
                      f"min={min(full_sizes)}  mean={sum(full_sizes)/len(full_sizes):.0f}",
                      flush=True)
            else:
                print(f"  [{trial+1}/{args.geo_trials}]  all failed so far", flush=True)

    print(f"\nFull-system results ({args.geo_trials} trials):")
    print(f"  Converged: {len(full_sizes)} ({100*len(full_sizes)/args.geo_trials:.0f}%)")
    if full_sizes:
        arr = sorted(full_sizes)
        print(f"  Min swap size:  {arr[0]}")
        print(f"  5th pct:        {arr[max(0,int(0.05*len(arr)))]}")
        print(f"  Median:         {arr[len(arr)//2]}")
        print(f"  Mean:           {sum(arr)/len(arr):.0f}")
        print(f"  95th pct:       {arr[min(len(arr)-1,int(0.95*len(arr)))]}")

    # ── Method 2: full-system upper bound from C++ solver ─────────────────────
    print(f"\n--- Method 2: Full-system upper bound ---")
    full_system_estimate(assignments, rng)

    # ── LTP sensitivity ───────────────────────────────────────────────────────
    if args.ltp_stages:
        small_trials = min(args.geo_trials, 100)
        ltp_sensitivity(assignments, rng, args.geo_max, small_trials, ltc_cache)

    # ── Summary ──────────────────────────────────────────────────────────────
    print("\n=== B.33a Summary ===")
    geo_min  = min(geo_sizes)  if geo_sizes  else None
    full_min = min(full_sizes) if full_sizes else None

    print(f"Geometric-only (4 families):  minimum={geo_min}  "
          f"converge_rate={100*len(geo_sizes)/args.geo_trials:.0f}%")
    print(f"Full system (10 families):    minimum={full_min}  "
          f"converge_rate={100*len(full_sizes)/args.geo_trials:.0f}%")

    if full_min is not None:
        if full_min < 100:
            print(f"\nConclusion: min swap ≤ {full_min} < 100.  B.33 Phase 3 FEASIBLE.")
        else:
            print(f"\nConclusion: min swap ≤ {full_min}.  B.33 Phase 3 UNCERTAIN.")
            print("  Note: this is an upper bound from score-guided repair.")
            print("  True minimum may be smaller; optimal swap construction unknown.")
    else:
        print(f"\nConclusion: No convergences within {args.geo_max} cells.")
        print("  True minimum likely > 300.  B.33 Phase 3 may be IMPRACTICAL.")
        print("  Consider: (a) increase --geo-max, (b) implement B.33a.exhaustive,")
        print("  (c) generate swap from C++ solver with two different DI values.")

    print("\nDone.")


if __name__ == "__main__":
    main()
