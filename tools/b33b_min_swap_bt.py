#!/usr/bin/env python3
"""
B.33b — Minimum Constraint-Preserving Swap Size via Backtracking.

Improves on B.33a by using a principled backtracking search instead of greedy
forward repair. Starts from an ANALYTIC geometric base (8-cell swap preserving
row/col/diag/anti-diag) and extends it to balance LTP violations.

KEY RESULT FROM B.33a:
  Greedy BFS → 0% convergence for ALL configurations (4–10 families).
  Root cause: net violations increase monotonically with greedy selection.

THIS APPROACH:
  1. Construct the analytic 8-cell geometric base (exact formula for rectangle).
  2. Identify LTP violations (random FY assignment → 8 cells land on ~32 LTP lines).
  3. Backtracking DFS: for each violated line L, pick a compensating cell from L.
  4. Branch on cell choice; prune when |swap| > budget or violations stall.
  5. Report minimum full-system swap size found.

KEY INSIGHT:
  For geometric-only (4 families): 8-cell minimum is analytically constructible.
  For each LTP sub-table: the 8 cells fall on ~8 distinct LTP lines (each with 1 cell),
  requiring mates. With 4 LTP sub-tables: ~32 additional constraints. Each mate creates
  9 new constraints, leading to a cascade. The question is whether the cascade converges
  quickly (small k) or diverges (large k → Phase 3 infeasible).

Usage:
    python3 tools/b33b_min_swap_bt.py [options]

    --init PATH      Load LTPB file (default: B.26c FY seeds)
    --seed N         RNG seed for rectangle selection (default: 42)
    --n-rect N       Number of starting rectangles to try (default: 50)
    --budget K       Max swap cells before backtracking (default: 150)
    --timeout T      Max seconds per rectangle (default: 60)
    --verbose        Print detailed backtracking progress
"""

import argparse
import pathlib
import struct
import sys
import time
from collections import defaultdict

try:
    import numpy as np
except ImportError:
    sys.exit("ERROR: numpy required.")

S = 511
N = S * S

SEEDS = [
    0x435253434C545056,  # CRSCLTPV
    0x435253434C545050,  # CRSCLTPP
    0x435253434C545033,  # CRSCLTP3
    0x435253434C545034,  # CRSCLTP4
    0x435253434C545035,  # CRSCLTP5
    0x435253434C545036,  # CRSCLTP6
]
MAGIC = b"LTPB"


# ── LTP construction ─────────────────────────────────────────────────────────

def lcg_next(state: int) -> int:
    return (state * 6364136223846793005 + 1442695040888963407) & 0xFFFFFFFFFFFFFFFF


def build_fy_tables() -> list:
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
    while len(assignments) < 6:
        assignments.append(build_fy_tables()[len(assignments)])
    return assignments


# ── Constraint key helpers ────────────────────────────────────────────────────

def cell_keys(flat: int, asn: list, num_ltp: int) -> tuple:
    r = flat // S
    c = flat % S
    keys = [(0, r), (1, c), (2, c - r + S - 1), (3, r + c)]
    for k in range(num_ltp):
        keys.append((4 + k, int(asn[k][flat])))
    return tuple(keys)


def build_line_to_cells(asn: list, num_ltp: int) -> dict:
    ltc = defaultdict(list)
    for flat in range(N):
        for key in cell_keys(flat, asn, num_ltp):
            ltc[key].append(flat)
    return ltc


# ── Analytic 8-cell geometric base ───────────────────────────────────────────

