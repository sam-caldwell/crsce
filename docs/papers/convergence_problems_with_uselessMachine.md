# CRSCE Decompression Convergence Problems: useless-machine.mp4

This note documents the behavior observed when compressing and then decompressing the sample media file
`docs/testData/useless-machine.mp4`. It captures what is working, what is not working, the tuning we tried, and
recommendations for next steps. This document is intended to help me sort out next steps.

## Summary

- ✅ Compression produces a CRSCE v1 container with a header and 83 blocks that match the expected size and layout.
- 🤷 Decompression fails on block 0 with no progress from Deterministic Elimination (DE) and Graphical Optimized Belief
  Propagation (GOBP). The solver reaches a steady state without convergence.
- 🤷 Increasing DE/GOBP iteration caps and loosening GOBP parameters does not lead to convergence for this input.

## Reproduction

Run the make target (clean, configure, lint, build, run tests, verify coverage, then run uselessMachine test):

- `make all test/uselessMachine`

This target sets the following environment variables for the run (very slow on large inputs):

- `CRSCE_DE_MAX_ITERS=20000`
- `CRSCE_GOBP_ITERS=20000`
- `CRSCE_GOBP_CONF=0.85`
- `CRSCE_GOBP_DAMP=0.20`

The `uselessTest` prints diagnostics and saves command logs under:

- `USL_LOG_DIR=build/uselessTest`
    - `compress.stdout.txt`, `compress.stderr.txt`
    - `decompress.stdout.txt`, `decompress.stderr.txt`

## Compression

- Input bytes: 2,682,901 bytes.
- Bits: 21,463,208.
- Block size (bits): 511×511 = 261,121.
- Blocks: ceil(21,463,208 / 261,121) = 83.
- Container size: header (28B) + blocks × payload (18,652B) = 28 + 83×18,652 = 1,548,144 bytes.
- Observed container size: 1,548,144 bytes (exact match).

uselessTest output includes:

- `USL_CONTAINER_VALIDATION=ok` (header parsed, block count consistent, payload sizes split correctly).

## Decompression

- Decompression is invoked via `decompress -in <container> -out <reconstructed>`.
- Failure mode on block 0:
    - DE: `Block Solver reached steady state (no progress)`.
    - GOBP: `GOBP reached steady state (no progress)`.
    - Final: `Block Solver failed` and `error: block 0 solve failed`.

A JSON failure snapshot is printed to stdout and saved to `decompress.stdout.txt`. Example values from a run:

- `total_blocks=83, blocks_attempted=1, blocks_successful=0, block_index=0`
- Phase: `end-of-iterations`, `iter=1`
- `solved=33,896` (or ~40,584 with stronger params in another run)
- `unknown_total=227,225` (or ~220,537 later)

Since 511×511 = 261,121 cells:

- ~13%–16% of cells are assigned before the solver stalls.
- ***No full solution is reached***, so cross-sum/LH verification does not run and no output is written.
    - Humorous note: You'd know this would have to happen on the Useless Machine file.

## Tuning Attempts

Tuning values were applied via environment variables (and also tried in direct CLI runs):

- Baseline (defaults): `CRSCE_DE_MAX_ITERS=200`, `CRSCE_GOBP_ITERS=300`, `CRSCE_GOBP_CONF=0.995`,
  `CRSCE_GOBP_DAMP=0.5` → No convergence.
- Stronger (in make target): `CRSCE_DE_MAX_ITERS=20000`, `CRSCE_GOBP_ITERS=20000`, `CRSCE_GOBP_CONF=0.85`,
  `CRSCE_GOBP_DAMP=0.20` → No convergence; still reaches steady state early.

Notes:

- The failure occurs on block 0. Later blocks are never attempted.
- With stronger parameters, the number of assigned cells increases slightly before stalling, but not enough to find a
  consistent full assignment.
    - I'm tempted to say this means I need to just push the parameters like Chuck Yeager and see what happens.
    - First I need more telemetry so I can see if the juice is worth the squeeze

## What’s Working

- Compression and container writing are correct (size/layout).
- Decompressor header parsing and block payload splitting are correct.
- Pre-locking logic for trailing padded bits is exercised.
- Structured failure logging works (JSON snapshot, stderr traces).
- My patience. I haven't muttered profanities or thrown anything ...yet.

## What’s Not Working

- The solver pipeline (DE followed by GOBP) fails to converge on the first block for this high-entropy input.
- Even with significantly higher iteration caps and more permissive GOBP parameters, solver progress plateaus.

## Recommendations

Short-term (for investigation):

- Add a “tuned” runner variant that exports more verbose diagnostics (e.g., `CRSCE_PRELOCK_DEBUG=1`, optional
  `CRSCE_GOBP_DEBUG=1`) and allows on/off toggles per phase from the environment.
- Dump per-iteration progress summaries (unknown totals, row/col pressure) under a guarded flag to reduce noise in
  normal runs.

