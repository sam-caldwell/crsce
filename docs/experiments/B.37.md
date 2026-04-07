### B.37 Score-Capped Simulated Annealing (Proposed)

#### B.37.1 Motivation

B.34–B.36 established a consistent failure mode: any optimization strategy that drives the
row-concentration score above ≈ 25M enters an anti-correlation zone where depth regresses.
The greedy hill-climber (B.34/B.35) peaks at score ≈ 20M and depth 93,805, then overshoots.
SA with T_init=2000 (B.36) passes through the optimal window too quickly and converges at 33M.

The root cause is the **score proxy overshoot problem**: the capped-quadratic reward function
has no natural stopping criterion — it continues rewarding concentration beyond the point where
additional concentration harms the solver.  A hard score cap prevents this overshoot, while
the SA acceptance criterion (exp(Δ/T) for downhill moves) allows genuine basin-hopping within
the optimal band.

#### B.37.2 Method

**Score-capped SA acceptance rule** (added to `hill_climb_one_sub`):

| Condition                              | Action    |
|---------------------------------------|-----------|
| δ > 0 and score_new ≤ CAP and floor OK| Accept    |
| δ > 0 and score_new > CAP             | Reject    |
| δ < 0 and T > 0                       | exp(δ/T)  |
| δ = 0 or T = 0 and δ < 0             | Reject    |

The score cap prevents the optimizer from ever entering the anti-correlation zone.  The SA
component allows it to make small downhill moves within the cap budget, enabling escape from
the greedy ceiling at 93,805.

**Parameters**:

- `--score-cap 22000000` — hard cap at 22M (inside the 15–25M optimal window, below the 20.7M
  peak from B.34)
- `--anneal --T_init 50 --T_final 5` — low temperature; P(accept|Δ=-500) ≈ 4.5% at T=50;
  explores locally without catastrophic randomization
- `--init tools/b35_best_ltp_table.bin` — start from the B.35 best (93,805); do not waste
  iterations replicating the B.34/B.35 greedy climb
- `--iters 50000000 --checkpoint 50000 --secs 20`

**Temperature calibration at T_init=50**:

| Δ      | P(accept) |
|--------|-----------|
| −50    | 36.8%     |
| −250   | 0.7%      |
| −1000  | ≈ 0       |

At T=50, only very small downhill moves are accepted.  The optimizer stays close to the 93,805
basin but can make short downhill excursions to reach neighbouring basins blocked by a small
score barrier.

**Implementation** — new `--score-cap N` flag in `hill_climb_one_sub`:

```python
# In hill_climb_one_sub (added to uphill acceptance branch):
if delta > 0:
    score_after = current_score + delta
    do_accept = (donor_new >= F_) and (score_after <= score_cap_)
```

`score_cap_` is a new module global defaulting to `sys.maxsize` (disabled), set from `--score-cap`.
`current_score` is passed into the function as a new parameter so the cap can be evaluated
per-move without recomputing the full score.

#### B.37.3 Expected Outcomes

**H1 (optimistic)**: Score-capped SA finds a basin above 93,805 in the 94,000–96,000 range.
The cap prevents overshoot; low-T SA makes basin-hopping excursions the greedy climber cannot.
`--save-best` captures any transient peak above 93,805.

**H2 (moderate)**: Score-capped SA returns consistently to 93,805.  The cap keeps it in the
optimal band but 93,805 is a true global optimum for the score proxy at this scale.  Best depth
unchanged; confirms that proxy-based optimization has reached its ceiling.

**H3 (proxy ceiling confirmed)**: Score-capped SA confirms 93,805 as the proxy ceiling.
Next required step: depth-direct SA (B.37b) or a fundamentally different proxy.

**H4 (cap too tight)**: A cap of 22M is too restrictive; the optimizer gets stuck because
uphill moves are rejected at the boundary and downhill moves lead back to lower-score states.
Symptom: acceptance rate near zero, no depth improvement.  Fix: raise cap to 24M or 26M.

#### B.37.4 Actual Results

**Outcome: H1/H2 partial — score-cap is working; new best depth 92,883 (+882 over starting point).**

| Run      | Init   | Seed | Cap | Best depth | Best T | Final score | Final depth |
|----------|--------|------|-----|------------|--------|-------------|-------------|
| B.37a-s42 | 92,001 | 42  | 22M | 92,001     | —      | 22,000,000  | 91,356      |
| B.37a-s137| 92,001 | 137 | 22M | **92,883** | 28.8   | 22,000,000  | 79,567      |

Best table saved to `tools/b37a_best_s137_ltp_table.bin`.

The score cap functioned as designed: score locked at exactly the cap (21,999,958–22,000,000)
throughout both runs.  SA downhill moves (T=50→5) allowed exploration within the band.

**Observations**:

1. **Score-capped SA finds improvements greedy cannot preserve.** s137 found 92,883 (+882 over
   the 92,001 start).  This is a strictly better result than running uncapped greedy from the same
   starting table (which overshoots to score=65M and regresses to depth=63K).

2. **Score at best-seen = cap boundary.** The 92,883 depth was found at score=21,999,996 — the
   table is essentially AT the cap when at its best.  Starting a follow-on run from this table
   with the same cap allows only SA downhill moves, which is suboptimal.  The next run must
   either use a higher cap (24M) or restart from the lower-score 92,001 table.

3. **T cooling too aggressive.** By T=16 (checkpoint 7 of 8), the depth had already regressed
   below the best.  Most SA benefit occurred in the T=50→20 range.  Future runs could use
   `--T_final 20` and longer `--iters` to keep the optimizer in the productive temperature range.

4. **seed=42 produced no improvement.** The same seed that found 92,001 from FY cannot find
   anything better from the 92,001 geometry — it converges back to the same local optimum.
   Multi-seed search is essential.

