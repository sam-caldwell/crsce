#!/usr/bin/env python3
"""
Loop runner for constraint-based uselessMachine test with watchdog.

Runs cycles of:
  make configure/constraint
  make build/constraint
  make test/uselessMachine PRESET=constraint

Logs are appended under build/uselessTest/:
  loop_configure.out, loop_build.out, loop_test.out

Environment:
  CRSCE_WATCHDOG_SECS controls the per-run watchdog inside the test binary.

Usage:
  python3 tools/useless_constraint_loop.py --hours 8 --watchdog 1800

To run unattended:
  mkdir -p build/uselessTest
  nohup python3 tools/useless_constraint_loop.py --hours 8 --watchdog 1800 \
    > build/uselessTest/loop_runner.out 2>&1 &
"""
from __future__ import annotations

import argparse
import os
import subprocess
import sys
import time


def append(path: str, text: str) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'a', encoding='utf-8') as f:
        f.write(text)


def run_cmd(cmd: list[str], env: dict[str, str] | None, out_path: str) -> int:
    proc = subprocess.Popen(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=env)
    assert proc.stdout is not None
    with open(out_path, 'a', encoding='utf-8') as out:
        for line in proc.stdout:
            out.write(line)
    return proc.wait()


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('--hours', type=float, default=8.0, help='Total hours to loop')
    ap.add_argument('--watchdog', type=int, default=1800, help='Per-run watchdog seconds (env CRSCE_WATCHDOG_SECS)')
    args = ap.parse_args(argv[1:])

    end_ts = time.monotonic() + args.hours * 3600.0

    base = os.path.join('build', 'uselessTest')
    os.makedirs(base, exist_ok=True)
    cfg_out = os.path.join(base, 'loop_configure.out')
    bld_out = os.path.join(base, 'loop_build.out')
    tst_out = os.path.join(base, 'loop_test.out')

    cycle = 0
    while time.monotonic() < end_ts:
        cycle += 1
        append(cfg_out, f"\n=== Cycle {cycle}: configure (constraint) ===\n")
        rc_cfg = run_cmd(['make', 'configure/constraint'], env=None, out_path=cfg_out)
        append(cfg_out, f"configure rc={rc_cfg}\n")

        append(bld_out, f"\n=== Cycle {cycle}: build (constraint) ===\n")
        rc_bld = run_cmd(['make', 'build/constraint', f'-j{os.cpu_count() or 2}'], env=None, out_path=bld_out)
        append(bld_out, f"build rc={rc_bld}\n")

        env = os.environ.copy()
        env['CRSCE_WATCHDOG_SECS'] = str(args.watchdog)
        append(tst_out, f"\n=== Cycle {cycle}: test/uselessMachine (constraint) ===\n")
        rc_tst = run_cmd(['make', 'test/uselessMachine', 'PRESET=constraint'], env=env, out_path=tst_out)
        append(tst_out, f"test rc={rc_tst}\n")

        # Backoff small delay between cycles to reduce churn
        time.sleep(5)

    append(tst_out, f"\nloop finished after {args.hours} hours\n")
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))

