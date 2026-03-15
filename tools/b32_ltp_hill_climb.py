#!/usr/bin/env python3
"""
B.32 — LTP Table Hill-Climber.

Directly optimize the LTP table cell-line assignment array (not seeds) to push
decompressor depth beyond the 91,090 seed-search ceiling (B.26c).

Two objective modes:
  early (default): reward LTP lines with cells in rows 0..K_PLATEAU (row-concentration)
    coverage[sub][L] = count of cells on line L with row <= K_PLATEAU
    score = Σ_{sub,L} max(0, coverage[sub][L] - THRESHOLD)²   (quadratic reward)

  band: reward LTP lines with cells in the frustration band K1..K2
    coverage[sub][L] = count of cells on line L with K1 <= row <= K2
    score = Σ_{sub,L} max(0, coverage[sub][L] - THRESHOLD)²   (quadratic reward)
    K1/K2 default to K_PLATEAU+1=179 and K_PLATEAU+32=210 (the transition zone).

Swap operation: within one sub-table, pick two lines La and Lb; swap one cell from
each. Preserves the uniform-511 invariant. Only productive if the two cells straddle
the target zone boundary — filtered to avoid zero-delta swaps.

Output: LTPB binary file loadable by the C++ runtime via CRSCE_LTP_TABLE_FILE.
Periodic validation: run a timed decompressor to measure actual depth; emit to JSON log.

Usage:
    python3 tools/b32_ltp_hill_climb.py [--iters N] [--secs N] [--out PATH] [--checkpoint N]
                                         [--init PATH] [--mode {early,band}] [--k1 N] [--k2 N]
                                         [--anneal] [--T_init F] [--T_final F]

    --iters N         Total hill-climb iterations (default 10_000_000)
    --secs N          Decompress seconds per depth measurement (default 20)
    --out PATH        Output LTPB file path (default tools/b32_ltp_table.bin)
    --checkpoint N    Checkpoint + validate every N accepted swaps (default 100_000)
    --no-validate     Skip decompressor validation (hill-climb only)
    --seed N          NumPy random seed for reproducibility (default 42)
    --init PATH       Load starting LTPB instead of building from Fisher-Yates seeds.
                      Use tools/b32_best_ltp_table.bin for second-pass from best.
    --mode {early,band}
                      Objective mode (default: early).
    --k1 N            Band-mode lower bound inclusive (default 179 = K_PLATEAU+1).
    --k2 N            Band-mode upper bound inclusive (default 210).
    --anneal          Enable simulated annealing: accept downhill score moves with
                      probability exp(delta/T) where T cools geometrically.
    --T_init F        Initial SA temperature (default 2000.0).
    --T_final F       Final SA temperature (default 5.0).
    --score-cap N     Hard cap on total score (0=disabled). Uphill moves that would
                      push score above N are rejected. Prevents anti-correlation
                      overshoot. Recommended: 22000000 for B.37.
    --kick N          Apply N random unconstrained swaps before hill-climbing (ILS
                      perturbation). Escapes local optima by jumping to a different
                      landscape basin. Recommended: 500-5000.
    --deflate K       Apply K targeted deflation swaps before hill-climbing (B.38).
                      Each deflation swap moves one early-row cell from a high-coverage
                      line (coverage > --deflate_high) to a late-row cell on a
                      low-coverage line (coverage < --deflate_low).  This reduces the
                      proxy score without random destruction, bringing over-concentrated
                      tables (score >30M) back into the productive hill-climb range
                      (~20-25M).  Stop early if score drops below --deflate_target.
    --deflate_high H  Minimum coverage for a donor line in deflation (default 350).
    --deflate_low  L  Maximum coverage for a receiver line in deflation (default 250).
    --deflate_target N  Stop deflation early once score < N (default 22000000).

Format of output LTPB file (consumed by LtpTable.cpp buildFromAssignment):
    Offset  Size        Field
    0       4           magic = "LTPB"
    4       4 uint32    version = 1
    8       4 uint32    S = 511
    12      4 uint32    num_subtables = 4
    16      4*N*2       uint16 assignment[4][261121]   (sub-major order)
Total: 16 + 2,088,968 = ~2 MB.
"""

import argparse
import json
import math
import os
import pathlib
import signal
import struct
import subprocess
import sys
import tempfile
import time

try:
    import numpy as np
except ImportError:
    sys.exit("ERROR: numpy required.  Run: pip install numpy")

REPO_ROOT    = pathlib.Path(__file__).resolve().parent.parent
BUILD_DIR    = REPO_ROOT / "build" / "llvm-release"
COMPRESS     = BUILD_DIR / "compress"
DECOMPRESS   = BUILD_DIR / "decompress"
SOURCE_VIDEO = REPO_ROOT / "docs" / "testData" / "useless-machine.mp4"
LOG_PATH     = REPO_ROOT / "tools" / "b32_hill_climb_log.jsonl"