**Gap to 100K**: 92,883 → 100,000 = 7,117 cells (+7.7%).  Continuing multi-seed search.

---

### B.37b Multi-Seed Score-Capped SA from Best Known Table (Implemented)

#### B.37b.1 Motivation

B.37a confirmed that score-capped SA finds improvements the uncapped greedy climber cannot
preserve.  seed=137 found 92,883 from 92,001 while seed=42 found nothing — the landscape has
multiple reachable basins depending on the random walk direction.  B.37b extends this by:

1. Running the B.35 seeds (5000 and 22) that previously found 93,231→93,805 via greedy, now
   with the score cap engaged, to see if those same seeds find analogous improvements.
2. Running additional fresh seeds from 92,001 to explore more of the band.
3. Running one seed from the 92,883 table with a raised cap (24M) to give the optimizer room
   to climb from the 92,883 geometry.

#### B.37b.2 Method

Four runs launched in parallel:

| Run        | Init    | Seed  | Cap | --T_init | --T_final |
|------------|---------|-------|-----|----------|-----------|
| B.37b-s5k  | 92,001  | 5000  | 22M | 50       | 20        |
| B.37b-s22  | 92,001  | 22    | 22M | 50       | 20        |
| B.37b-s777 | 92,001  | 777   | 22M | 50       | 20        |
| B.37b-hi   | 92,883  | 42    | 24M | 50       | 20        |

`--T_final 20` replaces 5 to avoid spending iterations at near-zero temperature where SA offers
no benefit over greedy (which is already blocked by the cap).

CLI invocations:

```bash
python3 tools/b32_ltp_hill_climb.py --init /tmp/b35_step1_best.bin \
    --anneal --T_init 50 --T_final 20 --score-cap 22000000 \
    --iters 50000000 --checkpoint 50000 --secs 20 --seed 5000 \
    --out /tmp/b37b_s5k.bin --save-best tools/b37b_best_s5k.bin

python3 tools/b32_ltp_hill_climb.py --init /tmp/b35_step1_best.bin \
    --anneal --T_init 50 --T_final 20 --score-cap 22000000 \
    --iters 50000000 --checkpoint 50000 --secs 20 --seed 22 \
    --out /tmp/b37b_s22.bin --save-best tools/b37b_best_s22.bin

python3 tools/b32_ltp_hill_climb.py --init /tmp/b35_step1_best.bin \
    --anneal --T_init 50 --T_final 20 --score-cap 22000000 \
    --iters 50000000 --checkpoint 50000 --secs 20 --seed 777 \
    --out /tmp/b37b_s777.bin --save-best tools/b37b_best_s777.bin

python3 tools/b32_ltp_hill_climb.py --init tools/b37a_best_s137_ltp_table.bin \
    --anneal --T_init 50 --T_final 20 --score-cap 24000000 \
    --iters 50000000 --checkpoint 50000 --secs 20 --seed 42 \
    --out /tmp/b37b_hi.bin --save-best tools/b37b_best_hi.bin
```

#### B.37b.3 Expected Outcomes

**H1**: seeds 5000 or 22 (the B.35 winners) find analogous improvements with the cap engaged,
reaching 93,000–94,000.  The cap prevents overshoot and allows the chain to continue.

**H2**: No seed exceeds 92,883.  The landscape from 92,001 has only one reachable basin at
92,883 within the 22M cap; other seeds converge back to 92,001 or find intermediate values.

**H3 (raised cap)**: The 24M cap run from 92,883 finds a new local optimum above 92,883,
suggesting that the cap at 22M was constraining too tightly.

#### B.37b.4 Actual Results

**Outcome: H2 — no seed exceeded 92,883 (best from B.37a).**

| Run        | Init    | Seed  | Cap | Best depth | Final depth |
|------------|---------|-------|-----|------------|-------------|
| B.37b-s5k  | 92,001  | 5000  | 22M | 92,706     | 91,592      |
| B.37b-s22  | 92,001  | 22    | 22M | 92,001     | 90,170      |
| B.37b-s777 | 92,001  | 777   | 22M | 92,001     | 91,712      |
| B.37b-hi   | 92,883  | 42    | 24M | 92,883     | 90,840      |

Overall best remains **92,883** (from B.37a-s137).  The raised-cap run (24M from 92,883)
produced no improvement — the 92,883 geometry is a local optimum within cap=24M.

s5k (seed=5000, the B.35 greedy chain winner) found 92,706 but not the 93,231 it found via
uncapped greedy.  This strongly suggests the path to 93,231 required visiting score states
above 22M (and possibly above 24M).  The cap is structurally blocking access to the 93K+ basins.

**Conclusion**: the 93K+ basins found in B.35 require a score path exceeding 22–24M.
The cap must be raised further, OR a different initialization must be found that starts closer
to the 93K basin geometry.  B.37c tests cap=25M and cap=28M to determine the minimum cap
required to reach 93K+.

---

### B.37c Cap-Sweep: Minimum Cap for 93K+ Basins (Implemented)

#### B.37c.1 Motivation

B.37a–b showed that cap=22M and cap=24M cannot reach the 93K+ basins found in B.35 (93,805).
The hypothesis is that those basins lie in a score corridor above 24M but below the
anti-correlation threshold (≈30M).  B.37c tests whether raising the cap to 25M or 28M
unlocks access to these basins, while the SA downhill acceptance prevents the runaway overshoot
that destroyed B.36.

Key constraint: at T_init=50, P(accept|Δ=-250) ≈ 0.7%.  This means SA can make small
downhill excursions but will quickly converge to the cap boundary.  The cap still prevents
runaway, but leaves room to visit states above 22M on the way to 93K+.

#### B.37c.2 Method

