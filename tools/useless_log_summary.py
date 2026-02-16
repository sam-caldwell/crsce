#!/usr/bin/env python3
"""
Summarize appended completion_stats.log runs.

Parses a file containing multiple pretty-printed JSON objects appended
one after another (not NDJSON). Produces summary stats for:
 - row_avg_pct
 - row_max_pct
 - valid_prefix
 - rows_committed (commit count proxy)

Usage:
  python3 tools/useless_log_summary.py [path]

Default path: build/uselessTest/completion_stats.log
"""
from __future__ import annotations

import json
import math
import os
import statistics as stats
import sys
from typing import Any, Dict, List


def load_objects(path: str) -> List[Dict[str, Any]]:
    # Stream parser for concatenated pretty JSON objects
    with open(path, 'r', encoding='utf-8') as f:
        data = f.read()

    objs: List[Dict[str, Any]] = []
    depth = 0
    in_str = False
    escape = False
    start_idx = -1
    for i, ch in enumerate(data):
        if in_str:
            if escape:
                escape = False
            elif ch == '\\':
                escape = True
            elif ch == '"':
                in_str = False
            continue
        else:
            if ch == '"':
                in_str = True
                continue
            if ch == '{':
                if depth == 0:
                    start_idx = i
                depth += 1
            elif ch == '}':
                depth -= 1
                if depth == 0 and start_idx != -1:
                    chunk = data[start_idx:i + 1]
                    try:
                        obj = json.loads(chunk)
                        objs.append(obj)
                    except json.JSONDecodeError:
                        pass
                    start_idx = -1
    return objs


def load_last_with_keys(path: str, required_keys: List[str]) -> Dict[str, Any] | None:
    """Parse concatenated pretty JSON from a file and return the last object
    containing all of the required_keys. Returns None if not found or on error."""
    try:
        with open(path, 'r', encoding='utf-8') as f:
            data = f.read()
    except Exception:
        return None
    depth = 0
    in_str = False
    escape = False
    start_idx = -1
    last: Dict[str, Any] | None = None
    for i, ch in enumerate(data):
        if in_str:
            if escape:
                escape = False
            elif ch == '\\':
                escape = True
            elif ch == '"':
                in_str = False
            continue
        else:
            if ch == '"':
                in_str = True
                continue
            if ch == '{':
                if depth == 0:
                    start_idx = i
                depth += 1
            elif ch == '}':
                depth -= 1
                if depth == 0 and start_idx != -1:
                    chunk = data[start_idx:i + 1]
                    try:
                        obj = json.loads(chunk)
                        if all(k in obj for k in required_keys):
                            last = obj
                    except Exception:
                        pass
                    start_idx = -1
    return last


def pct(v: float) -> float:
    return v


def pick(obj: Dict[str, Any], key: str, fallback: str | None = None, default: float | int | None = None):
    if key in obj:
        return obj[key]
    if fallback is not None and fallback in obj:
        return obj[fallback]
    return default


def quantiles(values: List[float], qs=(0.5, 0.9, 0.99)):
    if not values:
        return {q: math.nan for q in qs}
    vs = sorted(values)
    out = {}
    for q in qs:
        if q <= 0:
            out[q] = vs[0]
            continue
        if q >= 1:
            out[q] = vs[-1]
            continue
        # linear interpolation between points
        pos = (len(vs) - 1) * q
        lo = int(math.floor(pos))
        hi = int(math.ceil(pos))
        if lo == hi:
            out[q] = vs[lo]
        else:
            frac = pos - lo
            out[q] = vs[lo] * (1 - frac) + vs[hi] * frac
    return out