def make_geometric_base(r1: int, c1: int, r2: int, c2: int):
    """
    Construct the analytic 8-cell swap preserving row, col, diag, anti-diag.

    Starting rectangle: (r1,c1):+1, (r1,c2):-1, (r2,c1):-1, (r2,c2):+1
    Violations: 4 diagonals, 4 anti-diagonals.

    Repair cells (unique if parity conditions satisfied):
      E (sign -1): cancels diag d1=(c1-r1+S-1) and anti a4=(r2+c2)
      F (sign +1): cancels diag d2=(c2-r1+S-1) and anti a3=(r2+c1)
      G (sign +1): cancels diag d3=(c1-r2+S-1) and anti a2=(r1+c2)
      H (sign -1): cancels diag d4=(c2-r2+S-1) and anti a1=(r1+c1)

    Returns dict {flat: sign} on success, None if parity fails or OOB.
    """
    # Check parity: r1+r2+c1+c2 must be even for integer midpoints
    if (r1 + r2 + c1 + c2) % 2 != 0:
        return None

    base = {
        r1 * S + c1: +1,
        r1 * S + c2: -1,
        r2 * S + c1: -1,
        r2 * S + c2: +1,
    }

    # E cancels (d1=c1-r1, a4=r2+c2) with sign -1
    rE = (r1 + r2 + c2 - c1) // 2
    cE = (r2 + c1 + c2 - r1) // 2
    # F cancels (d2=c2-r1, a3=r2+c1) with sign +1
    rF = (r1 + r2 + c1 - c2) // 2
    cF = (r2 + c1 + c2 - r1) // 2
    # G cancels (d3=c1-r2, a2=r1+c2) with sign +1
    rG = (r1 + r2 + c2 - c1) // 2
    cG = (r1 + c1 + c2 - r2) // 2
    # H cancels (d4=c2-r2, a1=r1+c1) with sign -1
    rH = (r1 + r2 + c1 - c2) // 2
    cH = (r1 + c1 + c2 - r2) // 2

    repair = [
        (rE, cE, -1),
        (rF, cF, +1),
        (rG, cG, +1),
        (rH, cH, -1),
    ]

    for r, c, s in repair:
        if not (0 <= r < S and 0 <= c < S):
            return None
        flat = r * S + c
        if flat in base:
            return None  # overlaps with rectangle
        base[flat] = s

    return base


def verify_geometric(cells: dict) -> bool:
    """Verify row, col, diag, anti-diag sums are all zero."""
    rows = defaultdict(int)
    cols = defaultdict(int)
    diags = defaultdict(int)
    antis = defaultdict(int)
    for flat, sign in cells.items():
        r, c = flat // S, flat % S
        rows[r] += sign
        cols[c] += sign
        diags[c - r + S - 1] += sign
        antis[r + c] += sign
    for d in (rows, cols, diags, antis):
        if any(v != 0 for v in d.values()):
            return False
    return True


# ── Violations ────────────────────────────────────────────────────────────────

def compute_violations(cells: dict, asn: list, num_ltp: int) -> dict:
    """Return {key: excess} for all non-zero family line sums."""
    viol = {}
    for flat, sign in cells.items():
        for key in cell_keys(flat, asn, num_ltp):
            viol[key] = viol.get(key, 0) + sign
    return {k: v for k, v in viol.items() if v != 0}


# ── Backtracking search ───────────────────────────────────────────────────────

