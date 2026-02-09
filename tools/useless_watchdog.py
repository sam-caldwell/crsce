#!/usr/bin/env python3
"""
Watchdog for overnight uselessTest runs.

Monitors the night runner and completion logs, intervening when:
- The night runner process dies early -> restart it for remaining time.
- The completion log stalls beyond a threshold -> kick targeted sweeps.
- Recent runs show no positive signals -> re-run user A/B nudges.

Artifacts:
- build/uselessTest/watchdog.out        (own stdout/stderr when nohup'ed)
- build/uselessTest/watchdog.pid        (own PID)
- build/uselessTest/watchdog_alerts.log (append-only interventions log)
- build/uselessTest/watchdog_status.txt (last heartbeat snapshot)

Usage:
  python3 tools/useless_watchdog.py --hours 10 \
    [--pidfile build/uselessTest/night_runner.pid] \
    [--log build/uselessTest/completion_stats.log] \
    [--idle-minutes 45] [--check-interval 60]

Notes:
- Uses the existing tools/useless_night_runner.py and tools/useless_ab_sweep.py
  for any restart/boost actions.
"""
from __future__ import annotations

import argparse
import datetime as dt
import json
import os
import subprocess
import sys
import time
from typing import Any, Dict, List, Optional


def now_str() -> str:
    return dt.datetime.now().strftime('%Y-%m-%d %H:%M:%S')


def append(path: str, text: str) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'a', encoding='utf-8') as f:
        f.write(text)


def write_text(path: str, text: str) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', encoding='utf-8') as f:
        f.write(text)


def file_mtime(path: str) -> Optional[float]:
    try:
        return os.path.getmtime(path)
    except OSError:
        return None


def is_pid_alive(pid: int) -> bool:
    try:
        # Works on POSIX: signal 0 check
        os.kill(pid, 0)
        return True
    except ProcessLookupError:
        return False
    except PermissionError:
        # Assume alive but not signalable
        return True


def read_pid(path: str) -> Optional[int]:
    try:
        with open(path, 'r', encoding='utf-8') as f:
            t = f.read().strip()
        return int(t)
    except Exception:
        return None


def run_py(args: List[str], capture: bool = True, env: Optional[Dict[str, str]] = None) -> subprocess.CompletedProcess:
    return subprocess.run([sys.executable] + args, text=True, capture_output=capture, env=env)


def parse_concatenated_json(path: str, max_records: int = 50) -> List[Dict[str, Any]]:
    """Parse concatenated pretty-printed JSON objects; return last N records."""
    if not os.path.exists(path):
        return []
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
    if max_records and len(objs) > max_records:
        return objs[-max_records:]
    return objs


def pick_int(o: Dict[str, Any], key: str, default: int = 0) -> int:
    v = o.get(key)
    try:
        if isinstance(v, bool):
            return int(v)
        if isinstance(v, (int, float)):
            return int(v)
        if v is None:
            return default
        return int(str(v))
    except Exception:
        return default


def last_signals(records: List[Dict[str, Any]]) -> Dict[str, int]:
    if not records:
        return {'bnb_nodes_gt_0_81M': 0, 'contra_gt_3': 0, 'ms_success_gt_0': 0, 'prefix_gt_0': 0}
    last = records[-1]
    bnb_nodes = pick_int(last, 'micro_solver_bnb_nodes', 0)
    contra = pick_int(last, 'restart_contradiction_count', 0)
    ms_succ = pick_int(last, 'micro_solver_successes', 0)
    vpref = pick_int(last, 'valid_prefix', 0)
    return {
        'bnb_nodes_gt_0_81M': 1 if bnb_nodes > 810_000 else 0,
        'contra_gt_3': 1 if contra > 3 else 0,
        'ms_success_gt_0': 1 if ms_succ > 0 else 0,
        'prefix_gt_0': 1 if vpref > 0 else 0,
    }


def any_positive(records: List[Dict[str, Any]], window: int = 10) -> bool:
    if not records:
        return False
    sub = records[-window:] if len(records) > window else records
    for o in sub:
        if pick_int(o, 'micro_solver_bnb_nodes', 0) > 810_000:
            return True
        if pick_int(o, 'restart_contradiction_count', 0) > 3:
            return True
        if pick_int(o, 'micro_solver_successes', 0) > 0:
            return True
        if pick_int(o, 'valid_prefix', 0) > 0:
            return True
    return False


def start_night_runner(remaining_hours: float, out_path: str, pid_path: str) -> Optional[int]:
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    try:
        out = open(out_path, 'a', encoding='utf-8')
    except Exception:
        return None
    proc = subprocess.Popen(
        [sys.executable, os.path.join('tools', 'useless_night_runner.py'), '--hours', f'{remaining_hours:.3f}'],
        stdout=out, stderr=out, close_fds=True
    )
    write_text(pid_path, str(proc.pid))
    return proc.pid


