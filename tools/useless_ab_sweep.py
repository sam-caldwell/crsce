#!/usr/bin/env python3
"""
Run broadened A/B sweeps for uselessTest with environment-driven knobs.

Covers the following axes by default:
- CRSCE_AUDIT_FORCE_EVERY = 30
- CRSCE_AUDIT_GRACE = 15
- CRSCE_NEARLOCK_U in {24, 28}
- CRSCE_PAR_RS = 8
- CRSCE_PAR_RS_MAX_MS in {30000, 40000}
- CRSCE_MS_AMB_FALLBACK in {160, 192}
- CRSCE_MS_BNB_MS = 4500
- CRSCE_SOLVER_SEED in {4,5,6,7,8} (configurable)

Outputs 1 line per run with key params + quick signals/metrics.

Usage:
  python3 tools/useless_ab_sweep.py [--seeds 4-8] [--limit N] [--bin bin/uselessTest]
                                   [--verifyTick N] [--nearU N] [--parRS N]
                                   [--parMS N] [--ambFB N] [--msWindow N]
                                   [--msSwaps N] [--bnbMS N] [--bnbNodes N]
                                   [--auditForce N] [--auditGrace N]
                                   [--ms2-window N] [--ms2-max-ms N] [--ms2-max-nodes N]

Notes:
- Expects binaries in ./bin and docs/testData/useless-machine.mp4 in repo.
- Appends to build/uselessTest/completion_stats.log. Use tools/useless_log_summary.py for deeper analysis.
"""
from __future__ import annotations

import argparse
import itertools
import json
import os
import subprocess
import sys
import time
from typing import Any, Dict, List, Tuple


def parse_seeds(arg: str) -> List[int]:
    if '-' in arg:
        a, b = arg.split('-', 1)
        return list(range(int(a), int(b) + 1))
    return [int(x) for x in arg.split(',') if x]


def load_last_completion(path: str) -> Dict[str, Any] | None:
    """Parse last pretty JSON object from completion_stats.log."""
    if not os.path.exists(path):
        return None
    with open(path, 'r', encoding='utf-8') as f:
        data = f.read()
    depth = 0
    in_str = False
    escape = False
    start_idx = -1
    last = None
    for i, ch in enumerate(data):
        if in_str:
            if escape:
                escape = False
            elif ch == '\\':
                escape = True
            elif ch == '"':
                in_str = False
        else:
            if ch == '"':
                in_str = True
            elif ch == '{':
                if depth == 0:
                    start_idx = i
                depth += 1
            elif ch == '}':
                depth -= 1
                if depth == 0 and start_idx != -1:
                    try:
                        obj = json.loads(data[start_idx:i + 1])
                        last = obj
                    except Exception:
                        pass
                    start_idx = -1
    return last


def pick(o: Dict[str, Any], key: str, default: int | float = 0) -> int | float:
    v = o.get(key)
    if isinstance(v, (int, float)):
        return v
    try:
        return int(v) if v is not None else default
    except Exception:
        try:
            return float(v) if v is not None else default
        except Exception:
            return default


def run_one(bin_path: str, env: Dict[str, str]) -> Tuple[int, Dict[str, Any] | None]:
    # Ensure PATH contains repo bin
    full_env = dict(os.environ)
    full_env.update(env)
    full_env['PATH'] = os.pathsep.join([os.path.abspath('bin'), full_env.get('PATH', '')])
    # Run uselessTest; it prints key markers and writes completion_stats.log on failure
    try:
        cp = subprocess.run([bin_path], capture_output=True, text=True, env=full_env, timeout=3600)
    except subprocess.TimeoutExpired:
        return (124, None)
    # Attempt to locate row completion log path from stdout marker
    log_path = None
    for line in (cp.stdout or '').splitlines():
        if line.startswith('ROW_COMPLETION_LOG='):
            log_path = line.partition('=')[2].strip()
            break
    if not log_path:
        # default path
        log_path = os.path.join('build', 'uselessTest', 'completion_stats.log')
    last = load_last_completion(log_path)
    return (cp.returncode, last)


