#!/usr/bin/env python3
"""
B.26c Joint 2-Seed Search — CRSCE LTP sub-table joint optimizer.

Exhaustive search over all 36×36=1,296 combinations of CRSCE_LTP_SEED_1 and
CRSCE_LTP_SEED_2, with CRSCE_LTP_SEED_3 and CRSCE_LTP_SEED_4 fixed to their
current best values (CRSCLTP3, CRSCLTP4).

Motivation: The greedy sequential search (B.22) may have found a local optimum.
The phase-1 winner (CRSCLTP0) was chosen with seeds 2/3/4 at defaults; the
phase-2 winner (CRSCLTPG) was chosen with seed1=CRSCLTP0.  A jointly optimal
pair might differ from the sequentially chosen pair.

Strategy:
  - Seeds 3, 4 fixed to CRSCLTP3, CRSCLTP4 (all 36 candidates tied at 89,331).
  - Seeds 1 and 2 swept jointly over all 36×36=1,296 pairs.
  - Each candidate: re-compress source video with new seeds (avoids mismatched
    LTP sums), then run decompressor for `secs` seconds, record peak depth.
  - Results written progressively to JSON after each pair.

Usage:
    python3 tools/b26c_joint_seed_search.py [--secs N] [--preset PRESET] [--out FILE]
    python3 tools/b26c_joint_seed_search.py --secs 20 --preset llvm-release

Options:
    --secs N        Seconds per candidate test (default: 20)
    --preset P      CMake preset name (default: llvm-release)
    --out FILE      Write progressive results JSON to FILE
    --resume FILE   Resume from a prior partial results file (skip already-tested pairs)

Estimated runtime at 20s/test + 3s compress:
    1,296 pairs × 23s = ~8.3 hours (sequential)
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
# Defaults
# ---------------------------------------------------------------------------
REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
DEFAULT_PRESET = "llvm-release"
DEFAULT_SECS = 20

# Fixed seeds (phases 3+4 invariant — all 36 candidates tie at 89,331)
_PREFIX = b"CRSCLTP"
SEED3_FIXED = int.from_bytes(_PREFIX + bytes([0x33]), "big")  # CRSCLTP3
SEED4_FIXED = int.from_bytes(_PREFIX + bytes([0x34]), "big")  # CRSCLTP4

# Current baseline pair (greedy sequential result)
SEED1_BASELINE = int.from_bytes(_PREFIX + bytes([0x30]), "big")  # CRSCLTP0
SEED2_BASELINE = int.from_bytes(_PREFIX + bytes([0x47]), "big")  # CRSCLTPG
BASELINE_DEPTH = 89_331

# Candidate suffix bytes: '0'-'9' + 'A'-'Z' (36 options)
CANDIDATE_SUFFIXES = list(range(0x30, 0x3A)) + list(range(0x41, 0x5B))  # 36 total

# All 36 candidate seeds
def make_seed(suffix_byte: int) -> int:
    return int.from_bytes(_PREFIX + bytes([suffix_byte]), "big")

CANDIDATE_SEEDS = [make_seed(b) for b in CANDIDATE_SUFFIXES]

def seed_to_label(seed: int) -> str:
    """Convert a seed integer to its CRSCLTP? label."""
    b = seed & 0xFF
    ch = chr(b) if 0x20 <= b < 0x7F else f"\\x{b:02X}"
    return f"CRSCLTP{ch}"


# ---------------------------------------------------------------------------
# Path helpers
# ---------------------------------------------------------------------------
def find_binary(preset: str, name: str) -> pathlib.Path:
    for candidate in [
        REPO_ROOT / "build" / preset / name,
        REPO_ROOT / "build" / preset / "bin" / name,
    ]:
        if candidate.exists():
            return candidate
    sys.exit(f"Binary not found in build/{preset}/{name}\nRun: make build PRESET={preset}")


SOURCE_VIDEO = REPO_ROOT / "docs" / "testData" / "useless-machine.mp4"


def find_source_video() -> pathlib.Path:
    if not SOURCE_VIDEO.exists():
        sys.exit(f"Source video not found: {SOURCE_VIDEO}")
    return SOURCE_VIDEO


# ---------------------------------------------------------------------------
# Single candidate evaluation (same methodology as b22_seed_search.py)
# ---------------------------------------------------------------------------
def run_candidate(
    compress_bin: pathlib.Path,
    decompress_bin: pathlib.Path,
    source_video: pathlib.Path,
    seeds: list[int],
    secs: int,
    tmp_dir: pathlib.Path,
    run_id: str,
) -> int:
    """
    Re-compress source_video with the given seeds, then decompress for `secs` seconds.
    Returns peak depth observed in telemetry, or 0 on failure.
    """
    events_path = tmp_dir / f"events_{run_id}.jsonl"
    crsce_path  = tmp_dir / f"compressed_{run_id}.crsce"
    out_path    = tmp_dir / f"out_{run_id}.bin"

    seed_env: dict[str, str] = {}
    for i, seed in enumerate(seeds):
        seed_env[f"CRSCE_LTP_SEED_{i + 1}"] = str(seed)

    # Step 1: compress (fast, DI=0)
    compress_env = os.environ.copy()
    compress_env.update(seed_env)
    compress_env["DISABLE_COMPRESS_DI"]  = "1"
    compress_env["CRSCE_DISABLE_GPU"]    = "1"
    compress_env["CRSCE_WATCHDOG_SECS"]  = "60"
    try:
        cx_proc = subprocess.run(
            [str(compress_bin), "-in", str(source_video), "-out", str(crsce_path)],
            env=compress_env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            timeout=90,
        )
    except subprocess.TimeoutExpired:
        return 0
    if cx_proc.returncode != 0 or not crsce_path.exists():
        return 0

    # Step 2: decompress for `secs` seconds
    env = os.environ.copy()
    env.update(seed_env)
    env["CRSCE_EVENTS_PATH"]   = str(events_path)
    env["CRSCE_METRICS_FLUSH"] = "1"
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

    # Parse peak depth
    peak_depth = 0
    if events_path.exists():
        for line in events_path.read_text(errors="replace").splitlines():
            try:
                entry = json.loads(line)
                tags = entry.get("tags", {})
                d = tags.get("depth")
                if d is not None:
                    peak_depth = max(peak_depth, int(d))
            except (json.JSONDecodeError, ValueError):
                continue
        events_path.unlink(missing_ok=True)

    out_path.unlink(missing_ok=True)
    crsce_path.unlink(missing_ok=True)
    return peak_depth


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main() -> None:
    parser = argparse.ArgumentParser(description="B.26c joint 2-seed search for LTP seeds 1 and 2")
    parser.add_argument("--secs",   type=int, default=DEFAULT_SECS)
    parser.add_argument("--preset", default=DEFAULT_PRESET)
    parser.add_argument("--out",    default=str(REPO_ROOT / "tools" / "b26c_results.json"))
    parser.add_argument("--resume", default=None, help="Resume from partial results JSON")
    args = parser.parse_args()

    compress_bin   = find_binary(args.preset, "compress")
    decompress_bin = find_binary(args.preset, "decompress")
    source_video   = find_source_video()

    total_pairs = len(CANDIDATE_SEEDS) ** 2
    secs_per = args.secs + 5  # +~5s for compress
    est_hours = (total_pairs * secs_per) / 3600

    print("B.26c Joint 2-Seed Search")
    print(f"  compress:      {compress_bin}")
    print(f"  decompress:    {decompress_bin}")
    print(f"  source:        {source_video}")
    print(f"  secs/test:     {args.secs}  (+~5s compress)")
    print(f"  pairs:         {total_pairs}  (36 × 36)")
    print(f"  fixed seeds:   seed3=CRSCLTP3, seed4=CRSCLTP4")
    print(f"  baseline:      seed1=CRSCLTP0, seed2=CRSCLTPG, depth={BASELINE_DEPTH}")
    print(f"  est. runtime:  ~{est_hours:.1f} hours")
    print(f"  results:       {args.out}")

    # Load prior results if resuming
    completed: dict[str, int] = {}  # key="seed1hex,seed2hex" → depth
    all_results: list[dict] = []
    if args.resume:
        resume_path = pathlib.Path(args.resume)
        if resume_path.exists():
            prior = json.loads(resume_path.read_text())
            for r in prior.get("results", []):
                key = f"{r['seed1']},{r['seed2']}"
                completed[key] = r["depth"]
                all_results.append(r)
            print(f"  resumed:       {len(completed)} pairs already done")

    out_path = pathlib.Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    best_depth = max((r["depth"] for r in all_results), default=0)
    best_pair  = next((r for r in all_results if r["depth"] == best_depth), None)

    done_count = len(completed)
    remaining  = total_pairs - done_count

    print(f"\n{'='*60}")
    print(f"Starting joint sweep: {remaining} pairs remaining")
    print(f"{'='*60}\n")

    with tempfile.TemporaryDirectory(prefix="b26c_search_") as tmp_str:
        tmp_dir = pathlib.Path(tmp_str)

        pair_idx = 0
        for s1 in CANDIDATE_SEEDS:
            for s2 in CANDIDATE_SEEDS:
                pair_idx += 1
                key = f"{hex(s1)},{hex(s2)}"
                if key in completed:
                    continue

                label1 = seed_to_label(s1)
                label2 = seed_to_label(s2)
                run_id = f"p{pair_idx}"

                seeds = [s1, s2, SEED3_FIXED, SEED4_FIXED]
                elapsed_pairs = pair_idx - (total_pairs - remaining)
                elapsed_pairs = max(elapsed_pairs, 1)

                print(
                    f"  [{pair_idx:4d}/{total_pairs}] "
                    f"seed1={label1} seed2={label2} ... ",
                    end="",
                    flush=True,
                )

                depth = run_candidate(
                    compress_bin, decompress_bin, source_video,
                    seeds, args.secs, tmp_dir, run_id,
                )

                is_new_best = depth > best_depth
                marker = " *** NEW BEST ***" if is_new_best else ""
                beats_baseline = depth > BASELINE_DEPTH
                baseline_marker = " [beats baseline]" if beats_baseline else ""
                print(f"depth={depth}{baseline_marker}{marker}")

                if is_new_best:
                    best_depth = depth
                    best_pair = {"seed1": hex(s1), "seed2": hex(s2), "depth": depth}

                result = {
                    "pair_idx": pair_idx,
                    "seed1": hex(s1),
                    "seed2": hex(s2),
                    "seed1_label": label1,
                    "seed2_label": label2,
                    "seed3": hex(SEED3_FIXED),
                    "seed4": hex(SEED4_FIXED),
                    "depth": depth,
                    "beats_baseline": beats_baseline,
                }
                all_results.append(result)
                completed[key] = depth

                # Write progressively after each result
                summary = {
                    "baseline": {"seed1": hex(SEED1_BASELINE), "seed2": hex(SEED2_BASELINE),
                                 "depth": BASELINE_DEPTH},
                    "best_found": best_pair,
                    "progress": {"done": len(completed), "total": total_pairs},
                    "results": all_results,
                }
                out_path.write_text(json.dumps(summary, indent=2))

    # Final summary
    print(f"\n{'='*60}")
    print(f"B.26c Joint 2-Seed Search Complete")
    print(f"  Baseline:   seed1=CRSCLTP0, seed2=CRSCLTPG, depth={BASELINE_DEPTH}")
    if best_pair and best_pair.get("depth", 0) > BASELINE_DEPTH:
        print(f"  ** NEW BEST: seed1={best_pair['seed1']} seed2={best_pair['seed2']} depth={best_pair['depth']} **")
        print(f"  Improvement: +{best_pair['depth'] - BASELINE_DEPTH}")
    elif best_pair:
        print(f"  Best found: seed1={best_pair['seed1']} seed2={best_pair['seed2']} depth={best_pair['depth']}")
        print(f"  (Did not beat baseline of {BASELINE_DEPTH})")
    print(f"  Full results: {out_path}")

    # Sort and print top-10
    all_results.sort(key=lambda r: r["depth"], reverse=True)
    print(f"\nTop-10 pairs:")
    for i, r in enumerate(all_results[:10]):
        marker = " [baseline]" if r["seed1"] == hex(SEED1_BASELINE) and r["seed2"] == hex(SEED2_BASELINE) else ""
        print(f"  {i+1:2d}. seed1={r['seed1_label']} seed2={r['seed2_label']} depth={r['depth']}{marker}")


if __name__ == "__main__":
    main()