Algorithmic directions:

- Multi-phase GOBP schedule:
    - Iterate phases with varying damping and confidence, e.g.,
        - Phase A: high damping (0.6), high confidence (0.995)
        - Phase B: moderate damping (0.4), moderate confidence (0.95)
        - Phase C: low damping (0.2), lower confidence (0.85)
    - Between phases, run DE sweeps and measure net progress; bail if no improvement across phases.
- Limited backtracking hooks:
    - Identify a small set of highest-impact uncertain cells (by line residuals or message magnitude).
    - Branch on a single cell (depth 1–2) with lightweight DE propagation; abandon branch early if constraints are
      violated.
    - Guard with strict time/iteration limits to avoid pathological explosion.
- Early LH-assisted checks:
    - Integrate partial LH consistency checks for rows that become nearly determined; prune states that cannot match LH.
    - This may significantly reduce the search space before the final verify step.

Safety and test strategy:

- Keep all new strategies behind environment flags (off by default) to avoid destabilizing existing green tests.
- Add focused integration tests that exercise the adaptive/passive modes without relying on the large media file.

## How to Continue

- Run: `make test/uselessMachine` to reproduce, then inspect:
    - `build/uselessTest/decompress.stderr.txt`: DE/GOBP progress traces.
    - `build/uselessTest/decompress.stdout.txt`: JSON snapshot (unknown counts, iteration, etc.).
- If you want, create a variant target with even stronger parameters and enabled debug:

```
CRSCE_DE_MAX_ITERS=50000 \
CRSCE_GOBP_ITERS=50000 \
CRSCE_GOBP_CONF=0.80 \
CRSCE_GOBP_DAMP=0.15 \
CRSCE_PRELOCK_DEBUG=1 \
make test/uselessMachine
```

Be aware that this can be very slow and may still not converge on highly entropic content without additional heuristics.

## Data Captured

The following row-completion stats shows the failure is promising. I swear the ghost of Claude Shannon is just screwing
with me now.  We are >90% complete on several rows.  That is significant because we may just need to be more aggressive.
By the power of my redneck, oilfield heritage GET A BIGGER HAMMER!

```json
{
    "step": "row-completion-stats",
    "block_index": 0,
    "min_pct": 0,
    "avg_pct": 0.134895,
    "max_pct": 0.960861,
    "full_rows": 0,
    "rows": [
        0.166341,
        0.92955,
        0.549902,
        0.960861,
        0.436399,
        0.493151,
        0.606654,
        0.504892,
        0.679061,
        0.798434,
        0.645793,
        0.594912,
        0.51272,
        0.594912,
        0.563601,
        0.571429,
        0.553816,
        0.585127,
        0.559687,
        0.624266,
        0.626223,
        0.594912,
        0.58317,
        0.536204,
        0.565558,
        0.573386,
        0.618395,
        0.637965,
        0.569472,
        0.663405,
        0.659491,
        0.589041,
        0.565558,
        0.55773,
        0.495108,
        0.592955,
        0.563601,
        0.630137,
        0.528376,
        0.504892,
        0.577299,
        0.679061,
        0.559687,
        0.518591,
        0.551859,
        0.524462,
        0.587084,
        0.559687,
        0.636008,
        0.604697,
        0.530333,
        0.569472,
        0.549902,
        0.641879,
        0.585127,
        0.571429,
        0.69863,
        0.362035,
        0.328767,
        0.739726,
        0.385519,
        0.463796,
        0.710372,
        0.273973,
        0.350294,
        0.610568,
        0.589041,
        0.585127,
        0.587084,
        0.587084,
        0.587084,
        0.587084,
        0.581213,
        0.587084,
        0.58317,
        0.577299,
        0.58317,
        0.585127,
        0.579256,
        0.577299,
        0.577299,
        0.575342,
        0.573386,
        0.575342,
        0.569472,
        0.563601,
        0.567515,
        0.569472,
        0.561644,
        0.55773,
        0.55773,
        0.563601,
        0.55773,
        0.559687,
        0.559687,
        0.555773,
        0.551859,
        0.553816,
        0.555773,
        0.547945,
        0.542074,
        0.547945,
        0.547945,
        0.540117,
        0.542074,
        0.567515,
        0.420744,
        0.35225,
        0.266145,
        0.395303,
        0.35225,
        0.377691,
        0.260274,
        0.279843,
        0.236791,
        0.172211,
        0.246575,
        0.291585,
        0.228963,
        0.268102,
        0.248532,
        0.277886,
        0.252446,
        0.101761,
        0.248532,
        0.324853,
        0.246575,
        0.18591,
        0.234834,
        0.164384,
        0.178082,
        0.311155,
        0.199609,
        0.268102,
        0.264188,
        0.213307,
        0.176125,
        0.0156556,
        0.395303,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0
    ]
}
```
