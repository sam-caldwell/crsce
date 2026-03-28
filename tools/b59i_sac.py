#!/usr/bin/env python3
"""
B.59i: Singleton Arc Consistency at S=127 (from B.6)

At S=511: SAC cost ~1.7s per assignment (prohibitive).
At S=127: SAC cost ~25ms per pass (feasible).

SAC: for each free cell j, tentatively assign j=0, propagate; if infeasible,
j must be 1 (and vice versa). Iterate until no new forcings.

Usage:
    python3 tools/b59i_sac.py
    python3 tools/b59i_sac.py --block 2
"""

import argparse
import collections
import json
import numpy as np
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
S = 127
N = S * S
DIAG_COUNT = 2 * S - 1

LCG_A = 6364136223846793005
LCG_C = 1442695040888963407
LCG_MOD = 1 << 64
SEED1 = int.from_bytes(b"CRSCLTPV", "big")
SEED2 = int.from_bytes(b"CRSCLTPP", "big")


def build_yltp(seed):
    pool = list(range(N)); state = seed
    for i in range(N - 1, 0, -1):
        state = (state * LCG_A + LCG_C) % LCG_MOD
        j = int(state % (i + 1)); pool[i], pool[j] = pool[j], pool[i]
    mem = [0] * N
    for l in range(S):
        for s in range(S): mem[pool[l * S + s]] = l
    return mem


YLTP1 = build_yltp(SEED1)
YLTP2 = build_yltp(SEED2)


