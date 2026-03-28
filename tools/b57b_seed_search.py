#!/usr/bin/env python3
"""
B.57b: Joint 2-Seed Search at S=127 — Determine if B.57a regression is seed-dependent.

B.57a result: 3,651/16,129 = 22.6% depth with B.26c-winner seeds (CRSCLTPV+CRSCLTPP).
Those seeds were optimized for S=511; at S=127 the Fisher-Yates shuffle produces entirely
different partitions, so the seeds carry no optimization benefit.

Strategy:
  - S=127, 2 yLTPs (B.57 configuration).
  - Sweep CRSCE_LTP_SEED_1 × CRSCE_LTP_SEED_2 over top-12 candidates (144 pairs, ~36 min)
    or full 36×36 grid (1,296 pairs, ~5.4h) with --full.
  - Each candidate: compress MP4 with seed pair → decompress for N seconds → parse peak depth.
  - Progressive JSON output with resume support.

Usage:
    python3 tools/b57b_seed_search.py                          # top-12 subset (144 pairs)
    python3 tools/b57b_seed_search.py --full                   # all 36x36 (1,296 pairs)
    python3 tools/b57b_seed_search.py --secs 20 --preset arm64-release
    python3 tools/b57b_seed_search.py --resume tools/b57b_results.json
"""

import argparse
import json
import os
import pathlib
import signal
import subprocess
import sys
import tempfile
import time

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------
REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
DEFAULT_PRESET = "arm64-release"
DEFAULT_SECS = 15

# S=127 geometry
S = 127
N_CELLS = S * S  # 16,129

# B.57a baseline (unoptimized seeds from B.26c at S=511)
_PREFIX = b"CRSCLTP"
SEED1_BASELINE = int.from_bytes(_PREFIX + bytes([ord("V")]), "big")  # CRSCLTPV
SEED2_BASELINE = int.from_bytes(_PREFIX + bytes([ord("P")]), "big")  # CRSCLTPP
BASELINE_DEPTH = 3651  # B.57a: 3,600 initial + 51 DFS
BASELINE_INITIAL_FORCED = 3600
BASELINE_DEPTH_PCT = round(BASELINE_DEPTH / N_CELLS * 100, 1)

# Candidate suffix bytes: '0'-'9' + 'A'-'Z' (36 options)
CANDIDATE_SUFFIXES = list(range(0x30, 0x3A)) + list(range(0x41, 0x5B))

# Top-12 suffixes (most frequent in B.26c top-20 pairs at S=511)
TOP_12_SUFFIXES = [ord(c) for c in ["V", "P", "Z", "X", "B", "H", "R", "J", "G", "K", "9", "A"]]


def make_seed(suffix_byte: int) -> int:
    return int.from_bytes(_PREFIX + bytes([suffix_byte]), "big")


ALL_SEEDS = [make_seed(b) for b in CANDIDATE_SUFFIXES]
TOP_12_SEEDS = [make_seed(b) for b in TOP_12_SUFFIXES]


def seed_to_label(seed: int) -> str:
    b = seed & 0xFF
    ch = chr(b) if 0x20 <= b < 0x7F else f"\\x{b:02X}"
    return f"CRSCLTP{ch}"


# ---------------------------------------------------------------------------
# Path helpers
# ---------------------------------------------------------------------------
SOURCE_VIDEO = REPO_ROOT / "docs" / "testData" / "useless-machine.mp4"


def find_binary(preset: str, name: str) -> pathlib.Path:
    for candidate in [
        REPO_ROOT / "build" / preset / name,
        REPO_ROOT / "build" / preset / "bin" / name,
    ]:
        if candidate.exists():
            return candidate
    sys.exit(f"Binary not found in build/{preset}/{name}\nRun: make build PRESET={preset}")


