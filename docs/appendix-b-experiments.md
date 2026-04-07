## Appendix B. Experiments

This appendix documents all experiments conducted during the CRSCE research program (B.1 through B.63).
Each experiment is stored in a separate file under `docs/experiments/`.

### Early Solver Experiments (B.1-B.19)

- [B.1](experiments/B.1.md) — Abandoned
- [B.2](experiments/B.2.md) — Auxiliary Cross-Sum Partitions as Solver Accelerators
- [B.3](experiments/B.3.md) — Variable-Length Curve Partitions as LH Replacement
- [B.4](experiments/B.4.md) — Dynamic Row-Completion Priority in Cell Selection
- [B.5](experiments/B.5.md) — Hash Alternatives to Improve Search Depth
- [B.6](experiments/B.6.md) — Singleton Arc Consistency
- [B.7](experiments/B.7.md) — Neighborhood-Based Lookahead
- [B.8](experiments/B.8.md) — Adaptive Lookahead
- [B.9](experiments/B.9.md) — Non-Linear Lookup-Table Partitions
- [B.10](experiments/B.10.md) — Constraint-Tightness-Driven Cell Ordering
- [B.11](experiments/B.11.md) — Randomized Restarts with Heavy-Tail Mitigation
- [B.12](experiments/B.12.md) — Survey Propagation and Belief-Propagation-Guided Decimation
- [B.13](experiments/B.13.md) — Portfolio and Parallel Solving with Diversification
- [B.14](experiments/B.14.md) — Lightweight Nogood Recording
- [B.15](experiments/B.15.md) — DELETED
- [B.16](experiments/B.16.md) — Partial Row Constraint Tightening
- [B.17](experiments/B.17.md) — Look-Ahead on Row Completion
- [B.18](experiments/B.18.md) — Learning from Repeated Hash Failures
- [B.19](experiments/B.19.md) — Enhanced Stall Detection and Extended Probe Strategies

### LTP Optimization and Partition Experiments (B.20-B.37)

- [B.20](experiments/B.20.md) — LTP Substitution Experiment: Geometry versus Position
- [B.21](experiments/B.21.md) — Joint-Tiled Variable-Length LTP Partitions
- [B.22](experiments/B.22.md) — Partition Seed Search for Uniform-511 LTP
- [B.23](experiments/B.23.md) — Clipped-Triangular LTP Partitions
- [B.24](experiments/B.24.md) — LTP Hill-Climber: Direct Cell-Assignment Optimisation (ABANDONED)
- [B.25](experiments/B.25.md) — Reserved
- [B.26](experiments/B.26.md) — Joint Seed Search for LTP Sub-Tables
- [B.27](experiments/B.27.md) — Increasing Constraint Density via LTP5 and LTP6
- [B.28](experiments/B.28.md) — Interior-Byte Variant
- [B.29](experiments/B.29.md) — Single-Byte Prefix with CRSCLTP Suffix
- [B.30](experiments/B.30.md) — Pincer Decompression Option A: Propagation Sweep on Plateau (ABANDONED)
- [B.31](experiments/B.31.md) — Pincer Decompression Option B: Full Alternating-Direction DFS
- [B.32](experiments/B.32.md) — Four-Direction Iterative Pincer
- [B.33](experiments/B.33.md) — Complete-Then-Verify Solver with Constraint-Preserving Local Search
- [B.34](experiments/B.34.md) — LTP Table Hill-Climbing with Moderate Threshold and Save-Best
- [B.35](experiments/B.35.md) — Multi-Start Iterated Hill-Climbing
- [B.36](experiments/B.36.md) — Simulated Annealing on LTP Score Proxy
- [B.37](experiments/B.37.md) — Score-Capped Simulated Annealing (includes B.37b, B.37c, B.37d)

### Intermediate Experiments (B.38-B.57)

- [B.38](experiments/B.38.md) — Deflation-Kick ILS (SATURATED)
- [B.39](experiments/B.39.md) — N-Dimensional Constraint Geometry (PESSIMISTIC)
- [B.40](experiments/B.40.md) — Hash-Failure Correlation Harvesting (NULL)
- [B.41](experiments/B.41.md) — Cross-Dimensional Hashing + Four-Direction Pincer (INFEASIBLE)
- [B.42](experiments/B.42.md) — Pre-Branch Pruning Spectrum
- [B.43](experiments/B.43.md) — Bottom-Up Initialization via Row-Completion Look-Ahead (INFEASIBLE)
- [B.44](experiments/B.44.md) — Constraint Density Breakthrough
- [B.45](experiments/B.45.md) — LTP-Only Constraint System
- [B.46](experiments/B.46.md) — Variable-Length rLTP5/rLTP6
- [B.47](experiments/B.47.md) — Center-Spiral Variable-Length LTP Partitions (UNCHANGED)
- [B.48](experiments/B.48.md) — All-Spiral rLTP Constraint System (REGRESSION)
- [B.49](experiments/B.49.md) — Extended Hybrid: 4 Geometric + 2 yLTP + 4 rLTP
- [B.50](experiments/B.50.md) — rLTP Construction Alternatives
- [B.51](experiments/B.51.md) — rLTP Spirals Above and Below the Plateau (REGRESSION)
- [B.52](experiments/B.52.md) — Joint yLTP+rLTP Optimization for 100K Depth
- [B.54](experiments/B.54.md) — SMT Hybrid Solver with CRC-32 Row Verification
- [B.55](experiments/B.55.md) — LP Relaxation with Iterative Rounding
- [B.56](experiments/B.56.md) — Hierarchical Multi-Resolution Reconstruction
- [B.57](experiments/B.57.md) — Reduced Matrix Dimension S=127

### Combinator-Algebraic Solver (B.58-B.60)

- [B.58](experiments/B.58.md) — Combinator-Based Symbolic Solver at S=127
- [B.59](experiments/B.59.md) — Experiments at S=127 (DFS baselines)
- [B.60](experiments/B.60.md) — Vertical CRC-32 Hash: Cross-Axis GF(2) Constraints (includes B.60a-s)

### S=191 Optimization and Load-Bearing Analysis (B.61-B.62)

- [B.61](experiments/B.61.md) — Overlapping Blocks (FAILED)
- [B.62](experiments/B.62.md) — Optimizing at S=191 (includes B.62a-m: rLTP, load-bearing constraints, hybrid)

### Hybrid Combinator + DFS at S=127 (B.63)

- [B.63](experiments/B.63.md) — Hybrid Combinator + DFS (includes B.63a-i: sweep, restarts, CDCL, beam search, information budget)