# ── LTP geometry ────────────────────────────────────────────────────────────
S          = 511
N          = S * S          # 261,121
NUM_SUB    = 6              # sub-tables in the LTPB file (all 6, matching C++ chained pool)
K_PLATEAU  = 178            # row index of current TD frontier ≈ 91,090 / 511
# Quadratic reward threshold: lines with coverage > THRESHOLD get rewarded quadratically.
# Set to K_PLATEAU (uniform expectation = 178 early cells per line) so any above-average
# concentration immediately contributes. This avoids the cold-start problem where all
# Fisher-Yates coverages start below 340 (the plan's aspirational threshold).
# Increase THRESHOLD to 250-300 later once the hill-climber has created concentrated lines.
THRESHOLD  = K_PLATEAU      # = 178; overridden by --threshold CLI flag
# Coverage ceiling: cap per-line early-row coverage at MAX_COV in the reward function.
# Prevents over-concentration: if all 511 lines go to 500+, late rows lose LTP constraints.
# B.9.2 intent: 50-100 lines with u≈60-100 at plateau entry (≈ 411-451 early cells).
# With ceiling 451: lines rewarded up to 451 early cells; beyond that, no more reward.
# The solver still uses the full assignment; the ceiling only limits the score function.
MAX_COV    = 451            # = S - 60; overridden by --max_cov CLI flag
# Hard floor on per-line early-row coverage.  Any swap that would bring the DONOR line
# below this value is rejected, regardless of score delta.  Prevents catastrophic
# depletion of late-row LTP constraints.
#
# Equilibrium analysis (FLOOR=F, MAX_COV=M, total early cells per sub = 179*511 = 91,469):
#   Max concentrated lines K s.t. K*M + (511-K)*F = 91,469
#   → K = (91,469 - 511*F) / (M - F)
#   F=120, M=451 → K = (91,469 - 61,320) / 331 = 91 lines  (B.9.2 target: 50–100)
#   F=140, M=451 → K ≈ 64 lines
#   F=  0, M=451 → K = 202 lines (too many; prior regression experiment)
FLOOR      = 0              # 0 = disabled; overridden by --floor CLI flag

# ── Simulated annealing ──────────────────────────────────────────────────────
# When enabled (--anneal), downhill score moves are accepted with probability
# exp(delta / T) where T decays geometrically from T_INIT to T_FINAL.
# Temperature calibration (for typical Δ ≈ ±250 per swap, score ~20M):
#   T=2000: P(accept|Δ=-250) ≈ 88%  (highly exploratory)
#   T=200:  P(accept|Δ=-250) ≈ 29%
#   T=50:   P(accept|Δ=-250) ≈  0.7%
#   T=5:    P(accept|Δ=-250) ≈  4e-22 (essentially greedy)
ANNEAL     = False    # overridden by --anneal CLI flag
T_INIT     = 2000.0  # initial SA temperature; overridden by --T_init
T_FINAL    = 5.0     # final SA temperature; overridden by --T_final
# Score cap: hard-reject any uphill move that would push total score above this value.
# Prevents overshoot into the anti-correlation zone (score > 25M → depth regresses).
# 0 = disabled (default).  Recommended: 22_000_000 for B.37 score-capped SA.
SCORE_CAP  = 0       # overridden by --score-cap CLI flag

# ── Objective mode ────────────────────────────────────────────────────────────
# "early": reward cells in rows 0..K_PLATEAU (original B.34 objective)
# "band":  reward cells in the frustration band K1..K2 (B.35 Lever 2)
#   K1 = K_PLATEAU+1 = 179  (first row past current frontier)
#   K2 = K_PLATEAU+32 = 210  (~4% of rows; the transition zone the solver stalls in)
#   Uniform expectation per line: (K2-K1+1) = 32 cells in this band → THRESHOLD=32
MODE = "early"   # overridden by --mode CLI flag
K1   = K_PLATEAU + 1   # 179; overridden by --k1
K2   = K_PLATEAU + 32  # 210; overridden by --k2

# ── B.26c seed constants (reproduce Fisher-Yates initialization) ─────────────
SEEDS = [
    0x435253434C545056,   # CRSCLTPV  (sub-table 1, B.26c winner)
    0x435253434C545050,   # CRSCLTPP  (sub-table 2, B.26c winner)
    0x435253434C545033,   # CRSCLTP3
    0x435253434C545034,   # CRSCLTP4
    0x435253434C545035,   # CRSCLTP5  (B.27 addition)
    0x435253434C545036,   # CRSCLTP6  (B.27 addition)
]

# LTPB magic and version
MAGIC   = b"LTPB"
VERSION = 1


# ── Fisher-Yates LCG shuffle (replicates C++ buildSubTable) ─────────────────

def lcg_next(state: int) -> int:
    """One step of the 64-bit LCG used in LtpTable.cpp."""
    return (state * 6364136223846793005 + 1442695040888963407) & 0xFFFFFFFFFFFFFFFF


def build_all_sub_tables(seeds: list) -> list:
    """
    Build all sub-table assignment arrays replicating C++ buildAllPartitions exactly.

    C++ uses a SINGLE pool vector shared across all 6 passes (chained Fisher-Yates):
      pool = [0..N-1]  (initialized once)
      for each sub-table k:
          Fisher-Yates(seed_k) applied to current pool  (pool is re-shuffled in-place)
          assignment[pool[line*S + pos]] = line  (consecutive chunk of shuffled pool)

    Each pass starts from the previous pass's pool state — sub-tables are COUPLED.
    This differs from independent Fisher-Yates (fresh pool per pass).

    Formula: j = state % (i+1)  (full 64-bit, NOT state>>33 — C++ uses modulo)
    """
    pool = list(range(N))   # mutable list; shared across all passes (chained)
    assignments = []
    for seed in seeds:
        state = seed
        for i in range(N - 1, 0, -1):
            state = lcg_next(state)
            j = int(state % (i + 1))   # C++ formula: state % (i+1)
            pool[i], pool[j] = pool[j], pool[i]
        # Assign: consecutive chunks of shuffled pool → lines 0..510
        a = np.empty(N, dtype=np.uint16)
        for line in range(S):
            base = line * S
            for pos in range(S):
                a[pool[base + pos]] = line
        assignments.append(a)
    return assignments


