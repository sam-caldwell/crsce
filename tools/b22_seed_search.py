#!/usr/bin/env python3
"""
B.22 Partition Seed Search — CRSCE LTP sub-table seed optimizer.

Searches for four LCG seed values (CRSCE_LTP_SEED_1..4) that maximize
DFS depth in the CRSCE decompressor.  Runs the 'decompress' binary with
CRSCE_LTP_SEED_N env vars for a fixed window, reads peak depth from the
events.jsonl telemetry log, and outputs ranked results.

Usage:
    python3 tools/b22_seed_search.py [--secs N] [--preset PRESET] [--phase 1|2|3|4|all]

Options:
    --secs N        Seconds per candidate test (default: 45)
    --preset P      CMake preset name (default: llvm-release)
    --phase P       Which phase to run: 1, 2, 3, 4, or all (default: all)
    --out FILE      Write results JSON to FILE (default: tools/b22_results.json)

Environment:
    Seeds are 64-bit unsigned integers supplied as decimal or 0x-prefixed hex.
    Default seeds encode ASCII strings CRSCLTP1..CRSCLTP4:
        CRSCLTP1 = 0x4352534C54503031
        CRSCLTP2 = 0x4352534C54503032
        CRSCLTP3 = 0x4352534C54503033
        CRSCLTP4 = 0x4352534C54503034

Strategy (B.22.3 independent sub-table search):
    Phase 1: Fix seeds 2,3,4 to defaults; vary seed 1 over candidates.
    Phase 2: Fix seeds 3,4 to defaults, seed 1 to best from Phase 1; vary seed 2.
    Phase 3: Fix seed 4 to default, seeds 1,2 to phase bests; vary seed 3.
    Phase 4: Fix seeds 1,2,3 to phase bests; vary seed 4.
    Each phase: 36 candidates (last byte 0x30..0x39 + 0x41..0x5A = digits + A-Z).
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
DEFAULT_SECS = 45

# Default seeds: B.26c joint search winners (see tools/b26c_joint_seed_search.py).
# B.26c exhaustive 36×36 search found CRSCLTPV+CRSCLTPP = 91,090 depth (+1.97% over B.22 result).
# B.22 greedy result (CRSCLTP0 + CRSCLTPG = 89,331) was a local optimum.
# Seeds 3/4: invariant — all 36 candidates tied at 89,331 in phases 3+4.
_PREFIX = b"CRSCLTP"  # 7 bytes → 0x43 52 53 43 4C 54 50
DEFAULT_SEEDS = [
    int.from_bytes(_PREFIX + bytes([0x56]), "big"),  # CRSCLTPV (B.26c joint winner)
    int.from_bytes(_PREFIX + bytes([0x50]), "big"),  # CRSCLTPP (B.26c joint winner)
    int.from_bytes(_PREFIX + bytes([0x33]), "big"),  # CRSCLTP3 (invariant)
    int.from_bytes(_PREFIX + bytes([0x34]), "big"),  # CRSCLTP4 (invariant)
]

# Candidate suffix bytes: '0'-'9' + 'A'-'Z' (36 options)
CANDIDATE_SUFFIXES = list(range(0x30, 0x3A)) + list(range(0x41, 0x5B))  # 36 total


def make_seed(suffix_byte: int) -> int:
    """Make a candidate seed from the shared prefix + one suffix byte."""
    return int.from_bytes(_PREFIX + bytes([suffix_byte]), "big")


CANDIDATE_SEEDS = [make_seed(b) for b in CANDIDATE_SUFFIXES]


# ---------------------------------------------------------------------------
# Path helpers
# ---------------------------------------------------------------------------
def find_binary(preset: str, name: str) -> pathlib.Path:
    """Find a compiled binary under build/<preset>/<name> or build/<preset>/bin/<name>."""
    for candidate in [
        REPO_ROOT / "build" / preset / name,
        REPO_ROOT / "build" / preset / "bin" / name,
    ]:
        if candidate.exists():
            return candidate
    sys.exit(f"Binary not found in build/{preset}/{name}\nRun: make build PRESET={preset}")


SOURCE_VIDEO = REPO_ROOT / "docs" / "testData" / "useless-machine.mp4"


def find_source_video() -> pathlib.Path:
    """Return path to the test video (must exist)."""
    if not SOURCE_VIDEO.exists():
        sys.exit(f"Source video not found: {SOURCE_VIDEO}")
    return SOURCE_VIDEO


# ---------------------------------------------------------------------------
# Single candidate evaluation
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

    Re-compressing ensures the stored LTP sums match the seeds used during decompress.
    Without this step, the decompressor would use a partition defined by the new seeds
    but the stored sums would come from a different partition, inflating depth artificially.

    Returns the peak depth observed in the telemetry log, or 0 on failure.
    """
    events_path = tmp_dir / f"events_{run_id}.jsonl"
    crsce_path  = tmp_dir / f"compressed_{run_id}.crsce"
    out_path    = tmp_dir / f"out_{run_id}.bin"

    seed_env = {}
    for i, seed in enumerate(seeds):
        seed_env[f"CRSCE_LTP_SEED_{i + 1}"] = str(seed)

    # --- Step 1: compress (fast, DI=0) ---
    compress_env = os.environ.copy()
    compress_env.update(seed_env)
    compress_env["DISABLE_COMPRESS_DI"] = "1"
    compress_env["CRSCE_DISABLE_GPU"]   = "1"
    compress_env["CRSCE_WATCHDOG_SECS"] = "60"
    cx_proc = subprocess.run(
        [str(compress_bin), "-in", str(source_video), "-out", str(crsce_path)],
        env=compress_env,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        timeout=60,
    )
    if cx_proc.returncode != 0 or not crsce_path.exists():
        return 0

    # --- Step 2: decompress for `secs` seconds, measure depth ---
    env = os.environ.copy()
    env.update(seed_env)
    env["CRSCE_EVENTS_PATH"]    = str(events_path)
    env["CRSCE_METRICS_FLUSH"]  = "1"
    env["CRSCE_WATCHDOG_SECS"]  = str(secs + 10)

    proc = subprocess.Popen(
        [str(decompress_bin), "-in", str(crsce_path), "-out", str(out_path)],
        env=env,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )

    # Let it run for `secs` seconds, then SIGINT for clean flush
    time.sleep(secs)
    try:
        proc.send_signal(signal.SIGINT)
        proc.wait(timeout=5)
    except (ProcessLookupError, subprocess.TimeoutExpired):
        proc.kill()
        proc.wait()

    # Parse peak depth from events.jsonl
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
# Phase runner
# ---------------------------------------------------------------------------
def run_phase(
    phase: int,
    fixed_seeds: list[int],
    compress_bin: pathlib.Path,
    decompress_bin: pathlib.Path,
    source_video: pathlib.Path,
    secs: int,
    tmp_dir: pathlib.Path,
    all_results: list[dict],
) -> int:
    """
    Run one phase of the independent search.

    `fixed_seeds` is the 4-seed list with the phase's sub-table seed set to
    the default (it will be overridden by each candidate).  Returns the best
    seed found for this sub-table.

    Each candidate re-compresses the source video so that the stored LTP sums
    correctly match the candidate's partition (avoids spurious depth inflation
    from mismatched sums).
    """
    sub_idx = phase - 1  # 0-based
    print(f"\n{'='*60}")
    print(f"Phase {phase}: optimizing CRSCE_LTP_SEED_{phase}")
    print(f"  Fixed seeds: {[hex(s) for i, s in enumerate(fixed_seeds) if i != sub_idx]}")
    print(f"  Candidates: {len(CANDIDATE_SEEDS)}")
    print(f"  Seconds per test: {secs} (+ ~2s compress per candidate)")
    print(f"{'='*60}")

    phase_results = []
    for idx, candidate in enumerate(CANDIDATE_SEEDS):
        seeds = list(fixed_seeds)
        seeds[sub_idx] = candidate
        label = f"phase{phase}_cand{idx:02d}"

        suffix_byte = CANDIDATE_SUFFIXES[idx]
        suffix_char = chr(suffix_byte) if 0x20 <= suffix_byte < 0x7F else f"\\x{suffix_byte:02X}"
        seed_str = f"CRSCLTP{suffix_char}={hex(candidate)}"

        print(f"  [{idx+1:2d}/{len(CANDIDATE_SEEDS)}] seed_{phase}={seed_str} ... ", end="", flush=True)
        depth = run_candidate(compress_bin, decompress_bin, source_video, seeds, secs, tmp_dir, label)
        phase_results.append({"phase": phase, "sub_idx": sub_idx, "candidate": hex(candidate),
                               "seeds": [hex(s) for s in seeds], "depth": depth})
        all_results.append(phase_results[-1])
        print(f"depth={depth}")

    phase_results.sort(key=lambda r: r["depth"], reverse=True)
    best = phase_results[0]
    print(f"\nPhase {phase} best: seed={best['candidate']}  depth={best['depth']}")
    return int(best["candidate"], 16)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main() -> None:
    parser = argparse.ArgumentParser(description="B.22 partition seed search")
    parser.add_argument("--secs",   type=int, default=DEFAULT_SECS)
    parser.add_argument("--preset", default=DEFAULT_PRESET)
    parser.add_argument("--phase",  default="all")
    parser.add_argument("--out",    default=str(REPO_ROOT / "tools" / "b22_results.json"))
    args = parser.parse_args()

    phases_to_run: list[int]
    if args.phase == "all":
        phases_to_run = [1, 2, 3, 4]
    else:
        phases_to_run = [int(args.phase)]

    compress_bin   = find_binary(args.preset, "compress")
    decompress_bin = find_binary(args.preset, "decompress")
    source_video   = find_source_video()

    print(f"B.22 Seed Search (re-compress per candidate — correct LTP sums)")
    print(f"  compress:   {compress_bin}")
    print(f"  decompress: {decompress_bin}")
    print(f"  source:     {source_video}")
    print(f"  secs/test:  {args.secs} (+~2s compress per candidate)")
    print(f"  phases:     {phases_to_run}")
    print(f"  candidates: {len(CANDIDATE_SEEDS)} per phase")
    estimated = len(phases_to_run) * len(CANDIDATE_SEEDS) * (args.secs + 5)
    print(f"  est. time:  ~{estimated // 60}m {estimated % 60}s")

    all_results: list[dict] = []
    best_seeds = list(DEFAULT_SEEDS)

    with tempfile.TemporaryDirectory(prefix="b22_search_") as tmp_str:
        tmp_dir = pathlib.Path(tmp_str)
        for phase in phases_to_run:
            best_seed = run_phase(
                phase, best_seeds, compress_bin, decompress_bin, source_video,
                args.secs, tmp_dir, all_results
            )
            best_seeds[phase - 1] = best_seed

    # Write results
    out_path = pathlib.Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "best_seeds": [hex(s) for s in best_seeds],
        "best_seeds_decimal": best_seeds,
        "results": all_results,
    }
    out_path.write_text(json.dumps(summary, indent=2))
    print(f"\n{'='*60}")
    print(f"Best seeds found:")
    for i, s in enumerate(best_seeds):
        print(f"  CRSCE_LTP_SEED_{i+1} = {hex(s)}  (decimal: {s})")
    print(f"\nAdd to LtpTable.cpp:")
    for i, s in enumerate(best_seeds, 1):
        print(f"  constexpr std::uint64_t kSeed{i} = {hex(s)}ULL;")
    print(f"\nFull results: {out_path}")


if __name__ == "__main__":
    main()
