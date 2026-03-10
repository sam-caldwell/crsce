#!/usr/bin/env python3
"""
B.26d Merge Results — Combine per-worker output files from b26d_full_byte_joint_search.py.

Reads all b26d_results_w{N}.json files from the output directory, merges into a
single unified results file with global statistics and ranked pairs.

Usage:
    python3 tools/b26d_merge_results.py --out-dir tools/b26d_results --workers 4
    python3 tools/b26d_merge_results.py --out-dir tools/b26d_results --workers 4 --out merged.json

Options:
    --out-dir DIR   Directory containing per-worker result files (default: tools/b26d_results/)
    --workers N     Number of workers to merge (default: 4)
    --out FILE      Write merged results to FILE (default: <out-dir>/b26d_merged.json)
    --top N         Number of top results to print (default: 25)
"""

import argparse
import json
import pathlib
import statistics
import sys

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent

# B.26c baseline for comparison
BASELINE_DEPTH = 91_090
BASELINE_SEED1 = "0x4352534c54505056"  # CRSCLTPV
BASELINE_SEED2 = "0x4352534c54505050"  # CRSCLTPP


def load_worker_results(out_dir: pathlib.Path, workers: int) -> tuple[list[dict], list[int]]:
    """Load all worker result files. Returns (all_results, missing_workers)."""
    all_results: list[dict] = []
    missing: list[int] = []

    for w in range(workers):
        path = out_dir / f"b26d_results_w{w}.json"
        if not path.exists():
            missing.append(w)
            continue
        try:
            data = json.loads(path.read_text())
            results = data.get("results", [])
            # Tag each result with its source worker
            for r in results:
                r["worker"] = w
            all_results.extend(results)
            progress = data.get("progress", {})
            done = progress.get("done", len(results))
            total = progress.get("total", "?")
            print(f"  Worker {w}: {done}/{total} pairs loaded from {path.name}")
        except (json.JSONDecodeError, KeyError) as e:
            print(f"  Worker {w}: ERROR reading {path.name}: {e}")
            missing.append(w)

    return all_results, missing


def compute_statistics(all_results: list[dict]) -> dict:
    """Compute descriptive statistics over all depth measurements."""
    depths = [r["depth"] for r in all_results if r["depth"] > 0]
    if not depths:
        return {"count": 0}

    sorted_depths = sorted(depths)
    n = len(sorted_depths)

    stats = {
        "count": n,
        "zeros": sum(1 for r in all_results if r["depth"] == 0),
        "mean": statistics.mean(depths),
        "median": statistics.median(depths),
        "stdev": statistics.stdev(depths) if n > 1 else 0.0,
        "min": sorted_depths[0],
        "max": sorted_depths[-1],
        "p5": sorted_depths[int(n * 0.05)],
        "p25": sorted_depths[int(n * 0.25)],
        "p75": sorted_depths[int(n * 0.75)],
        "p95": sorted_depths[int(n * 0.95)],
        "beats_baseline": sum(1 for d in depths if d > BASELINE_DEPTH),
    }
    return stats