def main(argv: List[str]) -> int:
    # very small arg parser: optional --csv, then optional path
    do_csv = False
    args = [a for a in argv[1:] if a]
    if args and args[0] == '--csv':
        do_csv = True
        args = args[1:]
    path = args[0] if args else os.path.join('build', 'uselessTest', 'completion_stats.log')
    if not os.path.exists(path):
        print(f"error: file not found: {path}", file=sys.stderr)
        return 2
    objs = load_objects(path)
    if not objs:
        print("no records found")
        return 0

    if do_csv:
        # emit per-run CSV
        cols = [
            'rng_seed_belief', 'rng_seed_restarts', 'start_ms', 'end_ms',
            'row_avg_pct', 'row_max_pct', 'valid_prefix', 'rows_committed', 'cols_finished',
            'restarts_total', 'lock_in_prefix_count', 'lock_in_row_count', 'restart_contradiction_count',
            'boundary_finisher_attempts', 'boundary_finisher_successes'
        ]
        print(','.join(cols))
        for o in objs:
            row = {
                'rng_seed_belief': int(pick(o, 'rng_seed_belief', default=-1)),
                'rng_seed_restarts': int(pick(o, 'rng_seed_restarts', default=-1)),
                'start_ms': int(pick(o, 'start_ms', default=0)),
                'end_ms': int(pick(o, 'end_ms', default=0)),
                'row_avg_pct': float(pick(o, 'row_avg_pct', fallback='avg_pct', default=0.0)),
                'row_max_pct': float(pick(o, 'row_max_pct', fallback='max_pct', default=0.0)),
                'valid_prefix': int(pick(o, 'valid_prefix', default=0)),
                'rows_committed': int(pick(o, 'rows_committed', default=0)),
                'cols_finished': int(pick(o, 'cols_finished', default=0)),
                'restarts_total': int(pick(o, 'restarts_total', default=0)),
                'lock_in_prefix_count': int(pick(o, 'lock_in_prefix_count', default=0)),
                'lock_in_row_count': int(pick(o, 'lock_in_row_count', default=0)),
                'restart_contradiction_count': int(pick(o, 'restart_contradiction_count', default=0)),
                'boundary_finisher_attempts': int(pick(o, 'boundary_finisher_attempts', default=0)),
                'boundary_finisher_successes': int(pick(o, 'boundary_finisher_successes', default=0)),
            }
            print(','.join(str(row[c]) for c in cols))
        return 0

    row_avg: List[float] = []
    row_max: List[float] = []
    vprefix: List[int] = []
    commits: List[int] = []
    seeds: List[int] = []
    dsm_avg_pct: List[float] = []
    xsm_avg_pct: List[float] = []

    for o in objs:
        row_avg.append(float(pick(o, 'row_avg_pct', fallback='avg_pct', default=0.0)))
        row_max.append(float(pick(o, 'row_max_pct', fallback='max_pct', default=0.0)))
        vprefix.append(int(pick(o, 'valid_prefix', default=0)))
        commits.append(int(pick(o, 'rows_committed', default=0)))
        seeds.append(int(pick(o, 'rng_seed_belief', default=-1)))
        # Derive average target density over diagonals and anti-diagonals when arrays are present.
        # Using toroidal indexing, each family has S diagonals of length S; sum(dsm) or sum(xsm) equals total ones.
        try:
            dsm = o.get('dsm')
            lsm = o.get('lsm')
            if isinstance(dsm, list) and (isinstance(lsm, list) and len(lsm) > 0):
                S = max(1, int(len(lsm)))
                total_cells = float(S * S)
                dsum = float(sum(int(x) for x in dsm))
                dsm_avg_pct.append((dsum / total_cells) * 100.0)
        except Exception:
            pass
        try:
            xsm = o.get('xsm')
            lsm = o.get('lsm')
            if isinstance(xsm, list) and (isinstance(lsm, list) and len(lsm) > 0):
                S = max(1, int(len(lsm)))
                total_cells = float(S * S)
                xsum = float(sum(int(x) for x in xsm))
                xsm_avg_pct.append((xsum / total_cells) * 100.0)
        except Exception:
            pass

    def summarize(name: str, values: List[float]):
        if not values:
            print(f"- {name}: no data")
            return
        q = quantiles(values)
        mean = stats.fmean(values)
        stdev = stats.pstdev(values) if len(values) > 1 else 0.0
        print(
            f"- {name}: count={len(values)} min={min(values):.4f} p50={q[0.5]:.4f} p90={q[0.9]:.4f} p99={q[0.99]:.4f}"
            f"max={max(values):.4f} mean={mean:.4f} stdev={stdev:.4f}")

    print(f"Records: {len(objs)} from {path}")
    summarize('row_avg_pct', row_avg)
    summarize('row_max_pct', row_max)
    summarize('valid_prefix', list(map(float, vprefix)))
    summarize('rows_committed', list(map(float, commits)))
    # New: diagonal and anti-diagonal target densities (percentage of cells set by targets)
    # If not present in completion_stats.log, try to pull from sibling decompress.stdout.txt
    if not dsm_avg_pct or not xsm_avg_pct:
        dx_stdout = os.path.join(os.path.dirname(path), 'decompress.stdout.txt')
        last_dx = load_last_with_keys(dx_stdout, ['dsm', 'xsm'])
        if last_dx and isinstance(last_dx.get('lsm'), list):
            S = max(1, int(len(last_dx['lsm'])))
            total_cells = float(S * S)
            try:
                dsum = float(sum(int(x) for x in last_dx.get('dsm', [])))
                xsum = float(sum(int(x) for x in last_dx.get('xsm', [])))
                if not dsm_avg_pct:
                    dsm_avg_pct = [(dsum / total_cells) * 100.0]
                if not xsm_avg_pct:
                    xsm_avg_pct = [(xsum / total_cells) * 100.0]
            except Exception:
                pass
    if dsm_avg_pct:
        summarize('dsm_avg_pct', dsm_avg_pct)
    if xsm_avg_pct:
        summarize('xsm_avg_pct', xsm_avg_pct)

    # Top seeds by valid_prefix then row_avg_pct
    scored = list(zip(seeds, vprefix, row_avg))
    scored = [s for s in scored if s[0] >= 0]
    scored.sort(key=lambda t: (t[1], t[2]), reverse=True)
    top = scored[:10]
    if top:
        print("- top_seeds (seed, valid_prefix, row_avg_pct):")
        for s, vp, ravg in top:
            print(f"  seed={s:6d}  prefix={vp:4d}  row_avg_pct={ravg:.4f}")

    # For the last record, emit diagonal/anti-diagonal satisfaction vectors if unknown vectors are present.
    # Approximation: treat a diagonal as satisfied when its unknown count is zero at capture time.
    last = objs[-1]
    try:
        if isinstance(last.get('U_diag'), list):
            dsm_sat = [1 if int(u) == 0 else 0 for u in last['U_diag']]
            print(f"- dsm_avg_pct_last: {(sum(dsm_sat) / max(1, len(dsm_sat))) * 100.0:.4f}")
            print(f"- dsm: {dsm_sat}")
    except Exception:
        pass
    try:
        if isinstance(last.get('U_xdiag'), list):
            xsm_sat = [1 if int(u) == 0 else 0 for u in last['U_xdiag']]
            print(f"- xsm_avg_pct_last: {(sum(xsm_sat) / max(1, len(xsm_sat))) * 100.0:.4f}")
            print(f"- xsm: {xsm_sat}")
    except Exception:
        pass
    # If completion_stats last record lacks U_diag/U_xdiag, attempt to print from decompress stdout failure JSON
    if not isinstance(last.get('U_diag'), list) or not isinstance(last.get('U_xdiag'), list):
        dx_stdout = os.path.join(os.path.dirname(path), 'decompress.stdout.txt')
        last_dx = load_last_with_keys(dx_stdout, ['U_diag', 'U_xdiag'])
        if last_dx is not None:
            try:
                if isinstance(last_dx.get('U_diag'), list):
                    dsm_sat = [1 if int(u) == 0 else 0 for u in last_dx['U_diag']]
                    print(f"- dsm_avg_pct_last(dx): {(sum(dsm_sat) / max(1, len(dsm_sat))) * 100.0:.4f}")
                    print(f"- dsm(dx): {dsm_sat}")
            except Exception:
                pass
            try:
                if isinstance(last_dx.get('U_xdiag'), list):
                    xsm_sat = [1 if int(u) == 0 else 0 for u in last_dx['U_xdiag']]
                    print(f"- xsm_avg_pct_last(dx): {(sum(xsm_sat) / max(1, len(xsm_sat))) * 100.0:.4f}")
                    print(f"- xsm(dx): {xsm_sat}")
            except Exception:
                pass

    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))
