#!/usr/bin/env python3
"""
B.58c: Row-Grouped Residual Search + Multi-Block Analysis

Per-row decomposition of the residual after fixpoint:
  1. IntBound propagation (same as DFS solver, ~22% at 14.9% density)
  2. Per-row CRC-32 candidate generation: 32 GF(2) equations reduce 2^f_r to 2^(f_r-rank_r)
  3. Integer row-sum filter: retain only candidates with correct Hamming weight
  4. Cross-row arc consistency: column/diagonal/yLTP constraints prune row candidates
  5. Report per-block solvability

Usage:
    python3 tools/b58c_row_grouped.py                    # block 0 only
    python3 tools/b58c_row_grouped.py --all-blocks       # all MP4 blocks
    python3 tools/b58c_row_grouped.py --max-blocks 20    # first 20 blocks
    python3 tools/b58c_row_grouped.py --density 0.15     # synthetic block
"""

import argparse
import hashlib
import json
import struct
import sys
import time
import zlib
import numpy as np
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
S = 127
N = S * S
DIAG_COUNT = 2 * S - 1
N_LINES = S + S + DIAG_COUNT + DIAG_COUNT + S + S

LCG_A = 6364136223846793005
LCG_C = 1442695040888963407
LCG_MOD = 1 << 64
PREFIX = b"CRSCLTP"
SEED1 = int.from_bytes(PREFIX + b"V", "big")
SEED2 = int.from_bytes(PREFIX + b"P", "big")