Six runs: three cap levels (25M, 28M, 31M) × two seeds each.  All start from the 92,001 table.

| Run        | Init   | Seed  | Cap | --T_init | --T_final |
|------------|--------|-------|-----|----------|-----------|
| B.37c-25-s137 | 92,001 | 137 | 25M | 50 | 20 |
| B.37c-25-s5k  | 92,001 | 5000| 25M | 50 | 20 |
| B.37c-28-s137 | 92,001 | 137 | 28M | 50 | 20 |
| B.37c-28-s5k  | 92,001 | 5000| 28M | 50 | 20 |
| B.37c-31-s137 | 92,001 | 137 | 31M | 50 | 20 |
| B.37c-31-s5k  | 92,001 | 5000| 31M | 50 | 20 |

Rationale for cap levels:
- **25M**: just above the cap that locked us out (24M), minimum increment test.
- **28M**: mid-point between optimal (20M) and anti-correlation onset (≈30M).
- **31M**: approaching the anti-correlation zone; expected to show H4 regression.

#### B.37c.3 Expected Outcomes

**H1**: cap=25M or cap=28M seeds 137/5000 find depth ≥ 93,000 (demonstrating the 93K+ basins
   are reachable within a moderate cap).  This identifies the minimum viable cap.

**H2**: All cap levels converge to ≤ 92,883 — the 93K+ basins are not reachable even at 28M
   because the path requires specifically the uncapped greedy trajectory that passes through
   high-score intermediate states.

**H3 (anti-correlation at 31M)**: cap=31M runs show depth < 92,883 due to overshoot.
   Confirms that the useful cap range is 22–28M.

#### B.37c.4 Actual Results

**Outcome: H3 confirmed — higher caps are uniformly worse. Optimal cap is 22M.**

| Run        | Cap | Seed | Best depth |
|------------|-----|------|------------|
| B.37c-25-s137 | 25M | 137 | 92,472 |
| B.37c-25-s5k  | 25M | 5000| 92,139 |
| B.37c-28-s137 | 28M | 137 | 92,001 |
| B.37c-28-s5k  | 28M | 5000| 92,001 |
| B.37c-31-s137 | 31M | 137 | 92,001 |
| B.37c-31-s5k  | 31M | 5000| 92,001 |

The cap=22M result (92,883 from B.37a-s137) remains the best.  Cap=25M is second-best at 92,472;
cap=28M and above find nothing beyond the 92,001 starting point.

**Key finding: higher caps are worse, not better.** Each increment in cap allows the optimizer to
visit higher-score geometries, but those geometries have uniformly worse depth than the ≤22M band.
The optimal depth geometry is at score≈20–22M, not at 25–31M.

**The structural barrier**: the path from 92,001 to 93,231→93,805 (found in uncapped B.35) requires
*transiting* through intermediate score states that are momentarily high (>22M) but the optimizer
passes *through* rather than converging *to*.  The cap blocks these transits.  The solution is to
use uncapped greedy with fine checkpoints (so `--save-best` captures the transient peaks before
the run overshoots) rather than using a static cap.

**B.37d strategy**: uncapped greedy from 92,001 with checkpoint=10000 and patience=3, multiple
seeds — replicating the B.35 methodology but now with `--save-best` available to preserve peaks.

---

### B.37d Fine-Checkpoint Uncapped Greedy Multi-Seed from Best (Implemented)

#### B.37d.1 Motivation

B.37a–c established that score-capped SA is limited to ≈92,883 because the 93K+ basins require
transiting score states above 22M.  The original B.35 chain found 93,231→93,805 using uncapped
greedy with fine checkpoints (saving every 10K accepted swaps via `--save-best`).  B.37d
replicates this exactly from the 92,001 table, but now with `--patience 3` to stop early and
preserve the best-seen table before the run overshoots into the anti-correlation zone.

The critical parameter difference from B.37b: `--checkpoint 10000` (not 50000).  With 10K
checkpoint, the first save-best fires at 10K accepted swaps, before the greedy climber
overshoots.  With 50K checkpoint, the first fire was already past the peak.

#### B.37d.2 Method

Six seeds from the 92,001 table.  Uncapped (no --score-cap).  Fine checkpoint=10000.
patience=3 stops the run 30K accepted swaps after the last depth improvement.

| Run         | Init   | Seed | Checkpoint | Patience |
|-------------|--------|------|------------|----------|
| B.37d-s5k   | 92,001 | 5000 | 10K        | 3        |
| B.37d-s22   | 92,001 | 22   | 10K        | 3        |
| B.37d-s100  | 92,001 | 100  | 10K        | 3        |
| B.37d-s200  | 92,001 | 200  | 10K        | 3        |
| B.37d-s300  | 92,001 | 300  | 10K        | 3        |
| B.37d-s400  | 92,001 | 400  | 10K        | 3        |

CLI invocation pattern:

```bash
python3 tools/b32_ltp_hill_climb.py --init /tmp/b35_step1_best.bin \
    --iters 10000000 --checkpoint 10000 --patience 3 --secs 20 \
    --seed SEED --out /tmp/b37d_sSEED.bin --save-best tools/b37d_best_sSEED.bin
```

#### B.37d.3 Expected Outcomes

**H1**: seeds 5000 and/or 22 find 93,231–93,805 (reproducing B.35 with fine checkpoint).
   `--patience 3` stops the run before the score collapses to 65M, preserving the best table.

**H2**: All seeds find depths in the 92,001–92,883 range, no improvement over B.37a.
   This would indicate the 93K+ basins are only reachable from a specific starting geometry
   that we no longer have (the B.35 table chain was path-dependent).

**H3**: A seed not in the B.35 chain finds a depth > 93,805.  Unlikely but possible — the
   multi-seed search covers a wider landscape.

#### B.37d.4 Actual Results

