#!/usr/bin/env python3
"""
B.40 Phase 1 Analysis: Hash-Failure Correlation Harvesting.

Reads the B40P binary profile emitted by the CRSCE solver (via CRSCE_B40_PROFILE env var)
and computes the per-cell correlation score phi[r][c] = fail_rate - pass_rate.

Outputs:
  - Per-row summary: total fails, total passes, phi distribution
  - Top-K cells with highest |phi| per row (the cells most correlated with SHA-1 failure)
  - JSON output for Phase 2 consumption

Usage:
    python3 tools/b40_analyze.py /tmp/b40_profile.bin
    python3 tools/b40_analyze.py /tmp/b40_profile.bin --output tools/b40_phi.json
"""
# Copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.

import argparse
import json
import pathlib
import struct
import sys

try:
    import numpy as np
except ImportError:
    sys.exit("ERROR: numpy required. Install with: pip install numpy")


def load_b40p(path: pathlib.Path) -> dict:
    """Load a B40P binary profile file."""
    data = path.read_bytes()
    if len(data) < 6:
        sys.exit(f"ERROR: {path} too small ({len(data)} bytes)")

    magic = data[:4]
    if magic != b"B40P":
        sys.exit(f"ERROR: bad magic in {path}: {magic!r} (expected b'B40P')")

    dim = struct.unpack_from("<H", data, 4)[0]
    print(f"  Dimension: {dim}", flush=True)

    offset = 6
    n = dim

    # Per-row fail counts: uint32[dim]
    row_fails = np.frombuffer(data, dtype="<u4", count=n, offset=offset).copy()
    offset += n * 4

    # Per-row pass counts: uint32[dim]
    row_passes = np.frombuffer(data, dtype="<u4", count=n, offset=offset).copy()
    offset += n * 4

    # Per-cell fail counts: uint32[dim * dim]
    fail_count = np.frombuffer(data, dtype="<u4", count=n * n, offset=offset).copy().reshape(n, n)
    offset += n * n * 4

    # Per-cell pass counts: uint32[dim * dim]
    pass_count = np.frombuffer(data, dtype="<u4", count=n * n, offset=offset).copy().reshape(n, n)
    offset += n * n * 4

    expected_size = 6 + 2 * n * 4 + 2 * n * n * 4
    if len(data) < expected_size:
        sys.exit(f"ERROR: {path} size {len(data)} < expected {expected_size}")

    return {
        "dim": dim,
        "row_fails": row_fails,
        "row_passes": row_passes,
        "fail_count": fail_count,
        "pass_count": pass_count,
    }


