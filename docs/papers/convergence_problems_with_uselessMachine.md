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
with me now. We are >90% complete on several rows. That is significant because we may just need to be more aggressive.
By the power of my redneck, oilfield heritage GET A BIGGER HAMMER!

```text
{
    "step":"row-completion-stats",
    "block_index":0,
    "min_pct":0,
    "avg_pct":0.134895,
    "max_pct":0.960861,
    "full_rows":0,
    "rows":[
        0.166341,0.92955,0.549902,0.960861,0.436399,0.493151,0.606654,0.504892,0.679061,0.798434,0.645793,
        0.594912,0.51272,0.594912,0.563601,0.571429,0.553816,0.585127,0.559687,0.624266,0.626223,0.594912,0.58317,
        0.536204,0.565558,0.573386,0.618395,0.637965,0.569472,0.663405,0.659491,0.589041,0.565558,0.55773,0.495108,
        0.592955,0.563601,0.630137,0.528376,0.504892,0.577299,0.679061,0.559687,0.518591,0.551859,0.524462,0.587084,
        0.559687,0.636008,0.604697,0.530333,0.569472,0.549902,0.641879,0.585127,0.571429,0.69863,0.362035,0.328767,
        0.739726,0.385519,0.463796,0.710372,0.273973,0.350294,0.610568,0.589041,0.585127,0.587084,0.587084,0.587084,
        0.587084,0.581213,0.587084,0.58317,0.577299,0.58317,0.585127,0.579256,0.577299,0.577299,0.575342,0.573386,
        0.575342,0.569472,0.563601,0.567515,0.569472,0.561644,0.55773,0.55773,0.563601,0.55773,0.559687,0.559687,
        0.555773,0.551859,0.553816,0.555773,0.547945,0.542074,0.547945,0.547945,0.540117,0.542074,0.567515,0.420744,
        0.35225,0.266145,0.395303,0.35225,0.377691,0.260274,0.279843,0.236791,0.172211,0.246575,0.291585,0.228963,
        0.268102,0.248532,0.277886,0.252446,0.101761,0.248532,0.324853,0.246575,0.18591,0.234834,0.164384,0.178082,
        0.311155,0.199609,0.268102,0.264188,0.213307,0.176125,0.0156556,0.395303,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    ]
}
```

GOBP is reaching a state of zero progress. We need to give it a chance to converge by stimulating new activity.

## Option: Let's try being more aggressive!

Aggressive GOBP parameters:

- Yes; push further and measure:
- CRSCE_DE_MAX_ITERS=60000
- CRSCE_GOBP_ITERS=100000
- CRSCE_GOBP_CONF=0.70–0.75
- CRSCE_GOBP_DAMP=0.05–0.10
- CRSCE_GOBP_MULTIPHASE=1
- CRSCE_BACKTRACK=1, CRSCE_BT_DE_ITERS=300

### Outcome

- No change.

## Option: Annealing schedule

- Annealing schedule: vary damping/confidence in phases (already added: CRSCE_GOBP_MULTIPHASE=1), but you can go more
  extreme by lowering final‑phase confidence and damping.
- Current multiphase schedule (hard-coded) is:
    - Phase 1: damping 0.60, confidence 0.995
    - Phase 2: damping 0.40, confidence 0.95
    - Phase 3: damping 0.20, confidence 0.85
    - Phase 3: damping 0.20, confidence 0.85

> When CRSCE_GOBP_MULTIPHASE=1 (default now), the env vars CRSCE_GOBP_CONF/DAMP do not affect the phase values; they
> only apply to the single‑phase path. So even though the make target sets CONF=0.72 and DAMP=0.08, multiphase ignored
> those and used the fixed 0.85/0.20 in the final phase.

> We have wired env overrides for multi-phase so we can try more extreme annealing right away, e.g.:
> - CRSCE_GOBP_PHASE1_CONF/DAMP
> - CRSCE_GOBP_PHASE2_CONF/DAMP
> - CRSCE_GOBP_PHASE3_CONF/DAMP

### Outcome

This produced the same outcome

## Option: Aggressive annealing

- Env overrides:
    - CRSCE_GOBP_PHASE1_CONF=0.98 CRSCE_GOBP_PHASE1_DAMP=0.40
    - CRSCE_GOBP_PHASE2_CONF=0.90 CRSCE_GOBP_PHASE2_DAMP=0.20
    - CRSCE_GOBP_PHASE3_CONF=0.70 CRSCE_GOBP_PHASE3_DAMP=0.05
    - Optionally adjust per‑phase iters:
    - CRSCE_GOBP_PHASE1_ITERS=20000 CRSCE_GOBP_PHASE2_ITERS=30000 CRSCE_GOBP_PHASE3_ITERS=50000
- Keep the aggressive global caps/backtracking already in the make target (or set them explicitly).

### Outcome

