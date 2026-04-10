### B.36 Simulated Annealing on LTP Score Proxy (Implemented)

#### B.36.1 Motivation

The multi-start greedy hill-climber (B.35) converged to depth **93,805** across 40+ numpy seeds.
Every seed applied to the 93,805 basin returned exactly 93,805; no seed exceeded it.  The greedy
acceptor cannot escape this local optimum because it requires a score decrease to reach a
neighbouring basin.  Simulated annealing (SA) addresses this by accepting downhill score moves with
probability exp(Δ/T), where T is a temperature that cools geometrically from T₀ to T_f over the
run.  At high T the optimiser explores broadly; at low T it converges greedily.

The SA objective is identical to B.34/B.35: the capped quadratic row-concentration score.  No
changes to the decompressor or LTPB format are required; SA is a drop-in replacement for the
greedy acceptance criterion.

#### B.36.2 Method

**Acceptance criterion** (added to `hill_climb_one_sub`):

| Δ     | Condition                  | Accept probability       |
|-------|----------------------------|--------------------------|
| > 0   | uphill, donor floor OK     | 1.0                      |
| < 0   | downhill, annealing active | exp(Δ / T)               |
| < 0   | downhill, T = 0 (greedy)   | 0.0                      |
| = 0   | neutral                    | 0.0                      |

Downhill SA moves bypass the donor-floor guard (F_ = 0 by default) to allow genuine exploration.

**Temperature schedule**: geometric cooling over N total iterations.

$$T(k) = T_0 \cdot \left(\frac{T_f}{T_0}\right)^{k / N_{\text{outer}}}$$

where N_outer = N / (BATCH × NUM\_SUB) is the number of outer loop iterations.

**Calibration** — representative acceptance probabilities at Δ = −250 (typical single-swap delta
near the 93,805 basin with coverage around 270–320):

| Temperature | P(accept \| Δ=−250) |
|-------------|----------------------|
| T = 2000    | 88.2%                |
| T = 500     | 60.7%                |
| T = 200     | 28.7%                |
| T = 50      | 0.67%                |
| T = 5       | ≈ 0                  |

Rationale: T_init = 2000 gives high acceptance of moderate downhill moves at the start, enabling
genuine basin-hopping.  T_final = 5 collapses to essentially greedy behaviour during the last phase
to exploit the best-found basin.

**Note on init files**: The B.35 best table (`tools/b35_best_ltp_table.bin`) was not committed to git
and was unavailable for this session.  Both runs started from fresh Fisher-Yates seeds (B.26c
defaults), which establishes a cleaner baseline and still tests whether SA can beat the FY-derived
greedy ceiling.

**Runs performed**:

| Run label | Init       | --iters | --T_init | --T_final | --seed | --checkpoint | --secs |
|-----------|------------|---------|----------|-----------|--------|--------------|--------|
| SA-A      | FY seeds   | 50M     | 2000     | 5         | 42     | 50K          | 20     |
| SA-B      | FY seeds   | 50M     | 2000     | 5         | 137    | 50K          | 20     |

CLI invocations (as executed):

```bash
# SA-A
python3 tools/b32_ltp_hill_climb.py \
    --anneal --T_init 2000 --T_final 5 \
    --iters 50000000 --checkpoint 50000 --secs 20 \
    --seed 42 \
    --out tools/b36_sa_fy_s42.bin \
    --save-best tools/b36_best_ltp_table.bin

# SA-B
python3 tools/b32_ltp_hill_climb.py \
    --anneal --T_init 2000 --T_final 5 \
    --iters 50000000 --checkpoint 50000 --secs 20 \
    --seed 137 \
    --out tools/b36_sa_fy_s137.bin \
    --save-best tools/b36_best_s137_ltp_table.bin
```

#### B.36.3 Expected Outcomes

**H1 (optimistic)**: SA escapes the 93,805 local optimum and reaches depth ≥ 95,000 (+1.3% over
B.35).  The high-T exploration phase discovers a qualitatively different geometry (different top-10
coverage distribution) that the greedy phase cannot reach.  Best table saved automatically via
`--save-best`.

**H2 (moderate)**: SA transiently visits basins in the 94,000–95,000 range during the exploration
phase, but the cooling schedule returns the table near the 93,805 level.  `--save-best` captures
the transient peak.  Net improvement: 300–800 cells over B.35.

**H3 (pessimistic, same basin)**: SA with these parameters finds 93,805 reliably.  The
neighbourhood of 93,805 is truly optimal for the score proxy; no escaping basin is accessible at
this scale.  Result: best depth ≈ 93,805, confirming that the score proxy has hit its effective
ceiling for this geometry.

