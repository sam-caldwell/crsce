#!/usr/bin/env python3
"""
B.59g: SAT/ILP on S=127 Residual (from B.54)

At S=511: ILP timeout, SAT OOM (395M clauses), LP = random guessing.
At S=127: residual ~12,529 cells, ~1,014 lines, ~2M clauses — should fit in memory.

Method:
  1. Build CSM from MP4 block 0, run IntBound propagation (= DFS initial propagation)
  2. Extract residual: free cells + active constraints
  3. Encode as:
     (a) LP relaxation (scipy) — continuous relaxation of cardinality constraints
     (b) SAT with cardinality constraints (PySAT sequential counter) + CRC-32 XOR clauses
  4. Compare solution accuracy to correct CSM

Usage:
    python3 tools/b59g_sat_ilp.py
    python3 tools/b59g_sat_ilp.py --block 2
"""

import argparse
import collections
import json
import sys
import time
import zlib
import numpy as np
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
S = 127
N = S * S
DIAG_COUNT = 2 * S - 1

LCG_A = 6364136223846793005
LCG_C = 1442695040888963407
LCG_MOD = 1 << 64
PREFIX = b"CRSCLTP"
SEED1 = int.from_bytes(PREFIX + b"V", "big")
SEED2 = int.from_bytes(PREFIX + b"P", "big")


# ---------------------------------------------------------------------------
# Reuse helpers from b58c (IntBound propagation, CSM loading, yLTP)
# ---------------------------------------------------------------------------
def build_yltp_membership(seed):
    n = S * S
    pool = list(range(n))
    state = seed
    for i in range(n - 1, 0, -1):
        state = (state * LCG_A + LCG_C) % LCG_MOD
        pool[i], pool[int(state % (i + 1))] = pool[int(state % (i + 1))], pool[i]
    mem = [0] * n
    for line in range(S):
        for slot in range(S):
            mem[pool[line * S + slot]] = line
    return mem


YLTP1 = build_yltp_membership(SEED1)
YLTP2 = build_yltp_membership(SEED2)