**Outcome: H1 confirmed — fine checkpoints reproduce B.35 result; best depth 93,231.**

| Run        | Seed | Best depth |
|------------|------|------------|
| B.37d-s5k  | 5000 | **93,231** ← reproduced B.35 step-2 result |
| B.37d-s22  | 22   | 92,146     |
| B.37d-s100 | 100  | **93,230** ← independently found same basin |
| B.37d-s200 | 200  | 92,001     |
| B.37d-s300 | 300  | 92,594     |
| B.37d-s400 | 400  | 92,001     |

Best tables saved to `tools/b37d_best_s5000.bin` (93,231) and `tools/b37d_best_s100.bin` (93,230).

The critical parameter was `--checkpoint 10000`: with fine checkpoints, `--save-best` fires before
the greedy climber overshoots past the 93K basin.  With checkpoint=50000 (B.37b-s5k), the same
seed only found 92,706 because the first save-best fired after the overshoot.

Two independent seeds (5000 and 100) converged to the same basin at depth 93,230–93,231,
confirming this is a genuine local optimum for the score proxy, not a measurement artifact.

Seed=22 only found 92,146 from 92,001 — this seed's B.35 result of 93,805 came from starting
at 93,231, not 92,001.  The chain structure matters.

**Continuing chain**: from 93,231 (saved), now running B.37e with multiple seeds to find 93,805+.

---

### B.37e Fine-Checkpoint Greedy Chain from 93,231 (Implemented)

#### B.37e.1 Motivation

B.37d reproduced 93,231 via seed=5000 and seed=100 independently.  B.35 step-2 found 93,805 from
93,231 using seed=22 (but with checkpoint=50000 which was coarse).  B.37e tests whether fine
checkpoints (10K) from 93,231 can reliably reproduce 93,805 and whether other seeds find new
higher optima.

#### B.37e.2 Method

Starting table: `tools/b37d_best_s5000.bin` (depth 93,231).
Parameters: `--iters 10000000 --checkpoint 10000 --patience 3 --secs 20`

| Run       | Start | Seed | Checkpoint | Patience |
|-----------|-------|------|------------|----------|
| B.37e-s22   | 93,231 | 22   | 10K | 3 |
| B.37e-s42   | 93,231 | 42   | 10K | 3 |
| B.37e-s137  | 93,231 | 137  | 10K | 3 |
| B.37e-s500  | 93,231 | 500  | 10K | 3 |
| B.37e-s1000 | 93,231 | 1000 | 10K | 3 |
| B.37e-s2000 | 93,231 | 2000 | 10K | 3 |

#### B.37e.3 Expected Outcomes

- **H1 (success)**: seed=22 reproduces 93,805; possibly other seeds find new optima > 93,805.
- **H2 (partial)**: seed=22 reproduces 93,805 but no seed exceeds it; continue chain from 93,805.
- **H3 (stall)**: all seeds plateau at 93,231; landscape has no accessible exit from 93,231.

#### B.37e.4 Actual Results

| Run        | Seed | Best Depth |
|------------|------|------------|
| B.37e-s22  | 22   | **93,805** ← reproduced B.35 exactly |
| B.37e-s42  | 42   | 93,231 (no improvement) |
| B.37e-s137 | 137  | 93,231 (no improvement) |
| B.37e-s500 | 500  | 93,231 (no improvement) |
| B.37e-s1000| 1000 | 93,231 (no improvement) |
| B.37e-s2000| 2000 | 93,231 (no improvement) |

**Outcome: H2.** Seed=22 reliably finds 93,805; the chain FY→92,001→93,231→93,805 is confirmed
fully reproducible.  No seed exceeded 93,805.  Continuing chain: B.37f from 93,805
(`tools/b37e_best_s22.bin`).

---

### B.37f Fine-Checkpoint Greedy Chain from 93,805 (Implemented)

#### B.37f.1 Motivation

The chain FY→92,001→93,231→93,805 is reproducible.  B.37f attempts to extend the chain from
93,805 to a new optimum, using 12 seeds with fine checkpoints to capture peaks before overshoot.

#### B.37f.2 Method

Starting table: `tools/b37e_best_s22.bin` (depth 93,805).
Parameters: `--iters 10000000 --checkpoint 10000 --patience 3 --secs 20`

| Run       | Start  | Seed | Checkpoint | Patience |
|-----------|--------|------|------------|----------|
| B.37f-s22   | 93,805 | 22   | 10K | 3 |
| B.37f-s42   | 93,805 | 42   | 10K | 3 |
| B.37f-s100  | 93,805 | 100  | 10K | 3 |
| B.37f-s137  | 93,805 | 137  | 10K | 3 |
| B.37f-s200  | 93,805 | 200  | 10K | 3 |
| B.37f-s300  | 93,805 | 300  | 10K | 3 |
| B.37f-s400  | 93,805 | 400  | 10K | 3 |
| B.37f-s500  | 93,805 | 500  | 10K | 3 |
| B.37f-s1000 | 93,805 | 1000 | 10K | 3 |
| B.37f-s2000 | 93,805 | 2000 | 10K | 3 |
| B.37f-s5000 | 93,805 | 5000 | 10K | 3 |
| B.37f-s7777 | 93,805 | 7777 | 10K | 3 |

#### B.37f.3 Expected Outcomes

- **H1**: At least one seed finds depth > 93,805; continue chain.
- **H2**: All seeds plateau at 93,805; landscape local minimum confirmed; try broader diversity.
- **H3**: Some seeds find 94,000+; acceleration suggests chain nearing 100K attractor.

#### B.37f.4 Actual Results

All 12 seeds failed to improve over the 93,805 baseline.  Key diagnostic:

