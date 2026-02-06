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
- Deprecated single-phase knobs (CRSCE_GOBP_ITERS/CONF/DAMP) are no longer used.
- Use per-phase overrides instead: `CRSCE_GOBP_PHASE{1..4}_{CONF,DAMP,ITERS}`.

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

- Baseline (defaults): `CRSCE_DE_MAX_ITERS=200` with the built-in 4‑phase GOBP schedule → No convergence.
- Stronger (in make target): `CRSCE_DE_MAX_ITERS=20000` + tuned per‑phase overrides → No convergence; still reaches
  steady state early.

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

> GOBP runs a fixed 4‑phase schedule by default. To tune, use per‑phase overrides:
> - CRSCE_GOBP_PHASE1_CONF / CRSCE_GOBP_PHASE1_DAMP / CRSCE_GOBP_PHASE1_ITERS
> - CRSCE_GOBP_PHASE2_CONF / CRSCE_GOBP_PHASE2_DAMP / CRSCE_GOBP_PHASE2_ITERS
> - CRSCE_GOBP_PHASE3_CONF / CRSCE_GOBP_PHASE3_DAMP / CRSCE_GOBP_PHASE3_ITERS
> - CRSCE_GOBP_PHASE4_CONF / CRSCE_GOBP_PHASE4_DAMP / CRSCE_GOBP_PHASE4_ITERS

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
    "CRSCE_GOBP_MULTIPHASE":true,
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
    "CRSCE_GOBP_MULTIPHASE":true,
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
    "CRSCE_GOBP_MULTIPHASE":true,
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
    "CRSCE_GOBP_MULTIPHASE":true,
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

## Option: Shake, Rattle, and Roll

- Add a fourth phase to GOBP with a reheat/polish to shake loose from the plateaus.

### Outcome:

```text
% cat /Users/samcaldwell/git/sam-caldwell/crsce/build/uselessTest/completion_stats.log
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
  "cols":[...]
}
```

## Next steps...

- More permissive polish:
    - Stage 4 damping: 0.01 (was 0.00).
    - Stage 4 confidence: 0.48 (was 0.60).
- Wider focused completion:
    - Triggers when a row has ≤ ~5% unknowns instead of ≤1%. For S=511 this is ≤26 unknowns, so those 95–98% rows get
      actively
      finished.
- Kept reheat stage and shake behavior as-is.
- Updated row log’s phase arrays to match.

### Outcome

- That was a bit of a regression but we are doing 227 rows instead of 226.  *whomp, whomp*
- Boundary at 99.8043%: That’s a single undecided bit in row 227. Without the focus running inside multiphase, it never
  tipped
  to 100%, so the prefix couldn’t advance. It's like me in Junior high...so close, yet so far away.
- The boundary row (≈99.8043%) should have tipped; I missed immediately re‑verifying/locking after boundary focus
  inside the multiphase loop.

## Option: row-based hashing

We are not making progress anymore. Let's move to row-based hashing.

Thsi is our last outcome:

```text
{
  "step":"row-completion-stats",
  "block_index":0,
  "parameters":{
    "CRSCE_DE_MAX_ITERS":60000,
    "CRSCE_GOBP_MULTIPHASE":true,
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
  "valid_prefix":0,
  "lh_debug":{
    "seedHash":"220069c623862254266bc905f1d7fc0100c92860bc4749df551600542b639843",
    "row0Packed":"00000000000000000002030200040190000bb972200ac3f10013b7b6c410010800008c800052060400a3b700004a0be00cffb82f0377af8e0b3f7e955a6edb54",
    "expectedRow0":"c5b4423e973952bfc0a59e8dcacc7271949781ea006dc5af2bf04cf8ae7fa58e",
    "computedRow0":"2da8613830ec5b05cca218581ccefc45394f872a43b8c974d346fc8b5eb75d67"
  },
  "gobp_phases":{
    "conf":[0.995,0.8,0.9,0.48],
    "damp":[0.5,0.1,0.35,0.01],
    "iters":[500,3500,70000,890000]
  },
  "min_pct":0,
  "avg_pct":44.618,
  "max_pct":100,
  "full_rows":227,
  "rows":[100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,
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
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]
}
```

## Status Update

- Refactored the hash chaining for row-based chaining.
- Updated tests, etc.
- I need to switch to true per‑row gating.
- We are still focusing on top-left first. It's reflected in the answer. It's the same issue as Raddiz Sift (1990s)
    - We need to get GOBP to focus less on top-left and look at the entire CSM, where solutions may unlock the plateau.

## Status Update:

- I have made absolutely no progress. I've added more telemetry, but chain hashes weren't causing the problem and
  row-level hashes didn't fix anything.