```text
{
  "step":"row-completion-stats",
  "block_index":0,
  "parameters":{
    "CRSCE_DE_MAX_ITERS":60000,
    "CRSCE_GOBP_ITERS":100000,
    "CRSCE_GOBP_CONF":0.995,
    "CRSCE_GOBP_DAMP":0.5,
    "CRSCE_GOBP_MULTIPHASE":true,
    "CRSCE_BACKTRACK":true,
    "CRSCE_BT_DE_ITERS":300
  },
  "gobp_phases":{
    "conf":[0.98,0.9,0.7],
    "damp":[0.4,0.2,0.05],
    "iters":[20000,30000,50000]
  },
  "min_pct":0,
  "avg_pct":17.8002,
  "max_pct":97.2603,
  "full_rows":0,
  "rows":[
      41.683,94.9119,67.7104,97.2603,59.0998,63.6008,72.0157,64.1879,77.1037,85.7143,74.7554,71.2329,64.9706,
      71.2329,69.0802,69.863,67.9061,70.4501,67.9061,73.5812,73.5812,71.6243,70.0587,66.7319,69.4716,69.6673,72.7984,
      74.364,69.4716,76.3209,75.9295,71.2329,69.0802,68.4932,63.7965,71.4286,69.6673,73.9726,66.9276,65.362,70.6458,
      77.4951,69.0802,66.1448,68.4932,66.5362,71.4286,69.4716,74.7554,72.7984,66.7319,70.0587,68.8845,75.3425,71.2329,
      70.4501,79.4521,55.773,54.0117,81.9961,57.5342,62.818,80.0391,51.272,55.3816,73.3855,71.6243,71.82,71.4286,71.82,
      71.82,71.4286,71.6243,72.0157,71.82,71.0372,71.2329,71.6243,71.2329,71.0372,71.0372,71.0372,71.0372,71.0372,70.4501,
      70.4501,70.6458,70.4501,70.0587,70.4501,70.0587,70.2544,70.2544,69.6673,69.6673,69.863,69.6673,69.2759,69.6673,
      69.4716,68.8845,69.0802,68.8845,68.6888,68.2975,70.0587,59.0998,55.5773,51.0763,57.9256,54.9902,56.5558,50.6849,
      51.272,48.9237,45.5969,49.3151,51.272,48.5323,50.2935,49.9022,51.272,49.9022,42.2701,49.5108,54.4031,49.5108,
      47.1624,49.1194,45.7926,46.771,53.6204,47.5538,51.272,51.0763,48.1409,46.3796,36.0078,59.4912,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,0,0,0.195695
  ]
}
```

#### What improved

- avg_pct: 13.49% → 17.80% (+4.31 pts)  This is real progress!
- max_pct: 96.09% → 97.26% This is real progress!
- Rows: the “front segment” moved up; many rows now cluster around 70–75% (previously ~58%). Full rows are still 0, and
  there’s still a long tail of 0%.

## Option: Continue building from our Aggressive Annealing

Run this...

```
make build test/uselessMachine PRESET=llvm-debug \
    CRSCE_GOBP_PHASE1_CONF=0.99 \
    CRSCE_GOBP_PHASE1_DAMP=0.50 \
    CRSCE_GOBP_PHASE1_ITERS=10000 \
    CRSCE_GOBP_PHASE2_CONF=0.85 \
    CRSCE_GOBP_PHASE2_DAMP=0.15 \
    CRSCE_GOBP_PHASE2_ITERS=20000 \
    CRSCE_GOBP_PHASE3_CONF=0.60 \
    CRSCE_GOBP_PHASE3_DAMP=0.02 \
    CRSCE_GOBP_PHASE3_ITERS=70000 \
    CRSCE_BT_DE_ITERS=600
```

### Outcome