| Seed | Baseline | First ckpt score | First ckpt depth | Best |
|------|----------|-----------------|-----------------|------|
| 22   | 93,805   | 29.5M           | 81,550          | 93,805 |
| 42   | 93,805   | 29.5M           | 92,812          | 93,805 |
| 100  | 93,805   | 29.5M           | 72,926          | 93,805 |
| others | 93,805 | 29.5M           | 72K–86K         | 93,805 |

**Outcome: H2 (stall).**  The 93,805 table has score=27.7M — already near the
anti-correlation threshold.  Every accepted greedy swap raises score (+1.8M per 10K accepted),
reaching 33M within 30K accepted swaps.  Depth collapses before any improvement can fire.

**Root cause**: pure greedy hill-climb is structurally blocked from 93,805:
- Starting score 27.7M > 25M threshold; every greedy step raises it further
- First checkpoint at 29.5M → depth already collapsed
- The 94K+ basin (if it exists) is not reachable by pure uphill moves from 27.7M

**Implication**: an escape mechanism is required.  ILS (random kick + hill-climb) or SA
targeting score ≤ 27.7M during exploration may work.  B.37g implements the ILS approach.

---

### B.37g Iterated Local Search (ILS) from 93,805 (Implemented)

#### B.37g.1 Motivation

From 93,805, pure greedy is blocked (score rises monotonically → depth collapse).  ILS applies
a random perturbation ("kick") to escape the local basin, then hill-climbs to a new local
optimum.  If the 94K+ basin is accessible via a random walk of K steps, ILS will find it.

Added `--kick K` to `tools/b32_ltp_hill_climb.py`: applies K random unconstrained swaps to the
loaded table before the hill-climb phase.  Each kick swap: random sub-table, random la/lb/pos_a/pos_b,
applied unconditionally (no score filter, floor, or cap).  Coverage updated for zone-straddling kicks.

#### B.37g.2 Method

Starting table: `tools/b37e_best_s22.bin` (depth 93,805, score=27.7M).
Parameters: `--iters 10M --checkpoint 10000 --patience 3 --secs 20`

Three kick sizes × 6 seeds each = 18 runs:

| Variant | Kick | Seeds       | Notes |
|---------|------|-------------|-------|
| B.37g-A | 500  | 22,42,100,137,500,1000 | Small perturbation |
| B.37g-B | 2000 | 22,42,100,137,500,1000 | Medium perturbation |
| B.37g-C | 10000| 22,42,100,137,500,1000 | Large perturbation |

Expected post-kick scores:
- K=500: ~25-27M (minor score perturbation)
- K=2000: ~22-27M (moderate perturbation)
- K=10000: ~15-22M (significant perturbation, possibly back in optimal band)

#### B.37g.3 Expected Outcomes

- **H1 (success)**: at least one kick-size/seed combination finds depth > 93,805.
- **H2 (kick too small)**: K=500 returns to 93,805; need larger kick for escape.
- **H3 (kick too large)**: K=10000 degrades structure too much → climbs to 91K not 94K.
- **H4 (structural limit)**: no kick size finds 94K+; landscape has no accessible 94K basin.

#### B.37g.4 Actual Results

**Critical bug discovered and fixed**: save-best was never written when the kick itself produced
the best state.  Baseline depth was set but save_best_path was only written when a *checkpoint*
exceeded baseline.  Fix: write save_best_path immediately after baseline measurement.

| Variant | Kick | Seed | Baseline | Best Depth | Mechanism |
|---------|------|------|----------|-----------|-----------|
| B.37g-A | 500  | 1000 | 79,148   | **93,954** | kick→79K, greedy climbed |
| B.37g-B | 2000 | 42   | 94,004   | **94,004** | kick itself landed at 94K |
| B.37g-B | 2000 | 22   | 93,790   | 93,790    | no improvement |
| B.37g-C | 10000| all  | 81K-90K  | ≤89,988   | too destructive |

**Outcome: H1 (success).**  `kick=2000, seed=42` found depth **94,004** — the new best.
Best table: `tools/b37g_k2000_best_s42.bin`.

Two working ILS mechanisms:
1. **Direct basin jump**: kick=2000 lands directly in the 94K basin (kick score 27.5M ≈ 27.7M)
2. **Kick+climb**: kick=500 degrades to 79K, greedy hill-climb reaches 93,954 on the way back

Key finding: `kick=10000` is too destructive (degrades to 81–90K range); `kick=2000` is
sweet spot for basin-escaping from the 94K level.  Continuing chain: B.37h from 94,004.

---

### B.37h ILS Chain from 94,004 (Implemented)

#### B.37h.1 Motivation

B.37g found 94,004 via ILS kick=2000 from 93,805.  B.37h attempts the same strategy from 94,004,
seeking to reach 95,000+.

#### B.37h.2 Method

Starting table: `tools/b37g_k2000_best_s42.bin` (depth 94,004, score=27.7M).
Parameters: `--iters 10M --checkpoint 10000 --patience 3 --secs 20`

| Variant | Kick | Seeds |
|---------|------|-------|
| B.37h-A | 500  | 22,42,100,137,200,300,500,1000,2000,5000 |
| B.37h-B | 2000 | 22,42,100,137,200,300,500,1000,2000,5000 |

#### B.37h.3 Expected Outcomes

- **H1**: at least one seed/kick finds depth > 94,004.
- **H2**: all plateau at 94,004; need even larger kick or more diverse seeds.

#### B.37h.4 Actual Results