def run_ab_boost(label: str) -> None:
    # Re-run user-provided A and B with the specified overrides to nudge progress
    append(os.path.join('build', 'uselessTest', 'watchdog_alerts.log'), f"[{now_str()}] watchdog: {label}: re-running A and B\n")
    # Run A
    run_py([
        os.path.join('tools', 'useless_ab_sweep.py'),
        '--seeds', '5-5',
        '--verifyTick', '48',
        '--auditForce', '30', '--auditGrace', '10',
        '--nearU', '32', '--parRS', '8', '--parMS', '40000', '--ambFB', '192',
        '--msWindow', '128', '--msSwaps', '64',
    ], capture=True)
    # Run B (with slightly heavier nodes to widen search)
    run_py([
        os.path.join('tools', 'useless_ab_sweep.py'),
        '--seeds', '7-7',
        '--verifyTick', '48',
        '--auditForce', '30', '--auditGrace', '15',
        '--nearU', '28', '--parRS', '8', '--parMS', '45000', '--ambFB', '192',
        '--msWindow', '128', '--msSwaps', '64',
        '--ms2-window', '96', '--ms2-max-ms', '1200', '--ms2-max-nodes', '1200000',
        '--bnbNodes', '1500000',
    ], capture=True)


def main(argv: List[str]) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('--hours', type=float, default=10.0, help='How long to keep watching')
    ap.add_argument('--pidfile', default=os.path.join('build', 'uselessTest', 'night_runner.pid'))
    ap.add_argument('--log', default=os.path.join('build', 'uselessTest', 'completion_stats.log'))
    ap.add_argument('--idle-minutes', type=int, default=45, help='No log writes beyond this triggers a boost')
    ap.add_argument('--check-interval', type=int, default=60, help='Seconds between checks')
    args = ap.parse_args(argv[1:])

    # Record our PID
    write_text(os.path.join('build', 'uselessTest', 'watchdog.pid'), str(os.getpid()))

    end_ts = time.monotonic() + args.hours * 3600.0
    last_log_mtime = file_mtime(args.log)
    last_size = os.path.getsize(args.log) if os.path.exists(args.log) else 0
    last_boost_ts = 0.0

    while time.monotonic() < end_ts:
        # 1) Ensure runner alive
        runner_pid = read_pid(args.pidfile)
        runner_alive = is_pid_alive(runner_pid) if runner_pid else False
        remaining_h = max(0.0, (end_ts - time.monotonic()) / 3600.0)
        if not runner_alive and remaining_h > 0.05:
            append(os.path.join('build', 'uselessTest', 'watchdog_alerts.log'), f"[{now_str()}] runner not alive, restarting for {remaining_h:.2f}h\n")
            new_pid = start_night_runner(remaining_h, os.path.join('build', 'uselessTest', 'night_runner.out'), args.pidfile)
            append(os.path.join('build', 'uselessTest', 'watchdog_alerts.log'), f"[{now_str()}] restarted runner pid={new_pid}\n")

        # 2) Inspect logs
        mtime = file_mtime(args.log)
        size = os.path.getsize(args.log) if os.path.exists(args.log) else 0
        stalled = False
        if mtime is None:
            stalled = True
        else:
            idle_seconds = max(0.0, time.time() - mtime)
            stalled = idle_seconds > (args.idle_minutes * 60)

        records = parse_concatenated_json(args.log, max_records=50)
        sigs = last_signals(records)
        any_pos = any_positive(records, window=12)

        # 3) Intervene on stall (rate-limited)
        now_mono = time.monotonic()
        if stalled and (now_mono - last_boost_ts) > 15 * 60:
            run_ab_boost('stall detected')
            last_boost_ts = now_mono

        # 4) Intervene on no positive signals over window (rate-limited)
        if (not stalled) and (not any_pos) and (now_mono - last_boost_ts) > 30 * 60:
            run_ab_boost('no positive signals window')
            last_boost_ts = now_mono

        # 5) Heartbeat status
        status = []
        status.append(f"time: {now_str()}")
        status.append(f"runner_pid: {runner_pid if runner_pid else 'n/a'} alive={runner_alive}")
        status.append(f"remaining_hours: {remaining_h:.2f}")
        status.append(f"log_path: {args.log}")
        status.append(f"log_size: {size} bytes delta={size - last_size}")
        status.append(f"log_mtime: {dt.datetime.fromtimestamp(mtime).isoformat() if mtime else 'n/a'}")
        status.append(f"signals: contra_gt_3={sigs['contra_gt_3']} ms_success_gt_0={sigs['ms_success_gt_0']} prefix_gt_0={sigs['prefix_gt_0']} bnb_nodes>0.81M={sigs['bnb_nodes_gt_0_81M']}")
        status.append(f"stalled={stalled}")
        write_text(os.path.join('build', 'uselessTest', 'watchdog_status.txt'), '\n'.join(status) + '\n')

        last_log_mtime = mtime
        last_size = size
        time.sleep(max(1, args.check_interval))

    append(os.path.join('build', 'uselessTest', 'watchdog_alerts.log'), f"[{now_str()}] watchdog finished after {args.hours} hours\n")
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))