```text
{
  "step":"row-completion-stats",
  "block_index":0,
  "parameters":{
    "CRSCE_DE_MAX_ITERS":60000,
    "CRSCE_GOBP_ITERS":100000,
    "CRSCE_GOBP_CONF":0.995,
    "CRSCE_GOBP_DAMP":0.5,
    "CRSCE_GOBP_MULTIPHASE":true,
    "CRSCE_BACKTRACK":true,
    "CRSCE_BT_DE_ITERS":600
  },
  "gobp_phases":{
    "conf":[0.99,0.85,0.6],
    "damp":[0.5,0.15,0.02],
    "iters":[10000,20000,70000]
  },
  "min_pct":0,
  "avg_pct":20.3523,
  "max_pct":97.6517,
  "full_rows":0,
  "rows":[55.9687,95.499,73.3855,97.6517,67.5147,70.4501,76.7123,71.0372,80.0391,87.6712,78.6693,76.1252,71.82,
  75.9295,74.364,75.1468,73.7769,75.7339,73.9726,77.8865,77.6908,76.5166,76.3209,73.3855,75.1468,75.9295,77.8865,
  78.4736,75.3425,80.4305,79.8434,76.3209,74.9511,74.9511,72.407,76.908,75.9295,78.865,74.364,73.3855,76.1252,
  80.8219,75.3425,73.7769,75.3425,73.7769,76.908,75.9295,79.4521,78.0822,74.1683,76.3209,75.9295,79.4521,77.4951,
  76.7123,82.3875,69.4716,68.1018,84.5401,70.8415,73.1898,82.7789,64.775,68.6888,78.0822,76.908,77.1037,77.2994,
  77.1037,77.4951,77.1037,77.4951,77.4951,77.8865,76.7123,77.1037,77.1037,77.1037,77.8865,77.1037,76.7123,77.1037,
  77.6908,76.908,76.1252,77.1037,77.1037,75.9295,76.1252,76.3209,76.1252,76.3209,75.5382,75.1468,74.7554,75.1468,
  74.9511,74.364,74.364,74.1683,74.7554,74.1683,73.5812,73.5812,74.7554,64.9706,61.6438,58.1213,63.9922,61.2524,
  62.4266,56.7515,57.1429,54.9902,51.8591,54.9902,57.1429,54.4031,55.5773,55.3816,56.9472,56.1644,47.9452,55.5773,
  59.6869,55.773,53.4247,55.1859,51.6634,52.6419,59.2955,53.4247,57.7299,56.7515,54.7945,52.6419,45.0098,65.1663,
  3.7182,3.13112,3.5225,2.15264,5.87084,4.50098,4.69667,4.89237,1.95695,4.30528,6.65362,2.73973,3.5225,3.7182,2.34834,
  2.15264,2.34834,1.56556,0.587084,1.56556,0.587084,2.34834,0.195695,0.195695,2.54403,0.391389,0.587084,0,5.87084,
  1.36986,1.76125,0.391389,1.36986,0.391389,0.587084,2.93542,1.17417,0.782779,1.17417,0.782779,0.391389,3.5225,
  0.978474,0.978474,3.91389,0.782779,0.978474,0.195695,0.391389,5.67515,0.782779,0.587084,0.587084,3.7182,2.34834,
  2.34834,1.95695,0.587084,0.391389,0,2.54403,5.08806,0.195695,0.391389,0.195695,0.978474,0.391389,0.978474,7.82779,
  3.13112,0.391389,5.28376,0.195695,0,0.195695,0.391389,0,0.978474,3.5225,1.76125,0.391389,0.587084,2.93542,5.28376,
  0.391389,0.195695,0,0,2.93542,0.195695,0,0.195695,0.195695,9.39335,0,0.587084,0,0.391389,0,1.36986,2.15264,0.587084,
  3.32681,0.195695,0,0,0,0,2.34834,0,1.95695,0.587084,0.195695,0.195695,1.17417,3.32681,0.782779,0.391389,2.34834,
  1.76125,0,0.195695,0.587084,0.587084,0.195695,0.782779,0,1.17417,0,1.17417,0.391389,3.91389,0.782779,0,0,1.56556,
  2.15264,0.391389,0.587084,0.587084,0.782779,3.13112,0.782779,1.95695,0.587084,0.391389,0.978474,5.67515,0.391389,
  0.195695,4.50098,0.782779,3.5225,0.391389,0.391389,0.391389,0.978474,0.195695,9.39335,3.13112,0,0.195695,0.978474,
  0.587084,0.978474,0.195695,0.782779,0.587084,0.195695,0.195695,0.978474,0.391389,3.32681,0.391389,7.04501,1.56556,
  1.17417,1.76125,2.54403,0.391389,2.93542,1.36986,9.78474,0,0,0.587084,0.195695,1.56556,0.195695,0,2.34834,0.782779,
  1.36986,2.54403,0.391389,1.36986,0.391389,0.391389,1.17417,0.195695,0.978474,8.02348,3.13112,3.7182,0.782779,
  0.195695,0.587084,1.36986,0.391389,2.73973,0.195695,0,1.17417,0,0.391389,1.56556,0.195695,2.15264,0.782779,0.391389,
  0,0.195695,0,0,1.36986,0.782779,0.391389,1.36986,0.587084,0.391389,0.782779,0.587084,2.15264,0,2.54403,0,0,0.978474,
  0,1.36986,0.587084,1.36986,0,0,2.15264,0,0.195695,0,0,0,0,0,0,0,1.17417,1.17417,3.91389,1.56556,0.782779,0.587084,
  0.195695,0,0,0,0,0,0,0,2.34834,0.195695,0,1.95695,0.391389,0.195695,0.391389,0,0,0,2.34834,0.782779,0.195695,0.195695,
  0,0.195695,0,0.195695,0.782779,0.195695,1.56556,0.782779,0,0,0,1.36986,0,0.195695,0,0.391389,0,1.95695,0,0.195695,
  1.36986,0,0,0.391389,0,0,0,0,0.195695,0,0.587084,0,0.195695,0.978474,0.195695,0,0,2.73973,0.195695,0,1.56556,
  0.195695,2.54403,0,0.391389,1.56556,0.391389,1.56556,3.32681,1.17417,1.17417,0.391389,0.195695,1.56556,0,5.67515,
  0.782779,0.978474,1.17417,1.95695,0.782779,0.195695,0.391389,1.76125,1.56556,1.36986,0.391389,5.08806,0.978474,
  1.36986,0.782779,1.17417,1.95695,0,0.782779,1.36986,1.36986,1.76125,2.73973,2.34834,1.56556,6.45793,1.36986,2.34834,
  3.5225,0.782779,0.978474,0.978474,4.50098,3.13112
  ]
}
```