| Variant | Kick | Seed | Baseline | Best Depth | Notes |
|---------|------|------|----------|-----------|-------|
| B.37h-A | 500  | 300  | **94,026** | 94,026 | kick landed at 94K+ |
| B.37h-A | 500  | 1000 | 94,002    | 94,002 | |
| B.37h-A | 500  | 137  | 93,995    | 93,995 | |
| B.37h-B | 2000 | 1000 | **94,847** | 94,847 | kick landed at 94.8K ← NEW BEST |
| B.37h-B | 2000 | 500  | 93,780    | 93,780 | |
| B.37h-B | 2000 | 22   | 93,589    | 93,589 | |
| others  | both | —    | 80K–93K   | ≤93,438 | regression from start |

**Outcome: H1 (success).**  `kick=2000, seed=1000` found depth **94,847** — new best.
Best table: `tools/b37h_k2000_best_s1000.bin`.

Chain progression: FY→91,090→92,001→93,231→93,805→94,004→**94,847** (+5,757 / +6.3% over B.26c).

Pattern confirmed: ILS with kick=2000 is the dominant mechanism; the kick itself lands in
higher-depth basins directly (no further hill-climbing improvement).  kick=500 can also find
new optima via the kick+climb mechanism (79K→93,954 in B.37g-A).

---

### B.37i ILS Chain from 94,847 (Implemented)

#### B.37i.1 Motivation

The chain continues with ILS kick from the new best 94,847.  We now have a reliable pattern:
kick=2000 from depth D tends to land in a basin at depth D+600–1000.  Scaling to 100K
would require ~6–10 more ILS steps if each gain is +700–900.

#### B.37i.2 Method

Starting table: `tools/b37h_k2000_best_s1000.bin` (depth 94,847, score=27.35M).
Parameters: `--iters 10M --checkpoint 10000 --patience 3 --secs 20`
Two kick sizes × 12 seeds = 24 runs.

#### B.37i.3 Expected Outcomes

- **H1**: kick=2000 lands at ≥95,500; chain continues.
- **H2**: kick=2000 finds ≤94,847; plateau indicates weakening returns.

#### B.37i.4 Actual Results

| Variant | Kick | Seed | Baseline | Best Depth |
|---------|------|------|----------|-----------|
| B.37i-A | 500  | 137  | **94,484** | 94,484 |
| B.37i-A | 500  | 100  | 94,368    | 94,368 |
| B.37i-A | 500  | 42   | 94,048    | **94,275** (climbed) |
| B.37i-B | 2000 | 7777 | **94,744** | 94,744 |
| B.37i-B | 2000 | 42   | 93,808    | **94,841** (climbed) |
| others  | both | —    | 80K–94K   | ≤94,322 |

**Outcome: H2 (near-plateau).**  Best found: 94,841 (kick=2000, s42) — just 6 below record 94,847.
The 94,847 basin proves hard to escape.  Notable: kick=500/s137 found 94,484 and kick=2000/s7777
found 94,744; these are in different sub-basins than 94,847.

Continuing: B.37j applies larger kicks (1000, 3000, 5000) from 94,847 AND chains from the
94,484/94,744 sub-basins (which may have different landscape structure).

---

### B.37j Broader ILS Sweep (Implemented)

#### B.37j.1 Method

Three kick sizes × 12 seeds from 94,847, plus kick=2000 × 8 seeds from each of 94,484 and 94,744.
Total 36 + 16 = 52 parallel runs.

#### B.37j.2 Actual Results

**From 94,847 (kick=1000, 3000, 5000 × 12 seeds each = 36 runs)**:

| Kick | Best Depth | Notes |
|------|-----------|-------|
| 1000 | 94,642 (s42) | Below 94,847 |
| 3000 | 94,323 (s7777) | Below 94,847 |
| 5000 | 94,141 (s7777) | Below 94,847 |

**From 94,484 (kick=2000 × 8 seeds)**: best 94,743 (s42).
**From 94,744 (kick=2000 × 8 seeds)**: best 94,717 (s22).

**Outcome: H2 (plateau).** No improvement over 94,847 from 52 runs.

---

### B.37k Extended Seed Sweep from 94,847 (Completed)

kick=2000 × 36 seeds (seeds 3000–39000) from 94,847.
Best found: 94,452 (seed=11000).  **No improvement over 94,847.**

Total kick=2000 attempts from 94,847 after B.37h/i/j/k: ~60 runs, best = 94,841 (B.37i s42).

---

### B.37l Further Extension (Implemented)

kick=2000 × 36 seeds (seeds 40000–75000) from 94,847.  Simultaneously: 6 fresh FY chains
(seeds 100,200,300,400,500,999) to explore diverse landscape regions independent of the
current seed=42 chain.

**Motivation**: After 60+ kick=2000 failures from 94,847, either (a) success rate is ~1/100 and
we need more tries, or (b) the landscape around 94,847 has no accessible 95K+ basins via
kick=2000.  The fresh FY chains test hypothesis (b) by finding 92K tables via different paths,
which may have better landscape properties for reaching 95K+.

#### B.37l.1 Actual Results

36 seeds (40000–75000), all below 94,847.  Best: 94,394 (seed=68000).  **No improvement.**

After B.37h/i/j/k/l: 96+ kick=2000 attempts from 94,847 total, none > 94,847.
The 94,847 basin appears locally stable within kick=2000 reach.

---

### B.37m Large-Kick Exploration from 94,847 (Completed)

kick=8000 and kick=15000 × 12 seeds each from 94,847.  patience=5, --iters 30M.

| Kick | Best | Notes |
|------|------|-------|
| 8000 | 94,372 (s200) | Below 94,847 |
| 15000 | 94,208 (s22) | Below 94,847 |

**Confirmed**: larger kicks are not the solution.  kick=8000 gives mean depth ~92K; kick=15000 ~93K.
All remain below 94,847.

---

### B.37n Fresh FY Multi-Seed Step-1 Chains (Completed)

10 fresh FY chains (seeds 42–9999) with checkpoint=100K, patience=5, iters=30M.
(Previous FY runs used checkpoint=10K → terminating at 91,402, only 52K accepted swaps.
B.34 used checkpoint=50K–100K to reach 92K.)

