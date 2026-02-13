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
    [--dx-stdout build/uselessTest/decompress.stdout.txt] \
    [--idle-minutes 45] [--check-interval 60] \
    [--hb-stall-ms 30000] [--hb-stall-intervals 3] \
    [--boost-samples 384] [--boost-accepts 24] [--gobp-timeout 90000]

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
from typing import Any, Dict, List, Optional, Tuple
import re


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


def parse_last_gobp_heartbeat(path: str) -> Optional[Dict[str, int]]:
    """Return metrics from the last 'phase=gobp' heartbeat line in dx stdout.

    Extracts: ts_ms, iters, rect_acc, rect_att, stall_ms, acc_rate_bp (optional).
    Returns None if no gobp line found or file missing.
    """
    if not os.path.exists(path):
        return None
    try:
        with open(path, 'r', encoding='utf-8') as f:
            lines = f.read().splitlines()
    except Exception:
        return None
    last = None
    for line in reversed(lines[-1000:]):  # scan last 1000 lines max
        if 'phase=gobp' in line:
            last = line
            break
    if not last:
        return None
    # Example:
    # [1739480000000] phase=gobp solved=1234 unknown=510 gobp_status=1 phase_idx=2 iters=18432 rect_passes=384 rect_acc/att=96/18432 cells=0 rows_committed=12 cols_finished=0 stall_ms=12054
    m_ts = re.search(r"\[(\d+)\]", last)
    ts_ms = int(m_ts.group(1)) if m_ts else 0
    m_iters = re.search(r"\biters=(\d+)\b", last)
    iters = int(m_iters.group(1)) if m_iters else 0
    m_rect = re.search(r"rect_acc/att=(\d+)/(\d+)", last)
    rect_acc = int(m_rect.group(1)) if m_rect else 0
    rect_att = int(m_rect.group(2)) if m_rect else 0
    m_stall = re.search(r"stall_ms=(\d+|na)\b", last)
    stall_ms = int(m_stall.group(1)) if (m_stall and m_stall.group(1).isdigit()) else 0
    m_rate = re.search(r"acc_rate_bp=(\d+)\b", last)
    acc_rate_bp = int(m_rate.group(1)) if m_rate else 0
    return {
        'ts_ms': ts_ms,
        'iters': iters,
        'rect_acc': rect_acc,
        'rect_att': rect_att,
        'stall_ms': stall_ms,
        'acc_rate_bp': acc_rate_bp,
    }


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


def start_runner(remaining_hours: float, out_path: str, pid_path: str, script_path: str) -> Optional[int]:
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    try:
        out = open(out_path, 'a', encoding='utf-8')
    except Exception:
        return None
    proc = subprocess.Popen(
        [sys.executable, script_path, '--hours', f'{remaining_hours:.3f}'],
        stdout=out, stderr=out, close_fds=True
    )
    write_text(pid_path, str(proc.pid))
    return proc.pid