def run_one_direct(container: str, out_dir: str, env: Dict[str, str]) -> Tuple[int, Dict[str, Any] | None]:
    full_env = dict(os.environ)
    full_env.update(env)
    full_env['PATH'] = os.pathsep.join([os.path.abspath('bin'), full_env.get('PATH', '')])
    os.makedirs(out_dir, exist_ok=True)
    # unique output to avoid overwrite errors
    seed = env.get('CRSCE_SOLVER_SEED', '0')
    near = env.get('CRSCE_NEARLOCK_U', '')
    pms = env.get('CRSCE_PAR_RS_MAX_MS', '')
    amb = env.get('CRSCE_MS_AMB_FALLBACK', '')
    tag = f"s{seed}_u{near}_ms{pms}_af{amb}"
    out_path = os.path.join(out_dir, f"recon_{tag}.mp4")
    try:
        if os.path.exists(out_path):
            os.remove(out_path)
    except Exception:
        pass
    cmd = ['bin/decompress', '-in', container, '-out', out_path]
    try:
        cp = subprocess.run(cmd, capture_output=True, text=True, env=full_env, timeout=3600)
    except subprocess.TimeoutExpired:
        return (124, None)
    log_path = None
    for line in (cp.stdout or '').splitlines():
        if line.startswith('ROW_COMPLETION_LOG='):
            log_path = line.partition('=')[2].strip()
            break
    if not log_path:
        # default path in the chosen out_dir
        log_path = os.path.join(out_dir, 'completion_stats.log')
    last = load_last_completion(log_path)
    return (cp.returncode, last)


