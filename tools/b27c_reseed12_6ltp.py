#!/usr/bin/env python3
"""
B.27c — Re-search Seeds 1+2 in the 6-LTP Context.

Fix seeds 5+6 at their defaults (CRSCLTP5 + CRSCLTP6).
Coordinate-descent over seeds 1+2: sweep seed1 (256), freeze best;
sweep seed2, freeze best; iterate until convergence.

Question: does the 6-LTP optimal for seeds 1+2 differ from the 4-LTP winner
(CRSCLTPV + CRSCLTPP, depth 91,090)?
  same winner → 6-LTP operating point doesn't shift the seeds 1+2 landscape
  different winner → additional sub-tables interact non-trivially with seeds 1+2

Usage:
    python3 tools/b27c_reseed12_6ltp.py [--secs N] [--rounds N]
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

REPO_ROOT    = pathlib.Path(__file__).resolve().parent.parent
BUILD_DIR    = REPO_ROOT / "build" / "llvm-release"
COMPRESS     = BUILD_DIR / "compress"
DECOMPRESS   = BUILD_DIR / "decompress"
SOURCE_VIDEO = REPO_ROOT / "docs" / "testData" / "useless-machine.mp4"
OUT_DIR      = REPO_ROOT / "tools" / "b27c_results"

# Seeds 3+4: unchanged
SEED3 = 0x435253434C545033  # CRSCLTP3
SEED4 = 0x435253434C545034  # CRSCLTP4
# Seeds 5+6: fixed at defaults
SEED5_FIXED = 0x435253434C545035  # CRSCLTP5
SEED6_FIXED = 0x435253434C545036  # CRSCLTP6

# Starting point: B.26c winners
SEED1_DEFAULT = 0x435253434C545056  # CRSCLTPV
SEED2_DEFAULT = 0x435253434C545050  # CRSCLTPP

PREFIX = int.from_bytes(b"CRSCLTP", "big")  # 7-byte fixed prefix

BASELINE_DEPTH  = 91_090   # B.26c / B.27 ceiling
B26C_SEED1      = SEED1_DEFAULT
B26C_SEED2      = SEED2_DEFAULT


def make_seed(suffix_byte: int) -> int:
    return (PREFIX << 8) | (suffix_byte & 0xFF)


def seed_label(seed: int) -> str:
    suffix = seed & 0xFF
    try:
        ch = bytes([suffix]).decode("ascii")
        if ch.isprintable() and not ch.isspace():
            return f"CRSCLTP{ch}"
    except Exception:
        pass
    return f"CRSCLTP\\x{suffix:02X}"


def run_candidate(seed1: int, seed2: int, secs: int, tmp: pathlib.Path, tag: str) -> int:
    events_path = tmp / f"events_{tag}.jsonl"
    crsce_path  = tmp / f"compressed_{tag}.crsce"
    out_path    = tmp / f"out_{tag}.bin"

    seed_env = {
        "CRSCE_LTP_SEED_1": str(seed1),
        "CRSCE_LTP_SEED_2": str(seed2),
        "CRSCE_LTP_SEED_3": str(SEED3),
        "CRSCE_LTP_SEED_4": str(SEED4),
        "CRSCE_LTP_SEED_5": str(SEED5_FIXED),
        "CRSCE_LTP_SEED_6": str(SEED6_FIXED),
    }

    cx_env = os.environ.copy()
    cx_env.update(seed_env)
    cx_env["DISABLE_COMPRESS_DI"]  = "1"
    cx_env["CRSCE_DISABLE_GPU"]    = "1"
    cx_env["CRSCE_WATCHDOG_SECS"]  = "60"
    try:
        cx = subprocess.run(
            [str(COMPRESS), "-in", str(SOURCE_VIDEO), "-out", str(crsce_path)],
            env=cx_env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=90,
        )
    except subprocess.TimeoutExpired:
        return 0
    if cx.returncode != 0 or not crsce_path.exists():
        return 0

    dx_env = os.environ.copy()
    dx_env.update(seed_env)
    dx_env["CRSCE_EVENTS_PATH"]   = str(events_path)
    dx_env["CRSCE_METRICS_FLUSH"] = "1"
    dx_env["CRSCE_WATCHDOG_SECS"] = str(secs + 15)

    proc = subprocess.Popen(
        [str(DECOMPRESS), "-in", str(crsce_path), "-out", str(out_path)],
        env=dx_env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
    )
    time.sleep(secs)
    try:
        proc.send_signal(signal.SIGINT)
        proc.wait(timeout=5)
    except (ProcessLookupError, subprocess.TimeoutExpired):
        proc.kill()
        proc.wait()

    peak = 0
    if events_path.exists():
        for line in events_path.read_text(errors="replace").splitlines():
            try:
                d = json.loads(line).get("tags", {}).get("depth")
                if d is not None:
                    peak = max(peak, int(d))
            except (json.JSONDecodeError, ValueError):
                pass
        events_path.unlink(missing_ok=True)
    for p in (crsce_path, out_path):
        p.unlink(missing_ok=True)
    return peak


def sweep_axis(axis: int, fixed1: int, fixed2: int, secs: int,
               tmp: pathlib.Path, all_results: list) -> tuple[int, int]:
    """Sweep all 256 suffix bytes for axis 1 or 2. Returns (best_seed, best_depth)."""
    best_seed  = fixed1 if axis == 1 else fixed2
    best_depth = 0
    axis_depths: dict[int, int] = {}

    for byte_val in range(256):
        candidate = make_seed(byte_val)
        s1 = candidate if axis == 1 else fixed1
        s2 = candidate if axis == 2 else fixed2
        tag = f"ax{axis}_b{byte_val:03d}"
        depth = run_candidate(s1, s2, secs, tmp, tag)
        axis_depths[byte_val] = depth
        if depth > best_depth:
            best_depth = depth
            best_seed  = candidate
        b26c_marker = " [B.26c]" if candidate in (B26C_SEED1, B26C_SEED2) else ""
        best_marker = " ← best" if depth == best_depth else ""
        print(f"  seed{axis}={seed_label(candidate):<16s} depth={depth}"
              f"{b26c_marker}{best_marker}", flush=True)

    all_results.append({"axis": axis, "depths": axis_depths,
                         "best_seed": best_seed, "best_depth": best_depth})
    return best_seed, best_depth


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--secs",   type=int, default=20)
    parser.add_argument("--rounds", type=int, default=3)
    args = parser.parse_args()

    for p in (COMPRESS, DECOMPRESS, SOURCE_VIDEO):
        if not p.exists():
            sys.exit(f"ERROR: required path not found: {p}")

    OUT_DIR.mkdir(parents=True, exist_ok=True)

    print("B.27c — Re-search Seeds 1+2 in 6-LTP Context")
    print(f"  Seeds 5+6 fixed: CRSCLTP5 + CRSCLTP6 (defaults)")
    print(f"  Starting seed1:  {seed_label(SEED1_DEFAULT)}  (B.26c winner)")
    print(f"  Starting seed2:  {seed_label(SEED2_DEFAULT)}  (B.26c winner)")
    print(f"  Secs per eval:   {args.secs}")
    print(f"  Max rounds:      {args.rounds}")
    print(f"  Baseline:        {BASELINE_DEPTH}  (B.26c / B.27)")
    print()

    best1, best2 = SEED1_DEFAULT, SEED2_DEFAULT
    all_results: list = []
    final_depth = 0

    with tempfile.TemporaryDirectory(prefix="b27c_") as tmp_str:
        tmp = pathlib.Path(tmp_str)

        for rnd in range(1, args.rounds + 1):
            print(f"{'='*60}")
            print(f"Round {rnd}")
            print(f"{'='*60}")

            prev1, prev2 = best1, best2

            print(f"\nSweeping seed1 (seed2 fixed = {seed_label(best2)})...")
            best1, depth1 = sweep_axis(1, best1, best2, args.secs, tmp, all_results)
            print(f"  → Best seed1: {seed_label(best1)}  depth={depth1}")

            print(f"\nSweeping seed2 (seed1 fixed = {seed_label(best1)})...")
            best2, depth2 = sweep_axis(2, best1, best2, args.secs, tmp, all_results)
            print(f"  → Best seed2: {seed_label(best2)}  depth={depth2}")

            final_depth = depth2
            print(f"\nRound {rnd} summary: seed1={seed_label(best1)}  "
                  f"seed2={seed_label(best2)}  depth={final_depth}")

            if best1 == prev1 and best2 == prev2:
                print("  Converged.")
                break

    print()
    print("=" * 60)
    print("B.27c Final Result")
    print("=" * 60)
    print(f"  Best seed1: {seed_label(best1)}  (B.26c was {seed_label(B26C_SEED1)})")
    print(f"  Best seed2: {seed_label(best2)}  (B.26c was {seed_label(B26C_SEED2)})")
    delta = final_depth - BASELINE_DEPTH
    sign = "+" if delta >= 0 else ""
    print(f"  Peak depth: {final_depth}  ({sign}{delta} vs baseline {BASELINE_DEPTH})")
    print()
    if best1 == B26C_SEED1 and best2 == B26C_SEED2:
        print("INTERPRETATION: 6-LTP optimal = 4-LTP optimal. "
              "Additional sub-tables don't shift the seeds 1+2 landscape.")
    elif final_depth > BASELINE_DEPTH:
        print(f"INTERPRETATION: NEW BEST! 6-LTP finds a better pair than B.26c: "
              f"{seed_label(best1)} + {seed_label(best2)}")
    else:
        print(f"INTERPRETATION: 6-LTP finds a different winner but at same or lower depth: "
              f"{seed_label(best1)} + {seed_label(best2)}")

    out = OUT_DIR / "b27c_results.json"
    out.write_text(json.dumps({
        "experiment": "B.27c",
        "seed5_fixed": SEED5_FIXED, "seed6_fixed": SEED6_FIXED,
        "best_seed1": best1, "best_seed1_label": seed_label(best1),
        "best_seed2": best2, "best_seed2_label": seed_label(best2),
        "b26c_seed1": B26C_SEED1, "b26c_seed2": B26C_SEED2,
        "peak_depth": final_depth,
        "delta_vs_baseline": final_depth - BASELINE_DEPTH,
        "rounds": all_results,
    }, indent=2))
    print(f"\nResults written to: {out}")


if __name__ == "__main__":
    main()