def compute_phi(profile: dict) -> np.ndarray:
    """
    Compute phi[r][c] = fail_rate[r][c] - pass_rate[r][c].

    fail_rate[r][c] = fail_count[r][c] / total_fail[r]  (fraction of failures where cell=1)
    pass_rate[r][c] = pass_count[r][c] / total_pass[r]  (fraction of passes where cell=1)

    Positive phi: cell appears more often in failing assignments than passing.
    Negative phi: cell appears more often in passing assignments.
    Near-zero phi: cell is uncorrelated with hash outcome.
    """
    dim = profile["dim"]
    fail_count = profile["fail_count"].astype(np.float64)
    pass_count = profile["pass_count"].astype(np.float64)
    row_fails = profile["row_fails"].astype(np.float64)
    row_passes = profile["row_passes"].astype(np.float64)

    phi = np.zeros((dim, dim), dtype=np.float64)

    for r in range(dim):
        if row_fails[r] > 0 and row_passes[r] > 0:
            fail_rate = fail_count[r] / row_fails[r]
            pass_rate = pass_count[r] / row_passes[r]
            phi[r] = fail_rate - pass_rate
        # If no fails or no passes for a row, phi stays 0 (no signal)

    return phi


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("profile", type=pathlib.Path, help="Path to B40P binary profile")
    parser.add_argument("--output", type=pathlib.Path, default=None,
                        help="Path to write JSON phi table (default: stdout summary only)")
    parser.add_argument("--top-k", type=int, default=20,
                        help="Show top-K cells per row by |phi| (default: 20)")
    args = parser.parse_args()

    print(f"Loading B40P profile from {args.profile} ...", flush=True)
    profile = load_b40p(args.profile)

    dim = profile["dim"]
    row_fails = profile["row_fails"]
    row_passes = profile["row_passes"]

    print(f"\n{'='*72}", flush=True)
    print("PER-ROW SHA-1 EVENT SUMMARY", flush=True)
    print(f"{'='*72}", flush=True)

    total_fails = int(np.sum(row_fails))
    total_passes = int(np.sum(row_passes))
    active_rows = int(np.sum((row_fails > 0) | (row_passes > 0)))
    fail_rows = int(np.sum(row_fails > 0))
    pass_rows = int(np.sum(row_passes > 0))
    both_rows = int(np.sum((row_fails > 0) & (row_passes > 0)))

    print(f"  Total SHA-1 failures: {total_fails:,}", flush=True)
    print(f"  Total SHA-1 passes:   {total_passes:,}", flush=True)
    print(f"  Rows with any event:  {active_rows} / {dim}", flush=True)
    print(f"  Rows with failures:   {fail_rows}", flush=True)
    print(f"  Rows with passes:     {pass_rows}", flush=True)
    print(f"  Rows with both:       {both_rows}", flush=True)

    if total_fails == 0 and total_passes == 0:
        print("\n  NO SHA-1 EVENTS RECORDED. Solver may not have reached the plateau.", flush=True)
        print("  Try running longer (increase MAX_COMPRESSION_TIME).", flush=True)
        return

    # Show rows with most failures
    top_fail_rows = np.argsort(row_fails)[::-1][:20]
    print(f"\n  Top 20 rows by failure count:", flush=True)
    for r in top_fail_rows:
        if row_fails[r] == 0:
            break
        print(f"    row {r:3d}: {row_fails[r]:8,} fails, {row_passes[r]:8,} passes", flush=True)

    # Compute phi
    print(f"\n{'='*72}", flush=True)
    print("PHI CORRELATION ANALYSIS", flush=True)
    print(f"{'='*72}", flush=True)

    phi = compute_phi(profile)

    # Global phi distribution (across rows that have both fails and passes)
    rows_with_signal = np.where((row_fails > 0) & (row_passes > 0))[0]
    if len(rows_with_signal) == 0:
        print("\n  No rows with both failures and passes. Cannot compute phi.", flush=True)
        print("  This means every row that was verified either always failed or always passed.", flush=True)
        print("  B.40 H4 (null correlation): SHA-1 failures are row-level, not cell-level.", flush=True)
        return

    phi_active = phi[rows_with_signal]
    phi_flat = phi_active.flatten()
    phi_nonzero = phi_flat[phi_flat != 0]

    print(f"\n  Rows with phi signal: {len(rows_with_signal)}", flush=True)
    print(f"  Phi distribution (non-zero entries):", flush=True)
    if len(phi_nonzero) > 0:
        print(f"    count:  {len(phi_nonzero):,}", flush=True)
        print(f"    min:    {np.min(phi_nonzero):.6f}", flush=True)
        print(f"    p5:     {np.percentile(phi_nonzero, 5):.6f}", flush=True)
        print(f"    median: {np.median(phi_nonzero):.6f}", flush=True)
        print(f"    mean:   {np.mean(phi_nonzero):.6f}", flush=True)
        print(f"    p95:    {np.percentile(phi_nonzero, 95):.6f}", flush=True)
        print(f"    max:    {np.max(phi_nonzero):.6f}", flush=True)
        print(f"    std:    {np.std(phi_nonzero):.6f}", flush=True)

        # Assess signal strength
        abs_phi = np.abs(phi_nonzero)
        strong_cells = int(np.sum(abs_phi > 0.1))
        moderate_cells = int(np.sum(abs_phi > 0.05))
        print(f"\n  Cells with |phi| > 0.10: {strong_cells:,} (strong correlation)", flush=True)
        print(f"  Cells with |phi| > 0.05: {moderate_cells:,} (moderate correlation)", flush=True)

        if strong_cells == 0 and moderate_cells == 0:
            print(f"\n  B.40 OUTCOME H4: No significant cell-level correlation detected.", flush=True)
            print(f"  SHA-1 failures appear independent of individual cell assignments.", flush=True)
        elif strong_cells > 0:
            print(f"\n  B.40 OUTCOME: Significant correlation detected!", flush=True)
            print(f"  Proceed to Phase 2 (priority branching by phi).", flush=True)

    # Per-row top-K analysis
    print(f"\n{'='*72}", flush=True)
    print(f"TOP-{args.top_k} CELLS PER ROW BY |PHI|", flush=True)
    print(f"{'='*72}", flush=True)

    for r in rows_with_signal[:30]:  # Show at most 30 rows
        row_phi = phi[r]
        abs_row_phi = np.abs(row_phi)
        top_cols = np.argsort(abs_row_phi)[::-1][:args.top_k]

        if abs_row_phi[top_cols[0]] < 0.001:
            continue  # Skip rows with no signal

        print(f"\n  Row {r} (fails={int(row_fails[r]):,}, passes={int(row_passes[r]):,}):", flush=True)
        for rank, c in enumerate(top_cols):
            p = phi[r, c]
            if abs(p) < 0.001:
                break
            direction = "FAIL+" if p > 0 else "PASS+"
            print(f"    #{rank+1:2d}: col={c:3d}, phi={p:+.4f} ({direction})", flush=True)

    # Write JSON output
    if args.output is not None:
        print(f"\nWriting phi table to {args.output} ...", flush=True)

        result = {
            "experiment": "B.40 Phase 1",
            "dim": dim,
            "total_fails": total_fails,
            "total_passes": total_passes,
            "rows_with_signal": len(rows_with_signal),
            "phi_stats": {
                "mean": float(np.mean(phi_nonzero)) if len(phi_nonzero) > 0 else None,
                "std": float(np.std(phi_nonzero)) if len(phi_nonzero) > 0 else None,
                "strong_cells": strong_cells if len(phi_nonzero) > 0 else 0,
                "moderate_cells": moderate_cells if len(phi_nonzero) > 0 else 0,
            },
            "per_row": {},
        }

        # Store top-K phi cells per row (for Phase 2 consumption)
        for r in rows_with_signal:
            abs_row_phi = np.abs(phi[r])
            top_cols = np.argsort(abs_row_phi)[::-1][:50]  # Top 50 per row
            cells = []
            for c in top_cols:
                p = float(phi[r, int(c)])
                if abs(p) < 0.001:
                    break
                cells.append({"col": int(c), "phi": round(p, 6)})
            if cells:
                result["per_row"][str(int(r))] = {
                    "fails": int(row_fails[r]),
                    "passes": int(row_passes[r]),
                    "top_cells": cells,
                }

        with open(args.output, "w") as f:
            json.dump(result, f, indent=2)
        print("Done.", flush=True)


if __name__ == "__main__":
    main()
