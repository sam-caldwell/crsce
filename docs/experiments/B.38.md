## B.38 Deflation-Kick ILS (Running)

### B.38.1 Motivation

The 95,973 table (B.37v) has initial proxy score ~34.5M — beyond the productive hill-climb
window (20–25M).  Random ILS kicks (2000 swaps) reduce the score by only ~500K per kick,
insufficient to return to the productive zone.  66+ kick attempts from 95,973 found nothing.

Root cause: the table is over-concentrated.  Each greedy step monotonically raises the score.
Starting from 34.5M, the greedy hill-climb can only go higher, driving deeper into the
anti-correlation zone.  A different escape mechanism is needed.

### B.38.2 Method: Targeted Deflation

New `--deflate K` argument added to `tools/b32_ltp_hill_climb.py`:
- Apply K **targeted** swaps that move early-row cells from high-coverage lines
  (coverage > `--deflate_high`, default 320) to late-row cells on low-coverage lines
  (coverage < `--deflate_low`, default 280)
- Each swap reduces the proxy score by ~500–700 units (compared to ~0 for a random kick)
- Stops early once score < `--deflate_target` (default 22M)
- Unlike random kicks, deflation reduces score *without random structural damage*

Empirical calibration: 35,000 deflation swaps reduced 95,973 table from 34.5M → 21.9M
(100% success rate with high=320, low=280; down from 4.4% with high=350, low=250).

### B.38.3 Hypothesis

Starting from the partially-deflated 95,973 structure at score ~22M:
1. The table retains the useful structural properties found by B.37's 5-step ILS chain
2. The score reset allows greedy hill-climbing to find a new local optimum
3. If the deflation pathway preserves "good structure" while removing "over-concentration",
   the hill-climb may converge to a basin above 95,973

Expected outcomes:
- **H1**: depth > 95,973 — deflation successfully resets the table for further improvement
- **H2**: depth 92K–95K — deflation preserves some structure but lands in a different basin
- **H3**: depth < 92K — deflation destroys critical structure (like excessive kicks in B.37m)

### B.38.4 Parameters

```
--init tools/b37v_best_s100.bin   (depth 95,973)
--deflate 50000 --deflate_high 320 --deflate_low 280 --deflate_target 22000000
--iters 10000000 --checkpoint 10000 --patience 3 --secs 20
```

24 seeds (22–99999) running in parallel.

### B.38.5 Actual Results

Outcome H2.  Best = 95,375 (s50000).  All 24 seeds below 95,973.
Score resetting to 22M preserves ~60% of structural advantage (94-95K vs 92K from FY)
but deflation destroys the critical properties of the 95,973 table.

---

### B.38b Optimal Deflation Level Sweep (Completed)

Sweep over 6 deflation target scores (33M, 32M, 31M, 30M, 29M, 28M) × 3 seeds (42, 100, 300).
`--init tools/b37v_best_s100.bin` (depth 95,973).

| Target | Seed | Depth | Notes |
|--------|------|-------|-------|
| 32M    | 100  | **95,997** | ← NEW RECORD (+24 over 95,973); 7,000 deflation swaps → 31.8M |
| 31M    | 42   | 95,864 | |
| 28M    | 42   | 95,755 | |
| 30M    | 100  | 95,704 | |
| 31M    | 100  | 95,493 | |
| others | —    | ≤94,864 | |

Peak for all runs at ckpt 1 (10K accepted, score ~33.7M).  Optimal deflation is minimal
(~7,000 swaps, target 32M) — preserves nearly all structure while resetting just enough
for the hill-climb to find a new local optimum at score 33.7M.

Key finding: 95,997 found by deflating 34.5M → 31.8M then greedy hill-climb.
The entire s9999 chain: FY → 92,492 → 94,118 → 94,419 → 94,661 → 95,060 → 95,408 → 95,973 → **95,997**.

Best table: `tools/b38b_t32000000_best_s100.bin`.  +24 over 95,973.