def run_ab_boost(label: str) -> None:
    # Re-run user-provided A and B with the specified overrides to nudge progress
    append(
        os.path.join('build', 'uselessTest', 'watchdog_alerts.log'),
        f"[{now_str()}] watchdog: {label}: re-running A and B\n",
    )
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
    ap.add_argument('--dx-stdout', default=os.path.join('build', 'uselessTest', 'decompress.stdout.txt'))
    ap.add_argument('--idle-minutes', type=int, default=45, help='No log writes beyond this triggers a boost')
    ap.add_argument('--check-interval', type=int, default=60, help='Seconds between checks')
    ap.add_argument('--runner', choices=['night', 'c1c2'], default='night', help='Which runner to restart if dead')
    # Heartbeat stall detection params
    ap.add_argument('--hb-stall-ms', type=int, default=30000, help='Consider hard-stall if stall_ms exceeds this')
    ap.add_argument('--hb-stall-intervals', type=int, default=3, help='Intervals with unchanged rect_acc to declare hard-stall')
    # Next-run boost parameters on hard-stall
    ap.add_argument('--boost-samples', type=int, default=384, help='CRSCE_GOBP_STALL_BOOST_SAMPLES on restart')
    ap.add_argument('--boost-accepts', type=int, default=24, help='CRSCE_GOBP_STALL_BOOST_ACCEPTS on restart')
    ap.add_argument('--gobp-timeout', type=int, default=90000, help='CRSCE_GOBP_TIMEOUT_MS on restart')
    args = ap.parse_args(argv[1:])

    # Record our PID
    write_text(os.path.join('build', 'uselessTest', 'watchdog.pid'), str(os.getpid()))

    end_ts = time.monotonic() + args.hours * 3600.0
    # Track size for delta reporting
    last_size = os.path.getsize(args.log) if os.path.exists(args.log) else 0
    last_boost_ts = 0.0

    # Heartbeat state
    hb_last_acc = None  # type: Optional[int]
    hb_unchanged_cnt = 0
    last_hardstall_ts = 0.0

    while time.monotonic() < end_ts:
        # 1) Ensure runner alive
        runner_pid = read_pid(args.pidfile)
        runner_alive = is_pid_alive(runner_pid) if runner_pid else False
        remaining_h = max(0.0, (end_ts - time.monotonic()) / 3600.0)
        if not runner_alive and remaining_h > 0.05:
            append(
                os.path.join('build', 'uselessTest', 'watchdog_alerts.log'),
                f"[{now_str()}] runner not alive, restarting for {remaining_h:.2f}h\n",
            )
            # Choose script and out path based on --runner
            if args.runner == 'c1c2':
                script = os.path.join('tools', 'useless_night_runner_c1c2.py')
                outp = os.path.join('build', 'uselessTest', 'night_runner_followon.out')
            else:
                script = os.path.join('tools', 'useless_night_runner.py')
                outp = os.path.join('build', 'uselessTest', 'night_runner.out')
            new_pid = start_runner(remaining_h, outp, args.pidfile, script)
            append(
                os.path.join('build', 'uselessTest', 'watchdog_alerts.log'),
                f"[{now_str()}] restarted runner pid={new_pid}\n",
            )

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

        # 2b) Heartbeat-based stall detection (live gobp status)
        hb = parse_last_gobp_heartbeat(args.dx_stdout)
        hard_stall = False
        if hb is not None:
            rect_acc = hb['rect_acc']
            stall_ms = hb['stall_ms']
            acc_rate_bp = hb['acc_rate_bp']
            if hb_last_acc is None:
                hb_last_acc = rect_acc
                hb_unchanged_cnt = 0
            else:
                if rect_acc <= hb_last_acc:
                    hb_unchanged_cnt += 1
                else:
                    hb_unchanged_cnt = 0
                hb_last_acc = rect_acc
            # Hard-stall rule: stall_ms over threshold and rect_acc unchanged for N intervals and acc_rate very low
            if stall_ms >= args.hb_stall_ms and hb_unchanged_cnt >= args.hb_stall_intervals and acc_rate_bp <= 5:
                hard_stall = True

        # 3) Intervene on stall (rate-limited)
        now_mono = time.monotonic()
        if stalled and (now_mono - last_boost_ts) > 15 * 60:
            run_ab_boost('stall detected')
            last_boost_ts = now_mono

        # 4) Intervene on no positive signals over window (rate-limited)
        if (not stalled) and (not any_pos) and (now_mono - last_boost_ts) > 30 * 60:
            run_ab_boost('no positive signals window')
            last_boost_ts = now_mono

        # 4b) Hard-stall by heartbeat: kill+restart immediately with stronger env (rate-limited)
        if hard_stall and (now_mono - last_hardstall_ts) > 10 * 60 and remaining_h > 0.10:
            append(
                os.path.join('build', 'uselessTest', 'watchdog_alerts.log'),
                f"[{now_str()}] heartbeat hard-stall: stall_ms>={args.hb_stall_ms} unchanged={hb_unchanged_cnt}, restarting with boost env\n",
            )
            # Kill runner if alive
            if runner_alive and runner_pid:
                try:
                    os.kill(runner_pid, 15)
                except Exception:
                    pass
            # Strengthen env for relaunch (inherited by child)
            os.environ['CRSCE_GOBP_STALL_BOOST_SAMPLES'] = str(args.boost_samples)
            os.environ['CRSCE_GOBP_STALL_BOOST_ACCEPTS'] = str(args.boost_accepts)
            os.environ['CRSCE_GOBP_TIMEOUT_MS'] = str(args.gobp_timeout)
            # Restart immediately
            if args.runner == 'c1c2':
                script = os.path.join('tools', 'useless_night_runner_c1c2.py')
                outp = os.path.join('build', 'uselessTest', 'night_runner_followon.out')
            else:
                script = os.path.join('tools', 'useless_night_runner.py')
                outp = os.path.join('build', 'uselessTest', 'night_runner.out')
            pid2 = start_runner(remaining_h, outp, args.pidfile, script)
            append(
                os.path.join('build', 'uselessTest', 'watchdog_alerts.log'),
                f"[{now_str()}] restarted runner after hard-stall pid={pid2}\n",
            )
            last_hardstall_ts = now_mono

        # 5) Heartbeat status
        status = []
        status.append(f"time: {now_str()}")
        status.append(f"runner_pid: {runner_pid if runner_pid else 'n/a'} alive={runner_alive}")
        status.append(f"remaining_hours: {remaining_h:.2f}")
        status.append(f"log_path: {args.log}")
        status.append(f"log_size: {size} bytes delta={size - last_size}")
        status.append(f"log_mtime: {dt.datetime.fromtimestamp(mtime).isoformat() if mtime else 'n/a'}")
        status.append(
            f"signals: contra_gt_3={sigs['contra_gt_3']} "
            f"ms_success_gt_0={sigs['ms_success_gt_0']} "
            f"prefix_gt_0={sigs['prefix_gt_0']} "
            f"bnb_nodes>0.81M={sigs['bnb_nodes_gt_0_81M']}"
        )
        hb_line = (
            f"hb: ts={hb['ts_ms']} iters={hb['iters']} acc={hb['rect_acc']} att={hb['rect_att']} "
            f"stall_ms={hb['stall_ms']} acc_bp={hb['acc_rate_bp']} unchanged_cnt={hb_unchanged_cnt}"
            if hb is not None else 'hb: n/a'
        )
        status.append(f"stalled={stalled} hard_stall={hard_stall}")
        status.append(hb_line)
        write_text(os.path.join('build', 'uselessTest', 'watchdog_status.txt'), '\n'.join(status) + '\n')

        last_size = size
        time.sleep(max(1, args.check_interval))

    append(
        os.path.join('build', 'uselessTest', 'watchdog_alerts.log'),
        f"[{now_str()}] watchdog finished after {args.hours} hours\n",
    )
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))
