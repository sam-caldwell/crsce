#!/usr/bin/env python3
"""
B.59 Depth Hill-Climber for S=127.

Direct depth optimization (solver-in-the-loop). Swaps cells between lines
within LTP sub-tables, evaluates by running the actual C++ solver, and
accepts swaps that improve depth.

Usage:
    python3 tools/b59_depth_climb.py --out tools/b59b_best.bin               # from FY seeds
    python3 tools/b59_depth_climb.py --init tools/b59c_table.bin --out ...   # from rLTP table
    python3 tools/b59_depth_climb.py --evals 200 --secs 15 --swaps-per 50
"""

import argparse
import json
import os
import pathlib
import signal
import struct
import subprocess
import sys
import tempfile
import time

import numpy as np

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
BUILD_DIR = REPO_ROOT / "build" / "arm64-release"
COMPRESS = BUILD_DIR / "compress"
DECOMPRESS = BUILD_DIR / "decompress"
SOURCE_VIDEO = REPO_ROOT / "docs" / "testData" / "useless-machine.mp4"

S = 127
N = S * S
NUM_SUB = 2
MAGIC = b"LTPB"
VERSION = 1

LCG_A = 6364136223846793005
LCG_C = 1442695040888963407
LCG_MOD = 1 << 64

# Default seeds (B.57b best)
SEEDS = [
    int.from_bytes(b"CRSCLTPZ", "big"),
    int.from_bytes(b"CRSCLTPR", "big"),
]


def build_fy_assignment(seed):
    """Replicate C++ Fisher-Yates LCG."""
    pool = list(range(N))
    state = seed
    for i in range(N - 1, 0, -1):
        state = (state * LCG_A + LCG_C) % LCG_MOD
        j = int(state % (i + 1))
        pool[i], pool[j] = pool[j], pool[i]
    a = np.zeros(N, dtype=np.uint16)
    for line in range(S):
        for slot in range(S):
            a[pool[line * S + slot]] = line
    return a


def build_csr(a):
    """Build CSR: csr[line*S + pos] = flat cell index."""
    csr = np.empty(N, dtype=np.uint32)
    counts = np.zeros(S, dtype=np.int32)
    for flat in range(N):
        line = int(a[flat])
        csr[line * S + counts[line]] = flat
        counts[line] += 1
    return csr


def load_ltpb(path):
    """Load LTPB file, return list of assignments."""
    data = pathlib.Path(path).read_bytes()
    magic, ver, s, nsub = struct.unpack_from("<4sIII", data, 0)
    if magic != MAGIC or s != S:
        sys.exit(f"Invalid LTPB: magic={magic!r}, S={s}")
    assignments = []
    off = 16
    for _ in range(nsub):
        a = np.frombuffer(data[off:off + N * 2], dtype="<u2").copy()
        assignments.append(a)
        off += N * 2
    return assignments


def write_ltpb(path, assignments):
    """Write LTPB file."""
    with open(path, "wb") as f:
        f.write(struct.pack("<4sIII", MAGIC, VERSION, S, len(assignments)))
        for a in assignments:
            f.write(a.astype("<u2").tobytes())