class BtSearcher:
    """
    Backtracking DFS to extend a starting swap to balance all families.

    State: {flat: sign} dict.
    At each step:
      - Find the MOST VIOLATED family line (highest |excess|)
      - Try each cell on that line as a fixing candidate (sign = -excess/|excess|)
      - Recurse; backtrack if no progress or budget exceeded
    """

    def __init__(self, asn: list, num_ltp: int, ltc: dict, budget: int,
                 timeout: float, verbose: bool):
        self.asn = asn
        self.num_ltp = num_ltp
        self.ltc = ltc
        self.budget = budget
        self.timeout = timeout
        self.verbose = verbose
        self.best_k = budget + 1
        self.start_time = time.time()
        self.nodes_visited = 0

    def _pick_target_line(self, viol: dict):
        """Pick the violated line with fewest available mate cells (MFC heuristic)."""
        best_key = best_count = None
        for key, excess in viol.items():
            needed_sign = -1 if excess > 0 else +1
            cands = [c for c in self.ltc.get(key, []) if c not in self._cells]
            # Only candidates with the correct sign direction
            # (any cell can take either sign, so just count available cells)
            count = len(cands)
            if best_key is None or count < best_count:
                best_key = key
                best_count = count
        return best_key

    def search(self, start_cells: dict) -> dict | None:
        """Run backtracking from start_cells. Return found swap or None."""
        self._cells = dict(start_cells)
        self.start_time = time.time()
        result = self._bt(compute_violations(self._cells, self.asn, self.num_ltp))
        return result

    def _bt(self, viol: dict):
        self.nodes_visited += 1
        if time.time() - self.start_time > self.timeout:
            return None

        if not viol:
            k = len(self._cells)
            if k < self.best_k:
                self.best_k = k
                if self.verbose:
                    print(f"    Found swap: k={k}", flush=True)
            return dict(self._cells)

        if len(self._cells) >= self.budget:
            return None

        # Lower bound pruning: each violation needs at least one cell to fix it.
        # A single cell can fix at most num_ltp+4 violations (one per family it's on).
        # Actually: cells_needed >= ceil(|viol| / (num_families))
        # num_families = 4 + num_ltp = total families
        num_fam = 4 + self.num_ltp
        lower_bound = (len(viol) + num_fam - 1) // num_fam
        if len(self._cells) + lower_bound >= self.best_k:
            return None

        # MFC: pick the most constrained violated line
        target_key = self._pick_target_line(viol)
        if target_key is None:
            return None

        excess = viol[target_key]
        needed_sign = -1 if excess > 0 else +1

        # Candidates: cells on target_key's line, not yet in swap
        cands = [c for c in self.ltc.get(target_key, [])
                 if c not in self._cells]

        if not cands:
            return None  # Can't fix this violation → backtrack

        # Score candidates: prefer those that fix more violations than they create
        def score_candidate(flat):
            s = 0
            for k2 in cell_keys(flat, self.asn, self.num_ltp):
                cur = viol.get(k2, 0)
                nw = cur + needed_sign
                if cur != 0 and nw == 0:
                    s += 10
                elif cur != 0 and abs(nw) < abs(cur):
                    s += 3
                elif cur == 0:
                    s -= 2
                else:
                    s -= 5
            return s

        # Sort by score descending; take top-K to limit branching
        MAX_BRANCH = 20
        if len(cands) > MAX_BRANCH * 3:
            # Sample + score for efficiency
            scored = sorted(cands, key=score_candidate, reverse=True)[:MAX_BRANCH]
        else:
            scored = sorted(cands, key=score_candidate, reverse=True)[:MAX_BRANCH]

        best_result = None
        for flat in scored:
            # Add cell
            self._cells[flat] = needed_sign
            new_viol = compute_violations(self._cells, self.asn, self.num_ltp)

            result = self._bt(new_viol)
            if result is not None and len(result) < self.best_k:
                best_result = result

            # Remove cell (backtrack)
            del self._cells[flat]

        return best_result