def load_csm(data, block_idx):
    bits_per_block = S * S
    start_bit = block_idx * bits_per_block
    csm = np.zeros((S, S), dtype=np.uint8)
    for i in range(bits_per_block):
        src_bit = start_bit + i
        src_byte = src_bit // 8
        if src_byte >= len(data):
            break
        if (data[src_byte] >> (7 - src_bit % 8)) & 1:
            csm[i // S, i % S] = 1
    return csm


def build_constraints(csm):
    """Build line targets and member lists."""
    targets = []
    members = []

    for r in range(S):
        targets.append(int(np.sum(csm[r, :])))
        members.append([r * S + c for c in range(S)])
    for c in range(S):
        targets.append(int(np.sum(csm[:, c])))
        members.append([r * S + c for r in range(S)])
    for d in range(DIAG_COUNT):
        offset = d - (S - 1)
        m, s = [], 0
        for r in range(S):
            cv = r + offset
            if 0 <= cv < S:
                s += int(csm[r, cv])
                m.append(r * S + cv)
        targets.append(s)
        members.append(m)
    for x in range(DIAG_COUNT):
        m, s = [], 0
        for r in range(S):
            cv = x - r
            if 0 <= cv < S:
                s += int(csm[r, cv])
                m.append(r * S + cv)
        targets.append(s)
        members.append(m)
    for mem_table in [YLTP1, YLTP2]:
        line_sums = [0] * S
        line_mems = [[] for _ in range(S)]
        for flat in range(N):
            line_sums[mem_table[flat]] += int(csm[flat // S, flat % S])
            line_mems[mem_table[flat]].append(flat)
        for k in range(S):
            targets.append(line_sums[k])
            members.append(line_mems[k])

    cell_lines = [[] for _ in range(N)]
    for i, m in enumerate(members):
        for j in m:
            cell_lines[j].append(i)

    return targets, members, cell_lines


def int_bound_propagation(targets, members, cell_lines):
    """IntBound propagation → determined cells."""
    rho = list(targets)
    u = [len(m) for m in members]
    determined = {}
    free = set(range(N))

    queue = collections.deque(range(len(targets)))
    in_q = set(queue)

    while queue:
        i = queue.popleft()
        in_q.discard(i)
        if u[i] == 0:
            continue
        if rho[i] < 0 or rho[i] > u[i]:
            return determined, free, rho, u

        fv = None
        if rho[i] == 0:
            fv = 0
        elif rho[i] == u[i]:
            fv = 1
        if fv is None:
            continue

        for j in members[i]:
            if j not in free:
                continue
            determined[j] = fv
            free.discard(j)
            for li in cell_lines[j]:
                u[li] -= 1
                rho[li] -= fv
                if li not in in_q:
                    queue.append(li)
                    in_q.add(li)

    return determined, free, rho, u


# ---------------------------------------------------------------------------
# CRC-32 generator matrix
# ---------------------------------------------------------------------------
def build_crc32_gen():
    total_bytes = (S + 1 + 7) // 8
    zero_msg = bytes(total_bytes)
    c = zlib.crc32(zero_msg) & 0xFFFFFFFF
    c_vec = [(c >> (31 - i)) & 1 for i in range(32)]
    G = [[0] * S for _ in range(32)]
    for col in range(S):
        msg = bytearray(total_bytes)
        msg[col // 8] |= (1 << (7 - col % 8))
        val = (zlib.crc32(bytes(msg)) & 0xFFFFFFFF) ^ c
        for i in range(32):
            G[i][col] = (val >> (31 - i)) & 1
    return G, c_vec


G_CRC, C_VEC = build_crc32_gen()


# ---------------------------------------------------------------------------
# LP relaxation (scipy)
# ---------------------------------------------------------------------------
def lp_relaxation(free_set, determined, targets, members, rho, u, csm):
    """Solve LP relaxation: minimize 0 subject to sum constraints, 0 <= x <= 1."""
    from scipy.optimize import linprog
    from scipy.sparse import lil_matrix

    free_list = sorted(free_set)
    n_free = len(free_list)
    free_idx = {j: i for i, j in enumerate(free_list)}

    # Build equality constraints: for each active line, sum of free cells = rho[line]
    active_lines = [i for i in range(len(targets)) if u[i] > 0]
    n_eq = len(active_lines)

    A_eq = lil_matrix((n_eq, n_free))
    b_eq = np.zeros(n_eq)

    for row, li in enumerate(active_lines):
        for j in members[li]:
            if j in free_idx:
                A_eq[row, free_idx[j]] = 1
        b_eq[row] = rho[li]

    # Objective: minimize sum (arbitrary — we just want feasibility)
    c_obj = np.zeros(n_free)

    print(f"  LP: {n_free} variables, {n_eq} equality constraints")
    t0 = time.time()
    result = linprog(c_obj, A_eq=A_eq.tocsr(), b_eq=b_eq,
                     bounds=[(0, 1)] * n_free,
                     method='highs', options={'time_limit': 60})
    elapsed = time.time() - t0

    if result.success:
        x = result.x
        # Round to binary
        predicted = {free_list[i]: int(round(x[i])) for i in range(n_free)}
        # Compare to correct CSM
        correct = 0
        for j in free_list:
            actual = int(csm[j // S, j % S])
            if predicted[j] == actual:
                correct += 1
        accuracy = correct / n_free * 100
        print(f"  LP solved in {elapsed:.1f}s. Accuracy: {accuracy:.1f}% ({correct}/{n_free})")

        # Check how many are at 0/1 vs interior
        at_zero = sum(1 for i in range(n_free) if x[i] < 0.01)
        at_one = sum(1 for i in range(n_free) if x[i] > 0.99)
        interior = n_free - at_zero - at_one
        print(f"  LP solution: {at_zero} at 0, {at_one} at 1, {interior} interior (0.01-0.99)")
    else:
        accuracy = 0
        print(f"  LP failed: {result.message} ({elapsed:.1f}s)")

    return {"method": "LP", "n_vars": n_free, "n_constraints": n_eq,
            "time_s": round(elapsed, 2), "accuracy_pct": round(accuracy, 1),
            "success": result.success}


# ---------------------------------------------------------------------------
# SAT encoding (PySAT)
# ---------------------------------------------------------------------------
def sat_solve(free_set, determined, targets, members, rho, u, csm, row_crcs, timeout=600):
    """SAT with cardinality constraints + CRC-32 XOR clauses."""
    from pysat.solvers import Solver
    from pysat.card import CardEnc, EncType

    free_list = sorted(free_set)
    n_free = len(free_list)
    # PySAT variables are 1-indexed
    var_map = {j: i + 1 for i, j in enumerate(free_list)}
    next_var = n_free + 1

    clauses = []

    # Cardinality constraints for each active line
    active_lines = [(i, rho[i], [j for j in members[i] if j in free_set])
                    for i in range(len(targets)) if u[i] > 0]

    print(f"  SAT: {n_free} variables, {len(active_lines)} active lines")
    t0 = time.time()

    card_clauses = 0
    for li, rho_val, free_members in active_lines:
        if len(free_members) == 0:
            continue
        lits = [var_map[j] for j in free_members]
        if rho_val == 0:
            # All must be 0
            for lit in lits:
                clauses.append([-lit])
                card_clauses += 1
        elif rho_val == len(free_members):
            # All must be 1
            for lit in lits:
                clauses.append([lit])
                card_clauses += 1
        else:
            # sum(lits) == rho_val → sum >= rho_val AND sum <= rho_val
            # Use sequential counter encoding
            try:
                atmost = CardEnc.atmost(lits, bound=rho_val, top_id=next_var,
                                        encoding=EncType.seqcounter)
                if atmost.clauses:
                    nv = max(abs(l) for cl in atmost.clauses for l in cl)
                    next_var = max(next_var, nv + 1)
                clauses.extend(atmost.clauses)
                card_clauses += len(atmost.clauses)

                atleast = CardEnc.atleast(lits, bound=rho_val, top_id=next_var,
                                          encoding=EncType.seqcounter)
                if atleast.clauses:
                    nv = max(abs(l) for cl in atleast.clauses for l in cl)
                    next_var = max(next_var, nv + 1)
                clauses.extend(atleast.clauses)
                card_clauses += len(atleast.clauses)
            except Exception as e:
                print(f"  WARNING: Card encoding failed for line {li} (u={len(lits)}, rho={rho_val}): {e}")
                continue

    print(f"  Cardinality clauses: {card_clauses:,}")

    # CRC-32 XOR clauses (native XOR support via assumptions or encoding)
    # PySAT doesn't have native XOR, encode as CNF: XOR(a,b,...) = parity
    xor_clauses = 0
    for r in range(S):
        h = row_crcs[r]
        h_vec = [(h >> (31 - i)) & 1 for i in range(32)]

        for bit in range(32):
            # G_CRC[bit] . x_r = h_vec[bit] XOR C_VEC[bit]
            target_bit = h_vec[bit] ^ C_VEC[bit]

            # Collect free variables in this row with coefficient 1
            xor_lits = []
            parity = target_bit
            for c in range(S):
                flat = r * S + c
                if G_CRC[bit][c] == 0:
                    continue
                if flat in determined:
                    parity ^= determined[flat]
                elif flat in var_map:
                    xor_lits.append(var_map[flat])

            if len(xor_lits) == 0:
                # All variables determined — check consistency
                if parity != 0:
                    clauses.append([])  # Empty clause = UNSAT
                continue

            # Encode XOR constraint: XOR(lits) = parity
            # For small XOR (<= 10 vars), use Tseitin encoding
            # For large XOR, skip (too many clauses)
            if len(xor_lits) <= 12:
                xor_cnf = encode_xor(xor_lits, parity, next_var)
                next_var += len(xor_lits)  # rough upper bound on aux vars
                clauses.extend(xor_cnf)
                xor_clauses += len(xor_cnf)

    print(f"  CRC-32 XOR clauses: {xor_clauses:,}")
    print(f"  Total clauses: {len(clauses):,}, Total vars: {next_var - 1:,}")

    # Solve
    print(f"  Running SAT solver (timeout={timeout}s)...", flush=True)
    solver = Solver(name='g3', bootstrap_with=clauses)  # Glucose3
    t_solve = time.time()
    result = solver.solve()
    solve_time = time.time() - t_solve

    if result is True:
        model = solver.get_model()
        predicted = {}
        for j in free_list:
            v = var_map[j]
            predicted[j] = 1 if v in model else 0

        correct = sum(1 for j in free_list
                     if predicted[j] == int(csm[j // S, j % S]))
        accuracy = correct / n_free * 100
        print(f"  SAT SOLVED in {solve_time:.1f}s. Accuracy: {accuracy:.1f}% ({correct}/{n_free})")
    elif result is False:
        accuracy = 0
        print(f"  SAT UNSAT in {solve_time:.1f}s")
    else:
        accuracy = 0
        print(f"  SAT TIMEOUT in {solve_time:.1f}s")

    solver.delete()
    total_time = time.time() - t0

    return {"method": "SAT", "n_vars": n_free, "total_vars": next_var - 1,
            "n_clauses": len(clauses), "card_clauses": card_clauses,
            "xor_clauses": xor_clauses, "time_s": round(total_time, 2),
            "solve_time_s": round(solve_time, 2),
            "accuracy_pct": round(accuracy, 1) if result is True else None,
            "result": "SAT" if result is True else ("UNSAT" if result is False else "TIMEOUT")}


def encode_xor(lits, parity, top_id):
    """Encode XOR(lits) = parity as CNF clauses (Tseitin-style)."""
    if len(lits) == 1:
        if parity:
            return [[lits[0]]]
        else:
            return [[-lits[0]]]
    if len(lits) == 2:
        a, b = lits
        if parity:
            return [[a, b], [-a, -b]]
        else:
            return [[a, -b], [-a, b]]

    # For 3+ variables: chain through auxiliary variables
    # XOR(a, b, c, ...) = parity
    # Introduce aux vars: t1 = a XOR b, t2 = t1 XOR c, ...
    clauses = []
    aux = top_id
    prev = lits[0]

    for i in range(1, len(lits)):
        if i == len(lits) - 1:
            # Last: XOR(prev, lits[i]) = parity
            a, b = prev, lits[i]
            if parity:
                clauses.extend([[a, b], [-a, -b]])
            else:
                clauses.extend([[a, -b], [-a, b]])
        else:
            # Intermediate: aux = prev XOR lits[i]
            a, b, t = prev, lits[i], aux
            # t = a XOR b: (-a,-b,-t), (-a,b,t), (a,-b,t), (a,b,-t)
            clauses.extend([[-a, -b, -t], [-a, b, t], [a, -b, t], [a, b, -t]])
            prev = t
            aux += 1

    return clauses


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(description="B.59g: SAT/ILP on S=127 residual")
    parser.add_argument("--block", type=int, default=0)
    parser.add_argument("--out", default=str(REPO_ROOT / "tools" / "b59g_results.json"))
    args = parser.parse_args()

    mp4_path = REPO_ROOT / "docs" / "testData" / "useless-machine.mp4"
    data = mp4_path.read_bytes()

    print("B.59g: SAT/ILP on S=127 Residual")
    print(f"  S={S}, N={N}")
    print()

    # Load CSM
    csm = load_csm(data, args.block)
    density = np.sum(csm) / N
    print(f"Block {args.block}: density={density:.3f}")

    # Compute constraints
    targets, members, cell_lines = build_constraints(csm)

    # IntBound propagation
    print("Running IntBound propagation...", flush=True)
    t0 = time.time()
    determined, free_set, rho, u = int_bound_propagation(targets, members, cell_lines)
    print(f"  Determined: {len(determined)} ({len(determined)/N*100:.1f}%), "
          f"Free: {len(free_set)}, Time: {time.time()-t0:.2f}s")

    # Compute CRC-32
    total_bytes = (S + 1 + 7) // 8
    row_crcs = []
    for r in range(S):
        msg = bytearray(total_bytes)
        for c in range(S):
            if csm[r, c]:
                msg[c // 8] |= (1 << (7 - c % 8))
        row_crcs.append(zlib.crc32(bytes(msg)) & 0xFFFFFFFF)

    results = []

    # (a) LP relaxation
    print(f"\n{'='*60}")
    print("(a) LP Relaxation (scipy/HiGHS)")
    print(f"{'='*60}")
    lp_result = lp_relaxation(free_set, determined, targets, members, rho, u, csm)
    results.append(lp_result)

    # (b) SAT
    print(f"\n{'='*60}")
    print("(b) SAT (PySAT/Glucose3 + cardinality + CRC-32 XOR)")
    print(f"{'='*60}")
    sat_result = sat_solve(free_set, determined, targets, members, rho, u, csm, row_crcs)
    results.append(sat_result)

    # Summary
    print(f"\n{'='*60}")
    print(f"B.59g Summary — Block {args.block}")
    print(f"{'='*60}")
    for r in results:
        print(f"  {r['method']}: {r.get('result', 'OK')} "
              f"accuracy={r.get('accuracy_pct', 'N/A')}% "
              f"time={r['time_s']}s")

    # Save
    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    summary = {
        "experiment": "B.59g",
        "block": args.block,
        "density": round(density, 4),
        "determined": len(determined),
        "free": len(free_set),
        "results": results,
    }
    out_path.write_text(json.dumps(summary, indent=2))
    print(f"\n  Results: {out_path}")


if __name__ == "__main__":
    main()
