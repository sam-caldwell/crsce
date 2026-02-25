#!/usr/bin/env python3
"""
constraint_watchdog_loop.py

Continuously runs: make configure, make build, make test/uselessMachine
Parses heartbeat (decompress.stdout.txt) for success/failure and residual trends,
and appends a brief analysis to build/uselessTest/loop_analysis.log.

Usage:
  python3 tools/constraint_watchdog_loop.py --hours 24 --watchdog 3600 --sleep 5

Notes:
  - Assumes 'make test/uselessMachine' writes logs under build/uselessTest/.
  - Uses CRSCE_WATCHDOG_SECS=<watchdog> for each test run.
  - Appends analysis of the last hyb_residuals/hyb_accepts_pass lines.
"""
from __future__ import annotations

import argparse
import os
import subprocess
import time
from typing import Optional, Tuple, Iterable, List


def run(cmd: list[str], env: dict[str, str] | None = None) -> int:
    return subprocess.call(cmd, env=env)


def pids_matching(pattern: str) -> list[int]:
    try:
        out = subprocess.check_output(['pgrep', '-f', pattern], text=True)
        return [int(x) for x in out.strip().split() if x.strip().isdigit()]
    except subprocess.CalledProcessError:
        return []


def kill_pids(pids: Iterable[int]) -> None:
    for pid in pids:
        try:
            os.kill(pid, 9)
        except Exception:
            pass


def tail_file(path: str, n: int = 2000) -> list[str]:
    if not os.path.exists(path):
        return []
    try:
        with open(path, 'r', encoding='utf-8', errors='ignore') as f:
            lines = f.readlines()
        return lines[-n:]
    except Exception:
        return []


def parse_last_residuals(lines: list[str]) -> Optional[Tuple[int, int, int, int, int, int, int, int]]:
    # Returns (wv_err, wv_idx, wd_err, wd_idx, wx_err, wx_idx, total_cost, pass_no)
    for line in reversed(lines):
        if '"name":"hyb_residuals"' in line:
            # crude parse for tags
            def get(tag: str) -> Optional[int]:
                key = f'"{tag}":"'
                i = line.find(key)
                if i < 0:
                    return None
                j = line.find('"', i + len(key))
                if j < 0:
                    return None
                try:
                    return int(line[i + len(key):j])
                except ValueError:
                    return None
            wv_err = get('worst_vsm_error') or 0
            wv_idx = get('worst_vsm_index') or 0
            wd_err = get('worst_dsm_error') or 0
            wd_idx = get('worst_dsm_index') or 0
            wx_err = get('worst_xsm_error') or 0
            wx_idx = get('worst_xsm_index') or 0
            total = get('total_cost') or 0
            ps = get('pass') or 0
            return (wv_err, wv_idx, wd_err, wd_idx, wx_err, wx_idx, total, ps)
    return None


def parse_last_accepts(lines: list[str]) -> Optional[Tuple[int, int, int]]:
    # Returns (accepted, attempts, pass_no)
    for line in reversed(lines):
        if '"name":"hyb_accepts_pass"' in line:
            def get(tag: str) -> Optional[int]:
                key = f'"{tag}":"'
                i = line.find(key)
                if i < 0:
                    return None
                j = line.find('"', i + len(key))
                if j < 0:
                    return None
                try:
                    return int(line[i + len(key):j])
                except ValueError:
                    return None
            acc = get('accepted') or 0
            att = get('attempts') or 0
            ps = get('pass') or 0
            return (acc, att, ps)
    return None


def analyze(heartbeat_path: str, out_path: str) -> None:
    lines = tail_file(heartbeat_path, 5000)
    ok = any('"name":"block_end_ok"' in ln for ln in lines)
    fail = any('"name":"block_end_verify_fail"' in ln for ln in lines)
    res = parse_last_residuals(lines)
    acc = parse_last_accepts(lines)
    with open(out_path, 'a', encoding='utf-8') as out:
        out.write(f"\n=== Analysis @ {time.strftime('%Y-%m-%d %H:%M:%S')} ===\n")
        out.write(f"result: {'ok' if ok else ('fail' if fail else 'unknown')}\n")
        if res:
            wv_err, wv_idx, wd_err, wd_idx, wx_err, wx_idx, total, ps = res
            out.write(f"hyb_residuals: pass={ps} total_cost={total} "
                      f"worst_vsm=({wv_err}@{wv_idx}) worst_dsm=({wd_err}@{wd_idx}) worst_xsm=({wx_err}@{wx_idx})\n")
        if acc:
            acc_v, att_v, ps = acc
            out.write(f"hyb_accepts_pass: pass={ps} accepted={acc_v} attempts={att_v}\n")