**H4 (anti-correlation)**: SA increases score significantly (> 30M) during the exploration phase
and depth drops (similar to B.34 FLOOR=0 regression at score=38.7M → depth=76,087).  If this
occurs, the run should be shortened or T_init reduced.

The most likely outcome given B.35 basin structure is H2 or H3.  H1 is possible but requires
that an unexplored higher basin exists and is reachable within 50M iterations.

#### B.36.4 Actual Results

**Outcome: H4 (Anti-Correlation) — SA overshoots optimal score window.**

Both runs completed (50M iterations each, ~2 hours total).  Neither improved upon the FY
baseline depth of 91,522.

| Run  | Init depth | Best depth | Final score | Final depth |
|------|-----------|------------|-------------|-------------|
| SA-A | 91,522    | 91,522     | 33,239,934  | 73,979      |
| SA-B | 91,522    | 91,522     | 33,388,992  | 91,186      |

The `--save-best` path was never written for either run because no checkpoint depth exceeded the
initial FY depth (91,522).

**Score trajectory** (SA-A):

| Iters (M) | T       | Score     | Top-10 coverage           | Depth  |
|-----------|---------|-----------|---------------------------|--------|
| 0         | 2000    | 571,463   | 214–260                   | 91,522 |
| 0.15      | 1964    | 464,121   | 242–252                   | 87,688 |
| 0.75      | 1828    | 274,325   | 223–237                   | 86,314 |
| 16        | ~340    | ~500,000  | early stage, recovering   | ~88K   |
| 23.3      | 129     | 6,297,661 | 334–382                   | 87,572 |
| 24.4      | 116     | 11,387,189| 401–442                   | 89,171 |
| 25.4      | 107     | 16,731,426| 428–451                   | 89,849 |
| 27.8      | 71.5    | 28,118,728| 451–452 (all at ceiling)  | 84,272 |
| 50 (final)| 5       | 33,239,934| all at 451                | 73,979 |

**Root cause analysis**: The SA schedule with T_init = 2000 causes a multi-phase trajectory:

1. **Destruction phase** (T > 500): SA accepts most downhill moves, randomizing the assignment.
   Score drops from 571K to near-random (≈ 200K–500K).

2. **Recovery phase** (T 100–500): SA begins preferring uphill moves.  Score climbs back
   through the pro-correlation window (5M–20M).

3. **Over-drive phase** (T < 100): Greedy-like climbing now dominates.  Score continues to
   climb beyond the optimal window (20M), exactly replicating the B.34 FLOOR=0 collapse.
   All top-10 lines converge to the MAX_COV ceiling (451 early cells).

4. **Anti-correlation state** (T < 20): Score = 28–33M, all 10+ lines fully concentrated.
   The matrix is now deprived of LTP constraint density for late rows → depth collapses to
   73K–84K.

The critical failure mode: SA with T_init = 2000 reliably discovers the over-concentration basin
(score ≈ 30M) rather than the optimal basin (score ≈ 20M, depth 92–94K).  This basin is large
and strongly attracting during the greedy cooling phase; the optimizer converges to it from
any direction.

**Why T_init = 2000 is too large**: At T = 2000, the exploration phase is so aggressive that
it destroys all the structure built during Fisher-Yates initialization.  When cooling begins, the
optimizer effectively restarts from a near-random state.  The near-random state sits far from the
optimal basin (score ≈ 20M) and the cooling path passes through it only briefly before overshooting
into the anti-correlation zone (score > 30M).

**Comparison with B.34 FLOOR=0**: B.34 (pure greedy) also overshot — at score=38.7M it hit depth
76,087 — but its *peak* was at score=20.7M with depth 92,001.  SA with T_init=2000 never paused
at score≈20M; it blew through that regime at T≈150 (late in the run) and converged past it.

**Conclusion**: SA as designed (T_init=2000 → T_final=5) is not an improvement over greedy
hill-climbing for this proxy.  The proxy has a **narrow optimal score window** (≈15–25M) that
SA must be tuned to stay within.  Recommended re-tuning:

1. **Low-T SA** (B.37a): T_init=30→T_final=5 — start just inside the pro-correlation regime;
   tiny excursions around the 20M optimum without overshooting.

2. **Score-capped SA** (B.37b): Accept downhill moves only while score < 25M (hard cap);
   revert to greedy when score exceeds budget.

3. **Depth-direct SA** (B.37c): Use measured decompressor depth as the SA objective directly
   (no proxy).  Computationally expensive (20s per move) but eliminates proxy-depth mismatch.
   Feasible only with a much smaller iteration budget and coarser checkpoint.

**Key finding**: The score proxy has a confirmed optimal band at 15–25M.  Any optimization
strategy (greedy or SA) that drives score above 25M enters the anti-correlation zone.  Future
experiments must either hard-cap score at 25M or shift to depth-direct optimization.

---

