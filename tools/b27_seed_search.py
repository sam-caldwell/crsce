#!/usr/bin/env python3
"""
B.27 Full-Byte Joint 2-Seed Search — CRSCE LTP5/LTP6 joint optimizer.

Exhaustive search over all 256×256=65,536 combinations of CRSCE_LTP_SEED_5 and
CRSCE_LTP_SEED_6 (suffix bytes 0x00..0xFF), with seeds 1–4 fixed at the B.26c
joint-search winner values.

Fixed seeds (B.26c optimal):
    seed1 = CRSCLTPV  (depth 91,090 winner pair half-1)
    seed2 = CRSCLTPP  (depth 91,090 winner pair half-2)
    seed3 = CRSCLTP3  (invariant — all 36 candidates tie in B.22 Phase 3)
    seed4 = CRSCLTP4  (invariant — all 36 candidates tie in B.22 Phase 4)

Searching:
    seed5 = 0x00..0xFF suffix byte
    seed6 = 0x00..0xFF suffix byte
    → 65,536 total pairs

Usage:
    # Launch 4 workers (one per core):
    python3 tools/b27_seed_search.py --worker 0 --workers 4 &
    python3 tools/b27_seed_search.py --worker 1 --workers 4 &
    python3 tools/b27_seed_search.py --worker 2 --workers 4 &
    python3 tools/b27_seed_search.py --worker 3 --workers 4 &

    # Or launch all workers at once:
    python3 tools/b27_seed_search.py --workers 4 --launch-all

    # Merge results after all workers complete:
    python3 tools/b27_merge_results.py --out-dir tools/b27_results --workers 4

Options:
    --secs N        Seconds per candidate test (default: 20)
    --preset P      CMake preset name (default: llvm-release)
    --worker W      Worker index (0-based). Each worker handles a slice of seed5.
    --workers N     Total number of workers (default: 4)
    --launch-all    Launch all N workers as background processes, then wait.
    --out-dir DIR   Directory for per-worker result files (default: tools/b27_results/)

Partitioning (4 workers):
    Worker 0: seed5 in [0x00..0x3F], seed6 in [0x00..0xFF]  →  16,384 pairs
    Worker 1: seed5 in [0x40..0x7F], seed6 in [0x00..0xFF]  →  16,384 pairs
    Worker 2: seed5 in [0x80..0xBF], seed6 in [0x00..0xFF]  →  16,384 pairs
    Worker 3: seed5 in [0xC0..0xFF], seed6 in [0x00..0xFF]  →  16,384 pairs

Estimated runtime at 20s/test + 3s compress, 4 workers:
    16,384 pairs × 23s / worker = ~104 hours (~4.3 days) wall time
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
DEFAULT_WORKERS = 4

# Shared prefix for all CRSCE LTP seeds
_PREFIX = b"CRSCLTP"

# Fixed seeds (B.26c optimal for seeds 1+2; B.22 invariant for seeds 3+4)
SEED1_FIXED = int.from_bytes(_PREFIX + bytes([0x56]), "big")  # CRSCLTPV
SEED2_FIXED = int.from_bytes(_PREFIX + bytes([0x50]), "big")  # CRSCLTPP
SEED3_FIXED = int.from_bytes(_PREFIX + bytes([0x33]), "big")  # CRSCLTP3
SEED4_FIXED = int.from_bytes(_PREFIX + bytes([0x34]), "big")  # CRSCLTP4

# Baseline: B.27 with default seeds (CRSCLTP5, CRSCLTP6) → depth 91,090
SEED5_BASELINE = int.from_bytes(_PREFIX + bytes([0x35]), "big")  # CRSCLTP5
SEED6_BASELINE = int.from_bytes(_PREFIX + bytes([0x36]), "big")  # CRSCLTP6
BASELINE_DEPTH = 91_090

# Full byte range: 0x00..0xFF (256 candidates)
CANDIDATE_SUFFIXES = list(range(0x00, 0x100))  # 256 total


def make_seed(suffix_byte: int) -> int:
    """Make a candidate seed from the shared prefix + one suffix byte."""
    return int.from_bytes(_PREFIX + bytes([suffix_byte]), "big")


ALL_SEEDS = [make_seed(b) for b in CANDIDATE_SUFFIXES]


def seed_to_label(seed: int) -> str:
    """Convert a seed integer to its CRSCLTP? label."""
    b = seed & 0xFF
    if 0x21 <= b < 0x7F:
        ch = chr(b)
    else:
        ch = f"\\x{b:02X}"
    return f"CRSCLTP{ch}"


# ---------------------------------------------------------------------------
# Path helpers
# ---------------------------------------------------------------------------
def find_binary(preset: str, name: str) -> pathlib.Path:
    """Find a compiled binary under build/<preset>/<name>."""
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
# Worker partitioning
# ---------------------------------------------------------------------------
def partition_seed5_range(worker: int, workers: int) -> list[int]:
    """Return the seed5 suffix bytes assigned to this worker."""
    chunk = 256 // workers
    remainder = 256 % workers
    start = worker * chunk + min(worker, remainder)
    end = start + chunk + (1 if worker < remainder else 0)
    return list(range(start, end))


# ---------------------------------------------------------------------------
# Single candidate evaluation
# ---------------------------------------------------------------------------
def run_candidate(
    compress_bin: pathlib.Path,
    decompress_bin: pathlib.Path,
    source_video: pathlib.Path,
    seed5: int,
    seed6: int,
    secs: int,
    tmp_dir: pathlib.Path,
    run_id: str,
) -> int:
    """
    Re-compress source_video with seeds 1–6 (1–4 fixed, 5+6 varied), then
    decompress for `secs` seconds.  Returns peak depth observed in telemetry, or 0.
    """
    events_path = tmp_dir / f"events_{run_id}.jsonl"
    crsce_path  = tmp_dir / f"compressed_{run_id}.crsce"
    out_path    = tmp_dir / f"out_{run_id}.bin"

    seed_env = {
        "CRSCE_LTP_SEED_1": str(SEED1_FIXED),
        "CRSCE_LTP_SEED_2": str(SEED2_FIXED),
        "CRSCE_LTP_SEED_3": str(SEED3_FIXED),
        "CRSCE_LTP_SEED_4": str(SEED4_FIXED),
        "CRSCE_LTP_SEED_5": str(seed5),
        "CRSCE_LTP_SEED_6": str(seed6),
    }

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
# Single worker main loop
# ---------------------------------------------------------------------------
def run_worker(
    worker: int,
    workers: int,
    secs: int,
    preset: str,
    out_dir: pathlib.Path,
) -> None:
    """Run the search for this worker's seed5 partition."""
    compress_bin   = find_binary(preset, "compress")
    decompress_bin = find_binary(preset, "decompress")
    source_video   = find_source_video()

    seed5_suffixes = partition_seed5_range(worker, workers)
    seed5_seeds    = [make_seed(b) for b in seed5_suffixes]
    total_pairs    = len(seed5_seeds) * 256
    secs_per       = secs + 5
    est_hours      = (total_pairs * secs_per) / 3600

    out_path = out_dir / f"b27_results_w{worker}.json"

    print(f"B.27 Worker {worker}/{workers}")
    print(f"  compress:      {compress_bin}")
    print(f"  decompress:    {decompress_bin}")
    print(f"  source:        {source_video}")
    print(f"  secs/test:     {secs}  (+~5s compress)")
    print(f"  seed5 range:   0x{seed5_suffixes[0]:02X}..0x{seed5_suffixes[-1]:02X} "
          f"({len(seed5_suffixes)} values)")
    print(f"  seed6 range:   0x00..0xFF (256 values)")
    print(f"  pairs:         {total_pairs}")
    print(f"  fixed seeds:   seed1=CRSCLTPV seed2=CRSCLTPP seed3=CRSCLTP3 seed4=CRSCLTP4")
    print(f"  baseline:      CRSCLTP5+CRSCLTP6, depth={BASELINE_DEPTH}")
    print(f"  est. runtime:  ~{est_hours:.1f} hours")
    print(f"  results:       {out_path}")

    # Load prior results if resuming
    completed: dict[str, int] = {}
    all_results: list[dict] = []
    if out_path.exists():
        try:
            prior = json.loads(out_path.read_text())
            for r in prior.get("results", []):
                key = f"{r['seed5']},{r['seed6']}"
                completed[key] = r["depth"]
                all_results.append(r)
            print(f"  resumed:       {len(completed)} pairs already done")
        except (json.JSONDecodeError, KeyError):
            pass

    best_depth = max((r["depth"] for r in all_results), default=0)
    best_pair  = next((r for r in all_results if r["depth"] == best_depth), None)
    remaining  = total_pairs - len(completed)

    print(f"\n{'='*60}")
    print(f"Worker {worker}: {remaining} pairs remaining")
    print(f"{'='*60}\n")

    with tempfile.TemporaryDirectory(prefix=f"b27_w{worker}_") as tmp_str:
        tmp_dir = pathlib.Path(tmp_str)
        pair_count = 0

        for s5 in seed5_seeds:
            for s6_suffix in CANDIDATE_SUFFIXES:
                s6 = ALL_SEEDS[s6_suffix]
                pair_count += 1
                key = f"{hex(s5)},{hex(s6)}"
                if key in completed:
                    continue

                label5 = seed_to_label(s5)
                label6 = seed_to_label(s6)
                run_id = f"w{worker}_p{pair_count}"

                print(
                    f"  W{worker} [{pair_count:5d}/{total_pairs}] "
                    f"seed5={label5} seed6={label6} ... ",
                    end="",
                    flush=True,
                )

                depth = run_candidate(
                    compress_bin, decompress_bin, source_video,
                    s5, s6, secs, tmp_dir, run_id,
                )

                is_new_best = depth > best_depth
                marker = " *** NEW BEST ***" if is_new_best else ""
                beats_baseline = depth > BASELINE_DEPTH
                baseline_marker = " [beats baseline]" if beats_baseline else ""
                print(f"depth={depth}{baseline_marker}{marker}")

                if is_new_best:
                    best_depth = depth
                    best_pair = {"seed5": hex(s5), "seed6": hex(s6), "depth": depth}

                result = {
                    "pair_idx": pair_count,
                    "seed5": hex(s5),
                    "seed6": hex(s6),
                    "seed5_label": label5,
                    "seed6_label": label6,
                    "seed1": hex(SEED1_FIXED),
                    "seed2": hex(SEED2_FIXED),
                    "seed3": hex(SEED3_FIXED),
                    "seed4": hex(SEED4_FIXED),
                    "depth": depth,
                    "beats_baseline": beats_baseline,
                }
                all_results.append(result)
                completed[key] = depth

                # Write progressively
                summary = {
                    "worker": worker,
                    "workers": workers,
                    "seed5_range": [f"0x{seed5_suffixes[0]:02X}",
                                    f"0x{seed5_suffixes[-1]:02X}"],
                    "fixed": {
                        "seed1": hex(SEED1_FIXED), "seed1_label": "CRSCLTPV",
                        "seed2": hex(SEED2_FIXED), "seed2_label": "CRSCLTPP",
                        "seed3": hex(SEED3_FIXED), "seed3_label": "CRSCLTP3",
                        "seed4": hex(SEED4_FIXED), "seed4_label": "CRSCLTP4",
                    },
                    "baseline": {"seed5": hex(SEED5_BASELINE),
                                 "seed6": hex(SEED6_BASELINE),
                                 "depth": BASELINE_DEPTH},
                    "best_found": best_pair,
                    "progress": {"done": len(completed), "total": total_pairs},
                    "results": all_results,
                }
                out_path.write_text(json.dumps(summary, indent=2))

    # Final summary
    print(f"\n{'='*60}")
    print(f"Worker {worker} Complete")
    print(f"  Baseline:   CRSCLTP5+CRSCLTP6, depth={BASELINE_DEPTH}")
    if best_pair and best_pair.get("depth", 0) > BASELINE_DEPTH:
        print(f"  ** NEW BEST: seed5={best_pair['seed5']} seed6={best_pair['seed6']} "
              f"depth={best_pair['depth']} **")
        print(f"  Improvement: +{best_pair['depth'] - BASELINE_DEPTH}")
    elif best_pair:
        print(f"  Best found: seed5={best_pair['seed5']} seed6={best_pair['seed6']} "
              f"depth={best_pair['depth']}")
    print(f"  Results: {out_path}")

    # Print top-10 for this worker
    all_results.sort(key=lambda r: r["depth"], reverse=True)
    print(f"\nWorker {worker} Top-10:")
    for i, r in enumerate(all_results[:10]):
        marker = ""
        if r["seed5"] == hex(SEED5_BASELINE) and r["seed6"] == hex(SEED6_BASELINE):
            marker = " [baseline]"
        print(f"  {i+1:2d}. seed5={r['seed5_label']} seed6={r['seed6_label']} "
              f"depth={r['depth']}{marker}")


