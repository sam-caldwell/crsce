#!/usr/bin/env python3
"""
Overnight runner for uselessTest A/B sweeps (non‑direct) with periodic summaries.

Features:
- Kicks off two targeted runs (A, B) as provided by user.
- Then iterates a broader grid of overrides until the time budget expires.
- Appends all run metrics to build/uselessTest/completion_stats.log (handled by binaries).
- After each batch, appends a concise progress summary to build/uselessTest/night_summary.txt.
- Also captures sweep tables to build/uselessTest/night_sweep_table.txt.

Usage:
  python3 tools/useless_night_runner.py --hours 10

Tips to run unattended:
  mkdir -p build/uselessTest
  nohup python3 tools/useless_night_runner.py --hours 10 \
      > build/uselessTest/night_runner.out 2>&1 &
"""
from __future__ import annotations

import argparse
import datetime as dt
import itertools
import os
import subprocess
import sys
import time
from typing import Dict, List


def run_cmd(cmd: List[str], env: Dict[str, str] | None = None, capture: bool = True) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, text=True, capture_output=capture, env=env)


def now_str() -> str:
    return dt.datetime.now().strftime('%Y-%m-%d %H:%M:%S')


def append(path: str, text: str) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'a', encoding='utf-8') as f:
        f.write(text)


def run_sweep(args: argparse.Namespace, sweep_args: List[str], label: str) -> None:
    # Execute one sweep call and append outputs to files
    print(f"[{now_str()}] sweep: {label} :: {' '.join(sweep_args)}")
    sys.stdout.flush()
    cp = run_cmd([sys.executable, os.path.join('tools', 'useless_ab_sweep.py')] + sweep_args)
    table_path = os.path.join('build', 'uselessTest', 'night_sweep_table.txt')
    append(table_path, f"\n=== {now_str()} :: {label} ===\n")
    append(table_path, cp.stdout or '')
    # Append a short progress summary snapshot
    prog = run_cmd([sys.executable, os.path.join('tools', 'useless_progress.py')])
    summary_path = os.path.join('build', 'uselessTest', 'night_summary.txt')
    append(summary_path, f"\n[{now_str()}] {label}\n")
    append(summary_path, (prog.stdout or ''))


def main(argv: List[str]) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('--hours', type=float, default=10.0, help='Time budget in hours')
    ap.add_argument('--bin', default=os.path.join('bin', 'uselessTest'))
    ap.add_argument('--seeds', default='4-8')
    args = ap.parse_args(argv[1:])

    # Ensure binary exists
    if not os.path.exists(args.bin):
        print(f"error: binary not found: {args.bin}", file=sys.stderr)
        return 2

    end_ts = time.monotonic() + args.hours * 3600.0
    stage = 0

    # Stage 0: Warm-up — print current summary (if any)
    try:
        run_cmd([sys.executable, os.path.join('tools', 'useless_log_summary.py')], capture=False)
    except Exception:
        pass

    # Stage 1: Targeted Run A (user-provided)
    if time.monotonic() < end_ts:
        run_sweep(
            args,
            [
                '--seeds', '5-5',
                '--verifyTick', '48',
                '--auditForce', '30', '--auditGrace', '10',
                '--nearU', '32', '--parRS', '8', '--parMS', '40000', '--ambFB', '192',
                '--msWindow', '128', '--msSwaps', '64',
                # BNB time set in binary default (4500); keep seeds tight here
            ],
            label='Run A (seed=5, contradictions/adoptions)'
        )
        stage += 1

    # Stage 2: Targeted Run B (user-provided)
    if time.monotonic() < end_ts:
        run_sweep(
            args,
            [
                '--seeds', '7-7',
                '--verifyTick', '48',
                '--auditForce', '30', '--auditGrace', '15',
                '--nearU', '28', '--parRS', '8', '--parMS', '45000', '--ambFB', '192',
                '--msWindow', '128', '--msSwaps', '64',
                '--ms2-window', '96', '--ms2-max-ms', '1200', '--ms2-max-nodes', '1200000',
            ],
            label='Run B (seed=7, widen restarts + MS2)'
        )
        stage += 1

    # Stage 3+: Broad grid until time expires. Alternate configs; include MS2 on half.
    # Priority ordering to bias early signals
    nearU_vals = [32, 28, 24]
    parMS_vals = [45000, 40000]
    verify_ticks = [48, 64]
    audit_graces = [10, 15]
    bnb_nodes = [1200000, 1500000]

    combo_list = list(itertools.product(nearU_vals, parMS_vals, verify_ticks, audit_graces, bnb_nodes))
    # Repeat cycles until time runs out
    cycle = 0
    while time.monotonic() < end_ts:
        for idx, (u, pms, vt, ag, bn) in enumerate(combo_list):
            if time.monotonic() >= end_ts:
                break
            label = f"grid cycle={cycle} nearU={u} parMS={pms} vTick={vt} grace={ag} bnbNodes={bn} seeds={args.seeds}"
            sweep = [
                '--seeds', args.seeds,
                '--verifyTick', str(vt),
                '--auditForce', '30', '--auditGrace', str(ag),
                '--nearU', str(u), '--parRS', '8', '--parMS', str(pms), '--ambFB', '192',
                '--msWindow', '128', '--msSwaps', '64', '--bnbNodes', str(bn),
            ]
            # Include two-row micro-solver on alternate combos
            if (idx + cycle) % 2 == 0:
                sweep += ['--ms2-window', '96', '--ms2-max-ms', '1200', '--ms2-max-nodes', '1200000']
            run_sweep(args, sweep, label)
        cycle += 1

    print(f"[{now_str()}] night runner finished after {args.hours} hours")
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))