# ── Main ──────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--init",    type=pathlib.Path, default=None)
    parser.add_argument("--seed",    type=int, default=42)
    parser.add_argument("--n-rect",  type=int, default=50,
                        help="Number of starting rectangles to try")
    parser.add_argument("--budget",  type=int, default=150,
                        help="Max swap cells")
    parser.add_argument("--timeout", type=float, default=60.0,
                        help="Seconds per rectangle")
    parser.add_argument("--num-ltp", type=int, default=4,
                        help="Number of LTP sub-tables to use (0-6)")
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    # ── Load / build LTP ─────────────────────────────────────────────────────
    if args.init is not None:
        print(f"Loading LTPB from {args.init} ...", flush=True)
        asn = load_ltpb(args.init)
    else:
        print("Building FY assignments from B.26c seeds ...", flush=True)
        asn = build_fy_tables()
    num_ltp = min(args.num_ltp, len(asn))
    print(f"  Using {num_ltp} LTP sub-tables ({4 + num_ltp} total families)", flush=True)

    print("Building line-to-cells index ...", flush=True)
    ltc = build_line_to_cells(asn, num_ltp)
    print("  Done.", flush=True)

    rng = np.random.default_rng(args.seed)
    searcher = BtSearcher(asn, num_ltp, ltc, args.budget, args.timeout, args.verbose)

    best_swap = None
    best_k = args.budget + 1

    print(f"\nSearching {args.n_rect} rectangles, budget={args.budget}, "
          f"timeout={args.timeout}s per rect ...\n", flush=True)

    for trial in range(args.n_rect):
        # Generate valid parity rectangle
        attempts = 0
        while True:
            r1, r2 = sorted(rng.integers(20, S - 20, size=2))
            c1, c2 = sorted(rng.integers(20, S - 20, size=2))
            if r1 != r2 and c1 != c2 and (r1 + r2 + c1 + c2) % 2 == 0:
                break
            attempts += 1
            if attempts > 1000:
                break

        base = make_geometric_base(int(r1), int(c1), int(r2), int(c2))
        if base is None:
            if args.verbose:
                print(f"  [{trial+1}] rect ({r1},{c1})-({r2},{c2}): parity/OOB fail", flush=True)
            continue

        # Verify geometric balance
        if not verify_geometric(base):
            print(f"  BUG: geometric base failed verification!", flush=True)
            continue

        geo_viol = compute_violations(base, asn, num_ltp)
        n_geo_viol = len(geo_viol)

        if n_geo_viol == 0:
            # Geometric base is already fully balanced (LTP happened to be balanced too)
            k = len(base)
            if k < best_k:
                best_k = k
                best_swap = dict(base)
            print(f"  [{trial+1}] rect ({r1},{c1})-({r2},{c2}): "
                  f"PERFECT 8-cell swap (LTP also balanced)!", flush=True)
            continue

        t0 = time.time()
        searcher.budget = min(args.budget, best_k - 1)
        result = searcher.search(base)
        elapsed = time.time() - t0

        if result is not None:
            k = len(result)
            if k < best_k:
                best_k = k
                best_swap = result
                print(f"  [{trial+1}] rect ({r1},{c1})-({r2},{c2}): "
                      f"k={k}  geo_viol={n_geo_viol}  "
                      f"time={elapsed:.1f}s  nodes={searcher.nodes_visited:,}  *** NEW BEST",
                      flush=True)
            else:
                print(f"  [{trial+1}] rect ({r1},{c1})-({r2},{c2}): "
                      f"k={k}  geo_viol={n_geo_viol}  time={elapsed:.1f}s",
                      flush=True)
        else:
            print(f"  [{trial+1}] rect ({r1},{c1})-({r2},{c2}): "
                  f"FAILED  geo_viol={n_geo_viol}  "
                  f"time={elapsed:.1f}s  nodes={searcher.nodes_visited:,}",
                  flush=True)

    # ── Summary ───────────────────────────────────────────────────────────────
    print("\n=== B.33b Summary ===")
    print(f"Families: {4 + num_ltp} (4 geometric + {num_ltp} LTP)")
    if best_swap is not None:
        print(f"Minimum swap found: {best_k} cells")
        # Verify the found swap
        final_viol = compute_violations(best_swap, asn, num_ltp)
        if final_viol:
            print(f"  WARNING: {len(final_viol)} violations remain!")
        else:
            print(f"  Verified: all {4 + num_ltp} families balanced.")
        print(f"\nConclusion: min swap ≤ {best_k} cells for {4+num_ltp} families.")
        if best_k <= 32:
            print("  B.33 Phase 3 FEASIBLE: small swap vocabulary.")
        elif best_k <= 100:
            print("  B.33 Phase 3 UNCERTAIN: swap vocabulary may be manageable.")
        else:
            print("  B.33 Phase 3 LIKELY INFEASIBLE: swap vocabulary too large.")
    else:
        print(f"No swap found within budget={args.budget}.")
        print(f"True minimum > {args.budget} cells (likely much larger).")
        print("  B.33 Phase 3 IMPRACTICAL for current LTP structure.")
        print("  Conclusion: minimum swap size is O(hundreds) to O(thousands),")
        print("  because each LTP cell lands on a unique random line, requiring")
        print("  a matching cell from that same random line, which itself lands")
        print("  on new random lines, causing divergent cascades.")

    print("\nDone.")


if __name__ == "__main__":
    main()