| Seed | Best Step-1 Depth |
|------|------------------|
| 9999 | **92,492** ← new step-1 best |
| 500  | **92,245** |
| 400  | **92,041** |
| 100  | 91,851 |
| 42   | 91,836 |
| 5678 | 91,612 |
| 200,300,999,1234 | 91,522 |

Best tables saved as `tools/b37n_fystep1_best_s{seed}.bin`.  The s9999 chain produces a
92K table from a different landscape trajectory than the original s42 (B.34) chain.
These tables are in different landscape regions and may chain to higher-depth basins.

---

### B.37o Multi-Trajectory ILS Step-2 (Implemented)

kick=2000 × 12 seeds from each of the top-5 FY step-1 tables (seeds 9999, 500, 400, 100, 42).
60 parallel runs total.  Goal: find step-2 basins beyond 94,847 from diverse starting points.

#### B.37o.1 Actual Results

ILS kick=2000 × 12 seeds from each of the top-5 FY step-1 tables (9999, 500, 400, 100, 42).
Best results by starting table:

| Start Table | Seed | Best Depth | Notes |
|-------------|------|------------|-------|
| s9999 (92,492) | 42  | **94,118** | big jump from 92K in one ILS step |
| s500 (92,245)  | any | 93,746     | best from s500 trajectory |
| s400 (92,041)  | —   | ≤93,600    | below s9999/s500 basins |
| s100 (91,851)  | —   | ≤93,300    | weaker starting point |
| s42  (91,836)  | —   | ≤93,200    | below primary chain entry |

The s9999→94,118 result (kick=2000, seed=42) establishes an independent trajectory.
Table saved as `tools/b37q_from9999_best_s42.bin`.

---

### B.37p Pure-Greedy Step-2 from FY Tables (Completed, Failed)

Attempted pure greedy (no kick) from FY transient tables, starting with s9999 (92,492).
All seeds gave 92,009 or regression. Root cause: FY transient tables are NOT at true local
optima; the score is already elevated, and greedy immediately raises it further into the
anti-correlation zone (score > 27M → depth regression). The save-best fires at the initial
transient measurement, then all greedy checkpoints worsen.

**Conclusion**: FY chains produce transient peaks; ILS kick is required for step-2 (B.37q).

---

### B.37q ILS Step-2 from FY Transients (Completed)

ILS kick=2000 from 92,492 (s9999) and 92,245 (s500):

| Start     | Seed | Depth  | Notes |
|-----------|------|--------|-------|
| s9999 42  | 42   | **94,118** | ← kick landed directly in new basin |
| s9999 100 | 100  | 93,420 | |
| s500 42   | 42   | 93,746 | |

s9999 chain step 2: **92,492 → 94,118**.  Table: `tools/b37q_from9999_best_s42.bin`.

---

### B.37r Chain from 94,118 (s9999 Trajectory, Completed)

16 seeds with kick=2000 from 94,118:

| Seed | Best Depth | Notes |
|------|------------|-------|
| 400  | **94,419** | ← new step-3 best on s9999 chain |
| 22   | 94,003 | |
| 1000 | 93,892 | |

s9999 chain step 3: **94,118 → 94,419**.  Table: `tools/b37r_best_s400.bin`.

---

### B.37s Chain from 94,419 (s9999 Trajectory, Completed)

18 seeds with kick=2000 from 94,419.  Pattern: peak always at first checkpoint (10K accepted,
score ~27.5M), then score rises → depth regresses.

| Seed  | Best Depth | Notes |
|-------|------------|-------|
| 20000 | **94,661** | ← new step-4 best on s9999 chain |
| 7777  | 94,122 | |
| 30000 | 94,389 | |
| 9999  | 93,154 | |
| 300   | 92,737 | |
| 1000  | 91,867 | |
| others| ≤91,836 | |

s9999 chain step 4: **94,419 → 94,661**.  Table: `tools/b37s_best_s20000.bin`.

s9999 full chain: FY → 92,492 → 94,118 → 94,419 → 94,661.
Primary chain:    FY → 92,001 → 93,231 → 93,805 → 94,004 → 94,847 (record).

The s9999 chain remains 186 cells below the primary chain record.

---

### B.37t Chain from 94,661 + Extended Primary Sweep (Completed)

**A: 24 seeds from 94,661** (s9999 chain step 5).  kick=2000, checkpoint=10K, patience=3, secs=20.

| Seed  | Depth   | Notes |
|-------|---------|-------|
| 100   | **95,060** | ← NEW OVERALL BEST; peak at ckpt 2 (score=31M) |
| 200   | 94,833 | |
| 20000 | 94,795 | |
| 70000 | 94,730 | |
| 50000 | 94,475 | |
| 9999  | 94,295 | |
| others| ≤93,985 | |

Significant detail: s100 peak at checkpoint 2 (20K accepted, score=31M), not checkpoint 1 as
usual.  Baseline after kick was 94,408; ckpt 1 gave 83,728 (worse); ckpt 2 gave 95,060 (best);
ckpt 3 gave 80,055; stopped at ckpt 5 (patience exhausted).  The proxy score at the optimum
was 31M, higher than the typical 27.5M window — the optimal score band may extend higher
than previously characterized.

Best table: `tools/b37t_best_s100.bin`.  s9999 chain step 5: **94,661 → 95,060**.

**B: 12 seeds from 94,847** (primary chain, new seeds 111111–999999 range).
All below 94,847.  Best: s111111=93,537.  Primary chain confirmed stuck at 94,847.

---

### B.37u Chain from 95,060 (Completed)

24 seeds with kick=2000 from `tools/b37t_best_s100.bin` (depth 95,060).

