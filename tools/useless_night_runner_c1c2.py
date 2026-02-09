#!/usr/bin/env python3
"""
Follow-on night runner using C1 (contradictions) and C2 (adoptions+MS2) profiles.

Profiles:
- C1 (push contradictions):
  VERIFY_TICK=32, AUDIT_FORCE_EVERY=20, AUDIT_GRACE=5
  NEARLOCK_U in {20,24,28}, PAR_RS=12, PAR_RS_MAX_MS=20000
  MS_AMB_FALLBACK=160, BNB_NODES=1500000

- C2 (push adoptions + MS2):
  VERIFY_TICK=48, AUDIT_FORCE_EVERY=30, AUDIT_GRACE=10
  NEARLOCK_U in {28,24}, PAR_RS=8, PAR_RS_MAX_MS=40000
  MS_AMB_FALLBACK=160, MS_WINDOW=160, MS_KVAR_SWAPS=96
  MS2_WINDOW=128, MS2_MAX_MS=2000, MS2_MAX_NODES in {1500000, 2000000}
  MS_BNB_MS=6000, MS_BNB_MAX_NODES=2000000

Seeds: default 4-12 (configurable)

Usage:
  python3 tools/useless_night_runner_c1c2.py --hours 8 [--seeds 4-12]
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


def run_sweep(sweep_args: List[str], label: str) -> None:
    print(f"[{now_str()}] sweep: {label} :: {' '.join(sweep_args)}")
    sys.stdout.flush()
    cp = run_cmd([sys.executable, os.path.join('tools', 'useless_ab_sweep.py')] + sweep_args)
    table_path = os.path.join('build', 'uselessTest', 'night_sweep_table.txt')
    append(table_path, f"\n=== {now_str()} :: {label} ===\n")
    append(table_path, cp.stdout or '')
    prog = run_cmd([sys.executable, os.path.join('tools', 'useless_progress.py')])
    summary_path = os.path.join('build', 'uselessTest', 'night_summary.txt')
    append(summary_path, f"\n[{now_str()}] {label}\n")
    append(summary_path, (prog.stdout or ''))


def main(argv: List[str]) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('--hours', type=float, default=8.0, help='Time budget in hours for follow-on')
    ap.add_argument('--seeds', default='4-12')
    ap.add_argument('--bin', default=os.path.join('bin', 'uselessTest'))
    args = ap.parse_args(argv[1:])

    if not os.path.exists(args.bin):
        print(f"error: binary not found: {args.bin}", file=sys.stderr)
        return 2

    end_ts = time.monotonic() + args.hours * 3600.0

    # Warm-up
    try:
        run_cmd([sys.executable, os.path.join('tools', 'useless_log_summary.py')], capture=False)
    except Exception:
        pass

    cycle = 0
    while time.monotonic() < end_ts:
        # C1 cycle (contradictions)
        if time.monotonic() >= end_ts:
            break
        for u in [20, 24, 28]:
            if time.monotonic() >= end_ts:
                break
            label = f"C1 cycle={cycle} nearU={u} seeds={args.seeds}"
            sweep = [
                '--seeds', args.seeds,
                '--verifyTick', '32',
                '--auditForce', '20', '--auditGrace', '5',
                '--nearU', str(u), '--parRS', '12', '--parMS', '20000', '--ambFB', '160',
                '--msWindow', '128', '--msSwaps', '64', '--bnbNodes', '1500000',
            ]
            run_sweep(sweep, label)

        # C2 cycle (adoptions + MS2)
        if time.monotonic() >= end_ts:
            break
        for u in [28, 24]:
            if time.monotonic() >= end_ts:
                break
            for ms2nodes in [1500000, 2000000]:
                if time.monotonic() >= end_ts:
                    break
                label = f"C2 cycle={cycle} nearU={u} ms2Nodes={ms2nodes} seeds={args.seeds}"
                sweep = [
                    '--seeds', args.seeds,
                    '--verifyTick', '48',
                    '--auditForce', '30', '--auditGrace', '10',
                    '--nearU', str(u), '--parRS', '8', '--parMS', '40000', '--ambFB', '160',
                    '--msWindow', '160', '--msSwaps', '96',
                    '--ms2-window', '128', '--ms2-max-ms', '2000', '--ms2-max-nodes', str(ms2nodes),
                    '--bnbMS', '6000', '--bnbNodes', '2000000',
                ]
                run_sweep(sweep, label)

        cycle += 1

    print(f"[{now_str()}] follow-on runner finished after {args.hours} hours")
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))

