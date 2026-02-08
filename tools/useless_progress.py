#!/usr/bin/env python3
"""
Empirical progress summary for uselessTest completion logs.

Parses build/uselessTest/completion_stats.log (pretty JSON objects appended)
and reports key metrics for the last run, with deltas vs:
 - original baseline (first record in file), and
 - most-recent baseline (the previous record, if present).

Key metrics:
 - micro_solver_bnb_nodes (depth of search)
 - micro_solver_bnb_attempts
 - restart_contradiction_count
 - micro_solver_successes
 - valid_prefix
 - gobp_iters_run
 - rows_committed

Usage:
  python3 tools/useless_progress.py [path]

Default path: build/uselessTest/completion_stats.log
"""
from __future__ import annotations

import json
import os
import sys
from typing import Any, Dict, List, Optional


def load_objects(path: str) -> List[Dict[str, Any]]:
    """Parse concatenated pretty-printed JSON objects from a file."""
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


def pick(o: Dict[str, Any], key: str, default: int | float = 0) -> int | float:
    v = o.get(key)
    if isinstance(v, (int, float)):
        return v
    # Some JSON emitters stringify numbers; be lenient
    try:
        return int(v) if v is not None else default
    except Exception:
        try:
            return float(v) if v is not None else default
        except Exception:
            return default


def load(path: str) -> List[Dict[str, Any]]:
    if not os.path.exists(path):
        print(f"error: file not found: {path}", file=sys.stderr)
        sys.exit(2)
    objs = load_objects(path)
    if not objs:
        print("no records found")
        sys.exit(0)
    return objs


def metric_row(o: Dict[str, Any]) -> Dict[str, int | float]:
    return {
        'micro_solver_bnb_nodes': int(pick(o, 'micro_solver_bnb_nodes', 0)),
        'micro_solver_bnb_attempts': int(pick(o, 'micro_solver_bnb_attempts', 0)),
        'restart_contradiction_count': int(pick(o, 'restart_contradiction_count', 0)),
        'micro_solver_successes': int(pick(o, 'micro_solver_successes', 0)),
        'valid_prefix': int(pick(o, 'valid_prefix', 0)),
        'gobp_iters_run': int(pick(o, 'gobp_iters_run', 0)),
        'rows_committed': int(pick(o, 'rows_committed', 0)),
        'row_avg_pct': float(pick(o, 'row_avg_pct', 0.0)),
    }


def fmt_delta(cur: int | float, base: int | float) -> str:
    d = cur - base
    sign = '+' if d >= 0 else ''
    if isinstance(cur, float) or isinstance(base, float):
        return f"{sign}{d:.3f}"
    return f"{sign}{d}"


def main(argv: List[str]) -> int:
    path = argv[1] if len(argv) > 1 else os.path.join('build', 'uselessTest', 'completion_stats.log')
    objs = load(path)
    first = objs[0]
    prev: Optional[Dict[str, Any]] = objs[-2] if len(objs) >= 2 else None
    last = objs[-1]

    f = metric_row(first)
    p = metric_row(prev) if prev is not None else None
    l = metric_row(last)

    print(f"Records: {len(objs)} from {path}")
    print("Last run:")
    print(f"- micro_solver_bnb_nodes: {l['micro_solver_bnb_nodes']}")
    print(f"- micro_solver_bnb_attempts: {l['micro_solver_bnb_attempts']}")
    print(f"- restart_contradiction_count: {l['restart_contradiction_count']}")
    print(f"- micro_solver_successes: {l['micro_solver_successes']}")
    print(f"- valid_prefix: {l['valid_prefix']}")
    print(f"- gobp_iters_run: {l['gobp_iters_run']}")
    print(f"- rows_committed: {l['rows_committed']}")
    print(f"- row_avg_pct: {l['row_avg_pct']:.4f}")

    print("Vs original baseline:")
    print(f"- micro_solver_bnb_nodes: {fmt_delta(l['micro_solver_bnb_nodes'], f['micro_solver_bnb_nodes'])}")
    print(f"- restart_contradiction_count: {fmt_delta(l['restart_contradiction_count'], f['restart_contradiction_count'])}")
    print(f"- micro_solver_successes: {fmt_delta(l['micro_solver_successes'], f['micro_solver_successes'])}")
    print(f"- valid_prefix: {fmt_delta(l['valid_prefix'], f['valid_prefix'])}")

    if p is not None:
        print("Vs most‑recent baseline:")
        print(f"- micro_solver_bnb_nodes: {fmt_delta(l['micro_solver_bnb_nodes'], p['micro_solver_bnb_nodes'])}")
        print(f"- restart_contradiction_count: {fmt_delta(l['restart_contradiction_count'], p['restart_contradiction_count'])}")
        print(f"- micro_solver_successes: {fmt_delta(l['micro_solver_successes'], p['micro_solver_successes'])}")
        print(f"- valid_prefix: {fmt_delta(l['valid_prefix'], p['valid_prefix'])}")
    else:
        print("Vs most‑recent baseline: n/a (only one record)")

    # Quick signals
    signals: List[str] = []
    if l['micro_solver_bnb_nodes'] > 810_000:
        signals.append('bnb_nodes>0.81M')
    if l['restart_contradiction_count'] > 3:
        signals.append('contradiction_restarts>3')
    if l['micro_solver_successes'] > 0:
        signals.append('micro_solver_adoption')
    if l['valid_prefix'] > 0:
        signals.append('prefix_growth')
    print("Signals:", (', '.join(signals) if signals else '(none)'))
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))