#### What improved?

The overrides did help:

- Used schedule (from gobp_phases): conf=[0.99, 0.85, 0.60], damp=[0.50, 0.15, 0.02], iters=[10k, 20k, 70k],
  BT_DE_ITERS=600.
- Improvement: avg_pct 17.80 → 20.35 (+2.55 pts), max_pct 97.26 → 97.65, still 0 full rows.
- Shape: many rows now sit in the 74–83% band; long tail still ~0% though there’s a small “sprinkle” of low
  single‑digit completions near the end (so we did propagate a bit further).

## Options: More tweaks to annealing

```text
{
  "step":"row-completion-stats",
  "block_index":0,
  "parameters":{
    "CRSCE_DE_MAX_ITERS":60000,
    "CRSCE_GOBP_ITERS":100000,
    "CRSCE_GOBP_CONF":0.995,
    "CRSCE_GOBP_DAMP":0.5,
    "CRSCE_GOBP_MULTIPHASE":true,
    "CRSCE_BACKTRACK":true,
    "CRSCE_BT_DE_ITERS":600
  },
  "gobp_phases":{
    "conf":[0.99,0.85,0.55],
    "damp":[0.5,0.15,0.02],
    "iters":[1000,10000,89000]
  },
  "min_pct":0,
  "avg_pct":26.153,
  "max_pct":98.2387,
  "full_rows":0,
  "rows":[
  66.1448,96.8689,79.2564,98.2387,74.364,76.3209,81.8004,77.1037,85.3229,90.998,83.5616,81.8004,77.4951,
  81.2133,80.0391,80.4305,79.4521,81.2133,79.0607,83.5616,82.5832,81.8004,81.2133,79.4521,80.2348,81.8004,82.5832,
  83.3659,79.8434,84.9315,84.3444,81.409,80.0391,80.2348,77.6908,81.8004,81.0176,83.1703,79.6477,79.0607,80.8219,
  85.7143,80.6262,79.2564,80.4305,79.2564,81.409,81.0176,84.3444,83.1703,79.6477,81.8004,80.8219,84.3444,82.5832,
  81.8004,86.3014,75.7339,75.3425,88.0626,76.5166,78.6693,86.3014,73.3855,74.7554,82.5832,81.6047,81.6047,81.409,
  81.9961,81.6047,81.409,81.409,81.2133,81.8004,81.2133,81.409,81.409,81.2133,81.409,81.2133,81.2133,81.9961,81.2133,
  80.8219,81.0176,81.0176,81.409,80.4305,80.8219,80.4305,81.0176,80.6262,79.8434,80.2348,80.0391,80.2348,79.8434,
  80.0391,80.2348,79.2564,79.6477,79.8434,79.0607,79.0607,79.8434,74.7554,72.2114,69.6673,72.6027,71.4286,72.6027,
  68.4932,67.319,65.362,63.0137,64.3836,65.9491,63.2094,63.2094,63.4051,63.9922,63.2094,54.9902,61.4481,64.3836,
  62.2309,59.0998,61.2524,58.317,59.0998,64.3836,59.6869,62.818,62.6223,60.4697,58.9041,51.0763,69.4716,27.9843,
  29.9413,30.9198,30.137,30.9198,28.7671,28.5714,28.3757,24.0705,27.0059,29.1585,24.2661,23.8748,24.0705,23.6791,
  21.9178,22.5049,17.2211,19.1781,21.3307,18.9824,21.135,17.0254,17.8082,20.1566,16.047,13.6986,14.09,23.092,16.4384,
  14.6771,12.1331,13.8943,8.80626,10.1761,15.4599,11.7417,9.39335,10.5675,7.63209,9.58904,13.3072,4.89237,5.47945,
  15.2642,6.65362,9.00196,6.26223,7.4364,17.2211,5.87084,7.82779,3.91389,11.9374,10.7632,11.3503,9.98043,5.87084,
  4.30528,4.50098,11.7417,10.1761,3.32681,7.4364,4.10959,8.02348,3.7182,8.41487,16.8297,11.9374,4.10959,14.8728,
  4.69667,5.47945,6.84932,8.02348,4.89237,9.98043,13.6986,11.3503,4.30528,10.5675,12.5245,15.4599,5.67515,4.89237,
  5.08806,3.91389,13.8943,9.19765,6.06654,6.26223,8.41487,19.5695,3.91389,9.00196,3.5225,5.47945,4.69667,10.3718,
  11.546,10.5675,13.1115,3.32681,4.89237,6.45793,4.69667,6.06654,13.1115,5.28376,11.1546,8.80626,5.67515,4.89237,
  9.58904,11.1546,7.82779,4.30528,9.39335,10.7632,3.91389,7.63209,8.21918,6.65362,5.67515,6.26223,3.32681,7.4364,
  4.50098,9.58904,3.91389,11.9374,8.80626,3.91389,4.30528,7.82779,8.02348,2.15264,5.08806,4.69667,7.2407,11.546,
  5.47945,9.58904,4.69667,4.69667,7.2407,12.9159,6.06654,4.89237,10.7632,5.67515,10.3718,2.15264,5.67515,6.45793,
  5.47945,4.10959,16.2427,10.3718,2.34834,2.93542,6.84932,5.87084,6.26223,3.5225,4.89237,5.47945,1.95695,1.95695,
  5.67515,2.54403,9.00196,2.34834,13.1115,5.87084,6.65362,6.65362,9.00196,4.50098,9.39335,6.65362,16.047,2.73973,
  2.15264,4.10959,0.782779,5.08806,0.978474,1.36986,7.63209,5.47945,5.08806,6.45793,0.978474,4.10959,0.195695,1.56556,
  4.50098,0.587084,3.5225,13.3072,8.41487,9.00196,3.32681,0.391389,1.95695,4.30528,2.54403,8.80626,0.587084,0,5.47945,
  0,3.5225,7.63209,0.391389,6.84932,5.08806,3.5225,2.34834,3.7182,0,0.391389,5.08806,3.5225,1.17417,4.50098,2.34834,
  2.73973,3.13112,1.76125,7.2407,0.391389,7.04501,0.782779,0,5.08806,0.391389,5.28376,4.69667,5.47945,0.391389,
  0.587084,7.04501,0,3.5225,0.195695,0,0.587084,0,0.587084,0,0.782779,6.06654,5.67515,8.80626,5.67515,3.7182,4.50098,
  0,0.782779,3.7182,0.391389,0,0.391389,0,0.195695,6.65362,0,0.391389,7.04501,0.782779,0.391389,1.56556,0.195695,0,
  0.978474,8.80626,5.08806,3.5225,3.13112,2.15264,4.50098,0,3.91389,5.08806,1.76125,6.26223,5.47945,2.93542,4.30528,
  1.76125,5.87084,0,2.34834,0.978474,2.93542,0.978474,5.87084,2.54403,7.4364,3.7182,1.17417,2.93542,4.10959,4.89237,
  1.56556,3.91389,3.5225,6.84932,4.89237,4.10959,2.34834,5.08806,8.41487,1.95695,4.10959,4.10959,7.04501,5.87084,
  5.08806,9.19765,4.50098,5.47945,3.7182,1.95695,8.41487,6.06654,3.91389,10.9589,3.13112,7.82779,5.28376,5.87084,
  8.21918,5.28376,13.3072,2.93542,1.95695,4.50098,9.00196,7.63209,3.32681,5.47945,8.61057,4.69667,3.32681,4.69667,
  9.00196,3.7182,3.32681,7.04501,8.02348,5.08806,6.06654,2.93542,8.02348,7.82779,3.91389,10.1761,4.30528,3.5225,
  10.1761,8.41487,9.19765,6.06654,7.04501,4.30528,7.63209,8.61057,6.06654
  ]
}
```