# ---------------------------------------------------------------------------
# --launch-all: spawn all workers as background processes, then wait
# ---------------------------------------------------------------------------
def launch_all(workers: int, secs: int, preset: str, out_dir: pathlib.Path) -> None:
    """Spawn N worker processes and wait for all to complete."""
    script = str(pathlib.Path(__file__).resolve())
    procs: list[subprocess.Popen] = []

    print(f"B.27 seed5+seed6 joint search: launching {workers} workers...")
    print(f"  Fixed: seed1=CRSCLTPV, seed2=CRSCLTPP, seed3=CRSCLTP3, seed4=CRSCLTP4")
    print(f"  Searching: seed5 × seed6 over 0x00..0xFF (65,536 pairs total)")
    for w in range(workers):
        log_path = out_dir / f"b27_worker_{w}.log"
        log_file = open(log_path, "w")  # noqa: WPS515
        cmd = [
            sys.executable, script,
            "--worker", str(w),
            "--workers", str(workers),
            "--secs", str(secs),
            "--preset", preset,
            "--out-dir", str(out_dir),
        ]
        proc = subprocess.Popen(cmd, stdout=log_file, stderr=subprocess.STDOUT)
        procs.append(proc)
        print(f"  Worker {w}: PID {proc.pid}, log={log_path}")

    est_hours = (256 // workers) * 256 * (secs + 5) / 3600
    print(f"\nAll {workers} workers launched.")
    print(f"Monitor: tail -f {out_dir}/b27_worker_*.log")
    print(f"Estimated wall time: ~{est_hours:.1f} hours\n")

    failed = []
    for w, proc in enumerate(procs):
        rc = proc.wait()
        status = "OK" if rc == 0 else f"FAILED (rc={rc})"
        print(f"  Worker {w}: {status}")
        if rc != 0:
            failed.append(w)

    if failed:
        print(f"\nWARNING: Workers {failed} failed. Check logs.")
        print(f"Re-run: python3 tools/b27_seed_search.py --worker N --workers {workers}")
    else:
        print(f"\nAll workers complete. Merge results with:")
        print(f"  python3 tools/b27_merge_results.py --out-dir {out_dir} --workers {workers}")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main() -> None:
    parser = argparse.ArgumentParser(
        description="B.27 full-byte joint seed5+seed6 search for LTP5/LTP6"
    )
    parser.add_argument("--secs",       type=int, default=DEFAULT_SECS)
    parser.add_argument("--preset",     default=DEFAULT_PRESET)
    parser.add_argument("--worker",     type=int, default=None,
                        help="Worker index (0-based)")
    parser.add_argument("--workers",    type=int, default=DEFAULT_WORKERS,
                        help="Total number of workers")
    parser.add_argument("--launch-all", action="store_true",
                        help="Launch all workers as background processes")
    parser.add_argument("--out-dir",    default=str(REPO_ROOT / "tools" / "b27_results"),
                        help="Directory for per-worker result files")
    args = parser.parse_args()

    out_dir = pathlib.Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    if args.launch_all:
        launch_all(args.workers, args.secs, args.preset, out_dir)
    elif args.worker is not None:
        if args.worker < 0 or args.worker >= args.workers:
            sys.exit(f"Worker index {args.worker} out of range [0, {args.workers})")
        run_worker(args.worker, args.workers, args.secs, args.preset, out_dir)
    else:
        sys.exit("Specify --worker N or --launch-all. See --help.")


if __name__ == "__main__":
    main()