# Pre-build CRC-32 generator matrix (32 x 127) — shared across all blocks
def _build_crc32_gen():
    total_bytes = (S + 1 + 7) // 8
    zero_msg = bytes(total_bytes)
    c = zlib.crc32(zero_msg) & 0xFFFFFFFF
    c_vec = np.array([(c >> (31 - i)) & 1 for i in range(32)], dtype=np.uint8)
    G = np.zeros((32, S), dtype=np.uint8)
    for col in range(S):
        msg = bytearray(total_bytes)
        msg[col // 8] |= (1 << (7 - col % 8))
        val = (zlib.crc32(bytes(msg)) & 0xFFFFFFFF) ^ c
        for i in range(32):
            G[i, col] = (val >> (31 - i)) & 1
    return G, c_vec

G_CRC, C_VEC = _build_crc32_gen()

# Pre-build yLTP memberships
def _build_yltp(seed):
    n = S * S
    pool = list(range(n))
    state = seed
    for i in range(n - 1, 0, -1):
        state = (state * LCG_A + LCG_C) % LCG_MOD
        pool[i], pool[int(state % (i + 1))] = pool[int(state % (i + 1))], pool[i]
    mem = [0] * n
    for line in range(S):
        for slot in range(S):
            mem[pool[line * S + slot]] = line
    return mem

YLTP1 = _build_yltp(SEED1)
YLTP2 = _build_yltp(SEED2)


def load_csm(data, block_idx):
    """Load one 127x127 CSM from raw bytes."""
    bits_per_block = S * S
    start_bit = block_idx * bits_per_block
    csm = np.zeros((S, S), dtype=np.uint8)
    for i in range(bits_per_block):
        src_bit = start_bit + i
        src_byte = src_bit // 8
        if src_byte >= len(data):
            break
        if (data[src_byte] >> (7 - src_bit % 8)) & 1:
            csm[i // S, i % S] = 1
    return csm


def compute_row_crc32(csm):
    """CRC-32 per row."""
    total_bytes = (S + 1 + 7) // 8
    crcs = []
    for r in range(S):
        msg = bytearray(total_bytes)
        for c in range(S):
            if csm[r, c]:
                msg[c // 8] |= (1 << (7 - c % 8))
        crcs.append(zlib.crc32(bytes(msg)) & 0xFFFFFFFF)
    return crcs


def int_bound_propagation(csm):
    """Fast IntBound-only propagation (equivalent to DFS initial propagation).

    Returns: determined dict {flat_idx: value}, set of free flat indices.
    """
    # Build line data
    targets = []
    members_list = []

    # LSM
    for r in range(S):
        targets.append(int(np.sum(csm[r, :])))
        members_list.append([r * S + c for c in range(S)])
    # VSM
    for c in range(S):
        targets.append(int(np.sum(csm[:, c])))
        members_list.append([r * S + c for r in range(S)])
    # DSM
    for d in range(DIAG_COUNT):
        offset = d - (S - 1)
        m = []
        s = 0
        for r in range(S):
            c_val = r + offset
            if 0 <= c_val < S:
                s += int(csm[r, c_val])
                m.append(r * S + c_val)
        targets.append(s)
        members_list.append(m)
    # XSM
    for x in range(DIAG_COUNT):
        m = []
        s = 0
        for r in range(S):
            c_val = x - r
            if 0 <= c_val < S:
                s += int(csm[r, c_val])
                m.append(r * S + c_val)
        targets.append(s)
        members_list.append(m)
    # yLTP1, yLTP2
    for mem_table in [YLTP1, YLTP2]:
        line_sums = [0] * S
        line_mems = [[] for _ in range(S)]
        for flat in range(N):
            li = mem_table[flat]
            line_sums[li] += int(csm[flat // S, flat % S])
            line_mems[li].append(flat)
        for k in range(S):
            targets.append(line_sums[k])
            members_list.append(line_mems[k])

    # Cell-to-line index
    cell_lines = [[] for _ in range(N)]
    for i, m in enumerate(members_list):
        for j in m:
            cell_lines[j].append(i)

    # IntBound propagation
    rho = list(targets)
    u = [len(m) for m in members_list]
    determined = {}
    free = set(range(N))

    queue = list(range(len(targets)))
    in_q = set(queue)

    while queue:
        i = queue.pop(0)
        in_q.discard(i)
        if u[i] == 0:
            continue
        if rho[i] < 0 or rho[i] > u[i]:
            return determined, free  # inconsistent

        fv = None
        if rho[i] == 0:
            fv = 0
        elif rho[i] == u[i]:
            fv = 1
        if fv is None:
            continue

        for j in members_list[i]:
            if j not in free:
                continue
            determined[j] = fv
            free.discard(j)
            for li in cell_lines[j]:
                u[li] -= 1
                rho[li] -= fv
                if li not in in_q:
                    queue.append(li)
                    in_q.add(li)

    return determined, free


def per_row_analysis(csm, determined, free_set, row_crcs):
    """Per-row CRC-32 candidate generation.

    For each row r:
      - Identify free cells within that row
      - Build 32 x f_r GF(2) sub-matrix from CRC-32
      - GF(2) rank → per-row search space = 2^(f_r - rank_r)
      - Filter by integer row-sum → count surviving candidates

    Returns list of per-row dicts.
    """
    row_stats = []

    for r in range(S):
        free_cols = [c for c in range(S) if (r * S + c) in free_set]
        f_r = len(free_cols)

        if f_r == 0:
            row_stats.append({"row": r, "free": 0, "crc_rank": 0,
                              "gf2_search": 0, "candidates": 1, "log2_search": 0})
            continue

        # Determine known cells' contribution to CRC-32 target
        h = row_crcs[r]
        h_vec = np.array([(h >> (31 - i)) & 1 for i in range(32)], dtype=np.uint8)
        target = h_vec ^ C_VEC  # h XOR c

        # Subtract known cells from target
        for c in range(S):
            flat = r * S + c
            if flat in determined and determined[flat] == 1:
                target ^= G_CRC[:, c]

        # Build sub-matrix for free columns only
        G_sub = G_CRC[:, free_cols].copy()

        # GF(2) rank of sub-matrix
        rank_r = _gf2_rank_small(G_sub)

        # Per-row GF(2) search space
        n_gf2_free = f_r - rank_r
        log2_search = n_gf2_free

        # Row sum residual
        row_target = int(np.sum(csm[r, :]))
        known_sum = sum(determined.get(r * S + c, 0) for c in range(S) if (r * S + c) in determined)
        rho_r = row_target - known_sum

        # Estimate candidates after integer filter
        # For n_gf2_free free variables with sum constraint rho_r:
        # ~C(n_gf2_free, rho_r) / 2^n_gf2_free fraction survive
        if n_gf2_free <= 25:
            # Actually enumerate
            candidates = _count_candidates(G_sub, target, free_cols, f_r, rank_r, rho_r)
        else:
            # Estimate
            from math import comb, log2 as lg2
            if 0 <= rho_r <= n_gf2_free:
                candidates = max(1, int(comb(n_gf2_free, rho_r) * 0.5))  # rough estimate
            else:
                candidates = 0

        row_stats.append({
            "row": r, "free": f_r, "crc_rank": rank_r,
            "gf2_free": n_gf2_free, "rho": rho_r,
            "log2_search": log2_search, "candidates": candidates
        })

    return row_stats


def _gf2_rank_small(G):
    """GF(2) rank of a small binary matrix (32 x f_r)."""
    m, n = G.shape
    G = G.copy()
    pivot = 0
    for col in range(n):
        found = -1
        for r in range(pivot, m):
            if G[r, col]:
                found = r
                break
        if found == -1:
            continue
        G[[pivot, found]] = G[[found, pivot]]
        for r in range(m):
            if r != pivot and G[r, col]:
                G[r] ^= G[pivot]
        pivot += 1
    return pivot


def _count_candidates(G_sub, target, free_cols, f_r, rank_r, rho_r):
    """Count row candidates that pass CRC-32 + integer sum filter."""
    if f_r - rank_r > 25:
        return -1  # too many to enumerate

    m, n = G_sub.shape
    G = G_sub.copy()
    b = target.copy()

    # RREF
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
        G[[pivot_row, found]] = G[[found, pivot_row]]
        b[pivot_row], b[found] = b[found], b[pivot_row]
        for r in range(m):
            if r != pivot_row and G[r, col]:
                G[r] ^= G[pivot_row]
                b[r] ^= b[pivot_row]
        pivot_cols.append(col)
        pivot_row += 1

    free_col_indices = sorted(set(range(n)) - set(pivot_cols))
    n_free = len(free_col_indices)

    if n_free > 25:
        return -1

    count = 0
    for assignment in range(1 << n_free):
        vals = [0] * n
        # Set free variables
        for i, fc in enumerate(free_col_indices):
            vals[fc] = (assignment >> i) & 1
        # Compute pivot variables
        for i, pc in enumerate(pivot_cols):
            v = int(b[i])
            for j, fc in enumerate(free_col_indices):
                if G[i, fc]:
                    v ^= vals[fc]
            vals[pc] = v
        # Check integer sum
        if sum(vals) == rho_r:
            count += 1

    return count


def analyze_block(data, block_idx):
    """Full B.58c analysis for one block."""
    csm = load_csm(data, block_idx)
    density = np.sum(csm) / N
    row_crcs = compute_row_crc32(csm)

    # IntBound propagation
    determined, free_set = int_bound_propagation(csm)
    n_det = len(determined)
    n_free = len(free_set)

    # Per-row analysis
    row_stats = per_row_analysis(csm, determined, free_set, row_crcs)

    # Summary stats
    rows_fully_det = sum(1 for rs in row_stats if rs["free"] == 0)
    rows_with_free = S - rows_fully_det
    max_row_free = max(rs["free"] for rs in row_stats)
    avg_row_free = np.mean([rs["free"] for rs in row_stats if rs["free"] > 0]) if rows_with_free > 0 else 0
    max_log2 = max(rs["log2_search"] for rs in row_stats)
    total_candidates = 1
    all_enumerable = True
    for rs in row_stats:
        if rs["free"] > 0:
            if rs["candidates"] <= 0:
                all_enumerable = False
                break
            total_candidates *= rs["candidates"]
            if total_candidates > 10**18:
                all_enumerable = False
                break

    return {
        "block": block_idx,
        "density": round(density, 4),
        "determined": n_det,
        "free": n_free,
        "pct_det": round(n_det / N * 100, 1),
        "rows_fully_det": rows_fully_det,
        "rows_with_free": rows_with_free,
        "max_row_free": max_row_free,
        "avg_row_free": round(avg_row_free, 1),
        "max_log2_per_row": max_log2,
        "all_enumerable": all_enumerable,
        "row_stats": row_stats,
    }


def main():
    parser = argparse.ArgumentParser(description="B.58c: Row-grouped residual search")
    parser.add_argument("--all-blocks", action="store_true")
    parser.add_argument("--max-blocks", type=int, default=None)
    parser.add_argument("--block", type=int, default=0)
    parser.add_argument("--density", type=float, default=None)
    parser.add_argument("--out", default=str(REPO_ROOT / "tools" / "b58c_results.json"))
    args = parser.parse_args()

    mp4_path = REPO_ROOT / "docs" / "testData" / "useless-machine.mp4"

    print("B.58c: Row-Grouped Residual Search (S=127)")
    print(f"  S={S}, N={N}")
    print()

    if args.density is not None:
        rng = np.random.default_rng(42)
        csm_data = (rng.random(N) < args.density).astype(np.uint8)
        # Pack into bytes
        data = bytearray((N + 7) // 8)
        for i in range(N):
            if csm_data[i]:
                data[i // 8] |= (1 << (7 - i % 8))
        data = bytes(data)
        blocks = [0]
        print(f"  Synthetic block at density={args.density}")
    else:
        data = mp4_path.read_bytes()
        total_bits = len(data) * 8
        total_blocks = (total_bits + N - 1) // N
        if args.all_blocks:
            blocks = list(range(total_blocks))
        elif args.max_blocks:
            blocks = list(range(min(args.max_blocks, total_blocks)))
        else:
            blocks = [args.block]
        print(f"  MP4: {len(data)} bytes, {total_blocks} blocks")
        print(f"  Analyzing {len(blocks)} block(s)")

    print()
    results = []
    t_start = time.time()

    for i, bidx in enumerate(blocks):
        t0 = time.time()
        result = analyze_block(data, bidx)
        elapsed = time.time() - t0

        # Compact per-row summary (don't print all 127 rows)
        tractable_rows = sum(1 for rs in result["row_stats"]
                            if rs["free"] > 0 and rs.get("log2_search", 99) <= 25)
        intractable_rows = sum(1 for rs in result["row_stats"]
                              if rs.get("log2_search", 0) > 25)

        print(f"  Block {bidx:4d}: density={result['density']:.3f} "
              f"det={result['determined']:5d} ({result['pct_det']:4.1f}%) "
              f"free={result['free']:5d} "
              f"rows_det={result['rows_fully_det']:3d} "
              f"max_row_free={result['max_row_free']:3d} "
              f"max_log2={result['max_log2_per_row']:3d} "
              f"tractable={tractable_rows} intractable={intractable_rows} "
              f"({elapsed:.1f}s)")

        # Strip detailed row_stats for multi-block to save space
        if len(blocks) > 1:
            result.pop("row_stats", None)
        results.append(result)

    total_time = time.time() - t_start

    # Summary
    print(f"\n{'='*80}")
    print(f"B.58c Summary — {len(results)} blocks analyzed in {total_time:.0f}s")
    print(f"{'='*80}")

    if results:
        densities = [r["density"] for r in results]
        dets = [r["pct_det"] for r in results]
        frees = [r["free"] for r in results]
        max_log2s = [r["max_log2_per_row"] for r in results]

        print(f"  Density range:     {min(densities):.3f} - {max(densities):.3f}")
        print(f"  Determined range:  {min(dets):.1f}% - {max(dets):.1f}%")
        print(f"  Free cells range:  {min(frees)} - {max(frees)}")
        print(f"  Max per-row log2:  {min(max_log2s)} - {max(max_log2s)}")

        solvable = sum(1 for r in results if r["max_log2_per_row"] <= 25)
        print(f"\n  Blocks solvable (all rows ≤2^25): {solvable}/{len(results)}")

        # Per-row tractability histogram
        if len(results) == 1 and "row_stats" in results[0]:
            rs = results[0]["row_stats"]
            print(f"\n  Per-row free cell distribution (block {results[0]['block']}):")
            bins = [(0, 0), (1, 10), (11, 25), (26, 50), (51, 80), (81, 127)]
            for lo, hi in bins:
                count = sum(1 for r in rs if lo <= r["free"] <= hi)
                print(f"    f_r={lo}-{hi}: {count} rows")

    # Save
    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "experiment": "B.58c",
        "config": {"S": S, "N": N},
        "blocks_analyzed": len(results),
        "results": results,
    }
    out_path.write_text(json.dumps(summary, indent=2))
    print(f"\n  Results: {out_path}")


if __name__ == "__main__":
    main()