def load_csm(data, block_idx):
    csm = np.zeros((S, S), dtype=np.uint8)
    start = block_idx * N
    for i in range(N):
        sb = (start + i) // 8
        if sb < len(data) and (data[sb] >> (7 - (start + i) % 8)) & 1:
            csm[i // S, i % S] = 1
    return csm


class ConstraintSystem:
    """Mutable constraint system with snapshot/restore for SAC probing."""

    def __init__(self, csm):
        self.targets = []
        self.members = []
        self._build(csm)
        self.cell_lines = [[] for _ in range(N)]
        for i, m in enumerate(self.members):
            for j in m:
                self.cell_lines[j].append(i)

        self.rho = list(self.targets)
        self.u = [len(m) for m in self.members]
        self.determined = {}
        self.free = set(range(N))

    def _build(self, csm):
        for r in range(S):
            self.targets.append(int(np.sum(csm[r, :]))); self.members.append([r * S + c for c in range(S)])
        for c in range(S):
            self.targets.append(int(np.sum(csm[:, c]))); self.members.append([r * S + c for r in range(S)])
        for d in range(DIAG_COUNT):
            off = d - (S - 1); m = []; s = 0
            for r in range(S):
                cv = r + off
                if 0 <= cv < S: s += int(csm[r, cv]); m.append(r * S + cv)
            self.targets.append(s); self.members.append(m)
        for x in range(DIAG_COUNT):
            m = []; s = 0
            for r in range(S):
                cv = x - r
                if 0 <= cv < S: s += int(csm[r, cv]); m.append(r * S + cv)
            self.targets.append(s); self.members.append(m)
        for mt in [YLTP1, YLTP2]:
            ls = [0] * S; lm = [[] for _ in range(S)]
            for f in range(N): ls[mt[f]] += int(csm[f // S, f % S]); lm[mt[f]].append(f)
            for k in range(S): self.targets.append(ls[k]); self.members.append(lm[k])

    def snapshot(self):
        return (dict(self.determined), set(self.free), list(self.rho), list(self.u))

    def restore(self, snap):
        self.determined, self.free, self.rho, self.u = snap[0], snap[1], snap[2], snap[3]

    def assign(self, j, v):
        """Assign cell j = v and propagate. Returns False if inconsistent."""
        if j not in self.free:
            return self.determined.get(j) == v

        self.determined[j] = v
        self.free.discard(j)
        for li in self.cell_lines[j]:
            self.u[li] -= 1
            self.rho[li] -= v

        # Propagate
        q = collections.deque()
        for li in self.cell_lines[j]:
            q.append(li)
        inq = set(q)

        while q:
            i = q.popleft(); inq.discard(i)
            if self.u[i] == 0:
                continue
            if self.rho[i] < 0 or self.rho[i] > self.u[i]:
                return False

            fv = None
            if self.rho[i] == 0: fv = 0
            elif self.rho[i] == self.u[i]: fv = 1
            if fv is None: continue

            for jj in self.members[i]:
                if jj not in self.free: continue
                self.determined[jj] = fv
                self.free.discard(jj)
                for li2 in self.cell_lines[jj]:
                    self.u[li2] -= 1
                    self.rho[li2] -= fv
                    if li2 not in inq: q.append(li2); inq.add(li2)

        return True

    def initial_propagation(self):
        """Full IntBound propagation."""
        q = collections.deque(range(len(self.targets)))
        inq = set(q)
        while q:
            i = q.popleft(); inq.discard(i)
            if self.u[i] == 0: continue
            if self.rho[i] < 0 or self.rho[i] > self.u[i]: return False
            fv = None
            if self.rho[i] == 0: fv = 0
            elif self.rho[i] == self.u[i]: fv = 1
            if fv is None: continue
            for j in self.members[i]:
                if j not in self.free: continue
                self.determined[j] = fv; self.free.discard(j)
                for li in self.cell_lines[j]:
                    self.u[li] -= 1; self.rho[li] -= fv
                    if li not in inq: q.append(li); inq.add(li)
        return True


def sac_pass(cs):
    """One SAC pass: probe each free cell with v=0 and v=1.

    Returns number of newly forced cells.
    """
    forced = 0
    to_force = []  # (cell, value)

    for j in sorted(cs.free):
        v0_ok = True
        v1_ok = True

        # Probe v=0
        snap = cs.snapshot()
        if not cs.assign(j, 0):
            v0_ok = False
        cs.restore(snap)

        # Probe v=1
        snap = cs.snapshot()
        if not cs.assign(j, 1):
            v1_ok = False
        cs.restore(snap)

        if not v0_ok and not v1_ok:
            return -1  # contradiction
        elif not v0_ok:
            to_force.append((j, 1))
        elif not v1_ok:
            to_force.append((j, 0))

    # Apply forcings
    for j, v in to_force:
        if j in cs.free:
            cs.assign(j, v)
            forced += 1

    return forced


def main():
    parser = argparse.ArgumentParser(description="B.59i: SAC at S=127")
    parser.add_argument("--block", type=int, default=0)
    parser.add_argument("--max-sac-iters", type=int, default=20)
    parser.add_argument("--out", default=str(REPO_ROOT / "tools" / "b59i_results.json"))
    args = parser.parse_args()

    mp4_path = REPO_ROOT / "docs" / "testData" / "useless-machine.mp4"
    data = mp4_path.read_bytes()

    print("B.59i: Singleton Arc Consistency at S=127")
    print(f"  S={S}, N={N}")
    print()

    csm = load_csm(data, args.block)
    density = np.sum(csm) / N
    print(f"Block {args.block}: density={density:.3f}")

    cs = ConstraintSystem(csm)

    # Initial propagation
    print("Initial propagation...", flush=True)
    t0 = time.time()
    cs.initial_propagation()
    print(f"  Determined: {len(cs.determined)} ({len(cs.determined)/N*100:.1f}%), "
          f"Free: {len(cs.free)}, Time: {time.time()-t0:.2f}s")

    # SAC iterations
    print(f"\nSAC iterations (max {args.max_sac_iters}):")
    sac_stats = []
    total_sac_forced = 0

    for iteration in range(1, args.max_sac_iters + 1):
        n_free_before = len(cs.free)
        t0 = time.time()
        forced = sac_pass(cs)
        elapsed = time.time() - t0

        if forced == -1:
            print(f"  Iter {iteration}: CONTRADICTION")
            break

        total_sac_forced += forced
        pct = len(cs.determined) / N * 100
        print(f"  Iter {iteration}: forced={forced}, |D|={len(cs.determined)} ({pct:.1f}%), "
              f"|F|={len(cs.free)}, time={elapsed:.1f}s")

        sac_stats.append({
            "iteration": iteration,
            "forced": forced,
            "determined": len(cs.determined),
            "free": len(cs.free),
            "time_s": round(elapsed, 2),
        })

        if forced == 0:
            print("  Fixpoint reached.")
            break

    print(f"\n{'='*60}")
    print(f"B.59i Results — Block {args.block}")
    print(f"{'='*60}")
    print(f"  After IntBound: {len(cs.determined) - total_sac_forced} cells")
    print(f"  SAC forced:     {total_sac_forced} additional cells")
    print(f"  Total:          {len(cs.determined)} / {N} ({len(cs.determined)/N*100:.1f}%)")
    print(f"  Free:           {len(cs.free)}")
    print(f"  SAC iterations: {len(sac_stats)}")

    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    result = {
        "experiment": "B.59i",
        "block": args.block,
        "density": round(density, 4),
        "intbound_determined": len(cs.determined) - total_sac_forced,
        "sac_forced": total_sac_forced,
        "total_determined": len(cs.determined),
        "free": len(cs.free),
        "iterations": sac_stats,
    }
    out_path.write_text(json.dumps(result, indent=2))
    print(f"  Results: {out_path}")


if __name__ == "__main__":
    main()