# ---------------------------------------------------------------------------
# Single candidate evaluation
# ---------------------------------------------------------------------------
def run_candidate(
    compress_bin: pathlib.Path,
    decompress_bin: pathlib.Path,
    source_video: pathlib.Path,
    seed1: int,
    seed2: int,
    secs: int,
    tmp_dir: pathlib.Path,
    run_id: str,
) -> dict:
    """
    Compress with seed pair, decompress for `secs` seconds, parse peak depth.
    Returns {"depth": int, "initial_forced": int, "depth_pct": float}.
    """
    events_path = tmp_dir / f"events_{run_id}.jsonl"
    crsce_path = tmp_dir / f"compressed_{run_id}.crsce"
    out_path = tmp_dir / f"out_{run_id}.bin"

    seed_env = {
        "CRSCE_LTP_SEED_1": str(seed1),
        "CRSCE_LTP_SEED_2": str(seed2),
    }

    # Step 1: compress (DI=0, fast)
    compress_env = os.environ.copy()
    compress_env.update(seed_env)
    compress_env["DISABLE_COMPRESS_DI"] = "1"
    compress_env["CRSCE_DISABLE_GPU"] = "1"
    compress_env["CRSCE_EVENTS_PATH"] = "/dev/null"
    try:
        cx = subprocess.run(
            [str(compress_bin), "-in", str(source_video), "-out", str(crsce_path)],
            env=compress_env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            timeout=90,
        )
    except subprocess.TimeoutExpired:
        return {"depth": 0, "initial_forced": 0, "depth_pct": 0.0}
    if cx.returncode != 0 or not crsce_path.exists():
        return {"depth": 0, "initial_forced": 0, "depth_pct": 0.0}

    # Step 2: decompress for `secs` seconds
    env = os.environ.copy()
    env.update(seed_env)
    env["CRSCE_EVENTS_PATH"] = str(events_path)
    env["CRSCE_DISABLE_GPU"] = "1"
    env["CRSCE_WATCHDOG_SECS"] = str(secs + 15)

    proc = subprocess.Popen(
        [str(decompress_bin), "-in", str(crsce_path), "-out", str(out_path)],
        env=env,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )

    time.sleep(secs)
    try:
        proc.send_signal(signal.SIGINT)
        proc.wait(timeout=5)
    except (ProcessLookupError, subprocess.TimeoutExpired):
        proc.kill()
        proc.wait()

    # Parse peak depth and initial forced count
    peak_depth = 0
    initial_forced = 0
    b31_peak = 0
    if events_path.exists():
        for line in events_path.read_text(errors="replace").splitlines():
            try:
                entry = json.loads(line)
                tags = entry.get("tags", {})
                name = entry.get("name", "")

                # Peak depth from DFS iterations telemetry
                d = tags.get("depth")
                if d is not None:
                    peak_depth = max(peak_depth, int(d))

                bp = tags.get("b31_peak_depth")
                if bp is not None:
                    b31_peak = max(b31_peak, int(bp))

                # Initial forced cells from propagation event
                if name == "solver_dfs_initial_propagation":
                    fc = tags.get("forced_cells")
                    if fc is not None:
                        initial_forced = int(fc)

            except (json.JSONDecodeError, ValueError):
                continue
        events_path.unlink(missing_ok=True)

    out_path.unlink(missing_ok=True)
    crsce_path.unlink(missing_ok=True)

    # Total depth = initial_forced + max(dfs depth, b31_peak_depth)
    dfs_depth = max(peak_depth, b31_peak)
    total_depth = initial_forced + dfs_depth
    depth_pct = round(total_depth / N_CELLS * 100, 1)

    return {"depth": total_depth, "initial_forced": initial_forced,
            "dfs_depth": dfs_depth, "depth_pct": depth_pct}


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main() -> None:
    parser = argparse.ArgumentParser(
        description="B.57b: Joint 2-seed search at S=127 (2 yLTPs)")
    parser.add_argument("--secs", type=int, default=DEFAULT_SECS)
    parser.add_argument("--preset", default=DEFAULT_PRESET)
    parser.add_argument("--out", default=str(REPO_ROOT / "tools" / "b57b_results.json"))
    parser.add_argument("--resume", default=None, help="Resume from partial results JSON")
    parser.add_argument("--full", action="store_true", help="Full 36x36 grid (default: top-12)")
    args = parser.parse_args()

    compress_bin = find_binary(args.preset, "compress")
    decompress_bin = find_binary(args.preset, "decompress")
    if not SOURCE_VIDEO.exists():
        sys.exit(f"Source video not found: {SOURCE_VIDEO}")

    seeds = ALL_SEEDS if args.full else TOP_12_SEEDS
    seed_count = len(seeds)
    total_pairs = seed_count ** 2
    secs_per = args.secs + 5
    est_min = (total_pairs * secs_per) / 60

    print("B.57b Joint 2-Seed Search (S=127, 2 yLTPs)")
    print(f"  compress:      {compress_bin}")
    print(f"  decompress:    {decompress_bin}")
    print(f"  source:        {SOURCE_VIDEO}")
    print(f"  S:             {S}")
    print(f"  N_cells:       {N_CELLS}")
    print(f"  secs/test:     {args.secs}  (+~5s compress)")
    print(f"  seed grid:     {seed_count} x {seed_count} = {total_pairs} pairs")
    print(f"  baseline:      CRSCLTPV+CRSCLTPP, depth={BASELINE_DEPTH} ({BASELINE_DEPTH_PCT}%)")
    print(f"  target:        {int(N_CELLS * 0.30)} cells (30% = H3 threshold)")
    print(f"  est. runtime:  ~{est_min:.0f} min")
    print(f"  results:       {args.out}")

    # Load prior results if resuming
    completed: dict[str, dict] = {}
    all_results: list[dict] = []
    if args.resume:
        resume_path = pathlib.Path(args.resume)
        if resume_path.exists():
            prior = json.loads(resume_path.read_text())
            for r in prior.get("results", []):
                key = f"{r['seed1']},{r['seed2']}"
                completed[key] = r
                all_results.append(r)
            print(f"  resumed:       {len(completed)} pairs already done")

    out_path = pathlib.Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    best_depth = max((r["depth"] for r in all_results), default=0)
    best_pair = next((r for r in all_results if r["depth"] == best_depth), None)

    remaining = total_pairs - len(completed)
    print(f"\n{'='*72}")
    print(f"Starting sweep: {remaining} pairs remaining")
    print(f"{'='*72}\n")

    with tempfile.TemporaryDirectory(prefix="b57b_search_") as tmp_str:
        tmp_dir = pathlib.Path(tmp_str)
        pair_idx = 0

        for s1 in seeds:
            for s2 in seeds:
                pair_idx += 1
                key = f"{hex(s1)},{hex(s2)}"
                if key in completed:
                    continue

                label1 = seed_to_label(s1)
                label2 = seed_to_label(s2)
                run_id = f"p{pair_idx}"

                print(
                    f"  [{pair_idx:4d}/{total_pairs}] "
                    f"{label1}+{label2} ... ",
                    end="", flush=True,
                )

                result = run_candidate(
                    compress_bin, decompress_bin, SOURCE_VIDEO,
                    s1, s2, args.secs, tmp_dir, run_id,
                )

                is_new_best = result["depth"] > best_depth
                marker = " *** NEW BEST ***" if is_new_best else ""
                print(
                    f"depth={result['depth']} ({result['depth_pct']}%) "
                    f"[init={result['initial_forced']} dfs={result['dfs_depth']}]"
                    f"{marker}"
                )

                if is_new_best:
                    best_depth = result["depth"]
                    best_pair = {
                        "seed1": hex(s1), "seed2": hex(s2),
                        "seed1_label": label1, "seed2_label": label2,
                        **result,
                    }

                entry = {
                    "pair_idx": pair_idx,
                    "seed1": hex(s1), "seed2": hex(s2),
                    "seed1_label": label1, "seed2_label": label2,
                    **result,
                }
                all_results.append(entry)
                completed[key] = entry

                # Progressive write
                summary = {
                    "experiment": "B.57b",
                    "config": {
                        "S": S, "N_cells": N_CELLS, "ltp_count": 2,
                        "secs_per_eval": args.secs, "seed_count": seed_count,
                        "preset": args.preset,
                    },
                    "baseline": {
                        "seed1": hex(SEED1_BASELINE), "seed2": hex(SEED2_BASELINE),
                        "seed1_label": "CRSCLTPV", "seed2_label": "CRSCLTPP",
                        "depth": BASELINE_DEPTH, "depth_pct": BASELINE_DEPTH_PCT,
                        "initial_forced": BASELINE_INITIAL_FORCED,
                    },
                    "best_found": best_pair,
                    "progress": {"done": len(completed), "total": total_pairs},
                    "results": all_results,
                }
                out_path.write_text(json.dumps(summary, indent=2))

    # Final summary
    print(f"\n{'='*72}")
    print(f"B.57b Complete — {len(completed)} pairs evaluated")
    print(f"  Baseline:  CRSCLTPV+CRSCLTPP = {BASELINE_DEPTH} ({BASELINE_DEPTH_PCT}%)")
    if best_pair:
        bp_d = best_pair["depth"]
        bp_p = best_pair["depth_pct"]
        delta = bp_d - BASELINE_DEPTH
        print(f"  Best:      {best_pair.get('seed1_label','?')}+{best_pair.get('seed2_label','?')} "
              f"= {bp_d} ({bp_p}%), delta={delta:+d}")
        if bp_d >= int(N_CELLS * 0.30):
            print(f"  RESULT:    H3+ (≥30%) — seed optimization matters at S=127")
        else:
            print(f"  RESULT:    H4 (<30%) — regression is structural, not seed-dependent")
    print(f"  Results:   {out_path}")

    # Top-10
    all_results.sort(key=lambda r: r["depth"], reverse=True)
    print(f"\nTop-10 pairs:")
    for i, r in enumerate(all_results[:10]):
        bl = " [baseline]" if (r["seed1"] == hex(SEED1_BASELINE)
                               and r["seed2"] == hex(SEED2_BASELINE)) else ""
        print(f"  {i+1:2d}. {r['seed1_label']}+{r['seed2_label']} "
              f"depth={r['depth']} ({r['depth_pct']}%) "
              f"[init={r['initial_forced']} dfs={r['dfs_depth']}]{bl}")


if __name__ == "__main__":
    main()