| Seed  | Depth  | Notes |
|-------|--------|-------|
| 300   | **95,408** | ← NEW RECORD; kick→baseline 94,573; peak ckpt 1 score=32.8M |
| 90000 | 95,197 | |
| 1000  | 95,079 | |
| 25000 | 94,521 | |
| others| ≤91,870 | |

Three seeds exceeded 95,060.  Key detail: initial table score was 31M (up from 27.5M in B.37t).
The optimal score window is shifting upward as table quality improves.
Peak consistently at first checkpoint (10K accepted) then regression.

Best table: `tools/b37u_best_s300.bin`.  s9999 chain step 6: **95,060 → 95,408**.

s9999 chain: FY → 92,492 → 94,118 → 94,419 → 94,661 → 95,060 → **95,408**.

---

### B.37v Chain from 95,408 (Completed)

24 seeds with kick=2000 from `tools/b37u_best_s300.bin` (depth 95,408).

| Seed  | Depth  | Notes |
|-------|--------|-------|
| 100   | **95,973** | ← NEW RECORD; baseline 95,970; peak ckpt 1 score=34.5M |
| 200   | 94,769 | |
| 40000 | 94,234 | |
| 1000  | 94,068 | |
| others| ≤93,420 | |

Noteworthy: baseline after kick was 95,970 (kick itself nearly at peak). Only +3 cells gained
by ckpt 1.  Initial table score was 32.6M (rising from 31M in B.37t → 31.9M B.37u → 32.6M).
The optimal score window shifts upward with each improvement.

Best table: `tools/b37v_best_s100.bin`.  s9999 chain step 7: **95,408 → 95,973**.

s9999 chain: FY → 92,492 → 94,118 → 94,419 → 94,661 → 95,060 → 95,408 → **95,973**.

---

### B.37w Chain from 95,973 (Completed — Stall Confirmed)

24 seeds with kick=2000 from `tools/b37v_best_s100.bin` (depth 95,973).
Best: s1000=94,468.  All below 95,973.

The s9999 chain has stalled.  Initial score of the 95,973 table is ~33-34M (beyond the
previously characterized optimal window of 15-27M).  Each improvement step adds ~1-2M to
the initial score; we appear to be at or near the ceiling for this trajectory.

Score trajectory: 27.5M (94,661) → 31M (95,060) → 32.6M (95,408) → ~33M (95,973) → stall.

---

### B.37x Plateau-Breaking Strategies (Completed — Plateau Confirmed)

Three parallel strategies to escape the 95,973 basin:

**A: kick=500 from 95,973** (14 seeds):
best = 95,068 (s2000); all others ≤94,261. All below 95,973.

**B: kick=5000 from 95,973** (12 seeds):
best = 95,391 (s137); all others ≤94,867. All below 95,973.

**C: kick=2000 from 95,197** (12 seeds, alternative B.37u runner-up):
best = 94,915 (s300). All below 95,973.

**Diagnosis**: 95,973 table initial score is ~33-34M (outside previously known optimal window
of 15-27M).  Kicks bring it down 0.5-2M but cannot return to the 20-25M zone needed for
sustained hill-climbing.  The table has converged too far and lost exploitable degrees of
freedom.  Score trajectory: 27.5M→31M→32.6M→33M+; each step makes the next harder.

---

### B.37y Broad Fresh-Seed Sweep from 95,973 (Completed — Plateau Confirmed)

28 unexplored seeds (11111–314159) with kick=2000 from 95,973.
Best: 93,930 (s800008).  All 28 seeds below 95,973.

Distribution: range 81K–93.9K, mean ~90K.  Much worse than kicking from 94,661 (which found 95K+).
The 95,973 basin is well-isolated.  Total kick=2000 attempts from 95,973 across B.37w/x-A/y: 66 runs.
Zero found > 95,973.  **Plateau confirmed at 95,973 for kick=2000 ILS from current table.**

---

### B.37z Fresh Trajectory Attempt (Running)

**Step 1** (completed): 14 fresh FY chains with unexplored seeds (10000–555000),
checkpoint=100K, patience=5.

| Seed   | Step-1 Depth |
|--------|-------------|
| 77777  | **92,595** ← new step-1 best from fresh batch |
| 33333  | 92,557 |
| 111000 | 91,882 |
| 222000 | 91,361 |
| 555000 | 91,276 |
| others | 87,911–89,333 |

**Step 2** (completed): ILS kick=2000 from s77777 (92,595) and s33333 (92,557), 16 seeds each.

| Start | Best depth | Notes |
|-------|-----------|-------|
| s77777 | 93,856 (s400) | |
| s33333 | 92,623 (s500) | |

Both trajectories stall far below 95,973.  The s77777/s33333 basins are weaker than s9999,
which uniquely produced the strong 94K+ chain.  No new independent basin found.

**B.37 series final conclusion**: ILS with kick=2000 and current row-concentration proxy
reached **95,973** (s9999 chain, `tools/b37v_best_s100.bin`).  Exhaustive plateau
confirmation: 66+ kick=2000 runs from 95,973, 28 from 95,408 via B.37v-check, and 2 fresh
trajectory attempts (s77777/s33333) — all fail to exceed 95,973.

The binding constraint is not seed choice but the initial score level: the 95,973 table
has score ~33-34M, above the productive greedy window.  A kick of ≤2000 swaps reduces score
by only ~500K, insufficient to return to the optimal 20-25M band.  The ILS mechanism is
limited by kick size vs. table concentration — increasing kick beyond 5000 destroys enough
structure that the table climbs back only to 91-94K.

The row-concentration proxy has driven a **+5.36% improvement** over B.26c baseline
(91,090 → 95,973).  To continue, B.38 must either use a different optimization mechanism
or address the kick-concentration mismatch directly.

---

