#!/usr/bin/env python3
"""
B.27b — Weak Seeds 1+2, Coordinate-Descent Search of Seeds 5+6.

Fix seeds 1+2 at CRSCLTP1+CRSCLTP2 (weak, ~86,123 depth).
Coordinate-descent over seeds 5+6: sweep seed5 (256), freeze best;
sweep seed6, freeze best; iterate until convergence.

Question: can seeds 5+6 independently drive depth from ~86K toward 91K?
  yes → sub-tables 5+6 are capable; invariance in B.27 was headroom exhaustion
  no  → sub-tables 5+6 are structurally inert

Usage:
    python3 tools/b27b_weak12_search56.py [--secs N] [--workers N] [--rounds N]
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
OUT_DIR      = REPO_ROOT / "tools" / "b27b_results"

# Seeds 1+2: weak (pre-B.26c defaults)
SEED1_WEAK = 0x435253434C545031  # CRSCLTP1
SEED2_WEAK = 0x435253434C545032  # CRSCLTP2
# Seeds 3+4: unchanged
SEED3 = 0x435253434C545033  # CRSCLTP3
SEED4 = 0x435253434C545034  # CRSCLTP4
# Seeds 5+6 default (starting point for coord descent)
SEED5_DEFAULT = 0x435253434C545035  # CRSCLTP5
SEED6_DEFAULT = 0x435253434C545036  # CRSCLTP6

PREFIX = int.from_bytes(b"CRSCLTP", "big")  # 7-byte fixed prefix

BASELINE_DEPTH = 91_090
WEAK_DEPTH     = 86_123


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


def run_candidate(seed5: int, seed6: int, secs: int, tmp: pathlib.Path, tag: str) -> int:
    events_path = tmp / f"events_{tag}.jsonl"
    crsce_path  = tmp / f"compressed_{tag}.crsce"
    out_path    = tmp / f"out_{tag}.bin"

    seed_env = {
        "CRSCE_LTP_SEED_1": str(SEED1_WEAK),
        "CRSCE_LTP_SEED_2": str(SEED2_WEAK),
        "CRSCE_LTP_SEED_3": str(SEED3),
        "CRSCE_LTP_SEED_4": str(SEED4),
        "CRSCE_LTP_SEED_5": str(seed5),
        "CRSCE_LTP_SEED_6": str(seed6),
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


def sweep_axis(axis: int, fixed5: int, fixed6: int, secs: int,
               tmp: pathlib.Path, results_log: list) -> tuple[int, int]:
    """Sweep all 256 suffix bytes for the given axis. Returns (best_seed, best_depth)."""
    best_seed, best_depth = (fixed5 if axis == 5 else fixed6), 0
    depths = {}
    label = f"seed{axis}"

    for byte_val in range(256):
        candidate = make_seed(byte_val)
        s5 = candidate if axis == 5 else fixed5
        s6 = candidate if axis == 6 else fixed6
        tag = f"ax{axis}_b{byte_val:03d}"
        depth = run_candidate(s5, s6, secs, tmp, tag)
        depths[byte_val] = depth
        if depth > best_depth:
            best_depth = depth
            best_seed = candidate
        marker = " ← best" if depth == best_depth else ""
        print(f"  {label}={seed_label(candidate):<16s} depth={depth}{marker}", flush=True)

    results_log.append({"axis": axis, "depths": depths,
                         "best_seed": best_seed, "best_depth": best_depth})
    return best_seed, best_depth


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--secs",    type=int, default=20)
    parser.add_argument("--rounds",  type=int, default=3)
    args = parser.parse_args()

    for p in (COMPRESS, DECOMPRESS, SOURCE_VIDEO):
        if not p.exists():
            sys.exit(f"ERROR: required path not found: {p}")

    OUT_DIR.mkdir(parents=True, exist_ok=True)

    print("B.27b — Weak Seeds 1+2, Coordinate-Descent Search of Seeds 5+6")
    print(f"  Seeds 1+2 fixed: CRSCLTP1 + CRSCLTP2 (weak, ~{WEAK_DEPTH})")
    print(f"  Secs per eval:   {args.secs}")
    print(f"  Max rounds:      {args.rounds}")
    print(f"  Baseline:        {BASELINE_DEPTH}  (B.26c)")
    print()

    best5, best6 = SEED5_DEFAULT, SEED6_DEFAULT
    all_results = []

    with tempfile.TemporaryDirectory(prefix="b27b_") as tmp_str:
        tmp = pathlib.Path(tmp_str)

        for rnd in range(1, args.rounds + 1):
            print(f"{'='*60}")
            print(f"Round {rnd}")
            print(f"{'='*60}")

            prev5, prev6 = best5, best6

            # Sweep seed5
            print(f"\nSweeping seed5 (seed6 fixed = {seed_label(best6)})...")
            best5, depth5 = sweep_axis(5, best5, best6, args.secs, tmp, all_results)
            print(f"  → Best seed5: {seed_label(best5)}  depth={depth5}")

            # Sweep seed6
            print(f"\nSweeping seed6 (seed5 fixed = {seed_label(best5)})...")
            best6, depth6 = sweep_axis(6, best5, best6, args.secs, tmp, all_results)
            print(f"  → Best seed6: {seed_label(best6)}  depth={depth6}")

            print(f"\nRound {rnd} summary: seed5={seed_label(best5)}  "
                  f"seed6={seed_label(best6)}  depth={depth6}")

            # Convergence check
            if best5 == prev5 and best6 == prev6:
                print("  Converged (no change).")
                break

    # Final report
    print()
    print("=" * 60)
    print("B.27b Final Result")
    print("=" * 60)
    print(f"  Best seed5: {seed_label(best5)}")
    print(f"  Best seed6: {seed_label(best6)}")
    delta = depth6 - BASELINE_DEPTH
    sign = "+" if delta >= 0 else ""
    print(f"  Peak depth: {depth6}  ({sign}{delta} vs B.26c baseline {BASELINE_DEPTH})")
    print()
    if depth6 >= BASELINE_DEPTH - 500:
        print("INTERPRETATION: Seeds 5+6 can independently drive depth — "
              "B.27 invariance was headroom exhaustion.")
    elif depth6 > WEAK_DEPTH + 500:
        print("INTERPRETATION: Seeds 5+6 provide partial improvement over weak 1+2 baseline.")
    else:
        print("INTERPRETATION: Seeds 5+6 are structurally inert — "
              "no seed choice compensates for weak slots 1+2.")

    out = OUT_DIR / "b27b_results.json"
    out.write_text(json.dumps({
        "experiment": "B.27b",
        "seed1": SEED1_WEAK, "seed2": SEED2_WEAK,
        "best_seed5": best5, "best_seed5_label": seed_label(best5),
        "best_seed6": best6, "best_seed6_label": seed_label(best6),
        "peak_depth": depth6,
        "delta_vs_baseline": depth6 - BASELINE_DEPTH,
        "rounds": all_results,
    }, indent=2))
    print(f"\nResults written to: {out}")


if __name__ == "__main__":
    main()