### Outcome

- Uh...that's a significant improvement!
    - gobp_phases: conf=[0.99, 0.85, 0.55], damp=[0.50, 0.15, 0.02], iters=[1k, 10k, 89k]; BT_DE_ITERS=600.
        - avg_pct: 17.80 → 26.15 (+8.35 pts), max_pct: 97.65 → 98.24, still 0 full rows.
        - Shape:
            - many rows now in 80–87% band;
            - a lot more low single‑digit progress appears in the tail.
            - This is exactly what we want to see from a stronger Phase 3.

## Option: More COWBELL:

- Go even greedier in Phase 3:
    - Phase 1: conf=0.99, damp=0.50, iters=500
    - Phase 2: conf=0.80, damp=0.10, iters=4,500
    - Phase 3: conf=0.45–0.50, damp=0.01–0.02, iters=95,000
    - Keep CRSCE_BT_DE_ITERS=1200 (or 2000) to consolidate harder after backtracks.

```text
make build test/uselessMachine \
    PRESET=llvm-debug \
    CRSCE_GOBP_PHASE1_CONF=0.99 \
    CRSCE_GOBP_PHASE1_DAMP=0.50 \
    CRSCE_GOBP_PHASE1_ITERS=500 \
    CRSCE_GOBP_PHASE2_CONF=0.80 CRSCE_GOBP_PHASE2_DAMP=0.10 \
    CRSCE_GOBP_PHASE2_ITERS=4500 \
    CRSCE_GOBP_PHASE3_CONF=0.48 \
    CRSCE_GOBP_PHASE3_DAMP=0.01 \
    CRSCE_GOBP_PHASE3_ITERS=95000 \
    CRSCE_BT_DE_ITERS=1200
```