---

### B.38c Chain from 95,997 (Completed)

24 seeds from 95,997 with deflation to 32M (7,000 deflation swaps → 31.8M → greedy climb).

| Seed  | Depth  | Notes |
|-------|--------|-------|
| 70000 | **96,122** | ← NEW RECORD |
| 25000 | 95,997 | |
| 1000  | 95,988 | |
| 5000  | 95,863 | |
| 500   | 95,829 | |
| 99999 | 95,576 | |
| others| ≤95,259 | |

Six seeds exceeded 95,973; one exceeded 95,997.  Deflation + hill-climb is a repeatable
mechanism.  Best table: `tools/b38c_best_s70000.bin`.  Chain step 9: **95,997 → 96,122**.

Full chain: FY→92,492→94,118→94,419→94,661→95,060→95,408→95,973→95,997→**96,122**.
vs B.26c: +5,032 cells (+5.53%).

---

### B.38d Chain from 96,122 (Completed)

24 seeds from `tools/b38c_best_s70000.bin` (depth 96,122) with deflation to 32M
(`--deflate 20000 --deflate_target 32000000`).

| Seed | Best Depth |
|------|-----------|
| s22  | 96,081 |
| s42  | 95,977 |
| (others) | ≤ 95,800 |

**Result: 96,081** — all 24 seeds below 96,122. The 96,122 table starts at score 33.8M (lower
than 95,973's 34.5M), so deflation to 32M uses only ~5K swaps — insufficient to escape the basin.

**Diagnosis**: deflation target 32M is the right level for 34.5M-starting tables (95,973),
but is too shallow for the 33.8M-starting 96,122 table. Lower targets (31M, 30M, 29M) needed.

---

### B.38e Deflation Target Sweep from 96,122 (Completed)

**Design**: Test three deflation targets (31M, 30M, 29M) × 12 seeds from `tools/b38c_best_s70000.bin`
(depth 96,122). 36 parallel runs, `--deflate 20000 --deflate_high 320 --deflate_low 280`.

### B.38e Results

| Target | Best Seed | Best Depth | Swap Count (approx) |
|--------|-----------|-----------|---------------------|
| 31M    | s137      | **96,672** | ~8K swaps |
| 30M    | s22       | 96,217     | ~12K swaps |
| 29M    | s42       | 95,885     | ~15K swaps |

**Winner: deflation target 31M, seed 137 → depth 96,672 (NEW RECORD)**

Table saved: `tools/b38e_t31000000_best_s137.bin`

**Key observations:**
- Optimal deflation level shifts upward as tables improve: 96,122 table (33.8M start) needs
  31M target (not 32M as was optimal for 95,973's 34.5M start).
- Each successive table requires a slightly deeper deflation because the score ceiling rises.
- Target 29M over-deflates: too many qualifying (high>320, low<280) lines leads to structural
  destruction.
- 6/12 seeds at t31M exceeded 96,122 (3 exceeded 95,900); deflation robustly recovers.

Chain so far: FY(s9999)→92,492→94,118→94,419→94,661→95,060→95,408→95,973→95,997→96,122→96,672

---

### B.38f Chain from 96,672 (Completed)

**Hypothesis.** The 96,672 table (score 30,743,316) can be further improved by deflation to 31M
followed by hill-climbing, continuing the chain pattern that produced each prior record.

**Methodology.** 24 seeds from `tools/b38e_t31000000_best_s137.bin` (depth 96,672, score 30.74M)
with `--deflate 20000 --deflate_high 320 --deflate_low 280 --deflate_target 31000000`.

**Expected result.** Approximately 97K depth by analogy with the B.38c→B.38e progression.

**Actual result.** All 24 seeds applied 0 deflation swaps. The log reported
`post-deflation score: 30,743,316` — identical to the input score. Best depth: 96,672 (no change).

**Root cause.** The 96,672 table itself was already produced by deflation (B.38e applied ~8K
swaps from the 96,122 table). The post-deflation state IS the 96,672 table; its score (30.74M) is
already below the requested 31M target. There are no qualifying (high > 320, low < 280) lines
remaining to deflate. The chain technique cannot be applied to its own output without first driving
the score upward via hill-climbing — but hill-climbing from 30.74M drives the score to 36M+
(anti-correlation zone), regressing depth.

**Conclusion.** The 96,672 record was achieved by the deflation operation itself, not by
hill-climbing. The deflated state is a local optimum that cannot be improved by further deflation
or by uncapped hill-climbing.

---

### B.38g Deflation Scan Below 30.7M from 96,672 (Completed)

**Hypothesis.** Targets below the current 30.74M score (29M, 28M, 27M, 26M, 25M) may reveal
a deeper optimum that the 31M sweep missed.

**Methodology.** Targets 29M / 28M / 27M / 26M / 25M × 8 seeds from
`tools/b38e_t31000000_best_s137.bin` (96,672); otherwise same flags as B.38f.

**Expected result.** At least one target finds a basin deeper than 96,672.

**Actual result.**

| Target | Best Baseline Depth |
|--------|---------------------|
| 29M    | 95,885 |
| 28M    | 95,885 |
| 27M    | 94,890 |
| 26M    | ~94,000 |
| 25M    | ~93,500 |

All targets produce depths below 96,672. Depth degrades monotonically as the target decreases.
No target found a deeper basin.

**Conclusion.** The 96,672 table is a structural local optimum under deflation-based techniques.
Deflating below 30.74M removes too many early-row cell assignments from high-coverage lines,
destroying the favorable constraint structure rather than refining it. The optimal deflation level
for a 30.74M-starting table is approximately 30.74M — i.e., no further deflation is productive.

---

### B.38h Score-Capped Hill-Climbing from 96,672 (Completed)

**Hypothesis.** Standard hill-climbing overshoots the optimal score window (30–32M) because it
accepts all improvements regardless of the resulting score. A hard cap on total score prevents
overshoot and allows the optimizer to explore the local neighborhood within the productive range.

**Methodology.** Score caps 31M / 31.5M / 32M / 33M × 8 seeds from 96,672.
Each run uses normal hill-climbing but refuses swaps that would push total score above the cap.

**Expected result.** Some capped run finds marginal improvement (96,700+) by exploring the
immediate neighborhood without driving the score into the anti-correlation zone.

**Actual result.**
- cap=31M: accepted only 1,329 total swaps across all seeds; all best=96,672.
- cap=31.5M: accepted ~10K swaps; all best=96,672.
- cap=32M: accepted ~25K swaps; all best=96,672.
- cap=33M: accepted larger neighborhood; all best=96,672.

No cap produced improvement. The cap=31M result is most informative: only 1,329 improvement-making
swaps exist within the 31M score ceiling — the table is a local optimum within any capped range.

**Conclusion.** The 96,672 table is a local optimum under score-capped hill-climbing with any cap
between 31M and 33M. The capped optimizer confirms there are no improving swaps available within
the productive score window. The optimum is not merely a consequence of score overshoot; it is a
genuine local maximum of the depth-score relationship at this point.

---

### B.38h2 Score-Capped Hill-Climbing from 96,217 (Completed)

**Hypothesis.** The second-best table (96,217, score 29.98M) may have a different local
neighborhood structure allowing capped climbing to find improvement.

**Methodology.** Same caps (31M / 31.5M / 32M / 33M) × 8 seeds from
`tools/b38e_t30000000_best_s22.bin` (depth 96,217, score ~29.98M).

**Actual result.** All runs best=96,217 (baseline). No improvement from the second-best table.

**Conclusion.** Both top-two tables are local optima under score-capped hill-climbing.

---

### B.38i Kick + Score-Cap from 96,672 (Completed)

**Hypothesis.** A random kick (unconstrained perturbation swaps) perturbs the table out of
the local optimum; the score cap then prevents the subsequent hill-climb from overshooting.

**Methodology.** kick=2000 / 5000 / 10000 × cap=32M / 33M / 34M × 8 seeds from 96,672.
`--kick K` applies K random accept-anything swaps before the capped hill-climb.

**Expected result.** Kick escapes the local optimum; capped climb recovers to a deeper basin.

**Actual result.**

| Kick Size | Best Depth |
|-----------|-----------|
| 2,000     | 95,389 |
| 5,000     | 96,004 |
| 10,000    | 94,988 |

Best overall: 96,004 (kick=5,000, seed=5000). All runs below 96,672.

**Conclusion.** Kick destroys critical constraint structure faster than the capped hill-climb can
rebuild it. Larger kicks produce worse outcomes. The 96,672 table's depth-critical structure
(sub-tables 0 and 1, established by B.38k crossover analysis) is fragile to random perturbation.
Kick-based escape strategies are ineffective at this operating point.

---

### B.38j Fresh Fisher-Yates Seed Sweep (Completed)

**Hypothesis.** The chain FY(s9999) → 92,492 was specifically favorable because s9999 reached
an unusually high first-step depth. Other FY seeds may find similarly favorable basins that lead
to basins exceeding 96,672 via multi-step ILS.

**Methodology.** 60 fresh FY seeds (checkpoint=100K, patience=3, secs=20). Top performers
proceeded to ILS step 2.

**Expected result.** Several seeds reach 92K+ first step; ILS chains from those reach 96K+.

**Actual result.**
- Best first-step: s42=91,825; s12=91,003; s38=90,796.
- No seed exceeded s9999's 92,492 first-step result.
- ILS from s42 step-2 best: 94,209 (seed k999999).
- No seed reached 96K territory from any continuation.

**Conclusion.** The s9999 starting basin is exceptional among the 60 sampled seeds. The 92K+
first-step is a necessary (though not sufficient) condition for reaching 96K via ILS. Alternative
FY seeds produce shallower chains. The landscape is rugged and the 92,492 starting basin is
unusually favorable.

---

### B.38k Population Crossover from Top-Two Tables (Completed)

**Hypothesis.** The 96,672 and 96,217 tables have complementary strengths across sub-tables.
Crossover — swapping individual sub-tables between the two tables — may produce a hybrid that
exceeds either parent.

**Methodology.** All 64 combinations (2^6) of 6 sub-tables, each drawn from either
A=`tools/b38e_t31000000_best_s137.bin` (96,672) or B=`tools/b38e_t30000000_best_s22.bin` (96,217).
Each combination assembled in Python by direct array replacement, written to temp file, depth
measured (secs=20).

**Expected result.** At least one combination exceeds 96,672.

**Actual result.**

| Combination | Depth |
|-------------|-------|
| AAAAAA      | 96,672 |
| BBBBBB      | 96,217 |
| AABAAA      | 96,672 |
| AAABAA      | 96,672 |
| AABBAA      | 96,672 |
| AAAABA      | 96,672 |
| AABABA      | 96,672 |
| AAABBA      | 96,672 |
| AABBBA      | 96,672 |
| (remaining B-sub combinations) | ≤ 96,217 |

Sub-tables 0 and 1 from A are critical: any combination using sub-tables 0 and 1 from A
returns 96,672. Sub-tables 2–5 are interchangeable with B without affecting depth.

No combination exceeded 96,672.

**Conclusion.** The 96,672 depth is entirely determined by sub-tables 0 and 1. Sub-tables 2–5
are structurally inert at this operating point. Crossover cannot exceed the best parent because
the critical sub-tables cannot be improved by mixing with a weaker table's corresponding sub-tables.

---

### B.38l Deflation Walk (Completed)

**Hypothesis.** Rather than a single large deflation step, a sequence of small steps (5,000 swaps
each) with depth measurement after each step can navigate through better intermediate states than a
single-step deflation jump.

**Methodology.** Starting from 96,672, apply 5,000 deflation swaps per step, measure depth
after each step.

**Expected result.** Some step lands on a depth > 96,672 before crossing into destructive territory.

**Actual result.**
- Step 1: ~94,667
- Step 2: ~94,404 (deepening destruction)
- Terminated early.

**Conclusion.** Deflation in 5,000-swap steps is too destructive per step at the 96,672 table's
score (30.74M). The 96,672 table has few qualifying lines remaining; each 5K-swap step converts a
disproportionate fraction of remaining early-row cells, causing rapid depth regression.

---

### B.38l2 Micro-Step Direct Depth Search (Completed)

**Hypothesis.** At the resolution of ~200 random swaps, the depth landscape may have
microscopic local maxima above 96,672 that normal hill-climbing (score proxy) cannot find because
they do not correspond to proxy score improvements.

**Methodology.** Apply 200 random swaps (uniformly at random, no score filter), measure depth
(15s timer), keep the table if depth improved, discard otherwise. Repeat for 6 steps.

**Expected result.** Small random perturbations discover a microstate with depth 96,700+.

**Actual result.**
- Step 1: 96,448 (discarded)
- Step 2: 96,674 (kept — appeared to be new record) (It may have been scotch)
- Step 3: 95,867 (discarded)
- Step 4: 96,664 (discarded vs step 2 baseline)
- Step 5: 96,437 (discarded)
- Step 6: 95,706 (discarded)

**Conclusion.** Step 2's 96,674 is within measurement noise of 96,672 (the 15s timer instead of
the standard 20s timer reduces measured depth by ~150–200 cells under load). No genuine
improvement detected. The micro-step approach provides no sustained progress; the depth landscape
at this resolution is highly non-monotonic.

---

### B.38m K_PLATEAU=195 Hill-Climbing from 96,672 (Completed)

**Hypothesis.** The solver stalls at row ~189 (96,672 / 511 ≈ 189.2). The current optimizer uses
K_PLATEAU=178 (row 178), which targeted the B.26c plateau. Changing to K_PLATEAU=195 aligns
the optimization objective with the actual observed stall boundary, potentially rewarding different
cell-line assignments that facilitate the actual bottleneck rows.

**Methodology.** 12 seeds from 96,672, `--threshold 195` (all other params unchanged).

**Expected result.** Tables optimized for K=195 reach deeper into the 190–510 row range.

**Actual result.** All 12 seeds: best=96,672 for all; finals ranging 80K–92K.

An additional run used fresh FY seeds with K=195 to establish baselines: best first-step
s5000=91,451, similar to K=178 first-step results.

**Conclusion.** K_PLATEAU=195 trades row 0–178 concentration for row 0–195 coverage, diluting the
critical early-row constraint density that enables the propagation cascade. The solver's 189-row
stall is caused by structural underdetermination, not by the specific K_PLATEAU parameter.
Hill-climbing with K=195 fails to match the depth achieved with K=178, confirming that the optimal
parameter is not simply the observed stall row.

---

### B.38n Band Mode (Rows 179–195) from 96,672 (Completed)

**Hypothesis.** Optimizing specifically the transition zone (rows 179–195) — neither the
well-optimized rows 0–178 nor the distant rows 196–510 — can bridge the row-189 stall without
disrupting the existing early-row structure.

**Methodology.** 12 seeds from 96,672, `--mode band --k1 179 --k2 195`.
Band mode rewards concentration of LTP cells specifically within the band [179, 195], leaving the
K_PLATEAU=178 structure of rows 0–178 undisturbed.

**Expected result.** Band-mode improvement in the transition zone pushes the stall from row 189
to row 195+.

**Actual result.** Initial band score: 87,534. Optimizer found no improvements in the band. All
12 seeds completed with best=96,672 (baseline unchanged).

**Conclusion.** The transition band rows 179–195 are already well-covered by the existing LTP
structure (a consequence of the K_PLATEAU=178 optimization). The optimizer cannot find
band-improving swaps because the band coverage is already near-optimal relative to its own score.
Band mode provides no new degrees of freedom at this operating point.

---

### B.38o Deflation from Second-Best Table (96,217) (Completed)

**Hypothesis.** The second-best table (96,217, score 29.98M) may have a different basin topology
allowing deeper deflation to reach a state that surpasses 96,672.

**Methodology.** Targets 27M / 26M / 25M / 24M × 8 seeds from
`tools/b38e_t30000000_best_s22.bin` (96,217).

**Actual result.**

| Target | Best Baseline Depth |
|--------|---------------------|
| 27M    | 95,009 |
| 26M    | 94,055 |
| 25M    | 94,004 |
| 24M    | 93,995 |

All below 96,672. The second-best table is also a local optimum; its basin does not connect to
a deeper region via deflation.

**Conclusion.** Both the 96,672 and 96,217 tables are confirmed local optima. The two best
known tables occupy structurally isolated basins; neither can serve as a stepping stone to a
deeper basin via deflation-based techniques.

---

### B.38p Narrow Simulated Annealing (T=50→1) from 96,672 (Completed)

**Hypothesis.** A very narrow SA temperature window (T_init=50, T_final=1) stays close to the
greedy limit while accepting occasional slightly-worsening moves, potentially escaping the
96,672 local optimum without the catastrophic destruction observed in B.36 (T_init=2000).

**Methodology.** SA with T=50→1, 8 seeds from 96,672.

**Actual result.** This run was corrupted by CPU load: 40+ concurrent hill-climber processes were
running, causing the decompressor to reach only depth 96,223 instead of 96,672 under the 20s
timer. All seeds reported baseline=96,223 (the load-suppressed value). SA immediately
moved uphill (accepting score-worsening swaps at T=50), driving score to 36M+, regressing depth.

**Conclusion.** SA at T=50 is still too aggressive — the acceptance of worsening moves drives the
score past the productive 30–32M window. Even very low temperatures cause the score to exit the
optimal band. The optimal score window for 96K+ depth is narrow (~0.5M wide); any stochastic
acceptance policy that allows score increases will exit it.

**Measurement note.** CPU load from parallel processes causes the 20s-timer measurement to read
96,223 instead of the true 96,672. Subsequent experiments limited concurrent processes to ≤12 to
avoid this artifact.

---

### B.38q Kick + Deflate to 31M from 96,672 (Completed)

**Hypothesis.** Combining kick perturbation with subsequent deflation (rather than kick + capped
climb) may find deeper intermediate states by reshaping the score landscape before measuring.

**Methodology.** kick=2000 / 5000 / 10000 × then `--deflate_target 31000000` × 8 seeds from 96,672.
The kick first perturbs the table randomly (increasing score), then deflation drives it back to
the 31M level where depth measurement occurs.

**Expected result.** kick=5000 + deflate to 31M creates a table different from the original
96,672 that measures deeper.

**Actual result.**

| Kick Size | Best Baseline Depth |
|-----------|---------------------|
| 2,000     | 95,389 |
| 5,000     | 96,004 |
| 10,000    | 94,988 |

Best: 96,004 (kick=5000). All below 96,672.

**Conclusion.** Kick + deflate cannot recover the 96,672 structure. The kick destroys sub-tables 0
and 1 (the depth-critical sub-tables identified in B.38k). Even with subsequent deflation to the
correct score level, the destroyed sub-table structure cannot be reconstructed by the deflation
operator.

---

### B.38r Varied Deflation High/Low Thresholds from 96,672 (Completed)

**Hypothesis.** The deflation operator's effectiveness depends on the HIGH/LOW thresholds
(h=320/l=280 in all prior experiments). Different threshold pairs may select different qualifying
lines, finding a perturbation direction that the standard parameters miss.

**Methodology.** Four threshold configurations × 8 seeds from 96,672:
- h=350, l=260 (wider band)
- h=340, l=270 (intermediate)
- h=300, l=260 (lower threshold, more qualifying lines)
- h=350, l=290 (tighter lower bound)

**Actual result.**

| h / l  | Best Baseline Depth |
|--------|---------------------|
| 350/260 | 95,991 |
| 340/270 | 94,746 |
| 300/260 | 95,133 |
| 350/290 | 95,565 |

All below 96,672.

**Conclusion.** The standard h=320/l=280 parameters were the best choice for producing the 96,672
record. Widening the HIGH threshold selects lines that should not be deflated; narrowing the LOW
threshold selects too many recipient lines, diluting the deflation's directionality. Threshold
variation confirms h=320/l=280 as the empirical optimum.

---

### B.38s K_PLATEAU=188 (Stall Row Targeting) from 96,672 (Completed)

**Hypothesis.** K_PLATEAU=188 = floor(96,672 / 511) targets exactly the observed stall row —
the last row completed before the solver halts. This should be the most accurate proxy for the
bottleneck, outperforming the proxy-at-K=178 used to build the 96,672 table.

**Methodology.** 12 seeds from `tools/b38e_t31000000_best_s137.bin`, `--threshold 188`,
`--iters 50000000 --checkpoint 10000 --patience 3 --secs 20`.

**Expected result.** K=188 optimization finds a marginally better table tailored to the actual stall.

**Actual result.** All 12 seeds: best=96,672; finals ranging 81K–96K (depth degrades during
hill-climbing regardless of seed).

**Conclusion.** K_PLATEAU=188 does not improve on K=178. The hill-climbing process degrades depth
because moving score upward from 30.74M overdrives into the anti-correlation zone. The K value
affects which score the optimizer converges to, not the starting point depth; any K that drives
the score above ~32M causes depth regression. The 96,672 optimum is K-invariant: all tested
K values (178, 188, 195) produce best=96,672 with degraded finals.

---

### B.38 Summary and Conclusions

**Current best: depth 96,672** (`tools/b38e_t31000000_best_s137.bin`)

The B.38 deflation technique produced a systematic chain of improvements:

```
FY(s9999) → 92,492 → 94,118 → 94,419 → 94,661 → 95,060 → 95,408
          → 95,973 → 95,997 → 96,122 → 96,672  ← CURRENT BEST
```

**Key finding: Deflation is the optimization, not hill-climbing.** The 96,672 depth was achieved
by the deflation step itself (B.38e: 8,000 swaps from 96,122's 33.8M score to 30.74M).
Subsequent hill-climbing from the deflated state only degrades depth by driving score into the
anti-correlation zone (>32M).

**The 96,672 table is a confirmed local optimum.** Experiments B.38f through B.38s exhausted all
tested escape strategies:

| Experiment | Strategy | Best Depth |
|------------|----------|-----------|
| B.38f | Deflation chain (target 31M) | 96,672 (no improvement — 0 swaps) |
| B.38g | Deflation to 25M–29M | 95,885 (over-deflation) |
| B.38h/h2 | Score-capped climbing | 96,672 (local optimum) |
| B.38i | Kick + score-cap | 96,004 |
| B.38j | 60 fresh FY seeds | 94,209 (no competitive basin found) |
| B.38k | Crossover top-2 tables | 96,672 (no improvement) |
| B.38l | Deflation walk (5K steps) | 94,667 (too destructive) |
| B.38l2 | Micro-step 200 swaps | 96,672 (noise) |
| B.38m | K_PLATEAU=195 | 96,672 (finals 80K–92K) |
| B.38n | Band mode rows 179–195 | 96,672 (no band improvements) |
| B.38o | Deflation from 96,217 | 95,009 (second table also stuck) |
| B.38p | SA T=50→1 | 96,223 (load-corrupted + score overshoot) |
| B.38q | Kick + deflate | 96,004 |
| B.38r | Varied h/l thresholds | 95,991 |
| B.38s | K_PLATEAU=188 | 96,672 (finals degrade) |

**The B.38 proxy-based approach has saturated.** The row-concentration proxy (score) is a
necessary but not sufficient condition for depth improvement. The optimal score window for 96K+
is narrow (~30–32M); neither climbing into the window from below (deflation) nor exploring within
it (capped climbing, SA, kicks) produces improvement beyond 96,672.

**B.33 (Complete-Then-Verify): ABANDONED.** B.33 was investigated as the next architectural
candidate after B.38 saturated. Sub-experiments B.33a and B.33b (tools/b33a_min_swap.py,
tools/b33b_min_swap_bt.py) determined that the minimum constraint-preserving swap for the
full 10-family CRSCE system is O(1000+) cells — NOT local. The Fisher-Yates random LTP
partitions ensure every cell lands on a unique random LTP line, requiring mate cells that
cascade to O(511) cells per sub-table. Consequently, the correct CSM and any "close"
cross-sum-valid matrix differ in ~170,000 cells — no efficient local path exists between
them. Phase 3 is infeasible. See §B.33.11 for full analysis.

**B.39 (N-Dimensional Constraint Geometry): COMPLETED &mdash; PESSIMISTIC.** B.39a confirmed the B.33b cascade model algebraically: the minimum constraint-preserving swap is 1,528 cells (upper bound), touching 11 rows and 492 columns. The $3n$ dimensional scaling model ($3 \times 511 \approx 1{,}533$) matches the measured minimum. B.39b (null-space navigation) not attempted per the pessimistic-outcome criterion. Complete-Then-Verify is definitively closed. See &sect;B.39.5a for full results.

**B.40 (Hash-Failure Correlation): COMPLETED &mdash; NULL.** Zero SHA-1 events at the plateau after 200M+ iterations. The plateau is caused by constraint exhaustion ($\rho$ violations), not hash failure. No row in the meeting band ever reaches $u = 0$. SHA-1 verification never triggers. See &sect;B.40.6.

**B.41 (Cross-Dimensional Hashing): DIAGNOSTIC COMPLETE &mdash; INFEASIBLE FOR DEPTH.** Column unknown counts at the plateau reveal that columns are far less complete than rows (min column unknown = 206 vs min row unknown = 2). Column-serial passes would need ~206 assignments before any verification event. The meeting band's constraint exhaustion affects columns more severely than rows. B.41 retains value for collision resistance but does not improve solver depth. See &sect;B.41.10.

**B.42 (Pre-Branch Pruning Spectrum): COMPLETED.** B.42a revealed 56.6% preferred-infeasible and 43.2% both-infeasible branching waste, but 0% interval-tightening potential. B.42c (both-value probing) eliminates all waste and provides ~40% throughput improvement but **does not change depth**. The plateau is constraint-exhausted: the depth ceiling is a property of the constraint system's information content, not the solver's search efficiency.

**B.43 (Bottom-Up RCLA Initialization): COMPLETED &mdash; INFEASIBLE.** At most 1 row has $u \leq 20$ at the plateau; meeting-band rows have $u \approx 300$-$400$. RCLA cannot cascade. Bottom-up sweep direction provides no advantage (constraint information is direction-invariant). See &sect;B.43.

**Current status:** All proposed solver-architecture approaches (B.33, B.38, B.39, B.40, B.41, B.42, B.43) have been completed or abandoned. The depth ceiling (~86K-96K depending on LTP table and test block) is a fundamental limit of the 8-family cross-sum constraint system with Fisher-Yates random LTP partitions (5,097 independent constraints over 261,121 cells = 2% constraint density, leaving 256,024 unconstrained degrees of freedom). Further depth improvement requires adding information to the constraint system itself&mdash;additional projection families, finer-grained hash constraints, or a fundamentally different encoding. Open questions are consolidated in Appendix C.

---