# ── LTPB loader (for --init) ─────────────────────────────────────────────────

def load_ltpb(path: pathlib.Path) -> list:
    """
    Load all sub-table assignments from an LTPB file.
    Returns a list of NUM_SUB np.ndarray(N,) dtype=uint16.
    Exits with error on malformed file.
    """
    data = path.read_bytes()
    if len(data) < 16:
        sys.exit(f"ERROR: {path} too small to contain LTPB header")
    magic, version, s, num_sub = struct.unpack_from("<4sIII", data, 0)
    if magic != MAGIC:
        sys.exit(f"ERROR: bad magic in {path}: {magic!r} (expected b'LTPB')")
    if s != S:
        sys.exit(f"ERROR: S={s} in {path}, expected {S}")
    expected = 16 + num_sub * N * 2
    if len(data) < expected:
        sys.exit(f"ERROR: {path} truncated: {len(data)} < {expected} bytes")

    assignments = []
    offset = 16
    for _ in range(num_sub):
        a = np.frombuffer(data[offset: offset + N * 2], dtype="<u2").copy().astype(np.uint16)
        assignments.append(a)
        offset += N * 2

    if num_sub < NUM_SUB:
        # File has fewer sub-tables than NUM_SUB; extend with fresh Fisher-Yates.
        # Note: chained pool cannot be resumed mid-way, so we build from scratch for the gap.
        print(f"  File has {num_sub} sub-tables; building {NUM_SUB - num_sub} more from seeds…")
        extra = build_all_sub_tables(SEEDS)
        for idx in range(num_sub, NUM_SUB):
            assignments.append(extra[idx])

    return assignments[:NUM_SUB]


# ── Data structure initialisation ───────────────────────────────────────────

def build_csr(assignment: np.ndarray) -> np.ndarray:
    """
    Build the CSR reverse table.  csr[line * S + pos] = flat_index of pos-th cell on line.
    Returns csr array of shape (S*S,) uint32.
    """
    csr = np.empty(N, dtype=np.uint32)
    counts = np.zeros(S, dtype=np.int32)
    for flat in range(N):
        line = int(assignment[flat])
        csr[line * S + counts[line]] = flat
        counts[line] += 1
    return csr


def build_coverage(assignment: np.ndarray) -> np.ndarray:
    """
    coverage[line] = count of cells on that line in the target zone.
    early mode: zone = rows 0..K_PLATEAU
    band  mode: zone = rows K1..K2
    row_of[flat] = flat // S.
    """
    coverage = np.zeros(S, dtype=np.int32)
    flat_arr = np.arange(N, dtype=np.uint32)
    rows = flat_arr // S
    if MODE == "band":
        in_zone = (rows >= K1) & (rows <= K2)
    else:
        in_zone = rows <= K_PLATEAU
    for flat in range(N):
        if in_zone[flat]:
            coverage[int(assignment[flat])] += 1
    return coverage


def score_from_coverage(cov: np.ndarray) -> int:
    """score = Σ_L max(0, min(cov[L], MAX_COV) - THRESHOLD)²  (capped quadratic)"""
    capped = np.minimum(cov.astype(np.int64), MAX_COV)
    above  = np.maximum(0, capped - THRESHOLD)
    return int(np.sum(above * above))


# ── LTPB serialisation ───────────────────────────────────────────────────────

def write_ltpb(path: pathlib.Path, assignments: list) -> None:
    """Write assignments[0..NUM_SUB-1] to an LTPB binary file."""
    header = struct.pack("<4sIII", MAGIC, VERSION, S, NUM_SUB)
    with open(path, "wb") as f:
        f.write(header)
        for sub in range(NUM_SUB):
            f.write(assignments[sub].astype("<u2").tobytes())
    print(f"  Wrote {path} ({path.stat().st_size:,} bytes)")


# ── Decompressor validation ──────────────────────────────────────────────────

