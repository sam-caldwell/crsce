#!/usr/bin/env python3
"""
B.27a Swap Experiment — Value vs. Slot Position.

Put B.26c winning seeds (CRSCLTPV + CRSCLTPP) in slots 5+6.
Put pre-B.26c weak seeds  (CRSCLTP1 + CRSCLTP2) in slots 1+2.
Seeds 3+4 unchanged (CRSCLTP3 + CRSCLTP4).

Question: does the B.26c geometry work regardless of which slot it occupies?
  depth ≈ 91,090  → values are fungible across slots
  depth ≈ 86,123  → slot position matters structurally
"""

import json
import os
import pathlib
import signal
import subprocess
import sys
import tempfile
import time

REPO_ROOT   = pathlib.Path(__file__).resolve().parent.parent
BUILD_DIR   = REPO_ROOT / "build" / "llvm-release"
COMPRESS    = BUILD_DIR / "compress"
DECOMPRESS  = BUILD_DIR / "decompress"
SOURCE_VIDEO = REPO_ROOT / "docs" / "testData" / "useless-machine.mp4"

SECS = 20  # decompress duration per candidate (matches b27_seed_search.py default)

# Slot 1+2: pre-B.26c weak defaults
SEED1 = 0x435253434C545031  # CRSCLTP1
SEED2 = 0x435253434C545032  # CRSCLTP2
# Slot 3+4: unchanged
SEED3 = 0x435253434C545033  # CRSCLTP3
SEED4 = 0x435253434C545034  # CRSCLTP4
# Slot 5+6: B.26c winners moved here
SEED5 = 0x435253434C545056  # CRSCLTPV  ← B.26c winner
SEED6 = 0x435253434C545050  # CRSCLTPP  ← B.26c winner

# B.26c baseline for comparison
BASELINE_DEPTH = 91_090
WEAK_DEPTH     = 86_123  # approximate 4-LTP depth with CRSCLTP1+CRSCLTP2


def main() -> None:
    for p in (COMPRESS, DECOMPRESS, SOURCE_VIDEO):
        if not p.exists():
            sys.exit(f"ERROR: required path not found: {p}")

    seed_env = {
        "CRSCE_LTP_SEED_1": str(SEED1),
        "CRSCE_LTP_SEED_2": str(SEED2),
        "CRSCE_LTP_SEED_3": str(SEED3),
        "CRSCE_LTP_SEED_4": str(SEED4),
        "CRSCE_LTP_SEED_5": str(SEED5),
        "CRSCE_LTP_SEED_6": str(SEED6),
    }

    print("B.27a Swap Experiment")
    print("  Slots 1+2: CRSCLTP1 + CRSCLTP2  (weak, pre-B.26c defaults)")
    print("  Slots 3+4: CRSCLTP3 + CRSCLTP4  (unchanged)")
    print("  Slots 5+6: CRSCLTPV + CRSCLTPP  (B.26c winners)")
    print(f"  Decompress duration: {SECS}s")
    print(f"  Baseline: {BASELINE_DEPTH}  |  Weak reference: ~{WEAK_DEPTH}")
    print()

    with tempfile.TemporaryDirectory(prefix="b27a_") as tmp_str:
        tmp = pathlib.Path(tmp_str)
        crsce_path  = tmp / "swap.crsce"
        out_path    = tmp / "swap_out.bin"
        events_path = tmp / "swap_events.jsonl"

        # Step 1: compress
        print("Compressing with swapped seeds...", flush=True)
        cx_env = os.environ.copy()
        cx_env.update(seed_env)
        cx_env["DISABLE_COMPRESS_DI"]  = "1"
        cx_env["CRSCE_DISABLE_GPU"]    = "1"
        cx_env["CRSCE_WATCHDOG_SECS"]  = "60"

        cx = subprocess.run(
            [str(COMPRESS), "-in", str(SOURCE_VIDEO), "-out", str(crsce_path)],
            env=cx_env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=90,
        )
        if cx.returncode != 0 or not crsce_path.exists():
            sys.exit("ERROR: compress failed")
        print(f"  Compressed OK ({crsce_path.stat().st_size:,} bytes)")

        # Step 2: decompress for SECS seconds
        print(f"Decompressing for {SECS}s...", flush=True)
        dx_env = os.environ.copy()
        dx_env.update(seed_env)
        dx_env["CRSCE_EVENTS_PATH"]   = str(events_path)
        dx_env["CRSCE_METRICS_FLUSH"] = "1"
        dx_env["CRSCE_WATCHDOG_SECS"] = str(SECS + 15)

        proc = subprocess.Popen(
            [str(DECOMPRESS), "-in", str(crsce_path), "-out", str(out_path)],
            env=dx_env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        time.sleep(SECS)
        try:
            proc.send_signal(signal.SIGINT)
            proc.wait(timeout=5)
        except (ProcessLookupError, subprocess.TimeoutExpired):
            proc.kill()
            proc.wait()

        # Step 3: parse peak depth
        peak_depth = 0
        if events_path.exists():
            for line in events_path.read_text(errors="replace").splitlines():
                try:
                    entry = json.loads(line)
                    d = entry.get("tags", {}).get("depth")
                    if d is not None:
                        peak_depth = max(peak_depth, int(d))
                except (json.JSONDecodeError, ValueError):
                    continue

        # Report
        print()
        print("=" * 50)
        print(f"  Peak depth:  {peak_depth}")
        print(f"  Baseline:    {BASELINE_DEPTH}  (B.26c, slots 1+2 = CRSCLTPV+CRSCLTPP)")
        print(f"  Weak ref:    ~{WEAK_DEPTH}  (4-LTP, slots 1+2 = CRSCLTP1+CRSCLTP2)")
        delta = peak_depth - BASELINE_DEPTH
        sign = "+" if delta >= 0 else ""
        print(f"  vs baseline: {sign}{delta}")
        print()
        if peak_depth >= BASELINE_DEPTH - 500:
            print("INTERPRETATION: Values are fungible — B.26c geometry works in any slot.")
        else:
            print("INTERPRETATION: Slot position matters — sub-table index has structural significance.")
        print("=" * 50)


if __name__ == "__main__":
    main()