- We still reach a plateau around row 232, and we are not solving the first row (or any row)
- I have a hypothesis that plateaus may be defeated if we just back off and try again from a different perspective.
    - GobpSolver scan mode:
        - Added set_scan_flipped(bool) and a private flag to switch traversal.
        - Default: row-major, top→bottom, left→right.
        - Flipped: column-major, bottom→top, right→left.
        - Files: include/decompress/Gobp/GobpSolver.h, src/Decompress/Gobp/GobpSolver_solve_step.cpp.
        - Files: include/decompress/Gobp/GobpSolver.h, src/Decompress/Gobp/GobpSolver_solve_step.cpp.
    - Plateau handling (flip up to 3 passes):
        - In the GOBP loops, when gprog == 0, toggle scan mode and retry immediately, up to 3 consecutive flips without
          progress; reset counter when progress is made.
        - Only after 3 flip attempts do we allow micro‑shakes and “steady state” termination.
    - Expected Behavior summary
        - Start with normal (r,c) traversal.
        - On first plateau: flip perspective (c,r) and retry with current beliefs (no reset).
        - On next plateau(s): alternate back to top‑down, up to three consecutive flips.
        - If still stuck after flips: proceed with existing micro‑shake and phase termination logic.
    - other changes:
        - removed the non-multiphase code (it's not used anymore).
        - clean up documentation
        - purge old code to just do some housekeeping...and because I want to feel like Felix Dzerzhinski tonight...

### Outcome

Woohoo...227 to 229 rows...still no cigars, but we have more beliefs than a Republican caught in a New Orleans Bordello.

```text
 % cat build/uselessTest/completion_stats.log
{
  "step":"row-completion-stats",
  "block_index":0,
  "start_ms":1770258088673,
  "end_ms":1770258089886,
  "parameters":{
    "CRSCE_DE_MAX_ITERS":60000,
    "CRSCE_GOBP_MULTIPHASE":true,
    "CRSCE_BACKTRACK":true,
    "CRSCE_BT_DE_ITERS":1200,
    "kFocusMaxSteps":48,
    "kFocusBtIters":8000,
    "kRestarts":12,
    "kPerturbBase":0.008,
    "kPerturbStep":0.006,
    "kPolishShakes":6,
    "kPolishShakeJitter":0.008
  },
  "restarts":[
  ],
  "restarts_total":0,
  "rng_seed_belief":50159747054,
  "rng_seed_restarts":12648430,
  "rows_committed":0,
  "cols_finished":0,
  "boundary_finisher_attempts":0,
  "boundary_finisher_successes":0,
  "lock_in_prefix_count":0,
  "lock_in_row_count":0,
  "restart_contradiction_count":0,
  "gobp_iters_run":13,
  "gating_calls":4,
  "partial_adoptions":100,
  "unknown_history":[261121,261121,261121,261121,261121,217678,217473,217473,217473,217473,217473,217473,217473,217473],
  "valid_prefix":0,
  "verified_rows":[],
  "lh_debug":{
    "row0Packed":"00000000000003000002a30200040190000bb972200a43f00011b7a6c410010800008c800052048020a3b700004a0be00cbfb86f0377af8e0b3f7e955a6edab4",
    "expectedRow0":"b0ffead36d398d75f59a98e4866212542c7e25eec4d086ff137e0283594cc054",
    "computedRow0":"317f1b627c532ee29cecfa50e08a2e6c9fb3bd3c146a1ad6921e30d73b831554"
  },
  "gobp_phases":{
    "conf":[0.995,0.8,0.9,0.48],
    "damp":[0.5,0.1,0.35,0.01],
    "iters":[5000,7000,140000,890000]
  },
  "row_min_pct":0,
  "row_avg_pct":45.0094,
  "row_max_pct":100,
  "full_rows":229,
  "rows":[100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,
  100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,
  100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,
  100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,
  100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,
  100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,
  100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,
  100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,
  99.8043,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
  "col_min_pct":44.8141,
  "col_avg_pct":45.0094,
  "col_max_pct":45.0098,
  "cols":[45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,
  45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,
  45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,
  45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,
  45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,
  45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,
  45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,
  45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,
  45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,
  45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,
  45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,
  45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,
  45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,
  45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,
  45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,
  45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,
  45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,
  45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,
  45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,
  45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,
  45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,
  45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,
  45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,
  45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,
  45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,
  45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,
  45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,
  45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,
  45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,
  45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,
  45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,
  45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,
  45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,
  45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,
  45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,
  45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,
  45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,45.0098,44.8141]
}
```

## Update

- Houston: we have a problem...and maybe a solution!  We are hitting plateaus because we are mathematically running
  into unconstrained or lightly constrained areas of the CSM. We need to borrow from our freinds with the green hats
  and side step these traps rather than trying to muscle our way through them.
- Solution: We will implement a detect-and-avoid strategy to solve highly constrained regions first, which will create
  more constraints for the less constrained regions. Call it Asymmetric Algorithmic problem solving. If this doesn't
  work, I'm gonna stop and do the other thing the guys with the green hats taught me...drink until I can't think.

### Outcome

- No change on the uselessTest data.
- However, we have found the bug in test/random. We can now reproduce the issue

```text
 % make test/random && cat /Users/samcaldwell/git/sam-caldwell/crsce/build/testRunnerRandom/completion_stats.log
--- Building project with preset: llvm-debug ---
[0/2] Re-checking globbed directories...
ninja: no work to do.
--- ✅ Build complete ---
--- Running testRunnerRandom (preset: llvm-debug) ---
{
  "step":"hashInput",
  "start":1770263047721,
  "end":1770263047727,
  "hashInput":"/Users/samcaldwell/git/sam-caldwell/crsce/build/testRunnerRandom/random_input_1770263047704.bin",
  "rawInputSize":262144,
  "paddingSize":31618,
  "inputSize":293762,
  "hash":"4a8dd58778e25b26b9e197c9b4dea5671daee8ff74e341eded46df269e63a29a6365347f90eabc8642413b4e502c91cf3af0e965ea4858ca01a5b910095991aa"
}
[testrunner] running: /Users/samcaldwell/git/sam-caldwell/crsce/build/llvm-debug/compress -in /Users/samcaldwell/git/sam-caldwell/crsce/build/testRunnerRandom/random_input_1770263047704.bin -out /Users/samcaldwell/git/sam-caldwell/crsce/build/testRunnerRandom/random_output_1770263047727.crsce
{
  "step":"compress",
  "start":1770263047727,
  "end":1770263047863,
  "input":"/Users/samcaldwell/git/sam-caldwell/crsce/build/testRunnerRandom/random_input_1770263047704.bin",
  "hashInput":"4a8dd58778e25b26b9e197c9b4dea5671daee8ff74e341eded46df269e63a29a6365347f90eabc8642413b4e502c91cf3af0e965ea4858ca01a5b910095991aa",
  "output":"/Users/samcaldwell/git/sam-caldwell/crsce/build/testRunnerRandom/random_output_1770263047727.crsce",
  "rawInputSize":262144,
  "paddingSize":31618,
  "inputSize":293762,
  "compressedSize":167896,
  "compressTime":136
}
[testrunner] running: env CRSCE_PRELOCK_DEBUG=1 CRSCE_DE_MAX_ITERS=4000 /Users/samcaldwell/git/sam-caldwell/crsce/build/llvm-debug/decompress -in /Users/samcaldwell/git/sam-caldwell/crsce/build/testRunnerRandom/random_output_1770263047727.crsce -out /Users/samcaldwell/git/sam-caldwell/crsce/build/testRunnerRandom/random_reconstructed_1770263047727.bin
[testrunner] decompress.stdout:
ROW_COMPLETION_LOG=/Users/samcaldwell/git/sam-caldwell/crsce/build/testRunnerRandom/completion_stats.log
{
  "step":"decompress-failure",
  "total_blocks":9,
  "blocks_attempted":1,
  "blocks_successful":0,
  "blocks_failed":1,
  "block_index":0,
  "phase":"gobp",
  "iter":0,
  "solved":239656,
  "unknown_total":21465,
  "lsm":[239,273,247,262,264,267,250,253,249,261,263,266,229,241,255,283,266,261,244,253,259,247,253,255,259,234,269,244,263,269,254,247,244,269,261,267,237,264,275,264,236,251,267,278,259,253,272,237,243,251,240,264,247,268,247,265,264,254,238,242,240,252,270,251,248,245,275,252,273,238,234,256,261,241,266,259,242,263,262,256,279,239,253,275,241,264,244,264,261,247,258,262,260,256,262,253,252,252,271,243,267,239,245,245,249,257,223,264,263,230,263,249,247,255,270,265,246,235,249,258,262,244,248,273,268,261,246,251,247,253,250,245,252,254,254,261,268,248,249,252,270,251,262,259,249,265,261,249,251,255,272,252,256,257,244,247,247,251,253,248,253,242,243,238,254,254,252,255,251,254,255,255,249,268,260,245,236,271,265,254,248,261,250,258,257,257,252,259,242,271,244,248,257,240,259,254,218,264,278,241,264,270,236,247,248,236,284,258,250,259,243,259,252,239,254,251,251,264,269,251,261,242,241,257,244,249,227,259,267,240,263,230,247,268,240,250,261,278,245,250,276,254,264,238,238,272,265,254,258,266,252,258,268,253,257,270,272,248,271,246,248,244,241,252,250,262,245,251,255,248,251,259,251,269,254,262,254,256,262,265,255,242,250,247,245,247,274,265,246,267,260,260,259,263,247,256,272,232,262,256,264,263,264,268,267,248,236,267,237,248,241,253,251,254,271,255,265,262,260,267,250,266,244,255,253,250,252,263,267,261,253,260,268,267,246,267,258,237,257,248,253,230,237,263,250,240,253,263,259,228,265,277,253,249,259,234,274,273,260,244,260,261,247,275,256,268,245,267,248,256,250,264,255,248,254,257,261,259,249,240,268,242,256,246,269,266,259,241,259,262,258,270,243,258,257,266,263,233,264,238,256,258,260,269,269,289,266,251,261,232,261,261,250,277,259,255,255,261,247,246,250,247,262,262,254,258,247,269,262,246,256,246,264,232,258,249,264,254,254,233,262,254,257,264,236,262,265,261,261,271,246,230,260,249,264,265,236,241,243,262,273,244,256,253,269,229,262,259,252,247,258,246,248,255,268,249,256,260,250,270,259,242,259,264,277,254,267,260,253,255,254,264,250,256,247,261,257,253,263,269,253,241,257,262,258,247,256,266,255,256,254],
  "vsm":[259,253,242,249,244,267,255,245,248,250,242,256,250,249,267,265,263,265,248,230,244,269,257,258,263,249,259,260,256,261,241,255,262,267,267,246,252,271,270,267,250,253,269,256,246,262,264,243,270,250,258,284,256,243,255,244,258,246,237,263,254,256,252,240,260,249,250,255,252,263,248,247,246,276,262,267,256,246,243,259,243,252,244,253,263,264,256,263,256,265,237,283,240,261,259,245,265,239,254,268,249,251,247,265,253,252,265,256,243,252,262,257,251,251,280,244,259,252,277,272,262,266,266,258,251,270,256,242,265,258,250,267,246,240,265,249,256,262,273,238,280,254,248,252,259,260,253,248,246,260,265,242,239,262,274,248,260,246,249,280,233,256,247,257,268,269,244,247,272,262,251,259,253,235,257,249,259,243,263,263,263,244,264,286,250,237,259,257,247,274,262,263,254,272,251,255,249,278,248,250,284,237,246,244,245,266,260,236,247,253,243,258,251,267,260,253,249,258,268,257,264,256,253,257,257,225,241,250,276,253,246,260,244,247,253,242,275,266,228,245,278,238,270,273,266,243,247,258,261,256,247,255,252,257,265,242,234,259,261,263,257,252,240,253,258,251,273,260,223,245,246,266,266,239,264,270,254,282,250,245,231,257,252,257,239,247,249,241,244,254,270,262,244,254,250,260,267,269,245,257,264,269,239,246,258,245,245,258,271,254,282,244,255,247,278,258,247,262,242,256,250,241,249,238,252,252,263,266,245,257,259,258,270,242,248,237,245,248,252,240,259,239,271,246,256,261,252,264,259,278,236,239,280,266,250,245,242,254,247,257,263,254,261,252,257,249,253,260,250,247,256,244,245,245,271,260,250,240,249,259,263,263,255,256,258,256,263,254,248,257,248,256,239,256,263,252,266,246,264,261,271,262,249,241,256,254,246,275,255,239,255,261,243,266,245,255,263,266,242,278,251,252,244,274,245,249,258,261,246,256,279,232,259,236,261,257,273,234,250,262,254,256,266,263,263,288,261,232,249,262,256,261,272,260,260,252,268,250,246,247,240,255,241,262,245,246,257,263,241,261,240,256,269,270,256,251,243,275,238,257,263,256,263,249,258,257,258,262,255,243,266,263,236,263,255,230,261,262,237,250,250,253,256,258,258,269,248,271,251,248,253],
  "dsm":[238,237,236,244,258,256,248,264,247,249,239,258,252,261,273,266,276,243,261,230,248,273,257,257,263,263,232,239,256,230,255,248,253,273,259,265,281,255,249,259,256,261,242,248,258,270,233,252,243,268,237,260,242,274,259,253,259,244,284,258,247,294,223,251,249,251,249,224,247,255,235,268,268,237,260,258,235,263,269,238,216,257,239,234,250,260,261,245,263,242,247,258,233,271,230,257,268,263,234,252,240,257,242,262,251,269,258,244,229,245,247,253,256,248,253,244,258,242,254,238,245,266,278,258,270,262,259,252,274,257,253,284,255,247,252,252,254,247,259,260,253,263,247,283,272,261,234,245,238,273,245,259,265,266,266,252,255,264,265,264,259,264,247,215,231,258,248,284,270,295,251,267,237,263,240,254,270,265,236,244,261,255,260,250,264,259,262,251,260,239,278,260,281,258,245,268,243,253,263,256,266,252,250,251,249,255,232,285,255,253,264,262,253,236,261,251,244,260,264,274,262,262,258,265,269,252,248,253,227,225,283,247,265,274,283,278,263,264,257,236,255,262,249,251,258,250,244,247,255,231,268,262,267,257,266,246,267,265,256,269,281,246,232,255,260,243,260,259,256,239,260,254,257,265,246,252,270,268,232,244,244,241,229,252,258,265,240,273,259,267,244,262,241,240,257,268,236,266,260,224,265,273,254,238,253,266,252,256,262,251,266,248,255,268,267,240,268,277,282,249,276,239,258,254,269,258,267,239,263,269,261,258,274,265,250,248,241,262,245,254,261,255,270,252,259,241,242,232,262,252,262,239,244,254,269,256,249,244,272,255,248,238,260,259,277,293,259,281,275,232,240,240,261,262,258,252,257,258,243,257,259,252,265,245,262,254,259,260,257,234,252,256,257,249,258,251,254,256,254,246,246,243,266,254,256,279,252,248,250,256,261,249,251,245,260,276,247,256,267,252,270,256,255,265,233,268,247,266,251,261,269,254,249,250,250,247,247,274,260,257,270,260,247,250,241,266,245,252,259,272,245,257,260,250,240,252,246,254,250,239,278,234,261,256,267,253,241,235,261,264,238,277,264,255,252,252,244,250,250,251,259,244,255,274,258,262,247,242,267,249,243,278,257,258,278,254,271,270,246,265,239,264,225,260,241,252,241,271,248,248,251],
  "xsm":[267,247,264,248,253,258,250,256,260,253,252,241,265,271,241,263,260,251,267,259,261,265,253,264,248,243,250,241,243,258,253,262,249,247,244,246,248,271,242,263,250,246,271,263,259,230,256,269,260,245,261,254,237,235,243,269,245,250,245,264,242,262,262,260,248,258,248,264,265,256,256,260,278,254,267,260,263,224,264,274,263,246,242,257,263,250,260,287,260,256,251,249,247,266,267,262,280,250,259,249,271,245,255,234,263,270,245,266,238,259,244,258,247,239,237,251,273,256,257,245,234,252,234,259,244,273,253,263,256,266,247,257,256,270,245,252,262,262,260,247,250,261,260,280,264,261,243,243,261,244,271,260,252,275,265,258,273,270,263,241,254,249,247,257,236,248,263,249,256,263,243,246,234,244,222,227,241,259,251,257,256,269,255,253,265,247,239,280,276,250,250,245,262,254,238,269,279,249,263,239,248,256,256,248,243,250,248,266,265,240,239,269,249,250,244,254,274,254,250,273,253,249,261,256,248,265,253,274,257,249,245,238,252,257,252,230,267,238,249,257,257,246,244,273,268,247,254,272,251,264,250,259,262,244,263,249,261,270,254,243,264,266,247,209,235,251,254,247,260,268,244,257,243,251,240,239,260,266,262,273,261,270,264,254,249,256,244,255,258,255,252,254,270,256,270,255,237,267,244,245,242,252,269,256,270,248,269,236,233,254,260,229,264,243,257,244,264,255,247,237,258,267,261,275,243,255,259,241,254,251,255,269,248,265,243,235,238,258,255,265,257,265,255,268,260,264,263,252,262,242,249,255,245,261,253,250,257,263,254,253,235,266,253,256,253,263,251,254,256,258,275,239,258,246,244,277,236,248,264,277,251,220,249,260,277,270,267,251,260,259,260,270,248,237,261,247,241,251,248,265,246,257,272,231,271,290,249,273,254,247,261,265,248,268,264,282,274,259,253,234,251,254,258,257,258,249,268,263,240,273,247,264,225,269,259,251,241,257,250,274,249,258,246,266,259,276,274,248,242,259,248,255,260,254,269,266,256,247,263,252,260,266,239,278,259,235,253,269,247,252,272,256,247,237,261,265,271,266,234,248,261,265,259,270,255,237,233,232,271,253,257,280,240,263,255,254,228,234,248,255,252,260,231,254,263,260,272,267,271,257,268],
  "U_row":[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511],
  "U_col":[42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,43,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,43,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,43,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42],
  "U_diag":[42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,43,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,43,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,43,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42],
  "U_xdiag":[42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,43,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,43,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,43,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42],
  "unknown_sums":{"rows":21465,"cols":21465,"diags":21465,"xdiags":21465},
  "message":"GOBP: R underflow on a line while assigning 1: r=468, c=510, d=42, x=467"
}
ROW_COMPLETION_LOG=/Users/samcaldwell/git/sam-caldwell/crsce/build/testRunnerRandom/completion_stats.log

[testrunner] decompress.stderr:
Deterministic Elimination exit with no error (progress: 0)
Block Solver reached steady state (no progress)
Boundary finisher invoked: boundary=0 U_row[boundary]=511
Block Solver terminating...
Deterministic Elimination exit with no error (progress: 0)
Deterministic Elimination exit with no error (progress: 0)
Deterministic Elimination exit with no error (progress: 0)
Deterministic Elimination exit with no error (progress: 0)
Boundary finisher invoked: boundary=0 U_row[boundary]=511
GOBP reached steady state (no progress)
Deterministic Elimination exit with no error (progress: 0)
Deterministic Elimination exit with no error (progress: 0)
Deterministic Elimination exit with no error (progress: 0)
Deterministic Elimination exit with no error (progress: 0)
Boundary finisher invoked: boundary=0 U_row[boundary]=511
GOBP reached steady state (no progress)
Deterministic Elimination exit with no error (progress: 0)
Deterministic Elimination exit with no error (progress: 0)
Deterministic Elimination exit with no error (progress: 0)
Deterministic Elimination exit with no error (progress: 0)
Boundary finisher invoked: boundary=0 U_row[boundary]=511
GOBP reached steady state (no progress)
error: block 0 solve failed

{
  "step":"error",
  "subStep":"decompress",
  "start":1770263047863,
  "end":1770263048430,
  "elapsed":567,
  "timeoutMs":180000,
  "exitCode":4,
  "command":"env CRSCE_PRELOCK_DEBUG=1 CRSCE_DE_MAX_ITERS=4000 /Users/samcaldwell/git/sam-caldwell/crsce/build/llvm-debug/decompress -in /Users/samcaldwell/git/sam-caldwell/crsce/build/testRunnerRandom/random_output_1770263047727.crsce -out /Users/samcaldwell/git/sam-caldwell/crsce/build/testRunnerRandom/random_reconstructed_1770263047727.bin",
  "input":"/Users/samcaldwell/git/sam-caldwell/crsce/build/testRunnerRandom/random_output_1770263047727.crsce",
  "output":"/Users/samcaldwell/git/sam-caldwell/crsce/build/testRunnerRandom/random_reconstructed_1770263047727.bin",
  "decompressedSize":0,
  "hashInput":"4a8dd58778e25b26b9e197c9b4dea5671daee8ff74e341eded46df269e63a29a6365347f90eabc8642413b4e502c91cf3af0e965ea4858ca01a5b910095991aa",
  "stderr":"Deterministic Elimination exit with no error (progress: 0)\u000aBlock Solver reached steady state (no progress)\u000aBoundary finisher invoked: boundary=0 U_row[boundary]=511\u000aBlock Solver terminating...\u000aDeterministic Elimination exit with no error (progress: 0)\u000aDeterministic Elimination exit with no error (progress: 0)\u000aDeterministic Elimination exit with no error (progress: 0)\u000aDeterministic Elimination exit with no error (progress: 0)\u000aBoundary finisher invoked: boundary=0 U_row[boundary]=511\u000aGOBP reached steady state (no progress)\u000aDeterministic Elimination exit with no error (progress: 0)\u000aDeterministic Elimination exit with no error (progress: 0)\u000aDeterministic Elimination exit with no error (progress: 0)\u000aDeterministic Elimination exit with no error (progress: 0)\u000aBoundary finisher invoked: boundary=0 U_row[boundary]=511\u000aGOBP reached steady state (no progress)\u000aDeterministic Elimination exit with no error (progress: 0)\u000aDeterministic Elimination exit with no error (progress: 0)\u000aDeterministic Elimination exit with no error (progress: 0)\u000aDeterministic Elimination exit with no error (progress: 0)\u000aBoundary finisher invoked: boundary=0 U_row[boundary]=511\u000aGOBP reached steady state (no progress)\u000aerror: block 0 solve failed\u000a"
}
ROW_COMPLETION_LOG=/Users/samcaldwell/git/sam-caldwell/crsce/build/testRunnerRandom/completion_stats.log
{
  "step":"decompress-failure",
  "total_blocks":9,
  "blocks_attempted":1,
  "blocks_successful":0,
  "blocks_failed":1,
  "block_index":0,
  "phase":"gobp",
  "iter":0,
  "solved":239656,
  "unknown_total":21465,
  "lsm":[239,273,247,262,264,267,250,253,249,261,263,266,229,241,255,283,266,261,244,253,259,247,253,255,259,234,269,244,263,269,254,247,244,269,261,267,237,264,275,264,236,251,267,278,259,253,272,237,243,251,240,264,247,268,247,265,264,254,238,242,240,252,270,251,248,245,275,252,273,238,234,256,261,241,266,259,242,263,262,256,279,239,253,275,241,264,244,264,261,247,258,262,260,256,262,253,252,252,271,243,267,239,245,245,249,257,223,264,263,230,263,249,247,255,270,265,246,235,249,258,262,244,248,273,268,261,246,251,247,253,250,245,252,254,254,261,268,248,249,252,270,251,262,259,249,265,261,249,251,255,272,252,256,257,244,247,247,251,253,248,253,242,243,238,254,254,252,255,251,254,255,255,249,268,260,245,236,271,265,254,248,261,250,258,257,257,252,259,242,271,244,248,257,240,259,254,218,264,278,241,264,270,236,247,248,236,284,258,250,259,243,259,252,239,254,251,251,264,269,251,261,242,241,257,244,249,227,259,267,240,263,230,247,268,240,250,261,278,245,250,276,254,264,238,238,272,265,254,258,266,252,258,268,253,257,270,272,248,271,246,248,244,241,252,250,262,245,251,255,248,251,259,251,269,254,262,254,256,262,265,255,242,250,247,245,247,274,265,246,267,260,260,259,263,247,256,272,232,262,256,264,263,264,268,267,248,236,267,237,248,241,253,251,254,271,255,265,262,260,267,250,266,244,255,253,250,252,263,267,261,253,260,268,267,246,267,258,237,257,248,253,230,237,263,250,240,253,263,259,228,265,277,253,249,259,234,274,273,260,244,260,261,247,275,256,268,245,267,248,256,250,264,255,248,254,257,261,259,249,240,268,242,256,246,269,266,259,241,259,262,258,270,243,258,257,266,263,233,264,238,256,258,260,269,269,289,266,251,261,232,261,261,250,277,259,255,255,261,247,246,250,247,262,262,254,258,247,269,262,246,256,246,264,232,258,249,264,254,254,233,262,254,257,264,236,262,265,261,261,271,246,230,260,249,264,265,236,241,243,262,273,244,256,253,269,229,262,259,252,247,258,246,248,255,268,249,256,260,250,270,259,242,259,264,277,254,267,260,253,255,254,264,250,256,247,261,257,253,263,269,253,241,257,262,258,247,256,266,255,256,254],
  "vsm":[259,253,242,249,244,267,255,245,248,250,242,256,250,249,267,265,263,265,248,230,244,269,257,258,263,249,259,260,256,261,241,255,262,267,267,246,252,271,270,267,250,253,269,256,246,262,264,243,270,250,258,284,256,243,255,244,258,246,237,263,254,256,252,240,260,249,250,255,252,263,248,247,246,276,262,267,256,246,243,259,243,252,244,253,263,264,256,263,256,265,237,283,240,261,259,245,265,239,254,268,249,251,247,265,253,252,265,256,243,252,262,257,251,251,280,244,259,252,277,272,262,266,266,258,251,270,256,242,265,258,250,267,246,240,265,249,256,262,273,238,280,254,248,252,259,260,253,248,246,260,265,242,239,262,274,248,260,246,249,280,233,256,247,257,268,269,244,247,272,262,251,259,253,235,257,249,259,243,263,263,263,244,264,286,250,237,259,257,247,274,262,263,254,272,251,255,249,278,248,250,284,237,246,244,245,266,260,236,247,253,243,258,251,267,260,253,249,258,268,257,264,256,253,257,257,225,241,250,276,253,246,260,244,247,253,242,275,266,228,245,278,238,270,273,266,243,247,258,261,256,247,255,252,257,265,242,234,259,261,263,257,252,240,253,258,251,273,260,223,245,246,266,266,239,264,270,254,282,250,245,231,257,252,257,239,247,249,241,244,254,270,262,244,254,250,260,267,269,245,257,264,269,239,246,258,245,245,258,271,254,282,244,255,247,278,258,247,262,242,256,250,241,249,238,252,252,263,266,245,257,259,258,270,242,248,237,245,248,252,240,259,239,271,246,256,261,252,264,259,278,236,239,280,266,250,245,242,254,247,257,263,254,261,252,257,249,253,260,250,247,256,244,245,245,271,260,250,240,249,259,263,263,255,256,258,256,263,254,248,257,248,256,239,256,263,252,266,246,264,261,271,262,249,241,256,254,246,275,255,239,255,261,243,266,245,255,263,266,242,278,251,252,244,274,245,249,258,261,246,256,279,232,259,236,261,257,273,234,250,262,254,256,266,263,263,288,261,232,249,262,256,261,272,260,260,252,268,250,246,247,240,255,241,262,245,246,257,263,241,261,240,256,269,270,256,251,243,275,238,257,263,256,263,249,258,257,258,262,255,243,266,263,236,263,255,230,261,262,237,250,250,253,256,258,258,269,248,271,251,248,253],
  "dsm":[238,237,236,244,258,256,248,264,247,249,239,258,252,261,273,266,276,243,261,230,248,273,257,257,263,263,232,239,256,230,255,248,253,273,259,265,281,255,249,259,256,261,242,248,258,270,233,252,243,268,237,260,242,274,259,253,259,244,284,258,247,294,223,251,249,251,249,224,247,255,235,268,268,237,260,258,235,263,269,238,216,257,239,234,250,260,261,245,263,242,247,258,233,271,230,257,268,263,234,252,240,257,242,262,251,269,258,244,229,245,247,253,256,248,253,244,258,242,254,238,245,266,278,258,270,262,259,252,274,257,253,284,255,247,252,252,254,247,259,260,253,263,247,283,272,261,234,245,238,273,245,259,265,266,266,252,255,264,265,264,259,264,247,215,231,258,248,284,270,295,251,267,237,263,240,254,270,265,236,244,261,255,260,250,264,259,262,251,260,239,278,260,281,258,245,268,243,253,263,256,266,252,250,251,249,255,232,285,255,253,264,262,253,236,261,251,244,260,264,274,262,262,258,265,269,252,248,253,227,225,283,247,265,274,283,278,263,264,257,236,255,262,249,251,258,250,244,247,255,231,268,262,267,257,266,246,267,265,256,269,281,246,232,255,260,243,260,259,256,239,260,254,257,265,246,252,270,268,232,244,244,241,229,252,258,265,240,273,259,267,244,262,241,240,257,268,236,266,260,224,265,273,254,238,253,266,252,256,262,251,266,248,255,268,267,240,268,277,282,249,276,239,258,254,269,258,267,239,263,269,261,258,274,265,250,248,241,262,245,254,261,255,270,252,259,241,242,232,262,252,262,239,244,254,269,256,249,244,272,255,248,238,260,259,277,293,259,281,275,232,240,240,261,262,258,252,257,258,243,257,259,252,265,245,262,254,259,260,257,234,252,256,257,249,258,251,254,256,254,246,246,243,266,254,256,279,252,248,250,256,261,249,251,245,260,276,247,256,267,252,270,256,255,265,233,268,247,266,251,261,269,254,249,250,250,247,247,274,260,257,270,260,247,250,241,266,245,252,259,272,245,257,260,250,240,252,246,254,250,239,278,234,261,256,267,253,241,235,261,264,238,277,264,255,252,252,244,250,250,251,259,244,255,274,258,262,247,242,267,249,243,278,257,258,278,254,271,270,246,265,239,264,225,260,241,252,241,271,248,248,251],
  "xsm":[267,247,264,248,253,258,250,256,260,253,252,241,265,271,241,263,260,251,267,259,261,265,253,264,248,243,250,241,243,258,253,262,249,247,244,246,248,271,242,263,250,246,271,263,259,230,256,269,260,245,261,254,237,235,243,269,245,250,245,264,242,262,262,260,248,258,248,264,265,256,256,260,278,254,267,260,263,224,264,274,263,246,242,257,263,250,260,287,260,256,251,249,247,266,267,262,280,250,259,249,271,245,255,234,263,270,245,266,238,259,244,258,247,239,237,251,273,256,257,245,234,252,234,259,244,273,253,263,256,266,247,257,256,270,245,252,262,262,260,247,250,261,260,280,264,261,243,243,261,244,271,260,252,275,265,258,273,270,263,241,254,249,247,257,236,248,263,249,256,263,243,246,234,244,222,227,241,259,251,257,256,269,255,253,265,247,239,280,276,250,250,245,262,254,238,269,279,249,263,239,248,256,256,248,243,250,248,266,265,240,239,269,249,250,244,254,274,254,250,273,253,249,261,256,248,265,253,274,257,249,245,238,252,257,252,230,267,238,249,257,257,246,244,273,268,247,254,272,251,264,250,259,262,244,263,249,261,270,254,243,264,266,247,209,235,251,254,247,260,268,244,257,243,251,240,239,260,266,262,273,261,270,264,254,249,256,244,255,258,255,252,254,270,256,270,255,237,267,244,245,242,252,269,256,270,248,269,236,233,254,260,229,264,243,257,244,264,255,247,237,258,267,261,275,243,255,259,241,254,251,255,269,248,265,243,235,238,258,255,265,257,265,255,268,260,264,263,252,262,242,249,255,245,261,253,250,257,263,254,253,235,266,253,256,253,263,251,254,256,258,275,239,258,246,244,277,236,248,264,277,251,220,249,260,277,270,267,251,260,259,260,270,248,237,261,247,241,251,248,265,246,257,272,231,271,290,249,273,254,247,261,265,248,268,264,282,274,259,253,234,251,254,258,257,258,249,268,263,240,273,247,264,225,269,259,251,241,257,250,274,249,258,246,266,259,276,274,248,242,259,248,255,260,254,269,266,256,247,263,252,260,266,239,278,259,235,253,269,247,252,272,256,247,237,261,265,271,266,234,248,261,265,259,270,255,237,233,232,271,253,257,280,240,263,255,254,228,234,248,255,252,260,231,254,263,260,272,267,271,257,268],
  "U_row":[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511,511],
  "U_col":[42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,43,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,43,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,43,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42],
  "U_diag":[42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,43,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,43,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,43,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42],
  "U_xdiag":[42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,43,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,43,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,43,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42,42],
  "unknown_sums":{"rows":21465,"cols":21465,"diags":21465,"xdiags":21465},
  "message":"GOBP: R underflow on a line while assigning 1: r=468, c=510, d=42, x=467"
}
ROW_COMPLETION_LOG=/Users/samcaldwell/git/sam-caldwell/crsce/build/testRunnerRandom/completion_stats.log
{
  "step":"error",
  "runner":"testRunnerRandom",
  "message":"decompress exited with code 4",
  "exitCode":4
}
make: *** [test/random] Error 4
```

## Update

- What we know:
    - useless-machine.mp4 fails consistently.
    - testRunnerRandom fails consistently after we fixed a bug.
    - GOBP is failing to converge on a solution
    - Our latest refinements maybe showing some progress (below)
- What we don't know:
    - we do not know why the bug is happening.

```text
{
  "step":"row-completion-stats",
  "block_index":0,
  "start_ms":1770264413090,
  "end_ms":1770264414412,
  "parameters":{
    "CRSCE_DE_MAX_ITERS":60000,
    "CRSCE_GOBP_MULTIPHASE":true,
    "CRSCE_BACKTRACK":true,
    "CRSCE_BT_DE_ITERS":1200,
    "kFocusMaxSteps":48,
    "kFocusBtIters":8000,
    "kRestarts":12,
    "kPerturbBase":0.008,
    "kPerturbStep":0.006,
    "kPolishShakes":6,
    "kPolishShakeJitter":0.008
  },
  "restarts":[
  ],
  "restarts_total":0,
  "rng_seed_belief":50159747054,
  "rng_seed_restarts":12648430,
  "gobp_cells_solved":245009,
  "rows_committed":0,
  "cols_finished":0,
  "boundary_finisher_attempts":0,
  "boundary_finisher_successes":0,
  "lock_in_prefix_count":0,
  "lock_in_row_count":0,
  "restart_contradiction_count":0,
  "gobp_iters_run":13,
  "gating_calls":4,
  "partial_adoptions":100,
  "unknown_history":[261121,261121,261121,261121,261121,217678,217473,217473,217473,217473,217473,217473,217473,217473],
  "valid_prefix":0,
  "verified_rows":[],
  "lh_debug":{
    "row0Packed":"00000000000003000002a30200040190000bb972200a43f00011b7a6c410010800008c800052048020a3b700004a0be00cbfb86f0377af8e0b3f7e955a6edab4",
    "expectedRow0":"b0ffead36d398d75f59a98e4866212542c7e25eec4d086ff137e0283594cc054",
    "computedRow0":"317f1b627c532ee29cecfa50e08a2e6c9fb3bd3c146a1ad6921e30d73b831554"
  },
  "gobp_phases":{
    "conf":[0.995,0.8,0.9,0.48],
    "damp":[0.5,0.1,0.35,0.01],
    "iters":[5000,7000,140000,890000]
  },
  "row_min_pct":10.7632,
  "row_avg_pct":93.9296,
  "row_max_pct":100,
  "full_rows":235,
  "rows":[100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,
  100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,
  100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,
  100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,
  100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,
  100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,
  100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,
  100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,
  100,100,100,100,100,100,100,99.8043,99.8043,99.8043,99.8043,99.8043,99.8043,99.8043,99.8043,99.8043,99.8043,
  99.8043,99.8043,99.8043,99.8043,99.8043,99.8043,99.8043,99.8043,99.8043,99.8043,99.8043,99.8043,99.8043,99.8043,
  99.8043,99.8043,99.8043,99.8043,99.8043,99.8043,99.8043,99.8043,99.8043,99.8043,99.8043,99.8043,99.8043,99.8043,
  99.8043,99.8043,99.8043,99.8043,99.8043,99.8043,99.8043,99.8043,99.8043,99.8043,99.8043,99.8043,99.8043,99.8043,
  99.8043,99.8043,99.8043,99.8043,99.8043,99.8043,99.8043,99.8043,99.6086,99.6086,99.6086,99.6086,99.6086,99.6086,
  99.6086,99.6086,99.6086,99.6086,99.6086,99.6086,99.6086,99.6086,99.6086,99.6086,99.6086,99.6086,99.6086,99.6086,
  99.4129,99.2172,99.2172,98.8258,98.8258,98.8258,98.6301,98.4344,98.4344,98.2387,98.2387,98.2387,98.2387,98.2387,
  98.2387,98.0431,97.8474,97.8474,97.8474,97.8474,97.6517,97.6517,97.6517,97.6517,97.6517,97.456,97.456,97.456,
  97.456,97.2603,97.2603,97.2603,97.2603,97.2603,97.0646,97.0646,97.0646,97.0646,96.8689,96.8689,96.8689,96.8689,
  96.8689,96.6732,96.6732,96.6732,96.4775,96.4775,96.4775,96.0861,96.2818,96.2818,96.2818,96.2818,96.2818,96.2818,
  96.0861,96.0861,96.0861,96.0861,96.0861,96.0861,95.8904,95.8904,95.8904,95.8904,95.8904,95.8904,95.8904,95.8904,
  95.8904,95.8904,95.6947,95.6947,95.8904,95.6947,95.8904,95.8904,96.0861,96.0861,96.2818,96.2818,96.4775,96.4775,
  96.4775,96.6732,96.6732,96.8689,96.8689,97.0646,97.0646,97.0646,97.2603,97.2603,97.456,97.456,97.456,97.6517,
  97.6517,97.8474,97.8474,97.8474,98.0431,98.0431,98.2387,98.2387,98.4344,98.4344,98.4344,98.6301,98.6301,98.8258,
  98.8258,99.0215,99.0215,99.0215,99.2172,99.2172,99.4129,99.4129,99.6086,99.6086,99.6086,99.8043,99.8043,99.8043,
  99.8043,99.8043,100,100,100,100,100,100,98.2387,96.2818,94.5205,92.7593,90.411,87.6712,86.8885,87.0841,86.3014,
  86.4971,86.1057,85.91,86.1057,85.5186,85.5186,85.3229,85.1272,84.7358,84.1487,83.953,83.3659,81.9961,81.409,
  79.0607,77.4951,74.7554,71.2329,69.2759,67.5147,66.5362,64.5793,62.4266,60.8611,58.317,57.3386,54.9902,53.4247,
  53.0333,52.6419,51.6634,51.6634,51.6634,51.4677,51.8591,50.4892,50.0978,49.7065,48.728,45.9883,43.0528,40.5088,
  37.9648,36.3992,35.0294,34.4423,33.4638,31.7025,31.3112,29.5499,29.9413,27.7886,23.6791,16.8297,14.4814,12.7202,
  12.1331,10.9589,10.7632],
  "col_min_pct":45.9883,
  "col_avg_pct":93.9296,
  "col_max_pct":100,
  "cols":[100,89.2368,89.4325,89.6282,89.8239,90.0196,90.2153,90.411,90.6067,90.6067,90.998,91.1937,91.3894,91.5851,
  91.5851,91.7808,91.9765,92.1722,92.1722,92.5636,92.7593,92.7593,92.7593,93.1507,93.1507,93.3464,93.5421,93.7378,
  93.7378,93.9335,94.3249,94.1292,94.3249,94.7162,94.5205,94.9119,95.1076,95.3033,95.3033,95.499,95.8904,95.8904,
  96.0861,96.2818,96.2818,96.4775,96.6732,96.6732,96.6732,96.8689,96.6732,96.8689,96.6732,96.8689,96.6732,96.6732,
  96.6732,96.6732,96.6732,96.6732,96.4775,96.2818,96.2818,96.0861,96.2818,96.0861,95.6947,95.6947,95.6947,95.3033,
  95.1076,94.9119,94.5205,94.1292,93.9335,93.7378,94.3249,93.9335,93.9335,93.3464,92.7593,92.5636,92.3679,91.9765,
  91.7808,91.1937,91.3894,91.1937,91.3894,91.5851,91.5851,91.7808,91.9765,92.3679,92.5636,92.5636,92.5636,92.955,
  92.7593,92.7593,92.955,92.7593,92.955,92.955,92.7593,92.955,92.955,93.1507,93.1507,93.1507,93.3464,93.1507,92.955,
  93.1507,93.3464,93.5421,93.5421,93.3464,93.3464,93.3464,93.5421,93.5421,93.5421,93.7378,93.5421,93.5421,93.5421,
  93.7378,93.5421,93.7378,93.9335,93.7378,93.5421,93.5421,93.3464,93.3464,93.1507,93.1507,92.955,92.7593,92.7593,
  92.5636,92.3679,92.1722,92.1722,91.9765,91.9765,91.7808,91.7808,91.3894,91.7808,92.3679,92.5636,92.955,93.7378,
  94.7162,95.3033,95.8904,96.0861,97.0646,97.456,98.0431,98.2387,98.8258,99.2172,99.6086,99.8043,99.8043,100,100,
  100,100,100,100,100,100,100,100,100,100,100,100,99.2172,96.4775,94.1292,92.955,91.9765,90.998,90.2153,89.2368,
  88.2583,87.6712,86.6928,86.6928,86.6928,86.8885,86.8885,86.6928,86.8885,86.6928,86.6928,86.6928,86.8885,86.8885,
  87.0841,87.0841,87.2798,87.2798,87.4755,87.4755,87.4755,87.2798,87.4755,87.6712,87.6712,87.8669,87.6712,87.8669,
  88.2583,88.2583,88.2583,88.2583,88.454,88.6497,88.6497,88.8454,89.0411,89.2368,89.4325,89.2368,89.6282,89.6282,
  89.8239,90.0196,90.0196,90.2153,90.6067,90.8023,90.8023,91.1937,91.3894,91.5851,91.5851,91.5851,91.7808,91.7808,
  91.5851,91.5851,91.5851,91.7808,91.5851,91.5851,91.5851,91.5851,91.7808,91.7808,91.7808,91.7808,91.5851,91.7808,
  91.7808,91.7808,91.7808,91.9765,91.9765,91.7808,91.9765,91.9765,91.9765,91.7808,91.9765,91.9765,91.9765,92.1722,
  92.1722,92.1722,92.1722,92.3679,92.3679,92.5636,92.3679,92.5636,92.3679,92.5636,92.5636,92.7593,92.7593,92.7593,
  92.7593,92.955,92.955,93.1507,93.5421,93.5421,94.1292,93.9335,94.5205,94.9119,95.1076,95.3033,95.6947,95.6947,
  95.8904,95.8904,96.0861,96.2818,96.6732,96.4775,96.6732,96.6732,96.6732,96.8689,97.0646,97.2603,97.2603,97.2603,
  97.2603,97.2603,97.2603,97.456,97.456,97.456,97.456,97.456,97.456,97.6517,97.456,97.456,97.6517,97.6517,97.8474,
  97.6517,97.8474,98.0431,98.0431,98.6301,99.2172,99.0215,99.0215,99.0215,99.0215,98.8258,98.8258,98.8258,99.0215,
  99.0215,98.8258,99.0215,98.6301,98.8258,98.6301,98.8258,98.6301,98.8258,98.8258,98.6301,98.8258,98.4344,98.6301,
  98.8258,98.8258,98.8258,98.8258,98.8258,98.6301,98.8258,99.0215,99.0215,98.8258,99.0215,99.2172,99.2172,99.2172,
  99.2172,99.6086,99.8043,99.4129,99.6086,99.4129,99.6086,99.4129,99.6086,99.4129,99.4129,99.2172,99.2172,99.2172,
  99.2172,99.0215,99.0215,99.2172,98.8258,98.8258,98.8258,98.6301,98.6301,98.6301,98.8258,98.6301,98.6301,98.4344,
  98.4344,98.6301,98.4344,98.4344,98.6301,98.6301,98.2387,98.4344,98.6301,98.6301,98.8258,98.4344,98.8258,98.6301,
  98.8258,98.8258,98.6301,98.8258,98.8258,98.8258,98.8258,98.8258,98.8258,98.8258,98.6301,99.0215,98.8258,98.8258,
  98.6301,98.4344,98.6301,98.6301,98.4344,98.4344,98.4344,98.0431,98.0431,97.8474,97.8474,97.8474,97.456,97.2603,
  97.0646,97.0646,96.6732,96.8689,96.4775,96.2818,96.4775,96.4775,96.2818,96.2818,96.2818,96.0861,96.2818,96.2818,
  96.0861,95.8904,96.0861,96.2818,96.0861,96.0861,96.2818,96.2818,96.2818,96.0861,96.0861,96.2818,96.2818,96.2818,
  96.0861,96.2818,96.0861,96.2818,96.4775,96.2818,95.8904,95.6947,95.8904,96.0861,95.6947,95.8904,96.4775,97.2603,
  98.0431,98.0431,98.0431,97.0646,94.7162,91.7808,89.4325,87.4755,85.91,84.5401,82.1918,80.6262,79.0607,76.1252,
  73.3855,70.4501,69.6673,68.1018,67.1233,65.7534,66.1448,64.1879,64.3836,63.6008,58.9041,45.9883]
}
```

# Status

- Forcing focus worked: focus_boundary_attempts is non‑zero now; so gating was the blocker.
- Broadening Mode A to K1=24 did not change adoption (still 1 attempt, 0 locks).
- Micro‑solver did not produce a verified prefix on this input yet.

# Status

where we are

```text
{
  "step":"row-completion-stats",
  "block_index":0,
  "start_ms":1770358931563,
  "end_ms":1770359279356,
  "parameters":{
    "CRSCE_DE_MAX_ITERS":60000,
    "CRSCE_GOBP_MULTIPHASE":true,
    "CRSCE_BACKTRACK":true,
    "CRSCE_BT_DE_ITERS":1200,
    "kFocusMaxSteps":48,
    "kFocusBtIters":8000,
    "kRestarts":12,
    "kPerturbBase":0.008,
    "kPerturbStep":0.006,
    "kPolishShakes":6,
    "kPolishShakeJitter":0.008
  },
  "restarts":[
    {"restart_index":0,"prefix_rows":0,"unknown_total":171799,"action":"polish-shake"},
    {"restart_index":0,"prefix_rows":0,"unknown_total":171799,"action":"polish-shake"},
    {"restart_index":0,"prefix_rows":0,"unknown_total":171799,"action":"polish-shake"},
    {"restart_index":0,"prefix_rows":0,"unknown_total":171799,"action":"polish-shake"},
    {"restart_index":0,"prefix_rows":0,"unknown_total":171799,"action":"polish-shake"},
    {"restart_index":0,"prefix_rows":0,"unknown_total":171799,"action":"polish-shake"},
    {"restart_index":1,"prefix_rows":0,"unknown_total":171707,"action":"polish-shake"},
    {"restart_index":1,"prefix_rows":0,"unknown_total":171707,"action":"polish-shake"},
    {"restart_index":1,"prefix_rows":0,"unknown_total":171707,"action":"polish-shake"},
    {"restart_index":1,"prefix_rows":0,"unknown_total":171707,"action":"polish-shake"},
    {"restart_index":1,"prefix_rows":0,"unknown_total":171707,"action":"polish-shake"},
    {"restart_index":1,"prefix_rows":0,"unknown_total":171707,"action":"polish-shake"},
    {"restart_index":2,"prefix_rows":0,"unknown_total":172013,"action":"polish-shake"},
    {"restart_index":2,"prefix_rows":0,"unknown_total":172013,"action":"polish-shake"},
    {"restart_index":2,"prefix_rows":0,"unknown_total":172013,"action":"polish-shake"},
    {"restart_index":2,"prefix_rows":0,"unknown_total":172013,"action":"polish-shake"},
    {"restart_index":2,"prefix_rows":0,"unknown_total":172013,"action":"polish-shake"},
    {"restart_index":2,"prefix_rows":0,"unknown_total":172013,"action":"polish-shake"},
    {"restart_index":3,"prefix_rows":0,"unknown_total":171852,"action":"polish-shake"},
    {"restart_index":3,"prefix_rows":0,"unknown_total":171852,"action":"polish-shake"},
    {"restart_index":3,"prefix_rows":0,"unknown_total":171852,"action":"polish-shake"},
    {"restart_index":3,"prefix_rows":0,"unknown_total":171852,"action":"polish-shake"},
    {"restart_index":3,"prefix_rows":0,"unknown_total":171852,"action":"polish-shake"},
    {"restart_index":3,"prefix_rows":0,"unknown_total":171852,"action":"polish-shake"},
    {"restart_index":4,"prefix_rows":0,"unknown_total":171983,"action":"polish-shake"},
    {"restart_index":4,"prefix_rows":0,"unknown_total":171983,"action":"polish-shake"},
    {"restart_index":4,"prefix_rows":0,"unknown_total":171983,"action":"polish-shake"},
    {"restart_index":4,"prefix_rows":0,"unknown_total":171983,"action":"polish-shake"},
    {"restart_index":4,"prefix_rows":0,"unknown_total":171983,"action":"polish-shake"},
    {"restart_index":4,"prefix_rows":0,"unknown_total":171983,"action":"polish-shake"},
    {"restart_index":5,"prefix_rows":0,"unknown_total":172133,"action":"polish-shake"},
    {"restart_index":5,"prefix_rows":0,"unknown_total":172133,"action":"polish-shake"},
    {"restart_index":5,"prefix_rows":0,"unknown_total":172133,"action":"polish-shake"},
    {"restart_index":5,"prefix_rows":0,"unknown_total":172133,"action":"polish-shake"},
    {"restart_index":5,"prefix_rows":0,"unknown_total":172133,"action":"polish-shake"},
    {"restart_index":5,"prefix_rows":0,"unknown_total":172133,"action":"polish-shake"},
    {"restart_index":6,"prefix_rows":0,"unknown_total":171971,"action":"polish-shake"},
    {"restart_index":6,"prefix_rows":0,"unknown_total":171971,"action":"polish-shake"},
    {"restart_index":6,"prefix_rows":0,"unknown_total":171971,"action":"polish-shake"},
    {"restart_index":6,"prefix_rows":0,"unknown_total":171971,"action":"polish-shake"},
    {"restart_index":6,"prefix_rows":0,"unknown_total":171971,"action":"polish-shake"},
    {"restart_index":6,"prefix_rows":0,"unknown_total":171971,"action":"polish-shake"},
    {"restart_index":7,"prefix_rows":0,"unknown_total":171901,"action":"polish-shake"},
    {"restart_index":7,"prefix_rows":0,"unknown_total":171901,"action":"polish-shake"},
    {"restart_index":7,"prefix_rows":0,"unknown_total":171901,"action":"polish-shake"},
    {"restart_index":7,"prefix_rows":0,"unknown_total":171901,"action":"polish-shake"},
    {"restart_index":7,"prefix_rows":0,"unknown_total":171901,"action":"polish-shake"},
    {"restart_index":7,"prefix_rows":0,"unknown_total":171901,"action":"polish-shake"},
    {"restart_index":8,"prefix_rows":0,"unknown_total":171987,"action":"polish-shake"},
    {"restart_index":8,"prefix_rows":0,"unknown_total":171987,"action":"polish-shake"},
    {"restart_index":8,"prefix_rows":0,"unknown_total":171987,"action":"polish-shake"},
    {"restart_index":8,"prefix_rows":0,"unknown_total":171987,"action":"polish-shake"},
    {"restart_index":8,"prefix_rows":0,"unknown_total":171987,"action":"polish-shake"},
    {"restart_index":8,"prefix_rows":0,"unknown_total":171987,"action":"polish-shake"},
    {"restart_index":9,"prefix_rows":0,"unknown_total":171971,"action":"polish-shake"},
    {"restart_index":9,"prefix_rows":0,"unknown_total":171971,"action":"polish-shake"},
    {"restart_index":9,"prefix_rows":0,"unknown_total":171971,"action":"polish-shake"},
    {"restart_index":9,"prefix_rows":0,"unknown_total":171971,"action":"polish-shake"},
    {"restart_index":9,"prefix_rows":0,"unknown_total":171971,"action":"polish-shake"},
    {"restart_index":9,"prefix_rows":0,"unknown_total":171971,"action":"polish-shake"},
    {"restart_index":10,"prefix_rows":129,"unknown_total":171099,"action":"polish-shake"},
    {"restart_index":10,"prefix_rows":129,"unknown_total":171099,"action":"polish-shake"},
    {"restart_index":10,"prefix_rows":129,"unknown_total":171099,"action":"polish-shake"},
    {"restart_index":10,"prefix_rows":129,"unknown_total":171099,"action":"polish-shake"},
    {"restart_index":10,"prefix_rows":129,"unknown_total":171099,"action":"polish-shake"},
    {"restart_index":10,"prefix_rows":129,"unknown_total":171099,"action":"polish-shake"},
    {"restart_index":11,"prefix_rows":129,"unknown_total":171316,"action":"polish-shake"},
    {"restart_index":11,"prefix_rows":129,"unknown_total":171316,"action":"polish-shake"},
    {"restart_index":11,"prefix_rows":129,"unknown_total":171316,"action":"polish-shake"},
    {"restart_index":11,"prefix_rows":129,"unknown_total":171316,"action":"polish-shake"},
    {"restart_index":11,"prefix_rows":129,"unknown_total":171316,"action":"polish-shake"},
    {"restart_index":11,"prefix_rows":129,"unknown_total":171316,"action":"polish-shake"},
    {"restart_index":12,"prefix_rows":0,"unknown_total":171448,"action":"polish-shake"},
    {"restart_index":12,"prefix_rows":0,"unknown_total":171448,"action":"polish-shake"},
    {"restart_index":12,"prefix_rows":0,"unknown_total":171448,"action":"polish-shake"},
    {"restart_index":12,"prefix_rows":0,"unknown_total":171448,"action":"polish-shake"},
    {"restart_index":12,"prefix_rows":0,"unknown_total":171448,"action":"polish-shake"},
    {"restart_index":12,"prefix_rows":0,"unknown_total":171448,"action":"polish-shake"}
  ],
  "restarts_total":78,
  "rng_seed_belief":50159747054,
  "rng_seed_restarts":12648430,
  "gobp_cells_solved":1020531,
  "rows_committed":0,
  "cols_finished":0,
  "boundary_finisher_attempts":511,
  "boundary_finisher_successes":0,
  "focus_boundary_attempts":0,
  "focus_boundary_prefix_locks":0,
  "focus_boundary_partials":0,
  "final_backtrack1_attempts":1,
  "final_backtrack1_prefix_locks":0,
  "final_backtrack2_attempts":0,
  "final_backtrack2_prefix_locks":0,
  "lock_in_prefix_count":0,
  "lock_in_row_count":0,
  "restart_contradiction_count":0,
  "gobp_iters_run":471,
  "gating_calls":302,
  "partial_adoptions":55992,
  "micro_solver_attempts":714,
  "micro_solver_dp_attempts":0,
  "micro_solver_dp_feasible":0,
  "micro_solver_dp_infeasible":0,
  "micro_solver_dp_solutions_tested":0,
  "micro_solver_lh_verifications":0,
  "micro_solver_successes":0,
  "micro_solver_time_ms":0,
  "micro_solver_bnb_attempts":0,
  "micro_solver_bnb_nodes":0,
  "micro_solver_bnb_successes":0,
  "unknown_history":[208800,208800,208800,208800,208800,208800,188209,181233,175979,172475,171977,171971,171971,171971,171971,171971,171971,171971,171971,171971,171971,171971,261121,261121,261121,261121,210423,209127,208915,208802,208802,208802,208802,208802,208802,208802,208802,208802,188172,181130,175837,172392,171913,171902,171902,171901,171901,171901,171901,171901,171901,171901,171901,171901,171901,171901,261121,261121,261121,261121,210424,209127,208918,208805,208803,208803,208803,208803,208803,208803,208803,208803,208803,188220,181236,175948,172480,172107,171988,171987,171987,171987,171987,171987,171987,171987,171987,171987,171987,171987,261121,261121,261121,261121,210423,209126,208915,208805,208804,208804,208804,208804,208804,208804,208804,208804,208804,188199,181157,175893,172352,171978,171972,171971,171971,171971,171971,171971,171971,171971,171971,171971,171971,171971,261121,261121,261121,261120,261106,261054,261016,260996,260992,260991,260990,260990,260990,260990,260990,210403,209096,208892,208781,208779,208779,208779,208779,208779,208779,208779,208779,208779,187954,180890,175612,171842,171217,171099,171099,171099,171099,171099,171099,171099,171099,171099,171099,171099,261121,261121,261119,261118,261101,261061,261017,260998,260992,260992,260992,260991,260991,260991,260991,260991,210401,209093,208889,208778,208776,208776,208776,208776,208776,208776,208776,208776,208776,187961,180915,175669,172292,171540,171316,171316,171316,171316,171316,171316,171316,171316,171316,171316,171316,261121,261121,261120,261113,261091,261056,261017,260995,260992,260990,260990,260990,260990,260990,210405,209098,208895,208784,208782,208782,208782,208782,208782,208782,208782,208782,208782,187955,180858,175636,172041,171563,171448,171448,171448,171448,171448,171448,171448,171448,171448,171448,171448],
  "valid_prefix":0,
  "verified_rows":[],
  "lh_debug":{
    "row0Packed":"000000000000000000000000000000000000000000000000000197220004c169001bfffd827bfff727ffffe800000100001820000000040000050e9000000300",
    "expectedRow0":"b0ffead36d398d75f59a98e4866212542c7e25eec4d086ff137e0283594cc054",
    "computedRow0":"32785fabb08fe872d46699afca83722535c1cf7e08eb079ba5dc79545609bb5e"
  },
  "gobp_phases":{
    "conf":[0.995,0.7,0.85,0.55],
    "damp":[0.5,0.1,0.35,0.02],
    "iters":[8000,12000,300000,2000000]
  },
  "row_min_pct":0,
  "row_avg_pct":34.3416,
  "row_max_pct":100,
  "full_rows":133,
  "rows":[79.4521,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,100,79.0607,100,100,100,100,78.2779,75.7339,73.5812,63.9922,100,43.6399,47.5538,43.4442,40.1174,42.0744,35.4207,34.4423,34.2466,27.2016,30.137,31.8982,26.8102,27.0059,24.0705,24.2661,22.8963,20.7436,16.047,18.0039,19.9609,18.7867,19.5695,15.4599,16.2427,17.2211,13.3072,13.5029,13.6986,20.7436,13.6986,13.3072,10.9589,12.3288,8.80626,9.39335,13.8943,10.5675,9.39335,11.546,8.02348,7.63209,12.3288,5.87084,6.26223,13.3072,5.87084,8.41487,6.84932,8.41487,15.8513,7.04501,8.80626,6.06654,11.9374,11.3503,12.7202,10.7632,8.80626,7.2407,7.4364,12.3288,12.5245,4.69667,9.00196,4.69667,10.3718,4.30528,10.9589,18.1996,13.8943,7.4364,15.8513,9.19765,7.63209,11.1546,10.5675,10.1761,12.7202,16.4384,13.5029,7.63209,11.7417,14.6771,17.4168,8.61057,8.21918,7.04501,8.21918,14.6771,11.1546,7.82779,9.00196,10.3718,20.5479,6.45793,10.1761,7.63209,8.61057,7.63209,12.7202,13.1115,11.3503,15.0685,7.82779,7.4364,6.84932,7.4364,9.00196,15.0685,8.41487,13.5029,10.7632,8.21918,8.41487,11.3503,15.0685,11.1546,8.02348,13.5029,12.5245,7.04501,8.41487,10.3718,8.21918,6.65362,8.61057,6.45793,9.00196,7.63209,11.3503,7.04501,13.6986,10.5675,6.06654,5.67515,10.1761,11.3503,5.28376,8.21918,6.45793,8.21918,13.6986,10.1761,12.9159,7.2407,7.2407,10.1761,16.4384,9.00196,8.41487,15.6556,10.5675,13.5029,8.02348,10.3718,11.3503,7.82779,6.84932,20.1566,13.8943,6.84932,5.28376,9.98043,8.61057,9.39335,9.39335,7.63209,8.21918,8.21918,7.2407,9.00196,7.4364,12.7202,7.82779,15.6556,9.19765,9.00196,9.78474,12.1331,10.3718,12.7202,9.98043,19.1781,6.26223,4.50098,7.2407,5.87084,8.02348,5.28376,4.89237,10.7632,7.4364,7.82779,9.39335,3.7182,6.06654,2.15264,4.69667,6.26223,2.34834,4.50098,14.4814,10.1761,10.1761,4.69667,1.76125,2.34834,6.45793,3.32681,10.1761,2.54403,0.195695,5.87084,0.782779,5.28376,8.61057,1.56556,7.04501,6.06654,4.69667,2.54403,4.10959,0,0.587084,5.47945,4.69667,2.15264,5.67515,3.32681,3.7182,3.91389,2.93542,8.21918,1.56556,7.82779,1.76125,0.391389,6.45793,1.36986,6.45793,6.26223,6.26223,1.76125,1.76125,8.02348,0,4.69667,0.587084,0.195695,1.76125,0.782779,0.978474,0,1.95695,7.04501,7.2407,10.1761,7.04501,5.47945,6.06654,0.978474,2.73973,5.08806,0.978474,0.391389,0.587084,0.195695,1.76125,8.41487,0.391389,1.95695,8.21918,1.36986,0.978474,2.73973,1.17417,0.978474,1.95695,10.1761,8.02348,7.2407,3.91389,3.32681,8.80626,4.69667,4.89237,5.87084,2.73973,7.2407,6.65362,9.58904,10.9589,4.69667,6.84932,6.84932,4.69667,6.65362,4.69667,7.2407,7.04501,10.9589,14.6771,5.67515,9.39335,10.5675,6.26223,12.9159,9.39335,12.1331,12.3288,14.6771,13.3072,7.63209,9.00196,12.9159,15.2642,9.39335,11.1546,11.3503,9.39335,13.3072,11.3503,15.8513,11.1546,7.4364,9.39335,9.19765,13.5029,12.5245,5.67515,16.047,7.4364,12.5245,10.9589,10.5675,14.8728,11.7417,18.9824,8.61057,8.61057,8.02348,15.0685,14.6771,11.546,13.1115,16.6341,9.78474,8.80626,12.5245,14.4814,10.5675,10.7632,16.6341,17.8082,10.3718,16.047,12.9159,18.7867,17.4168,12.1331,18.7867,13.5029,13.5029,19.9609,19.7652,20.5479,15.6556,18.9824,17.4168,19.7652,19.3738,17.0254],
  "col_min_pct":26.8102,
  "col_avg_pct":34.3416,
  "col_max_pct":47.3581,
  "cols":[39.9217,34.8337,33.4638,42.4658,38.7476,36.5949,38.9432,36.7906,39.726,37.5734,34.638,38.1605,41.0959,36.9863,38.9432,40.5088,38.1605,43.0528,41.0959,41.2916,33.4638,41.2916,36.5949,40.1174,39.9217,36.0078,37.9648,38.9432,34.8337,39.1389,34.8337,36.2035,41.683,40.9002,40.1174,38.1605,39.3346,41.4873,39.9217,39.5303,44.6184,46.9667,43.2485,40.1174,36.0078,39.3346,34.2466,39.9217,33.4638,40.7045,39.1389,40.3131,36.3992,42.6614,42.2701,42.8571,36.0078,37.3777,32.0939,34.8337,36.5949,38.3562,33.6595,35.0294,31.5068,35.4207,31.8982,32.4853,33.2681,32.4853,33.4638,35.6164,37.5734,32.8767,38.7476,34.0509,30.5284,31.5068,32.2896,30.5284,34.0509,30.5284,31.5068,30.7241,30.9198,32.2896,33.0724,29.9413,30.5284,30.9198,31.3112,30.3327,31.8982,30.9198,36.0078,31.7025,31.3112,34.2466,34.2466,33.4638,32.2896,31.8982,35.225,31.3112,31.7025,33.0724,32.681,32.2896,33.0724,35.4207,32.2896,34.0509,32.0939,34.8337,31.8982,33.4638,33.0724,31.7025,31.5068,31.7025,33.4638,31.3112,30.9198,33.4638,31.7025,31.8982,34.638,42.0744,34.0509,32.2896,35.4207,33.6595,33.2681,34.638,35.6164,33.8552,34.2466,36.5949,35.8121,34.638,33.8552,33.6595,34.8337,36.5949,34.4423,37.3777,36.7906,35.6164,36.0078,35.6164,35.0294,35.4207,35.8121,35.6164,36.3992,36.0078,37.7691,37.9648,37.3777,36.9863,37.182,38.1605,39.5303,38.1605,38.9432,38.9432,40.3131,40.5088,41.2916,43.4442,40.1174,41.8787,41.4873,41.683,42.8571,41.0959,42.4658,42.0744,42.2701,42.2701,41.2916,40.1174,40.1174,40.7045,39.1389,39.5303,39.1389,37.7691,39.1389,39.3346,38.1605,39.3346,36.7906,39.5303,34.8337,35.225,34.0509,35.225,33.4638,36.0078,34.638,40.1174,33.8552,32.4853,34.2466,32.681,32.681,32.681,31.5068,30.7241,33.4638,30.5284,30.7241,29.9413,29.5499,29.3542,29.1585,32.0939,32.0939,31.7025,28.9628,30.7241,30.3327,29.3542,29.7456,30.5284,30.137,28.9628,30.5284,29.9413,27.9843,28.5714,28.7671,30.7241,28.9628,30.5284,30.3327,29.9413,29.7456,29.3542,30.3327,29.7456,29.5499,29.3542,30.3327,29.3542,28.3757,29.3542,29.3542,28.18,28.5714,28.18,29.3542,28.9628,27.7886,28.18,32.0939,30.3327,29.9413,28.3757,28.18,28.3757,27.9843,27.9843,27.3973,28.9628,27.9843,27.7886,28.18,28.3757,28.7671,28.5714,29.7456,28.9628,28.5714,28.3757,28.18,29.9413,27.9843,27.9843,26.8102,28.18,28.5714,28.18,27.9843,27.3973,27.7886,28.5714,28.18,27.3973,27.3973,28.5714,29.7456,27.3973,29.9413,31.8982,29.5499,32.2896,32.0939,34.0509,31.3112,31.3112,33.2681,31.1155,31.1155,30.9198,31.8982,30.5284,32.681,32.2896,33.8552,32.2896,34.8337,32.8767,33.6595,32.4853,32.8767,32.4853,32.8767,31.1155,32.4853,32.8767,31.8982,32.681,31.8982,32.2896,33.6595,35.0294,33.6595,34.2466,34.638,33.8552,35.8121,34.4423,34.0509,33.8552,35.8121,35.6164,36.7906,36.3992,34.4423,34.638,35.8121,34.4423,36.3992,34.8337,36.2035,33.6595,35.6164,33.2681,34.4423,34.0509,34.8337,34.638,33.8552,34.0509,33.0724,32.8767,35.0294,33.6595,34.4423,36.3992,35.0294,35.0294,35.8121,34.638,34.2466,35.6164,34.638,35.225,34.2466,34.0509,35.225,33.6595,34.638,34.4423,35.6164,34.638,35.0294,34.2466,34.0509,33.6595,33.8552,34.2466,33.8552,34.638,33.8552,33.2681,33.6595,34.2466,34.638,35.4207,34.0509,33.6595,34.0509,33.4638,34.0509,36.2035,36.0078,34.8337,34.8337,34.0509,33.4638,34.2466,34.638,34.0509,33.6595,32.681,33.2681,32.681,33.2681,32.681,34.8337,33.6595,33.2681,33.8552,33.4638,34.0509,33.2681,33.4638,33.6595,37.3777,35.4207,32.8767,33.8552,36.3992,32.681,32.0939,36.2035,34.4423,36.5949,34.0509,35.4207,33.8552,34.4423,33.0724,33.0724,33.4638,33.4638,34.4423,31.5068,33.0724,34.2466,32.681,34.2466,33.0724,35.4207,33.6595,35.4207,35.6164,35.6164,35.225,37.9648,33.0724,33.0724,31.8982,31.7025,36.2035,34.2466,40.7045,34.8337,34.2466,34.8337,32.0939,35.4207,31.7025,31.3112,37.5734,32.8767,33.2681,37.9648,37.182,39.5303,35.0294,37.3777,33.0724,37.182,37.9648,34.8337,31.7025,35.8121,35.8121,32.8767,35.225,33.0724,32.0939,35.6164,39.1389,35.4207,32.2896,36.5949,36.2035,39.5303,35.4207,37.182,36.2035,34.8337,37.182,33.8552,38.7476,39.1389,36.7906,35.8121,33.2681,38.1605,44.227,38.7476,40.3131,42.4658,39.9217,47.3581]
}
```

### Analysis:

Compare the last two snapshots:

- Earlier (short run): 13 GOBP iters, gobp_cells_solved=245,009, gating_calls=4, restarts_total=0,
  partial_adoptions=100, boundary_finisher_attempts=0, valid_prefix=0. The row/col averages are very high (
  row_avg_pct≈93.93%, col_avg_pct≈93.93%), but no rows are LH‑verified at the prefix (valid_prefix=0). That
  is a cosmetically “complete” but unverified state produced by a brief pass with almost no gating or finishers.
- Earlier (short run): 13 GOBP iters, gobp_cells_solved=245,009, gating_calls=4, restarts_total=0,
  partial_adoptions=100, boundary_finisher_attempts=0, valid_prefix=0. The row/col averages are very high (
  row_avg_pct≈93.93%, col_avg_pct≈93.93%), but no rows are LH‑verified at the prefix (valid_prefix=0). That
  is a cosmetically “complete” but unverified state produced by a brief pass with almost no gating or finishers.
- Latest (full, instrumented run): 471 GOBP iters, gobp_cells_solved=1,020,531, gating_calls=302, restarts_total=78,
  partial_adoptions=55,992,boundary_finisher_attempts=511, valid_prefix=0. We plateaued around unknown≈171–172k
  repeatedly and refused to lock rows that don’t pass LH. That conservatism drops row_avg_pct to ≈34.34% and
  full_rows=133, but it prevents the solver from cementing contradictions. In other words: we explored more (4× cells
  processed; heavy restarts and gating) and accepted less unless it survived LH gates.

What changed under the hood:

- Finishers actually engage (boundary_finisher_attempts=511 vs 0), with focused boundary gating; micro‑solvers now run
  late and only under feasibility/LH gates instead of spraying speculative locks early.
- Restarts and “polish shakes” are doing their job: we shake out brittle plateaus and revert to a safe baseline when
  LH/prefix can’t advance, rather than keeping misleadingly “complete” rows.

Bottom line: 

Both runs end with valid_prefix=0. The earlier “high completion” wasn’t real progress toward a verified
prefix; it was an optimistic transient with minimal gating. The current pipeline is stricter, more instrumented, and
actively avoids locking wrong rows. That’s the right direction for turning effort into LH‑verified prefix growth rather
than cosmetic fill — and the counters show the system is now plumbed to try, measure, and iterate where it matters.