def main(argv: List[str]) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('--seeds', default='4-8', help='Seed list/range, e.g. 4-8 or 1,2,3')
    ap.add_argument('--limit', type=int, default=0, help='Max runs (0 = no limit)')
    ap.add_argument('--bin', default=os.path.join('bin', 'uselessTest'), help='Path to uselessTest binary')
    ap.add_argument('--direct', action='store_true', help='Run bin/decompress directly (compress once, faster)')
    ap.add_argument(
        '--container',
        default=os.path.join('build', 'uselessTest', 'useless-machine.crsce'),
        help='CRSCE container path for --direct',
    )
    ap.add_argument(
        '--outdir',
        default=os.path.join('build', 'uselessSweep'),
        help='Output directory for --direct runs',
    )
    # Targeted overrides
    ap.add_argument('--nearU', type=int, help='Override CRSCE_NEARLOCK_U single value')
    ap.add_argument('--parRS', type=int, help='Override CRSCE_PAR_RS single value (default 8)')
    ap.add_argument('--parMS', type=int, help='Override CRSCE_PAR_RS_MAX_MS single value')
    ap.add_argument('--ambFB', type=int, help='Override CRSCE_MS_AMB_FALLBACK single value')
    ap.add_argument('--verifyTick', type=int, help='Override CRSCE_VERIFY_TICK')
    ap.add_argument('--msWindow', type=int, help='Override CRSCE_MS_WINDOW')
    ap.add_argument('--msSwaps', type=int, help='Override CRSCE_MS_KVAR_SWAPS')
    ap.add_argument('--bnbMS', type=int, help='Override CRSCE_MS_BNB_MS')
    ap.add_argument('--bnbNodes', type=int, help='Override CRSCE_MS_BNB_MAX_NODES')
    ap.add_argument('--auditForce', type=int, help='Override CRSCE_AUDIT_FORCE_EVERY')
    ap.add_argument('--auditGrace', type=int, help='Override CRSCE_AUDIT_GRACE')
    # Two-row micro-solver knobs
    ap.add_argument('--ms2-window', type=int, dest='ms2_window', help='CRSCE_MS2_WINDOW')
    ap.add_argument('--ms2-max-ms', type=int, dest='ms2_max_ms', help='CRSCE_MS2_MAX_MS')
    ap.add_argument('--ms2-max-nodes', type=int, dest='ms2_max_nodes', help='CRSCE_MS2_MAX_NODES')
    args = ap.parse_args(argv[1:])

    seeds = parse_seeds(args.seeds)
    bin_path = args.bin
    if not os.path.exists(bin_path):
        print(f'error: binary not found: {bin_path}', file=sys.stderr)
        return 2

    # Default sweep axes
    audit_force_every = [args.auditForce] if args.auditForce else [30]
    audit_grace = [args.auditGrace] if args.auditGrace else [15]
    nearlock_u = [24, 28]
    par_rs = [8]
    par_rs_max_ms = [30000, 40000]
    amb_fallback = [160, 192]
    bnb_ms = [args.bnbMS] if args.bnbMS else [4500]
    if args.nearU:
        nearlock_u = [args.nearU]
    if args.parMS:
        par_rs_max_ms = [args.parMS]
    if args.ambFB:
        amb_fallback = [args.ambFB]

    # Prepare container if using direct mode
    if args.direct:
        cont = args.container
        if not os.path.exists(cont):
            os.makedirs(os.path.dirname(cont), exist_ok=True)
            src = os.path.join('docs', 'testData', 'useless-machine.mp4')
            if not os.path.exists(src):
                print(f'error: source not found: {src}', file=sys.stderr)
                return 2
            # produce container once
            cenv = dict(os.environ)
            cenv['PATH'] = os.pathsep.join([os.path.abspath('bin'), cenv.get('PATH', '')])
            cp = subprocess.run(['bin/compress', '-in', src, '-out', cont], capture_output=True, text=True, env=cenv)
            if cp.returncode != 0:
                sys.stderr.write(cp.stdout)
                sys.stderr.write(cp.stderr)
                print('error: compress failed', file=sys.stderr)
                return 2

    # Run combinations
    combos = list(itertools.product(nearlock_u, par_rs_max_ms, amb_fallback, bnb_ms, seeds))
    count = 0
    header = (
        'seed  nearU  parRS  parMS  ambFB  bnbMS  exit  '
        'ms_bnb_nodes  ms_success  restarts_contra  valid_prefix  rows_committed  '
        'gobp_iters  row_ms  row_it  rad_ms  rad_it  signals'
    )
    print(header)
    for (u, par_ms, amb, bms, seed) in combos:
        if args.limit and count >= args.limit:
            break
        env = {
            'CRSCE_AUDIT_FORCE_EVERY': str(audit_force_every[0]),
            'CRSCE_AUDIT_GRACE': str(audit_grace[0]),
            'CRSCE_NEARLOCK_U': str(u),
            'CRSCE_PAR_RS': str(args.parRS if args.parRS else par_rs[0]),
            'CRSCE_PAR_RS_MAX_MS': str(par_ms),
            'CRSCE_MS_AMB_FALLBACK': str(amb),
            'CRSCE_MS_BNB_MS': str(bms),
            'CRSCE_SOLVER_SEED': str(seed),
        }
        if args.verifyTick:
            env['CRSCE_VERIFY_TICK'] = str(args.verifyTick)
        if args.msWindow:
            env['CRSCE_MS_WINDOW'] = str(args.msWindow)
        if args.msSwaps:
            env['CRSCE_MS_KVAR_SWAPS'] = str(args.msSwaps)
        if args.bnbNodes:
            env['CRSCE_MS_BNB_MAX_NODES'] = str(args.bnbNodes)
        # Two-row micro-solver options
        if args.ms2_window:
            env['CRSCE_MS2_WINDOW'] = str(args.ms2_window)
        if args.ms2_max_ms:
            env['CRSCE_MS2_MAX_MS'] = str(args.ms2_max_ms)
        if args.ms2_max_nodes:
            env['CRSCE_MS2_MAX_NODES'] = str(args.ms2_max_nodes)
        if args.direct:
            rc, last = run_one_direct(args.container, args.outdir, env)
        else:
            rc, last = run_one(bin_path, env)
        # collect metrics
        ms_nodes = ms_succ = rest_contra = vpref = rcommit = gobp_iters = 0
        signals: List[str] = []
        if last:
            ms_nodes = int(pick(last, 'micro_solver_bnb_nodes', 0))
            ms_succ = int(pick(last, 'micro_solver_successes', 0))
            rest_contra = int(pick(last, 'restart_contradiction_count', 0))
            vpref = int(pick(last, 'valid_prefix', 0))
            rcommit = int(pick(last, 'rows_committed', 0))
            gobp_iters = int(pick(last, 'gobp_iters_run', 0))
            row_ms = int(pick(last, 'time_row_phase_ms', 0))
            row_it = int(pick(last, 'row_phase_iterations', 0))
            rad_ms = int(pick(last, 'time_radditz_ms', 0))
            rad_it = int(pick(last, 'radditz_iterations', 0))
            if ms_nodes > 810_000:
                signals.append('bnb_nodes>0.81M')
            if rest_contra > 3:
                signals.append('contra>3')
            if ms_succ > 0:
                signals.append('micro_adopt')
            if vpref > 0:
                signals.append('prefix_growth')
            if row_ms > 0:
                signals.append('row_phase')
            if rad_ms > 0:
                signals.append('radditz')
        print(
            f"{seed:4d}  {u:5d}  {par_rs[0]:5d}  {par_ms:5d}  {amb:5d}  {bms:5d}  {rc:4d}  "
            f"{ms_nodes:12d}  {ms_succ:10d}  {rest_contra:15d}  {vpref:12d}  {rcommit:14d}  "
            f"{gobp_iters:11d}  {row_ms:6d}  {row_it:6d}  {rad_ms:6d}  {rad_it:6d}  "
            f"{','.join(signals) if signals else '-'}"
        )
        sys.stdout.flush()
        count += 1
        # Small delay to reduce log interleaving issues
        time.sleep(0.05)

    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))