def main() -> None:
    parser = argparse.ArgumentParser(description="Merge B.26d per-worker results")
    parser.add_argument("--out-dir", default=str(REPO_ROOT / "tools" / "b26d_results"),
                        help="Directory containing per-worker result files")
    parser.add_argument("--workers", type=int, default=4,
                        help="Number of workers to merge")
    parser.add_argument("--out", default=None,
                        help="Output file path (default: <out-dir>/b26d_merged.json)")
    parser.add_argument("--top", type=int, default=25,
                        help="Number of top results to print")
    args = parser.parse_args()

    out_dir = pathlib.Path(args.out_dir)
    if not out_dir.exists():
        sys.exit(f"Output directory not found: {out_dir}")

    out_path = pathlib.Path(args.out) if args.out else out_dir / "b26d_merged.json"

    print(f"B.26d Merge Results")
    print(f"  out-dir:  {out_dir}")
    print(f"  workers:  {args.workers}")
    print(f"  output:   {out_path}")
    print()

    all_results, missing = load_worker_results(out_dir, args.workers)

    if missing:
        print(f"\nWARNING: Missing or unreadable worker files: {missing}")
        print(f"  Re-run with: python3 tools/b26d_full_byte_joint_search.py "
              f"--worker N --workers {args.workers}")

    if not all_results:
        sys.exit("\nNo results to merge.")

    # Deduplicate by (seed1, seed2) key, keeping the higher depth
    seen: dict[str, dict] = {}
    for r in all_results:
        key = f"{r['seed1']},{r['seed2']}"
        if key not in seen or r["depth"] > seen[key]["depth"]:
            seen[key] = r
    deduped = list(seen.values())

    # Sort by depth descending
    deduped.sort(key=lambda r: r["depth"], reverse=True)

    # Statistics
    stats = compute_statistics(deduped)

    # Global best
    best = deduped[0] if deduped else None

    total_expected = 256 * 256
    coverage = len(deduped) / total_expected * 100

    print(f"\n{'='*60}")
    print(f"Merged Results Summary")
    print(f"{'='*60}")
    print(f"  Total pairs:       {len(deduped)} / {total_expected} ({coverage:.1f}% coverage)")
    if stats.get("zeros", 0) > 0:
        print(f"  Zero-depth:        {stats['zeros']} (compress/decompress failures)")
    print(f"  Baseline:          CRSCLTPV+CRSCLTPP, depth={BASELINE_DEPTH}")
    if best:
        delta = best["depth"] - BASELINE_DEPTH
        sign = "+" if delta >= 0 else ""
        print(f"  Global best:       seed1={best['seed1']} seed2={best['seed2']} "
              f"depth={best['depth']} ({sign}{delta} vs baseline)")
    if stats["count"] > 0:
        print(f"  Beats baseline:    {stats['beats_baseline']} pairs")
        print(f"  Mean depth:        {stats['mean']:.1f}")
        print(f"  Median depth:      {stats['median']:.1f}")
        print(f"  Std deviation:     {stats['stdev']:.1f}")
        print(f"  Range:             [{stats['min']}, {stats['max']}]")
        print(f"  Percentiles:       p5={stats['p5']}  p25={stats['p25']}  "
              f"p75={stats['p75']}  p95={stats['p95']}")

    # Print top-N
    top_n = min(args.top, len(deduped))
    print(f"\nTop-{top_n} pairs:")
    for i, r in enumerate(deduped[:top_n]):
        label1 = r.get("seed1_label", r["seed1"])
        label2 = r.get("seed2_label", r["seed2"])
        marker = ""
        if r["depth"] > BASELINE_DEPTH:
            marker = " [beats baseline]"
        elif r["seed1"] == BASELINE_SEED1 and r["seed2"] == BASELINE_SEED2:
            marker = " [baseline]"
        print(f"  {i+1:3d}. seed1={label1:<16s} seed2={label2:<16s} depth={r['depth']}{marker}")

    # Write merged output
    merged = {
        "search": "B.26d full-byte joint 2-seed search",
        "candidate_range": "0x00..0xFF (256 per seed axis)",
        "total_pairs_expected": total_expected,
        "total_pairs_completed": len(deduped),
        "coverage_pct": round(coverage, 2),
        "baseline": {
            "seed1": BASELINE_SEED1,
            "seed2": BASELINE_SEED2,
            "depth": BASELINE_DEPTH,
        },
        "best_found": {
            "seed1": best["seed1"],
            "seed2": best["seed2"],
            "seed1_label": best.get("seed1_label", ""),
            "seed2_label": best.get("seed2_label", ""),
            "depth": best["depth"],
            "delta_vs_baseline": best["depth"] - BASELINE_DEPTH,
        } if best else None,
        "statistics": stats,
        "results": deduped,
    }
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(merged, indent=2))

    print(f"\nMerged results written to: {out_path}")

    # Print C++ constant suggestion if new best found
    if best and best["depth"] > BASELINE_DEPTH:
        print(f"\nUpdate LtpTable.cpp:")
        print(f"  constexpr std::uint64_t kSeed1 = {best['seed1']}ULL;  "
              f"// {best.get('seed1_label', '')}")
        print(f"  constexpr std::uint64_t kSeed2 = {best['seed2']}ULL;  "
              f"// {best.get('seed2_label', '')}")


if __name__ == "__main__":
    main()
