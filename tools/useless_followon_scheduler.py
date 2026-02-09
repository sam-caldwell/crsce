#!/usr/bin/env python3
"""
Scheduler that waits for the current night runner to finish, then launches the
follow-on runner (C1/C2 profile).

Usage:
  python3 tools/useless_followon_scheduler.py \
    --hours 8 \
    [--wait-pidfile build/uselessTest/night_runner.pid]

Artifacts:
  - build/uselessTest/night_runner_followon.pid
  - build/uselessTest/night_runner_followon.out
  - build/uselessTest/followon_scheduler.out (if nohup'ed by caller)
"""
from __future__ import annotations

import argparse
import os
import subprocess
import sys
import time


def is_pid_alive(pid: int | None) -> bool:
    if not pid:
        return False
    try:
        os.kill(pid, 0)
        return True
    except ProcessLookupError:
        return False
    except PermissionError:
        return True


def read_pid(path: str) -> int | None:
    try:
        with open(path, 'r', encoding='utf-8') as f:
            return int(f.read().strip())
    except Exception:
        return None


def write_text(path: str, text: str) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', encoding='utf-8') as f:
        f.write(text)


def launch_followon(hours: float) -> int:
    os.makedirs(os.path.join('build', 'uselessTest'), exist_ok=True)
    out = open(os.path.join('build', 'uselessTest', 'night_runner_followon.out'), 'a', encoding='utf-8')
    proc = subprocess.Popen(
        [sys.executable, os.path.join('tools', 'useless_night_runner_c1c2.py'), '--hours', f'{hours:.3f}'],
        stdout=out, stderr=out, close_fds=True
    )
    write_text(os.path.join('build', 'uselessTest', 'night_runner_followon.pid'), str(proc.pid))
    return proc.pid


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('--hours', type=float, default=8.0, help='Follow-on run duration')
    ap.add_argument('--wait-pidfile', default=os.path.join('build', 'uselessTest', 'night_runner.pid'))
    ap.add_argument('--poll-seconds', type=int, default=30)
    args = ap.parse_args(argv[1:])

    # Record scheduler pid
    write_text(os.path.join('build', 'uselessTest', 'followon_scheduler.pid'), str(os.getpid()))

    # Wait for current runner to finish
    while True:
        pid = read_pid(args.wait_pidfile)
        if not is_pid_alive(pid):
            break
        time.sleep(max(1, args.poll_seconds))

    # Launch follow-on runner
    pid2 = launch_followon(args.hours)
    write_text(os.path.join('build', 'uselessTest', 'followon_scheduler.last'), f'launched followon pid={pid2}\n')
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))

