#!/usr/bin/env python3
"""
B.60b: Full Combinator Pipeline with LH + VH (Dual CRC-32) at S=127

Extends B.58b by replacing 2 yLTP sub-tables with per-column CRC-32 (VH).
Implements: BuildSystem -> Fixpoint(GaussElim, IntBound, CrossDeduce, Propagate) -> Verify

GF(2) system: 8,888 x 16,129
  - 760 geometric parity (127 LSM + 127 VSM + 253 DSM + 253 XSM)
  - 4,064 CRC-32 LH (127 rows x 32)
  - 4,064 CRC-32 VH (127 cols x 32)

Integer constraints: 760 geometric lines (no yLTP).

Usage:
    python3 tools/b60b_dual_hash_solve.py
    python3 tools/b60b_dual_hash_solve.py --block 5
    python3 tools/b60b_dual_hash_solve.py --density 0.3
    python3 tools/b60b_dual_hash_solve.py --all-densities
"""

import argparse
import hashlib
import json
import struct
import time
import zlib
import numpy as np
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
S = 127
N = S * S  # 16,129
DIAG_COUNT = 2 * S - 1  # 253
N_GEO_LINES = S + S + DIAG_COUNT + DIAG_COUNT  # 760


# ---------------------------------------------------------------------------
# CSM loading
# ---------------------------------------------------------------------------
def load_csm_from_file(path, block_idx=0):
    """Load block_idx-th 127x127 CSM from a raw file."""
    data = Path(path).read_bytes()
    bits_per_block = S * S
    start_bit = block_idx * bits_per_block
    csm = np.zeros((S, S), dtype=np.uint8)

    for i in range(bits_per_block):
        src_bit = start_bit + i
        src_byte = src_bit // 8
        src_bit_in_byte = 7 - (src_bit % 8)
        if src_byte < len(data):
            val = (data[src_byte] >> src_bit_in_byte) & 1
            csm[i // S, i % S] = val

    return csm, float(np.sum(csm)) / N


def make_random_csm(density, seed=42):
    """Generate a random S x S CSM at approximately the given density."""
    rng = np.random.default_rng(seed)
    csm = (rng.random((S, S)) < density).astype(np.uint8)
    return csm, float(np.sum(csm)) / N


# ---------------------------------------------------------------------------
# Cross-sum computation (geometric only, no yLTP)
# ---------------------------------------------------------------------------
def compute_geometric_sums(csm):
    """Compute 760 geometric cross-sum targets and line memberships."""
    targets = np.zeros(N_GEO_LINES, dtype=np.int32)
    members = [[] for _ in range(N_GEO_LINES)]
    idx = 0

    # LSM
    for r in range(S):
        targets[idx] = int(np.sum(csm[r, :]))
        members[idx] = [r * S + c for c in range(S)]
        idx += 1

    # VSM
    for c in range(S):
        targets[idx] = int(np.sum(csm[:, c]))
        members[idx] = [r * S + c for r in range(S)]
        idx += 1

    # DSM
    for d in range(DIAG_COUNT):
        offset = d - (S - 1)
        mems = []
        s = 0
        for r in range(S):
            c = r + offset
            if 0 <= c < S:
                s += int(csm[r, c])
                mems.append(r * S + c)
        targets[idx] = s
        members[idx] = mems
        idx += 1

    # XSM
    for x in range(DIAG_COUNT):
        mems = []
        s = 0
        for r in range(S):
            c = x - r
            if 0 <= c < S:
                s += int(csm[r, c])
                mems.append(r * S + c)
        targets[idx] = s
        members[idx] = mems
        idx += 1

    assert idx == N_GEO_LINES
    return targets, members


# ---------------------------------------------------------------------------
# CRC-32
# ---------------------------------------------------------------------------
def build_crc32_generator_matrix():
    """Build 32 x 127 GF(2) generator matrix for CRC-32."""
    total_bytes = (S + 1 + 7) // 8  # 16 bytes
    zero_msg = bytes(total_bytes)
    c = zlib.crc32(zero_msg) & 0xFFFFFFFF
    c_vec = np.array([(c >> (31 - i)) & 1 for i in range(32)], dtype=np.uint8)

    G_crc = np.zeros((32, S), dtype=np.uint8)
    for col in range(S):
        msg = bytearray(total_bytes)
        byte_idx = col // 8
        bit_idx = 7 - (col % 8)
        msg[byte_idx] |= (1 << bit_idx)
        crc_one = zlib.crc32(bytes(msg)) & 0xFFFFFFFF
        col_val = crc_one ^ c
        for i in range(32):
            G_crc[i, col] = (col_val >> (31 - i)) & 1

    return G_crc, c_vec


def compute_row_crc32(csm):
    """Compute CRC-32 for each row."""
    total_bytes = (S + 1 + 7) // 8
    crcs = []
    for r in range(S):
        msg = bytearray(total_bytes)
        for c in range(S):
            if csm[r, c]:
                msg[c // 8] |= (1 << (7 - (c % 8)))
        crcs.append(zlib.crc32(bytes(msg)) & 0xFFFFFFFF)
    return crcs


def compute_col_crc32(csm):
    """Compute CRC-32 for each column (VH).

    Column message: 127 data bits (x_{0,c}, x_{1,c}, ..., x_{126,c}) + 1 pad bit = 128 bits.
    Same encoding as row message but with column data.
    """
    total_bytes = (S + 1 + 7) // 8
    crcs = []
    for c in range(S):
        msg = bytearray(total_bytes)
        for r in range(S):
            if csm[r, c]:
                msg[r // 8] |= (1 << (7 - (r % 8)))
        crcs.append(zlib.crc32(bytes(msg)) & 0xFFFFFFFF)
    return crcs


def compute_block_hash(csm):
    """SHA-256 block hash of CSM (row-major, 2 uint64 words per row)."""
    buf = bytearray(S * 16)
    for r in range(S):
        word0 = 0
        word1 = 0
        for c in range(S):
            if csm[r, c]:
                if c < 64:
                    word0 |= (1 << (63 - c))
                else:
                    word1 |= (1 << (63 - (c - 64)))
        struct.pack_into(">QQ", buf, r * 16, word0, word1)
    return hashlib.sha256(bytes(buf)).digest()


# ---------------------------------------------------------------------------
# GF(2) system assembly (geometric + LH + VH)
# ---------------------------------------------------------------------------
def build_gf2_system(geo_targets, geo_members, row_crcs, col_crcs, G_crc, c_vec):
    """Build the full 8,888 x 16,129 GF(2) matrix and target vector.

    Layout:
      rows 0..759:       geometric parity
      rows 760..4823:    LH (127 rows x 32)
      rows 4824..8887:   VH (127 cols x 32)
    """
    n_geo = N_GEO_LINES      # 760
    n_lh = S * 32             # 4,064
    n_vh = S * 32             # 4,064
    m = n_geo + n_lh + n_vh   # 8,888

    G = np.zeros((m, N), dtype=np.uint8)
    b = np.zeros(m, dtype=np.uint8)

    # Geometric parity
    for i in range(n_geo):
        for j in geo_members[i]:
            G[i, j] = 1
        b[i] = geo_targets[i] % 2

    # LH equations (row r: 32 equations on variables 127*r .. 127*r+126)
    for r in range(S):
        h = row_crcs[r]
        h_vec = np.array([(h >> (31 - i)) & 1 for i in range(32)], dtype=np.uint8)
        target_vec = h_vec ^ c_vec

        base_row = n_geo + r * 32
        base_col = r * S
        for i in range(32):
            for c in range(S):
                G[base_row + i, base_col + c] = G_crc[i, c]
            b[base_row + i] = target_vec[i]

    # VH equations (column c: 32 equations on variables {c, 127+c, 254+c, ...})
    for c in range(S):
        h = col_crcs[c]
        h_vec = np.array([(h >> (31 - i)) & 1 for i in range(32)], dtype=np.uint8)
        target_vec = h_vec ^ c_vec

        base_row = n_geo + n_lh + c * 32
        for i in range(32):
            for r in range(S):
                G[base_row + i, r * S + c] = G_crc[i, r]
            b[base_row + i] = target_vec[i]

    return G, b


# ---------------------------------------------------------------------------
# GF(2) RREF
# ---------------------------------------------------------------------------
def gauss_elim_rref(G, b):
    """GF(2) Gaussian elimination producing RREF.

    Returns (pivot_cols, free_cols, rank, G_rref, b_rref).
    Modifies G and b in-place.
    """
    m, n = G.shape
    pivot_cols = []
    pivot_row = 0

    for col in range(n):
        found = -1
        for r in range(pivot_row, m):
            if G[r, col]:
                found = r
                break
        if found == -1:
            continue

        if found != pivot_row:
            G[[pivot_row, found]] = G[[found, pivot_row]]
            b[pivot_row], b[found] = b[found], b[pivot_row]

        mask = G[:, col].astype(bool)
        mask[pivot_row] = False
        G[mask] ^= G[pivot_row]
        b[mask] ^= b[pivot_row]

        pivot_cols.append(col)
        pivot_row += 1

    rank = len(pivot_cols)
    free_cols = sorted(set(range(n)) - set(pivot_cols))

    return pivot_cols, free_cols, rank, G[:rank], b[:rank]


def extract_determined_from_rref(G_rref, b_rref, pivot_cols, free_cols):
    """Find cells determined by GF(2) alone (pivots with no free dependencies)."""
    free_set = set(free_cols)
    determined = {}

    for i, pcol in enumerate(pivot_cols):
        row = G_rref[i]
        has_free_dep = False
        for fc in free_cols:
            if row[fc]:
                has_free_dep = True
                break
        if not has_free_dep:
            determined[pcol] = int(b_rref[i])

    return determined


# ---------------------------------------------------------------------------
# Integer constraint state (geometric only)
# ---------------------------------------------------------------------------
class IntConstraints:
    def __init__(self, targets, members):
        self.targets = list(targets)
        self.members = [list(m) for m in members]
        self.rho = list(targets)
        self.u = [len(m) for m in members]

        self.cell_lines = [[] for _ in range(N)]
        for i, mems in enumerate(self.members):
            for j in mems:
                self.cell_lines[j].append(i)

    def determine(self, j, v):
        for li in self.cell_lines[j]:
            self.u[li] -= 1
            self.rho[li] -= v


# ---------------------------------------------------------------------------
# Combinators
# ---------------------------------------------------------------------------
def int_bound(ic, D, F, val):
    """IntBound: rho=0 -> force 0, rho=u -> force 1."""
    newly = {}
    queue = list(range(N_GEO_LINES))
    in_queue = set(queue)

    while queue:
        i = queue.pop(0)
        in_queue.discard(i)

        if ic.u[i] == 0:
            continue
        if ic.rho[i] < 0 or ic.rho[i] > ic.u[i]:
            return {}, True

        forced_value = None
        if ic.rho[i] == 0:
            forced_value = 0
        elif ic.rho[i] == ic.u[i]:
            forced_value = 1

        if forced_value is None:
            continue

        for j in ic.members[i]:
            if j not in F:
                continue
            newly[j] = forced_value
            F.discard(j)
            D.add(j)
            val[j] = forced_value
            ic.determine(j, forced_value)
            for li in ic.cell_lines[j]:
                if li not in in_queue:
                    queue.append(li)
                    in_queue.add(li)

    return newly, False


def cross_deduce(ic, D, F, val):
    """CrossDeduce: pairwise cross-line deduction."""
    newly = {}

    for j in list(F):
        if j in newly:
            continue
        j_lines = ic.cell_lines[j]
        forced = None

        for v in (0, 1):
            feasible = True
            for li in j_lines:
                new_rho = ic.rho[li] - v
                new_u = ic.u[li] - 1
                if new_rho < 0 or new_rho > new_u:
                    feasible = False
                    break
            if not feasible:
                if forced is not None:
                    return {}, True
                forced = 1 - v

        if forced is not None:
            newly[j] = forced

    for j, v in newly.items():
        F.discard(j)
        D.add(j)
        val[j] = v
        ic.determine(j, v)

    return newly, False


def propagate_gf2(G, b, newly_determined):
    """Substitute determined cells into GF(2) system."""
    for j, v in newly_determined.items():
        if v == 1:
            mask = G[:, j].astype(bool)
            b[mask] ^= 1
        G[:, j] = 0


# ---------------------------------------------------------------------------
# Fixpoint
# ---------------------------------------------------------------------------
def fixpoint(G, b, ic, max_iter=200):
    """Iterate combinators to convergence."""
    D = set()
    F = set(range(N))
    val = {}
    stats = []

    for iteration in range(1, max_iter + 1):
        prev_D = len(D)

        # Phase A: GF(2) GaussElim
        t0 = time.time()
        pivot_cols, free_cols, rank, G_rref, b_rref = gauss_elim_rref(G.copy(), b.copy())
        t_gauss = time.time() - t0

        det_gf2 = extract_determined_from_rref(G_rref, b_rref, pivot_cols, free_cols)
        det_gf2 = {j: v for j, v in det_gf2.items() if j in F}

        if det_gf2:
            for j, v in det_gf2.items():
                F.discard(j)
                D.add(j)
                val[j] = v
                ic.determine(j, v)
            propagate_gf2(G, b, det_gf2)

        # Phase B: IntBound
        t0 = time.time()
        det_int, inconsistent = int_bound(ic, D, F, val)
        t_int = time.time() - t0
        if inconsistent:
            print(f"  Iter {iteration}: INCONSISTENT in IntBound")
            return D, F, val, stats, False
        if det_int:
            propagate_gf2(G, b, det_int)

        # Phase C: CrossDeduce
        t0 = time.time()
        det_cross, inconsistent = cross_deduce(ic, D, F, val)
        t_cross = time.time() - t0
        if inconsistent:
            print(f"  Iter {iteration}: INCONSISTENT in CrossDeduce")
            return D, F, val, stats, False
        if det_cross:
            propagate_gf2(G, b, det_cross)

        new_D = len(D)
        gain = new_D - prev_D

        iter_stats = {
            "iteration": iteration,
            "D": new_D, "F": len(F),
            "gain": gain,
            "from_gauss": len(det_gf2),
            "from_intbound": len(det_int),
            "from_crossdeduce": len(det_cross),
            "rank": rank,
            "t_gauss": round(t_gauss, 3),
            "t_int": round(t_int, 3),
            "t_cross": round(t_cross, 3),
        }
        stats.append(iter_stats)

        pct = new_D / N * 100
        print(f"  Iter {iteration}: |D|={new_D} ({pct:.1f}%) |F|={len(F)} "
              f"[+{len(det_gf2)} GF2, +{len(det_int)} IntBound, +{len(det_cross)} CrossDeduce] "
              f"rank={rank} ({t_gauss:.1f}s)")

        if gain == 0 or new_D == N:
            break

    return D, F, val, stats, True


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def run_one(csm, source_label, actual_density, bh):
    """Run the B.60b pipeline on one CSM. Returns result dict."""
    popcount = int(np.sum(csm))
    print(f"  Density: {actual_density:.3f} ({popcount} / {N})")

    # Compute projections
    print("  Computing geometric cross-sums...", flush=True)
    geo_targets, geo_members = compute_geometric_sums(csm)

    print("  Computing CRC-32 LH (rows)...", flush=True)
    row_crcs = compute_row_crc32(csm)

    print("  Computing CRC-32 VH (columns)...", flush=True)
    col_crcs = compute_col_crc32(csm)

    print("  Building CRC-32 generator matrix...", flush=True)
    G_crc, c_vec = build_crc32_generator_matrix()

    # Build GF(2) system
    print(f"  Building GF(2) system (8888 x {N})...", flush=True)
    t0 = time.time()
    G, b_vec = build_gf2_system(geo_targets, geo_members, row_crcs, col_crcs, G_crc, c_vec)
    print(f"  Done in {time.time() - t0:.2f}s. Shape: {G.shape}")

    # Build integer constraints (geometric only)
    ic = IntConstraints(geo_targets, geo_members)

    # Run fixpoint
    print(f"\n  {'=' * 68}")
    print(f"  Fixpoint iteration — {source_label}")
    print(f"  {'=' * 68}")
    t_start = time.time()
    D, F, val, stats, feasible = fixpoint(G, b_vec, ic)
    t_total = time.time() - t_start

    n_determined = len(D)
    n_free = len(F)
    pct = n_determined / N * 100

    print(f"\n  Results — {source_label}")
    print(f"  Determined: {n_determined} / {N} ({pct:.1f}%)")
    print(f"  Free:       {n_free}")
    print(f"  Time:       {t_total:.1f}s")

    if stats:
        total_gauss = sum(s["from_gauss"] for s in stats)
        total_int = sum(s["from_intbound"] for s in stats)
        total_cross = sum(s["from_crossdeduce"] for s in stats)
        print(f"  GaussElim:  {total_gauss}")
        print(f"  IntBound:   {total_int}")
        print(f"  CrossDeduce:{total_cross}")

    # Verify if fully solved
    verified = False
    if n_free == 0 and feasible:
        print("\n  *** FULLY SOLVED — verifying SHA-256... ***")
        recon = np.zeros((S, S), dtype=np.uint8)
        for j, v in val.items():
            recon[j // S, j % S] = v
        recon_bh = compute_block_hash(recon)
        if recon_bh == bh:
            print("  SHA-256 VERIFIED!")
            verified = True
            if np.array_equal(recon, csm):
                print("  Matches original CSM: True")
        else:
            print("  SHA-256 MISMATCH")

    # Outcome
    if n_free == 0:
        outcome = "FULL_SOLVE"
    elif n_free <= 25:
        outcome = "TRACTABLE_SEARCH"
    elif n_free <= 2000:
        outcome = "H2_MAJOR_IMPROVEMENT"
    elif n_free <= 8000:
        outcome = "H3_MODERATE_IMPROVEMENT"
    else:
        outcome = "H4_NO_IMPROVEMENT"

    print(f"  Outcome: {outcome}")

    return {
        "source": source_label,
        "density": round(actual_density, 4),
        "popcount": popcount,
        "n_determined": n_determined,
        "n_free": n_free,
        "pct_determined": round(pct, 2),
        "feasible": feasible,
        "verified": verified,
        "total_time_s": round(t_total, 2),
        "iterations": len(stats),
        "outcome": outcome,
        "stats": stats,
    }


def main():
    parser = argparse.ArgumentParser(description="B.60b: Dual-hash combinator pipeline at S=127")
    parser.add_argument("--block", type=int, default=0, help="Block index from MP4")
    parser.add_argument("--density", type=float, default=None, help="Synthetic block density")
    parser.add_argument("--all-densities", action="store_true",
                        help="Test synthetic blocks at 10%, 30%, 50%, 70%, 90% + MP4 block 0")
    parser.add_argument("--out", default=str(REPO_ROOT / "tools" / "b60b_results.json"))
    args = parser.parse_args()

    mp4_path = REPO_ROOT / "docs" / "testData" / "useless-machine.mp4"

    print("B.60b: Dual-Hash Combinator Pipeline (LH + VH, no yLTP)")
    print(f"  S={S}, N={N}, geometric_lines={N_GEO_LINES}")
    print(f"  GF(2) equations: 8,888 (760 geo + 4,064 LH + 4,064 VH)")
    print()

    all_results = []

    if args.all_densities:
        # MP4 block 0
        print(f"{'#' * 72}")
        print(f"MP4 block 0")
        print(f"{'#' * 72}")
        csm, density = load_csm_from_file(mp4_path, 0)
        bh = compute_block_hash(csm)
        all_results.append(run_one(csm, "mp4_block_0", density, bh))
        print()

        # Synthetic densities
        for d in [0.1, 0.3, 0.5, 0.7, 0.9]:
            print(f"{'#' * 72}")
            print(f"Synthetic density={d}")
            print(f"{'#' * 72}")
            csm, actual_d = make_random_csm(d)
            bh = compute_block_hash(csm)
            all_results.append(run_one(csm, f"synthetic_{d}", actual_d, bh))
            print()
    elif args.density is not None:
        csm, actual_d = make_random_csm(args.density)
        bh = compute_block_hash(csm)
        all_results.append(run_one(csm, f"synthetic_{args.density}", actual_d, bh))
    else:
        csm, density = load_csm_from_file(mp4_path, args.block)
        bh = compute_block_hash(csm)
        all_results.append(run_one(csm, f"mp4_block_{args.block}", density, bh))

    # Summary
    print(f"\n{'=' * 72}")
    print("B.60b Summary")
    print(f"{'=' * 72}")
    print(f"{'Source':25s} {'Density':>8s} {'|D|':>7s} {'|F|':>7s} {'%Det':>7s} {'Outcome':25s}")
    print("-" * 80)
    for r in all_results:
        print(f"{r['source']:25s} {r['density']:8.3f} {r['n_determined']:7d} {r['n_free']:7d} "
              f"{r['pct_determined']:6.1f}% {r['outcome']:25s}")

    # B.58b comparison
    print(f"\nB.58b comparison (LH + 2 yLTP):")
    print(f"  B.58b MP4 block 0: |D|=3,600 (22.3%), |F|=12,529")
    if all_results:
        mp4_r = [r for r in all_results if "mp4" in r["source"]]
        if mp4_r:
            r = mp4_r[0]
            improvement = r["n_determined"] - 3600
            null_reduction = 12529 - r["n_free"]
            print(f"  B.60b MP4 block 0: |D|={r['n_determined']} ({r['pct_determined']:.1f}%), "
                  f"|F|={r['n_free']}")
            print(f"  Improvement: +{improvement} determined, -{null_reduction} free")

    # Write results
    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "experiment": "B.60b",
        "config": {
            "S": S, "N": N,
            "geo_lines": N_GEO_LINES,
            "lh_equations": S * 32,
            "vh_equations": S * 32,
            "total_gf2_equations": 8888,
        },
        "results": all_results,
    }
    out_path.write_text(json.dumps(summary, indent=2))
    print(f"\nResults: {out_path}")


if __name__ == "__main__":
    main()