def collect_residual_series(lines: List[str], max_points: int = 256) -> List[Tuple[int,int,int]]:
    """Return up to max_points tuples (pass_no, worst_vsm_error, total_cost) from hyb_residuals lines in order."""
    out: List[Tuple[int,int,int]] = []
    for ln in lines:
        if '"name":"hyb_residuals"' not in ln:
            continue
        def get(tag: str) -> Optional[int]:
            key = f'"{tag}":"'
            i = ln.find(key)
            if i < 0:
                return None
            j = ln.find('"', i + len(key))
            if j < 0:
                return None
            try:
                return int(ln[i + len(key):j])
            except ValueError:
                return None
        ps = get('pass'); wv = get('worst_vsm_error'); tc = get('total_cost')
        if ps is None or wv is None or tc is None:
            continue
        out.append((ps, wv, tc))
        if len(out) > max_points:
            out.pop(0)
    return out


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('--hours', type=float, default=24.0)
    ap.add_argument('--watchdog', type=int, default=3600)
    ap.add_argument('--sleep', type=int, default=5)
    args = ap.parse_args(argv[1:])

    base = os.path.join('build', 'uselessTest')
    os.makedirs(base, exist_ok=True)
    hb_path = os.path.join(base, 'decompress.stdout.txt')
    an_path = os.path.join(base, 'loop_analysis.log')

    end_ts = time.monotonic() + args.hours * 3600.0
    cycle = 0
    tune_level = 0  # 0=baseline, 1=moderate, 2=aggressive
    while time.monotonic() < end_ts:
        cycle += 1
        print(f"\n=== Cycle {cycle}: configure/build/test ===", flush=True)
        rc = run(['make', 'configure'])
        if rc != 0:
            print(f"configure failed rc={rc}", flush=True)
            break
        rc = run(['make', 'build'])
        if rc != 0:
            print(f"build failed rc={rc}", flush=True)
            break
        env = os.environ.copy()
        env['CRSCE_WATCHDOG_SECS'] = str(args.watchdog)
        # Ensure both heartbeat and o11y events go to the same visible log
        env['CRSCE_HEARTBEAT_PATH'] = hb_path
        env['CRSCE_O11Y_FLUSH_MS'] = '5000'
        # Apply tuning overrides according to tune_level
        if tune_level >= 1:
            # Increase sweep cadence and strength moderately
            env['CRSCE_HYB_SWEEP_PERIOD'] = '8'
            env['CRSCE_HYB_VSM_SWEEP_MAX_MOVES'] = '8192'
            env['CRSCE_HYB_DSM_SWEEP_MAX_MOVES'] = '16384'
            env['CRSCE_HYB_XSM_SWEEP_MAX_MOVES'] = '16384'
            env['CRSCE_HYB_GPU_SAMPLES'] = env.get('CRSCE_HYB_GPU_SAMPLES','2097152')
        if tune_level >= 2:
            # Go aggressive
            env['CRSCE_HYB_SWEEP_PERIOD'] = '4'
            env['CRSCE_HYB_VSM_SWEEP_MAX_MOVES'] = '65536'
            env['CRSCE_HYB_DSM_SWEEP_MAX_MOVES'] = '65536'
            env['CRSCE_HYB_XSM_SWEEP_MAX_MOVES'] = '65536'
            env['CRSCE_HYB_GPU_SAMPLES'] = env.get('CRSCE_HYB_GPU_SAMPLES','4194304')
        # Proactively ensure only one decompressor/test is running
        kill_pids(pids_matching(r'/decompress( |$)'))
        kill_pids(pids_matching(r'/uselessTest( |$)'))
        # Truncate heartbeat log at cycle start for fresh analysis
        try:
            with open(hb_path, 'w', encoding='utf-8') as _f:
                pass
        except Exception:
            pass
        # Enforce requested runtime tuning (explicit overrides)
        env['CRSCE_HYB_SAMPLES_PER_PASS'] = '262144'
        env['CRSCE_HYB_SWEEP_PERIOD'] = '2'  # set to 2 (can tighten to 1 if needed)
        env['CRSCE_HYB_STALL_PASSES'] = '16'
        env['CRSCE_HYB_GPU_SAMPLES'] = '4194304'
        env['CRSCE_HYB_VSM_SWEEP_MAX_MOVES'] = '16384'
        env['CRSCE_HYB_DSM_SWEEP_MAX_MOVES'] = '32768'
        env['CRSCE_HYB_XSM_SWEEP_MAX_MOVES'] = '32768'
        rc = run(['make', 'test/uselessMachine'], env=env)
        # If the test returns but a decompressor remains, force cleanup
        kill_pids(pids_matching(r'/decompress( |$)'))
        analyze(hb_path, an_path)
        if rc == 0:
            print("test rc=0 (ok)", flush=True)
            # If block_end_ok was seen, we could decide to exit or continue gathering data.
        else:
            print(f"test rc={rc}", flush=True)
        # Light tuning heuristic for next cycle based on last residuals series
        lines = tail_file(hb_path, 5000)
        series = collect_residual_series(lines, max_points=256)
        if series:
            first_ps, first_wv, first_tc = series[0]
            last_ps, last_wv, last_tc = series[-1]
            # If worst VSM remains > 8 and total_cost reduction < 5%, step up tuning (max level 2)
            reduction = 0.0
            if first_tc > 0:
                reduction = (float(first_tc) - float(last_tc)) / float(first_tc)
            if last_wv > 8 and reduction < 0.05 and tune_level < 2:
                tune_level += 1
                with open(an_path, 'a', encoding='utf-8') as out:
                    out.write(f"tuning: promoting to level {tune_level} for next cycle; last_wv={last_wv} reduction={reduction:.3f}\n")
        time.sleep(args.sleep)
    return 0


if __name__ == '__main__':
    import sys
    raise SystemExit(main(sys.argv))