### Outcome:

- <span style="color:red">**BINGO!  100% on some cells**</span>

```text
{
  "step":"row-completion-stats",
  "block_index":0,
  "parameters":{
    "CRSCE_DE_MAX_ITERS":60000,
    "CRSCE_GOBP_ITERS":100000,
    "CRSCE_GOBP_CONF":0.995,
    "CRSCE_GOBP_DAMP":0.5,
    "CRSCE_GOBP_MULTIPHASE":true,
    "CRSCE_BACKTRACK":true,
    "CRSCE_BT_DE_ITERS":1200
  },
  "gobp_phases":{
    "conf":[0.99,0.8,0.48],
    "damp":[0.5,0.1,0.01],
    "iters":[500,4500,95000]
  },
  "min_pct":0,
  "avg_pct":44.618,
  "max_pct":100,
  "full_rows":227,
  "rows":[
  100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,
  100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,
  100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,
  100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,
  100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,
  100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,
  100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,
  100,100,100,100,100,99.8043,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
  ]
}
```

- Result highlights
    - `avg_pct`: 44.62% (up from 26.15%).
    - `max_pct`: 100%.
    - `full_rows`: 227 rows fully solved.
    - Phase schedule used: `conf=[0.99, 0.80, 0.48]`, `damp=[0.50, 0.10, 0.01]`, `iters=[500, 4500, 95000]`.
    - Strong consolidation after backtracks: `CRSCE_BT_DE_ITERS=1200`.

- What this means
    - Annealing plus heavier consolidation pushed a large prefix of rows to 100%.
    - We’ve crossed a critical threshold where prefix‑LH gating can accelerate the rest:
        - As rows at the top are fully solved, verify the LH chain prefix; if valid, we can trust those rows and focus
          effort on the remaining tail (reduces the search space).
        - If the prefix fails, we can immediately restart/backtrack out of a bad basin (rather than sinking cycles).
    - Next steps I will be implementing:
        - Prefix‑LH gating (solver‑only)...'cause cryptography is cool:
            - `CRSCE_LH_PREFIX_GATE=1`
            - During GOBP/DE sweeps, detect the largest contiguous solved prefix of rows; verify against LH. If it
              fails, trigger a restart/backtrack; if it passes, lock in that prefix aggressively and continue.
            - Restarts + perturbation (plateau escape):
                - `CRSCE_GOBP_RESTARTS=3` (or 5)
                - `CRSCE_GOBP_PERTURB=0.01` (small random jitter to beliefs at restart)
            - Focused row completion:
                - When a row hits ≥99%, run a localized, deeper DE/backtrack loop targeting just its unknowns to tip it
                  to 100%.

### Outcome

- After all the above, we are stuck at 226 rows solved of block 0.
- Because of hash chaining, we have to solve the CSM top to bottom. Was hash chaining really a bad idea?

## Option: Shake, Rattle and Roll

- Add a fourth phase to GOBP with a reheat/polish to shake loose from the plateaus.

### Outcome:

