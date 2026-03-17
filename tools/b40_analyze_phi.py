#!/usr/bin/env python3
"""
B.40 Phase 1 Analysis: Hash-Failure Correlation Harvesting.

Reads the binary profile produced by RowDecomposedController when
CRSCE_B40_PROFILE is set, computes phi[r][c] = fail_rate - pass_rate,
and emits per-row reordering tables for Phase 2.

Usage:
    python3 tools/b40_analyze_phi.py <profile.b40>

Output files (written alongside the input):
    <profile>_phi.csv       -- full phi matrix (row, col, phi, fail_count, pass_count)
    <profile>_row_order.csv -- per-row cell ordering by descending phi
    <profile>_summary.txt   -- aggregate statistics
"""
# Copyright (c) 2026 Sam Caldwell. See LICENSE.txt for details.

import struct
import sys
from pathlib import Path

import numpy as np


def load_b40_profile(path: Path) -> tuple[int, np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    """Load a B40P binary profile file.

    Returns (kS, row_fails, row_passes, fail_count, pass_count).
    """
    with open(path, "rb") as fh:
        magic = fh.read(4)
        if magic != b"B40P":
            raise ValueError(f"Bad magic: {magic!r}, expected b'B40P'")
        (dim,) = struct.unpack("<H", fh.read(2))

        row_fails = np.frombuffer(fh.read(dim * 4), dtype=np.uint32).copy()
        row_passes = np.frombuffer(fh.read(dim * 4), dtype=np.uint32).copy()
        fail_count = np.frombuffer(fh.read(dim * dim * 4), dtype=np.uint32).reshape(dim, dim).copy()
        pass_count = np.frombuffer(fh.read(dim * dim * 4), dtype=np.uint32).reshape(dim, dim).copy()

    return dim, row_fails, row_passes, fail_count, pass_count


def compute_phi(
    dim: int,
    row_fails: np.ndarray,
    row_passes: np.ndarray,
    fail_count: np.ndarray,
    pass_count: np.ndarray,
) -> np.ndarray:
    """Compute phi[r][c] = fail_rate[r][c] - pass_rate[r][c].

    fail_rate[r][c] = fail_count[r][c] / row_fails[r]   (fraction of row-r hash failures
                                                           where cell (r,c) was 1)
    pass_rate[r][c] = pass_count[r][c] / row_passes[r]   (fraction of row-r hash passes
                                                           where cell (r,c) was 1)

    phi > 0 means cell (r,c) being 1 correlates with hash failure (likely wrong).
    phi < 0 means cell (r,c) being 1 correlates with hash pass (likely correct).
    """
    phi = np.zeros((dim, dim), dtype=np.float64)
    for r in range(dim):
        if row_fails[r] > 0:
            fail_rate = fail_count[r].astype(np.float64) / row_fails[r]
        else:
            fail_rate = np.zeros(dim, dtype=np.float64)

        if row_passes[r] > 0:
            pass_rate = pass_count[r].astype(np.float64) / row_passes[r]
        else:
            pass_rate = np.zeros(dim, dtype=np.float64)

        phi[r] = fail_rate - pass_rate
    return phi


def main() -> None:
    """Entry point."""
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <profile.b40>", file=sys.stderr)
        sys.exit(1)

    path = Path(sys.argv[1])
    dim, row_fails, row_passes, fail_count, pass_count = load_b40_profile(path)
    phi = compute_phi(dim, row_fails, row_passes, fail_count, pass_count)

    stem = path.with_suffix("")

    # --- Full phi matrix CSV ---
    phi_path = Path(f"{stem}_phi.csv")
    with open(phi_path, "w") as fh:
        fh.write("row,col,phi,fail_count,pass_count\n")
        for r in range(dim):
            for c in range(dim):
                fh.write(f"{r},{c},{phi[r, c]:.6f},{fail_count[r, c]},{pass_count[r, c]}\n")
    print(f"Wrote {phi_path}")

    # --- Per-row ordering by descending phi ---
    order_path = Path(f"{stem}_row_order.csv")
    with open(order_path, "w") as fh:
        fh.write("row,rank,col,phi\n")
        for r in range(dim):
            order = np.argsort(-phi[r])  # descending
            for rank, c in enumerate(order):
                fh.write(f"{r},{rank},{c},{phi[r, c]:.6f}\n")
    print(f"Wrote {order_path}")

    # --- Summary statistics ---
    summary_path = Path(f"{stem}_summary.txt")
    total_events = int(row_fails.sum() + row_passes.sum())
    rows_with_data = int(np.sum((row_fails + row_passes) > 0))

    # Find rows with strongest phi signal
    row_phi_range = np.zeros(dim, dtype=np.float64)
    for r in range(dim):
        if (row_fails[r] + row_passes[r]) > 0:
            row_phi_range[r] = phi[r].max() - phi[r].min()

    top_rows = np.argsort(-row_phi_range)[:20]

    with open(summary_path, "w") as fh:
        fh.write("B.40 Phase 1 Summary\n")
        fh.write("=" * 40 + "\n\n")
        fh.write(f"Matrix dimension:     {dim}\n")
        fh.write(f"Total hash events:    {total_events}\n")
        fh.write(f"Total failures:       {int(row_fails.sum())}\n")
        fh.write(f"Total passes:         {int(row_passes.sum())}\n")
        fh.write(f"Rows with data:       {rows_with_data}/{dim}\n\n")

        fh.write(f"Global phi range:     [{phi.min():.6f}, {phi.max():.6f}]\n")
        fh.write(f"Global phi mean:      {phi[phi != 0].mean():.6f}\n")
        fh.write(f"Global phi std:       {phi[phi != 0].std():.6f}\n\n")

        fh.write("Top 20 rows by phi range (strongest signal):\n")
        fh.write(f"{'Row':>5} {'Fails':>8} {'Passes':>8} {'PhiMin':>10} {'PhiMax':>10} {'Range':>10}\n")
        for r in top_rows:
            fh.write(
                f"{r:5d} {row_fails[r]:8d} {row_passes[r]:8d} "
                f"{phi[r].min():10.6f} {phi[r].max():10.6f} {row_phi_range[r]:10.6f}\n"
            )

        # Decision criteria
        fh.write("\n\nB.40 Decision Criteria\n")
        fh.write("-" * 40 + "\n")
        strong_signal_rows = int(np.sum(row_phi_range > 0.1))
        fh.write(f"Rows with phi range > 0.1: {strong_signal_rows}\n")
        fh.write(f"Rows with phi range > 0.05: {int(np.sum(row_phi_range > 0.05))}\n")

        if strong_signal_rows > dim // 4:
            fh.write("\n=> OUTCOME H1/H2 likely: strong phi signal in >25% of rows.\n")
            fh.write("   Proceed to Phase 2 (within-row reordering by phi).\n")
        elif strong_signal_rows > 0:
            fh.write("\n=> OUTCOME H3 likely: weak phi signal.\n")
            fh.write("   Consider row-subset targeting or pivot to B.39a.\n")
        else:
            fh.write("\n=> OUTCOME H4 likely: no phi signal.\n")
            fh.write("   Pivot full attention to B.39a.\n")

    print(f"Wrote {summary_path}")

    # Console summary
    print(f"\n{dim}x{dim} matrix, {total_events} hash events "
          f"({int(row_fails.sum())} fail, {int(row_passes.sum())} pass)")
    print(f"Phi range: [{phi.min():.4f}, {phi.max():.4f}]")
    print(f"Strong-signal rows (range>0.1): {strong_signal_rows}/{dim}")


if __name__ == "__main__":
    main()