def measure_depth(ltpb_path: pathlib.Path, secs: int) -> int:
    """Run the decompressor for secs seconds and return peak depth."""
    with tempfile.TemporaryDirectory(prefix="b32_val_") as tmp_str:
        tmp       = pathlib.Path(tmp_str)
        crsce_p   = tmp / "val.crsce"
        out_p     = tmp / "val.bin"
        events_p  = tmp / "val.jsonl"

        # Compress
        cx_env = os.environ.copy()
        cx_env["CRSCE_LTP_TABLE_FILE"]  = str(ltpb_path)
        cx_env["DISABLE_COMPRESS_DI"]   = "1"
        cx_env["CRSCE_DISABLE_GPU"]     = "1"
        cx_env["CRSCE_WATCHDOG_SECS"]   = "60"
        try:
            cx = subprocess.run(
                [str(COMPRESS), "-in", str(SOURCE_VIDEO), "-out", str(crsce_p)],
                env=cx_env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=90,
            )
        except subprocess.TimeoutExpired:
            return 0
        if cx.returncode != 0 or not crsce_p.exists():
            return 0

        # Decompress for secs seconds
        dx_env = os.environ.copy()
        dx_env["CRSCE_LTP_TABLE_FILE"]  = str(ltpb_path)
        dx_env["CRSCE_EVENTS_PATH"]     = str(events_p)
        dx_env["CRSCE_METRICS_FLUSH"]   = "1"
        dx_env["CRSCE_WATCHDOG_SECS"]   = str(secs + 15)
        proc = subprocess.Popen(
            [str(DECOMPRESS), "-in", str(crsce_p), "-out", str(out_p)],
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
        if events_p.exists():
            for line in events_p.read_text(errors="replace").splitlines():
                try:
                    d = json.loads(line).get("tags", {}).get("depth")
                    if d is not None:
                        peak = max(peak, int(d))
                except (json.JSONDecodeError, ValueError):
                    pass
    return peak


# ── Hill-climbing inner loop (one sub-table at a time) ──────────────────────

def hill_climb_one_sub(
    assignment: np.ndarray,
    csr: np.ndarray,
    coverage: np.ndarray,
    rng: np.random.Generator,
    n_iters: int,
    temperature: float = 0.0,
    current_score: int = 0,
) -> tuple:
    """
    Run n_iters hill-climb (or SA) steps on one sub-table.
    Returns (accepted_count, total_score_delta).
    assignment, csr, coverage are modified in-place.

    Objective (capped quadratic):
      reward(c) = max(0, min(c, MAX_COV) - THRESHOLD)²
      delta = reward(new_la) + reward(new_lb) - reward(old_la) - reward(old_lb)
    Ceiling MAX_COV prevents all lines from over-concentrating into early rows,
    which would strip late-row LTP constraint density.

    When temperature > 0 (SA mode), downhill moves (delta < 0) are accepted with
    probability exp(delta / temperature).  Uphill moves always apply the donor-floor
    guard; downhill SA moves bypass the floor to allow genuine exploration.

    When SCORE_CAP > 0, uphill moves that would push (current_score + delta) above
    SCORE_CAP are rejected.  This prevents overshoot into the anti-correlation zone.
    current_score must be the true total score across all sub-tables so the cap is
    evaluated globally, not per-sub-table.
    """
    S_     = S
    K_     = K_PLATEAU
    K1_    = K1
    K2_    = K2
    T_     = THRESHOLD
    M_     = MAX_COV
    F_     = FLOOR          # donor-floor: reject swaps that would bring donor below this
    N_     = N
    band_  = (MODE == "band")
    temp_  = temperature    # SA temperature (0.0 = pure greedy)
    cap_   = SCORE_CAP      # 0 = disabled
    score_ = current_score  # running total score (updated as moves are accepted)

    row_of = np.arange(N_, dtype=np.uint32) // S_  # row_of[flat] = flat // S

    accepted = 0
    score_delta = 0

    for _ in range(n_iters):
        la = int(rng.integers(0, S_))
        lb = int(rng.integers(0, S_))
        if la == lb:
            continue

        pos_a = int(rng.integers(0, S_))
        pos_b = int(rng.integers(0, S_))

        flat_a = int(csr[la * S_ + pos_a])
        flat_b = int(csr[lb * S_ + pos_b])

        r_a = int(row_of[flat_a])
        r_b = int(row_of[flat_b])
        if band_:
            zone_a = bool(K1_ <= r_a <= K2_)
            zone_b = bool(K1_ <= r_b <= K2_)
        else:
            zone_a = bool(r_a <= K_)
            zone_b = bool(r_b <= K_)
        if zone_a == zone_b:
            continue  # no coverage change

        da = int(zone_b) - int(zone_a)   # +1 or -1
        old_la = int(coverage[la])
        old_lb = int(coverage[lb])
        new_la = old_la + da
        new_lb = old_lb - da

        # Capped quadratic reward: cap at MAX_COV before squaring
        def r(c: int) -> int:
            return max(0, min(c, M_) - T_) ** 2

        delta = r(new_la) + r(new_lb) - r(old_la) - r(old_lb)

        # Acceptance decision.
        # da < 0  → la is the donor (loses a zone cell); da > 0 → lb is the donor.
        donor_new = new_la if da < 0 else new_lb
        if delta > 0:
            # Uphill: accept iff donor floor OK and score cap not exceeded.
            score_new = score_ + delta
            do_accept = (donor_new >= F_) and (cap_ == 0 or score_new <= cap_)
        elif temp_ > 0.0 and delta < 0:
            # Downhill: SA acceptance — bypass floor and cap to allow exploration.
            do_accept = float(rng.random()) < math.exp(delta / temp_)
        else:
            do_accept = False

        if do_accept:
            assignment[flat_a] = lb
            assignment[flat_b] = la
            csr[la * S_ + pos_a] = flat_b
            csr[lb * S_ + pos_b] = flat_a
            coverage[la] = new_la
            coverage[lb] = new_lb
            accepted += 1
            score_delta += delta
            score_ += delta

    return accepted, score_delta


# ── Main ─────────────────────────────────────────────────────────────────────

def main() -> None:
    global THRESHOLD, MAX_COV, FLOOR, MODE, K1, K2, ANNEAL, T_INIT, T_FINAL, SCORE_CAP  # noqa: PLW0603
    parser = argparse.ArgumentParser(description="B.32 LTP hill-climber")
    parser.add_argument("--iters",      type=int, default=10_000_000,
                        help="Total hill-climb iterations (default 10M)")
    parser.add_argument("--secs",       type=int, default=20,
                        help="Decompress seconds per depth measurement (default 20)")
    parser.add_argument("--out",        type=pathlib.Path,
                        default=REPO_ROOT / "tools" / "b32_ltp_table.bin",
                        help="Output LTPB file path")
    parser.add_argument("--checkpoint", type=int, default=100_000,
                        help="Checkpoint every N accepted swaps (default 100K)")
    parser.add_argument("--no-validate", action="store_true",
                        help="Skip decompressor validation (hill-climb only)")
    parser.add_argument("--seed",       type=int, default=42,
                        help="NumPy random seed (default 42)")
    parser.add_argument("--threshold",  type=int, default=None,
                        help="Coverage reward threshold (default: K_PLATEAU=178 for early mode, "
                             "K2-K1+1=32 for band mode = uniform expectation in the zone)")
    parser.add_argument("--max_cov",    type=int, default=MAX_COV,
                        help=f"Per-line coverage ceiling in reward (default {MAX_COV} = S-60; "
                             f"prevents late-row constraint starvation)")
    parser.add_argument("--floor",      type=int, default=FLOOR,
                        help="Hard floor on donor early-row coverage (0=disabled). "
                             "Rejects swaps that would bring the donor line below this value. "
                             "With FLOOR=120, MAX_COV=451: equilibrium ≈ 91 lines at 451, "
                             "420 lines at 120 (B.9.2 target: 50-100 concentrated lines).")
    parser.add_argument("--save-best",  type=pathlib.Path, default=None,
                        help="Path to save the LTPB file whenever depth improves. "
                             "If omitted, defaults to <out-stem>_best<ext>. "
                             "Critical: prevents the best table from being overwritten "
                             "when the run continues past the depth peak.")
    parser.add_argument("--patience",   type=int, default=0,
                        help="Stop early after N consecutive checkpoints without depth "
                             "improvement (0=disabled, run all --iters). With --checkpoint 50000 "
                             "and --patience 4, stops ~200K accepted swaps after peak.")
    parser.add_argument("--init",       type=pathlib.Path, default=None,
                        help="Load initial LTPB table from file instead of building from "
                             "Fisher-Yates seeds. Use tools/b32_best_ltp_table.bin for a "
                             "second-pass hill-climb starting from the B.34 best table.")
    parser.add_argument("--mode",       type=str, default="early",
                        choices=["early", "band"],
                        help="Objective mode: 'early' = reward rows 0..K_PLATEAU (default); "
                             "'band' = reward frustration band K1..K2 (rows ~179-210).")
    parser.add_argument("--k1",         type=int, default=K_PLATEAU + 1,
                        help=f"Band-mode lower bound inclusive (default {K_PLATEAU+1})")
    parser.add_argument("--k2",         type=int, default=K_PLATEAU + 32,
                        help=f"Band-mode upper bound inclusive (default {K_PLATEAU+32})")
    parser.add_argument("--anneal",     action="store_true",
                        help="Enable simulated annealing: accept downhill moves with "
                             "probability exp(delta/T).  T decays geometrically from "
                             "--T_init to --T_final over --iters total iterations.")
    parser.add_argument("--T_init",     type=float, default=T_INIT,
                        help=f"Initial SA temperature (default {T_INIT}).  "
                             f"At T=2000: P(accept|Δ=-250)≈88%%.")
    parser.add_argument("--T_final",    type=float, default=T_FINAL,
                        help=f"Final SA temperature (default {T_FINAL}).  "
                             f"At T=5: P(accept|Δ=-250)≈0 (essentially greedy).")
    parser.add_argument("--score-cap",  type=int, default=0,
                        help="Hard cap on total score (0=disabled).  Uphill moves that "
                             "would push total score above this value are rejected.  "
                             "Prevents overshoot into the anti-correlation zone (>25M). "
                             "Recommended: 22000000 for B.37 score-capped SA.")
    parser.add_argument("--kick",       type=int, default=0,
                        help="Apply K random (unconstrained) swaps to the loaded table "
                             "before hill-climbing.  Each kick swap picks a random sub-table, "
                             "two random lines, two random positions, and applies the swap "
                             "unconditionally (no score filter, no floor, no cap).  "
                             "Use for ILS: kick escapes local optima, then greedy hill-climb "
                             "finds a new local optimum.  Recommended kick sizes: 500-5000.  "
                             "Combine with --seed for reproducible kicks and different "
                             "perturbation trajectories.")
    parser.add_argument("--deflate",         type=int, default=0,
                        help="Apply K targeted deflation swaps before hill-climbing (B.38).  "
                             "Each swap moves an early-row cell from a high-coverage line to a "
                             "late-row cell on a low-coverage line, reducing proxy score.  "
                             "Applied after --kick if both are specified.  "
                             "Stops early if score < --deflate_target.")
    parser.add_argument("--deflate_high",    type=int, default=350,
                        help="Donor coverage threshold for deflation (default 350).")
    parser.add_argument("--deflate_low",     type=int, default=250,
                        help="Receiver coverage threshold for deflation (default 250).")
    parser.add_argument("--deflate_target",  type=int, default=22_000_000,
                        help="Stop deflation early once total score drops below this "
                             "value (default 22000000).  Set to 0 to always apply all K swaps.")
    args = parser.parse_args()
    MODE      = args.mode
    K1        = args.k1
    K2        = args.k2
    MAX_COV   = args.max_cov
    FLOOR     = args.floor
    ANNEAL     = args.anneal
    T_INIT     = args.T_init
    T_FINAL    = args.T_final
    SCORE_CAP  = args.score_cap
    # Default threshold: uniform expectation for the target zone
    if args.threshold is not None:
        THRESHOLD = args.threshold
    elif MODE == "band":
        THRESHOLD = K2 - K1 + 1   # e.g. 32 for K1=179, K2=210
    else:
        THRESHOLD = K_PLATEAU      # 178

    # Derive save-best path
    if args.save_best is None:
        stem = args.out.stem
        suffix = args.out.suffix
        save_best_path = args.out.with_name(f"{stem}_best{suffix}")
    else:
        save_best_path = args.save_best

    for p in (COMPRESS, DECOMPRESS, SOURCE_VIDEO):
        if not p.exists():
            sys.exit(f"ERROR: required path not found: {p}")

    validate = not args.no_validate
    REPO_ROOT.joinpath("tools").mkdir(parents=True, exist_ok=True)

    if MODE == "band":
        zone_desc = f"rows {K1}..{K2}  (frustration band, {K2-K1+1} rows)"
    else:
        zone_desc = f"rows 0..{K_PLATEAU}  (early band)"

    print("B.32 — LTP Hill-Climber")
    print(f"  Mode:         {MODE}  ({zone_desc})")
    print(f"  Total iters:  {args.iters:,}")
    print(f"  THRESHOLD:    {THRESHOLD}  (reward when zone-coverage > threshold)")
    print(f"  MAX_COV:      {MAX_COV}  (per-line reward ceiling; prevents late-row starvation)")
    if FLOOR > 0:
        k_eq = int((91_469 - 511 * FLOOR) / (MAX_COV - FLOOR)) if MAX_COV > FLOOR else 0
        print(f"  FLOOR:        {FLOOR}  (donor floor; equilibrium ≈ {k_eq} concentrated lines per sub)")
    print(f"  Checkpoint:   every {args.checkpoint:,} accepted swaps")
    if args.patience > 0:
        print(f"  Patience:     {args.patience} checkpoints without improvement → early stop")
    if ANNEAL:
        print(f"  Annealing:    ON  T_init={T_INIT}  T_final={T_FINAL}")
        _p_init = math.exp(-250.0 / T_INIT)
        _p_final = math.exp(-250.0 / T_FINAL) if T_FINAL > 0 else 0.0
        print(f"              P(accept|Δ=-250): {_p_init:.3f} → {_p_final:.2e}")
    else:
        print(f"  Annealing:    OFF (greedy hill-climb)")
    if SCORE_CAP > 0:
        print(f"  Score cap:    {SCORE_CAP:,}  (uphill moves rejected if score would exceed cap)")
    print(f"  Validate:     {validate}  ({args.secs}s per depth measurement)")
    if args.kick > 0:
        print(f"  Kick:         {args.kick:,}  (random ILS perturbation swaps before hill-climb)")
    if args.deflate > 0:
        print(f"  Deflate:      {args.deflate:,}  (targeted score-reducing swaps; "
              f"high>{args.deflate_high}, low<{args.deflate_low}, target<{args.deflate_target:,})")
    if args.init is not None:
        print(f"  Init from:    {args.init}  (second-pass from saved table)")
    else:
        print(f"  Init from:    Fisher-Yates seeds (B.26c)")
    print(f"  Output:       {args.out}")
    print(f"  Save-best:    {save_best_path}")
    print(f"  NumPy seed:   {args.seed}")
    print()

    rng = np.random.default_rng(args.seed)

    # ── Initialise from file or Fisher-Yates ─────────────────────────────────
    if args.init is not None:
        if not args.init.exists():
            sys.exit(f"ERROR: --init file not found: {args.init}")
        print(f"Loading initial table from {args.init}…")
        t_init = time.time()
        all_assignments = load_ltpb(args.init)
        elapsed_init = time.time() - t_init
        print(f"  Loaded {len(all_assignments)} sub-tables ({elapsed_init:.2f}s)")
    else:
        print("Initialising from B.26c Fisher-Yates (chained pool, all 6 sub-tables)…")
        t_init = time.time()
        all_assignments = build_all_sub_tables(SEEDS)  # list of 6 np.ndarray
        elapsed_init = time.time() - t_init
        print(f"  Fisher-Yates complete ({elapsed_init:.1f}s total for all 6 sub-tables)")

    assignments = []
    csrs        = []
    coverages   = []
    total_score = 0

    for sub in range(NUM_SUB):
        a   = all_assignments[sub]
        c   = build_csr(a)
        cov = build_coverage(a)
        sub_score = score_from_coverage(cov)
        total_score += sub_score
        print(f"  Sub-table {sub+1}: score={sub_score:,}  max_cov={int(np.max(cov))}")
        assignments.append(a)
        csrs.append(c)
        coverages.append(cov)

    print(f"\nInitial total score: {total_score:,}")

    # ── ILS kick: random perturbation before hill-climbing ───────────────────
    if args.kick > 0:
        print(f"\nApplying {args.kick:,} random kicks (ILS perturbation)…")
        n_applied = 0
        for _ in range(args.kick):
            sub    = int(rng.integers(0, NUM_SUB))
            la     = int(rng.integers(0, S))
            lb     = int(rng.integers(0, S))
            if la == lb:
                continue
            pos_a  = int(rng.integers(0, S))
            pos_b  = int(rng.integers(0, S))
            flat_a = int(csrs[sub][la * S + pos_a])
            flat_b = int(csrs[sub][lb * S + pos_b])
            # Apply swap unconditionally — no score or floor filter.
            assignments[sub][flat_a] = lb
            assignments[sub][flat_b] = la
            csrs[sub][la * S + pos_a] = flat_b
            csrs[sub][lb * S + pos_b] = flat_a
            # Update coverage if this swap straddles the zone boundary.
            r_a = flat_a // S
            r_b = flat_b // S
            if MODE == "band":
                zone_a = bool(K1 <= r_a <= K2)
                zone_b = bool(K1 <= r_b <= K2)
            else:
                zone_a = bool(r_a <= K_PLATEAU)
                zone_b = bool(r_b <= K_PLATEAU)
            if zone_a != zone_b:
                da = int(zone_b) - int(zone_a)
                coverages[sub][la] += da
                coverages[sub][lb] -= da
            n_applied += 1
        # Recompute total score after kick (coverage changed for zone-straddling kicks).
        total_score = sum(score_from_coverage(cov) for cov in coverages)
        print(f"  Applied {n_applied:,} kicks  post-kick score: {total_score:,}")

    # ── Deflation: targeted score-reducing swaps (B.38) ─────────────────────
    if args.deflate > 0:
        high_def    = args.deflate_high
        low_def     = args.deflate_low
        target_def  = args.deflate_target
        print(f"\nApplying up to {args.deflate:,} deflation swaps "
              f"(high>{high_def}, low<{low_def}, target score<{target_def:,})…")
        n_deflated  = 0
        n_attempts  = 0
        max_attempts = args.deflate * 100  # cap attempts; each deflation swap needs ~20-100 tries
        while (n_deflated < args.deflate and n_attempts < max_attempts
               and (target_def == 0 or total_score > target_def)):
            n_attempts += 1
            sub = int(rng.integers(0, NUM_SUB))
            # Donor: a line with coverage above high_def
            high_lines = np.where(coverages[sub] > high_def)[0]
            if len(high_lines) == 0:
                continue
            la = int(rng.choice(high_lines))
            # Early-row cells on la
            la_cells = csrs[sub][la * S:(la + 1) * S]
            early_mask = (la_cells // S) <= K_PLATEAU
            early_positions = np.where(early_mask)[0]
            if len(early_positions) == 0:
                continue
            pos_a = int(rng.choice(early_positions))
            flat_a = int(csrs[sub][la * S + pos_a])
            # Receiver: a line with coverage below low_def
            low_lines = np.where(coverages[sub] < low_def)[0]
            if len(low_lines) == 0:
                continue
            lb = int(rng.choice(low_lines))
            # Late-row cells on lb
            lb_cells = csrs[sub][lb * S:(lb + 1) * S]
            late_mask = (lb_cells // S) > K_PLATEAU
            late_positions = np.where(late_mask)[0]
            if len(late_positions) == 0:
                continue
            pos_b = int(rng.choice(late_positions))
            flat_b = int(csrs[sub][lb * S + pos_b])
            # Apply the deflation swap (always reduces coverage[la], increases coverage[lb])
            assignments[sub][flat_a] = lb
            assignments[sub][flat_b] = la
            csrs[sub][la * S + pos_a] = flat_b
            csrs[sub][lb * S + pos_b] = flat_a
            coverages[sub][la] -= 1
            coverages[sub][lb] += 1
            n_deflated += 1
            # Recompute score periodically (avoid recomputing every swap for speed)
            if n_deflated % 1000 == 0:
                total_score = sum(score_from_coverage(cov) for cov in coverages)
        total_score = sum(score_from_coverage(cov) for cov in coverages)
        print(f"  Applied {n_deflated:,} deflation swaps ({n_attempts:,} attempts)  "
              f"post-deflation score: {total_score:,}")
    print()

    # Initial checkpoint: write and (optionally) measure baseline depth
    write_ltpb(args.out, assignments)
    best_depth = 0
    best_score = total_score

    if validate:
        print(f"Baseline depth measurement ({args.secs}s)…")
        best_depth = measure_depth(args.out, args.secs)
        print(f"  Baseline depth: {best_depth}  (expected ~91,090)")
        # Always save the initial (potentially kicked) table as the starting best.
        # Without this, ILS runs where the kick itself produces the best state would
        # never write save_best_path (the checkpoint loop only fires when depth > best_depth).
        write_ltpb(save_best_path, assignments)
        print(f"    → saved initial table to {save_best_path}")

    log_record = {
        "event":     "init",
        "score":     total_score,
        "depth":     best_depth,
        "floor":     FLOOR,
        "mode":      MODE,
        "init_file": str(args.init) if args.init else None,
        "timestamp": time.time(),
    }
    with open(LOG_PATH, "a") as log:
        log.write(json.dumps(log_record) + "\n")

    # ── Hill-climbing loop ───────────────────────────────────────────────────
    total_iters       = 0
    total_accepted    = 0
    since_checkpoint  = 0
    stale_checkpoints = 0      # consecutive checkpoints without depth improvement
    checkpoint_start  = time.time()

    BATCH = 1000  # iterations per inner call

    # SA temperature schedule: geometric decay from T_INIT to T_FINAL
    current_T = T_INIT if ANNEAL else 0.0
    if ANNEAL and T_INIT > T_FINAL > 0.0:
        _n_outer  = max(1, args.iters // (BATCH * NUM_SUB))
        _T_decay  = (T_FINAL / T_INIT) ** (1.0 / _n_outer)
    else:
        _T_decay  = 1.0

    print(f"{'='*60}")
    if ANNEAL:
        print(f"Starting simulated annealing hill-climb…  T={current_T:.1f}")
    else:
        print("Starting hill-climb…")
    print(f"{'='*60}")

    while total_iters < args.iters:
        # Round-robin over sub-tables
        for sub in range(NUM_SUB):
            accepted, delta = hill_climb_one_sub(
                assignments[sub], csrs[sub], coverages[sub], rng, BATCH,
                current_T, total_score,
            )
            total_accepted  += accepted
            since_checkpoint += accepted
            total_score     += delta
        total_iters += BATCH * NUM_SUB

        # Decay SA temperature after each full round over all sub-tables
        if ANNEAL:
            current_T = max(T_FINAL, current_T * _T_decay)

        # Checkpoint
        if since_checkpoint >= args.checkpoint:
            elapsed = time.time() - checkpoint_start
            rate = since_checkpoint / max(elapsed, 1e-9)
            print(f"  iters={total_iters:>11,}  accepted={total_accepted:>10,}  "
                  f"score={total_score:>12,}  rate={rate:,.0f}/s")

            # Emit coverage stats for top-10 most-covered lines (sub 0)
            top10 = np.argsort(coverages[0])[-10:][::-1]
            top_covs = [int(coverages[0][k]) for k in top10]
            print(f"    top-10 coverage (sub1): {top_covs}")

            write_ltpb(args.out, assignments)

            if validate:
                depth = measure_depth(args.out, args.secs)
                marker = ""
                if depth > best_depth:
                    best_depth = depth
                    best_score = total_score
                    stale_checkpoints = 0
                    marker = " ← NEW BEST"
                    # Save the best-seen table to a separate file so it is never
                    # overwritten by later (regressed) states.
                    write_ltpb(save_best_path, assignments)
                    print(f"    → saved best table to {save_best_path}")
                else:
                    stale_checkpoints += 1
                T_label = f"  T={current_T:.1f}" if ANNEAL else ""
                print(f"    depth={depth}  (best={best_depth}){T_label}{marker}", flush=True)
                log_record = {
                    "event":       "checkpoint",
                    "iters":       total_iters,
                    "accepted":    total_accepted,
                    "score":       total_score,
                    "depth":       depth,
                    "best_depth":  best_depth,
                    "floor":       FLOOR,
                    "mode":        MODE,
                    "anneal":      ANNEAL,
                    "temperature": current_T,
                    "score_cap":   SCORE_CAP,
                    "stale":       stale_checkpoints,
                    "timestamp":   time.time(),
                    "top10_cov":   top_covs,
                }
            else:
                log_record = {
                    "event":       "checkpoint",
                    "iters":       total_iters,
                    "accepted":    total_accepted,
                    "score":       total_score,
                    "floor":       FLOOR,
                    "mode":        MODE,
                    "anneal":      ANNEAL,
                    "temperature": current_T,
                    "score_cap":   SCORE_CAP,
                    "timestamp":   time.time(),
                    "top10_cov":   top_covs,
                }
            with open(LOG_PATH, "a") as log:
                log.write(json.dumps(log_record) + "\n")

            since_checkpoint = 0
            checkpoint_start = time.time()

            # Early-stop: terminate if depth has not improved for patience checkpoints
            if validate and args.patience > 0 and stale_checkpoints >= args.patience:
                print(f"\n  Early stop: no depth improvement for {stale_checkpoints} "
                      f"consecutive checkpoints.")
                break

    # ── Final output ─────────────────────────────────────────────────────────
    print()
    print("=" * 60)
    print("B.32 Final Result")
    print("=" * 60)
    print(f"  Total iters:    {total_iters:,}")
    print(f"  Total accepted: {total_accepted:,}")
    print(f"  Final score:    {total_score:,}  (initial: {best_score:,})")
    if validate:
        print(f"  Best depth:     {best_depth}  (baseline ~91,090)")
        delta = best_depth - 91_090
        sign = "+" if delta >= 0 else ""
        print(f"  vs B.26c:       {sign}{delta}")
    print(f"  Output file:    {args.out}  (current/final state)")
    print(f"  Best-seen file: {save_best_path}  (best depth table)")
    write_ltpb(args.out, assignments)

    if validate:
        print(f"\nFinal depth measurement ({args.secs}s)…")
        final_depth = measure_depth(args.out, args.secs)
        print(f"  Final depth: {final_depth}")

    with open(LOG_PATH, "a") as log:
        log.write(json.dumps({
            "event":       "final",
            "iters":       total_iters,
            "accepted":    total_accepted,
            "score":       total_score,
            "best_depth":  best_depth,
            "floor":       FLOOR,
            "mode":        MODE,
            "anneal":      ANNEAL,
            "T_init":      T_INIT,
            "T_final":     T_FINAL,
            "score_cap":   SCORE_CAP,
            "init_file":   str(args.init) if args.init else None,
            "timestamp":   time.time(),
        }) + "\n")
    print(f"\nLog written to: {LOG_PATH}")


if __name__ == "__main__":
    main()