```text
% cat /Users/samcaldwell/git/sam-caldwell/crsce/build/uselessTest/row_completion_stats.log
{
  "step":"row-completion-stats",
  "block_index":0,
  "parameters":{
    "CRSCE_DE_MAX_ITERS":60000,
    "CRSCE_GOBP_ITERS":100000,
    "CRSCE_GOBP_CONF":0.995,
    "CRSCE_GOBP_DAMP":0.5,
    "CRSCE_GOBP_MULTIPHASE":true,
    "CRSCE_BACKTRACK":true,
    "CRSCE_BT_DE_ITERS":1200,
    "kFocusMaxSteps":48,
    "kFocusBtIters":8000,
    "kRestarts":8,
    "kPerturb":0.0125,
    "kPolishShakes":3,
    "kPolishShakeJitter":0.005
  },
  "restarts":[
    {"restart_index":0,"prefix_rows":0,"unknown_total":208700,"action":"polish-shake"},
    {"restart_index":0,"prefix_rows":0,"unknown_total":208700,"action":"polish-shake"},
    {"restart_index":0,"prefix_rows":0,"unknown_total":208700,"action":"polish-shake"},
    {"restart_index":1,"prefix_rows":0,"unknown_total":208700,"action":"polish-shake"},
    {"restart_index":1,"prefix_rows":0,"unknown_total":208700,"action":"polish-shake"},
    {"restart_index":1,"prefix_rows":0,"unknown_total":208700,"action":"polish-shake"},
    {"restart_index":2,"prefix_rows":0,"unknown_total":208700,"action":"polish-shake"},
    {"restart_index":2,"prefix_rows":0,"unknown_total":208700,"action":"polish-shake"},
    {"restart_index":2,"prefix_rows":0,"unknown_total":208700,"action":"polish-shake"},
    {"restart_index":3,"prefix_rows":0,"unknown_total":208714,"action":"polish-shake"},
    {"restart_index":3,"prefix_rows":0,"unknown_total":208714,"action":"polish-shake"},
    {"restart_index":3,"prefix_rows":0,"unknown_total":208714,"action":"polish-shake"},
    {"restart_index":4,"prefix_rows":0,"unknown_total":208714,"action":"polish-shake"},
    {"restart_index":4,"prefix_rows":0,"unknown_total":208714,"action":"polish-shake"},
    {"restart_index":4,"prefix_rows":0,"unknown_total":208714,"action":"polish-shake"},
    {"restart_index":5,"prefix_rows":0,"unknown_total":208706,"action":"polish-shake"},
    {"restart_index":5,"prefix_rows":0,"unknown_total":208706,"action":"polish-shake"},
    {"restart_index":5,"prefix_rows":0,"unknown_total":208706,"action":"polish-shake"},
    {"restart_index":6,"prefix_rows":0,"unknown_total":208712,"action":"polish-shake"},
    {"restart_index":6,"prefix_rows":0,"unknown_total":208712,"action":"polish-shake"},
    {"restart_index":6,"prefix_rows":0,"unknown_total":208712,"action":"polish-shake"},
    {"restart_index":7,"prefix_rows":0,"unknown_total":208712,"action":"polish-shake"},
    {"restart_index":7,"prefix_rows":0,"unknown_total":208712,"action":"polish-shake"},
    {"restart_index":7,"prefix_rows":0,"unknown_total":208712,"action":"polish-shake"},
    {"restart_index":8,"prefix_rows":0,"unknown_total":208713,"action":"polish-shake"},
    {"restart_index":8,"prefix_rows":0,"unknown_total":208713,"action":"polish-shake"},
    {"restart_index":8,"prefix_rows":0,"unknown_total":208713,"action":"polish-shake"}
  ],
  "gobp_phases":{
    "conf":[0.995,0.8,0.9,0.6],
    "damp":[0.5,0.1,0.35,0],
    "iters":[500,3500,7000,89000]
  },
  "min_pct":0,
  "avg_pct":20.0704,
  "max_pct":97.6517,
  "full_rows":0,
  "rows":[54.2074,95.499,71.82,97.6517,65.7534,68.8845,74.7554,69.0802,79.4521,87.2798,77.2994,74.5597,69.6673,74.5597,
  72.7984,72.7984,71.2329,73.7769,72.0157,76.3209,77.1037,75.3425,74.1683,71.6243,72.7984,73.9726,76.3209,77.6908,
  72.9941,79.0607,78.2779,74.1683,73.7769,73.5812,70.8415,74.7554,73.9726,76.908,71.82,70.6458,74.364,79.6477,
  74.1683,71.0372,73.1898,72.407,75.5382,73.9726,77.6908,76.908,72.9941,74.1683,73.9726,78.4736,75.1468,74.364,
  81.6047,66.3405,64.775,83.953,67.9061,70.0587,82.3875,61.6438,65.1663,76.7123,76.908,76.7123,75.7339,75.7339,
  75.3425,76.3209,75.9295,75.9295,75.5382,75.9295,75.7339,74.9511,75.7339,76.1252,76.1252,76.3209,76.1252,75.9295,
  75.9295,75.3425,75.7339,75.3425,74.9511,74.9511,74.5597,75.3425,74.7554,74.9511,75.1468,75.1468,75.3425,74.7554,
  75.3425,74.7554,74.364,74.1683,73.9726,73.7769,73.7769,75.1468,66.1448,62.6223,58.1213,64.1879,61.6438,62.818,
  56.7515,58.1213,55.1859,51.4677,55.1859,57.3386,54.2074,56.1644,55.3816,56.5558,55.5773,47.1624,55.3816,59.0998,
  55.1859,52.0548,54.2074,50.8806,52.2505,58.7084,52.4462,57.1429,55.773,53.4247,51.272,41.683,64.5793,3.32681,
  1.95695,2.54403,1.76125,4.50098,2.54403,3.91389,4.10959,1.56556,3.32681,5.08806,2.15264,1.95695,2.73973,1.36986,
  1.76125,1.36986,1.36986,0.782779,1.17417,0.391389,1.36986,0.587084,0.391389,1.36986,0.195695,0.391389,0.391389,
  4.50098,0.782779,0.978474,0.195695,0.587084,0.195695,0.391389,2.54403,0.587084,0.587084,0.782779,0.391389,0.391389,
  2.73973,0.978474,0.978474,3.5225,0.782779,1.17417,0.391389,0.195695,4.89237,0.782779,0.391389,0.587084,3.32681,
  2.15264,2.34834,1.76125,0.391389,0.587084,0.195695,2.54403,4.89237,0.391389,0.391389,0.195695,0.391389,0.782779,
  0.782779,8.41487,2.73973,0.587084,4.50098,0.195695,0,0,0.391389,0.195695,1.36986,3.91389,1.76125,0.391389,0.782779,
  2.73973,5.47945,0.391389,0.195695,0,0,3.32681,0.391389,0,0.195695,0,9.78474,0,0.587084,0,0.391389,0,1.95695,2.54403,
  1.17417,3.5225,0.195695,0,0,0.195695,0.195695,2.73973,0,2.54403,0.978474,0.587084,0.195695,1.56556,3.5225,1.17417,
  0.391389,2.34834,2.73973,0.195695,0.391389,1.17417,0.782779,0.391389,0.978474,0,1.56556,0,2.34834,0.587084,3.91389,
  0.978474,0,0.587084,1.56556,1.56556,0.587084,0.978474,0.587084,1.36986,3.5225,0.782779,2.73973,0.782779,0.587084,
  1.17417,6.26223,0.587084,0.391389,3.7182,0.782779,3.91389,0.195695,0.587084,0.782779,0.978474,0.195695,10.3718,
  3.32681,0,0.195695,0.978474,0.391389,1.17417,0.391389,0.587084,0.782779,0.195695,0.195695,0.782779,0.587084,2.93542,
  0.391389,6.26223,1.36986,1.17417,1.76125,2.73973,0.587084,2.73973,0.978474,8.80626,0,0.195695,0.391389,0.195695,
  0.978474,0.195695,0.195695,2.34834,0.587084,0.782779,2.15264,0.391389,1.17417,0.587084,0.391389,0.587084,0.391389,
  0.587084,6.84932,2.93542,2.54403,0.587084,0.195695,0.587084,1.36986,0.195695,2.54403,0.195695,0,0.587084,0,0.587084,
  1.17417,0.195695,1.76125,0.782779,0.587084,0,0,0.195695,0,0.587084,0.782779,0.391389,1.76125,0.782779,0,1.17417,
  0.782779,1.36986,0,2.73973,0,0,0.978474,0.195695,1.76125,0.978474,1.76125,0,0,2.73973,0,0,0,0,0,0,0.391389,0,0,
  1.36986,1.56556,4.50098,1.95695,1.56556,0.978474,0.391389,0,0.782779,0.195695,0,0,0,0,2.93542,0.195695,0,2.34834,
  0.391389,0.195695,0.391389,0,0,0.195695,3.32681,1.36986,0.587084,0.391389,0,0.391389,0,0.782779,1.17417,0.195695,
  2.15264,1.36986,0,0.391389,0,1.95695,0,0,0,0.587084,0,2.54403,0,1.17417,1.76125,0,0,0.587084,0,0,0,0,0.782779,0,
  0.978474,0.195695,0.195695,2.15264,0.195695,0,0,3.7182,0.978474,0.391389,2.93542,0,2.54403,0.195695,0.391389,2.93542,
  1.17417,1.36986,4.89237,0.782779,2.15264,0.978474,0.978474,1.76125,0,6.65362,0.978474,0.782779,1.76125,2.93542,
  1.17417,0,0.587084,2.73973,1.76125,1.36986,0.587084,6.26223,1.17417,1.36986,1.36986,1.76125,1.76125,0.195695,
  0.587084,1.56556,1.76125,1.56556,3.5225,1.95695,1.36986,7.04501,1.95695,2.54403,3.91389,1.36986,0.978474,1.36986,
  5.08806,2.93542]
}
```

## Next steps...

- More permissive polish:
    - Stage 4 damping: 0.01 (was 0.00).
    - Stage 4 confidence: 0.48 (was 0.60).
- Wider focused completion:
    - Triggers when a row has ≤ ~5% unknowns instead of ≤1%. For S=511 this is ≤26 unknowns, so those 95–98% rows get actively
      finished.
- Kept reheat stage and shake behavior as-is.
- Updated row log’s phase arrays to match.

### Outcome

- That was a bit of a regression but we are doing 227 rows instead of 226.  *whomp, whomp*
- Boundary at 99.8043%: That’s a single undecided bit in row 227. Without the focus running inside multiphase, it never tipped
  to 100%, so the prefix couldn’t advance.  It's like me in Junior high...so close, yet so far away.
- 