def measure_depth(ltpb_path, secs, tmp_dir):
    """Compress + decompress, return (total_depth, initial_forced, dfs_depth)."""
    crsce = tmp_dir / "test.crsce"
    out = tmp_dir / "test.bin"
    events = tmp_dir / "events.jsonl"

    for f in [crsce, out, events]:
        f.unlink(missing_ok=True)

    env = os.environ.copy()
    env.update({
        "CRSCE_LTP_TABLE_FILE": str(ltpb_path),
        "CRSCE_DISABLE_GPU": "1",
        "DISABLE_COMPRESS_DI": "1",
        "CRSCE_EVENTS_PATH": "/dev/null",
    })

    # Compress
    try:
        r = subprocess.run([str(COMPRESS), "-in", str(SOURCE_VIDEO), "-out", str(crsce)],
                          env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=90)
        if r.returncode != 0 or not crsce.exists():
            return 0, 0, 0
    except subprocess.TimeoutExpired:
        return 0, 0, 0

    # Decompress
    env["CRSCE_EVENTS_PATH"] = str(events)
    proc = subprocess.Popen([str(DECOMPRESS), "-in", str(crsce), "-out", str(out)],
                           env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(secs)
    try:
        proc.send_signal(signal.SIGINT)
        proc.wait(timeout=5)
    except (ProcessLookupError, subprocess.TimeoutExpired):
        proc.kill()
        proc.wait()

    # Parse
    forced = 0
    peak = 0
    if events.exists():
        for line in events.read_text(errors="replace").splitlines():
            try:
                e = json.loads(line)
                tags = e.get("tags", {})
                if e.get("name") == "solver_dfs_initial_propagation":
                    forced = int(tags.get("forced_cells", 0))
                d = tags.get("depth")
                if d is not None:
                    peak = max(peak, int(d))
                bp = tags.get("b31_peak_depth")
                if bp is not None:
                    peak = max(peak, int(bp))
            except (json.JSONDecodeError, ValueError):
                continue

    for f in [crsce, out, events]:
        f.unlink(missing_ok=True)

    return forced + peak, forced, peak


def do_swap(assignments, csrs, rng):
    """Perform one random cell-line swap within a random sub-table.
    Returns (sub, la, lb, pos_a, pos_b) for undo."""
    sub = int(rng.integers(0, NUM_SUB))
    a = assignments[sub]
    csr = csrs[sub]
    la, lb = rng.integers(0, S, size=2)
    while la == lb:
        lb = int(rng.integers(0, S))
    pos_a = int(rng.integers(0, S))
    pos_b = int(rng.integers(0, S))
    flat_a = int(csr[la * S + pos_a])
    flat_b = int(csr[lb * S + pos_b])

    # Swap assignments
    a[flat_a], a[flat_b] = a[flat_b], a[flat_a]
    csr[la * S + pos_a], csr[lb * S + pos_b] = flat_b, flat_a

    return sub, la, lb, pos_a, pos_b, flat_a, flat_b


def undo_swap(assignments, csrs, sub, la, lb, pos_a, pos_b, flat_a, flat_b):
    """Undo a swap."""
    a = assignments[sub]
    csr = csrs[sub]
    a[flat_a], a[flat_b] = a[flat_b], a[flat_a]
    csr[la * S + pos_a], csr[lb * S + pos_b] = flat_a, flat_b


def main():
    parser = argparse.ArgumentParser(description="B.59 depth hill-climber (S=127)")
    parser.add_argument("--init", default=None, help="Starting LTPB file (default: from seeds)")
    parser.add_argument("--out", required=True, help="Output best LTPB file")
    parser.add_argument("--evals", type=int, default=100, help="Number of depth evaluations")
    parser.add_argument("--secs", type=int, default=15, help="Seconds per evaluation")
    parser.add_argument("--swaps-per", type=int, default=50, help="Random swaps per evaluation batch")
    parser.add_argument("--seed", type=int, default=42, help="RNG seed")
    parser.add_argument("--log", default=None, help="JSON log file path")
    args = parser.parse_args()

    rng = np.random.default_rng(args.seed)

    # Load or build initial table
    if args.init:
        print(f"Loading initial table: {args.init}")
        assignments = load_ltpb(args.init)
    else:
        print(f"Building from seeds: {[hex(s) for s in SEEDS]}")
        assignments = [build_fy_assignment(s) for s in SEEDS]

    csrs = [build_csr(a) for a in assignments]

    # Validate
    for i, a in enumerate(assignments):
        counts = np.bincount(a, minlength=S)
        assert np.all(counts == S), f"Sub-table {i} not uniform"

    print(f"\nB.59 Depth Hill-Climber")
    print(f"  S={S}, N={N}, sub-tables={NUM_SUB}")
    print(f"  Evals: {args.evals}, secs/eval: {args.secs}, swaps/batch: {args.swaps_per}")
    print(f"  Output: {args.out}")

    # Initial measurement
    with tempfile.TemporaryDirectory(prefix="b59_climb_") as tmp:
        tmp_dir = pathlib.Path(tmp)
        ltpb_path = tmp_dir / "current.bin"

        write_ltpb(ltpb_path, assignments)
        print(f"\nMeasuring initial depth...", flush=True)
        total, forced, dfs = measure_depth(ltpb_path, args.secs, tmp_dir)
        print(f"  Initial: total={total} ({total / N * 100:.1f}%) [init={forced} dfs={dfs}]")

        best_depth = total
        best_assignments = [a.copy() for a in assignments]
        write_ltpb(pathlib.Path(args.out), best_assignments)

        log_entries = []
        total_swaps = 0
        improvements = 0

        for ev in range(1, args.evals + 1):
            # Save pre-batch state
            saved = [a.copy() for a in assignments]
            saved_csrs = [c.copy() for c in csrs]

            # Apply batch of random swaps
            swap_log = []
            for _ in range(args.swaps_per):
                info = do_swap(assignments, csrs, rng)
                swap_log.append(info)
                total_swaps += 1

            # Evaluate
            write_ltpb(ltpb_path, assignments)
            total, forced, dfs = measure_depth(ltpb_path, args.secs, tmp_dir)

            if total > best_depth:
                best_depth = total
                best_assignments = [a.copy() for a in assignments]
                write_ltpb(pathlib.Path(args.out), best_assignments)
                improvements += 1
                marker = " *** NEW BEST ***"
            else:
                # Revert
                assignments[:] = saved
                csrs[:] = saved_csrs
                marker = ""

            pct = total / N * 100
            print(f"  [{ev:3d}/{args.evals}] depth={total} ({pct:.1f}%) "
                  f"[init={forced} dfs={dfs}] best={best_depth}{marker}")

            entry = {"eval": ev, "depth": total, "forced": forced, "dfs": dfs,
                     "best": best_depth, "swaps": total_swaps}
            log_entries.append(entry)

        # Summary
        print(f"\n{'='*60}")
        print(f"B.59 Depth Climb Complete")
        print(f"  Evaluations: {args.evals}")
        print(f"  Total swaps: {total_swaps}")
        print(f"  Improvements: {improvements}")
        print(f"  Best depth: {best_depth} ({best_depth / N * 100:.1f}%)")
        print(f"  Output: {args.out}")

        if args.log:
            log_path = pathlib.Path(args.log)
            log_path.write_text(json.dumps({"entries": log_entries}, indent=2))
            print(f"  Log: {args.log}")


if __name__ == "__main__":
    main()
