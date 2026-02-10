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
                                             [--seed-chunk-size 0]
                                             [--auto-prune]
Notes:
- When --seed-chunk-size > 0, seeds are processed in rotating chunks per cycle
  to preserve diversity across long runs.
- If --auto-prune is set, after the first full C1+C2 cycle the script scans
  completion_stats.log for the cycle's records and writes a keep list based on
  early signals, then restricts subsequent cycles to those survivors.
  Keep if any of: bnb_nodes>0.81M, restart_contradiction_count>=1,
  micro_solver2_attempts>0; strong keep if micro_solver_successes>0 or
  valid_prefix>0.
"""
from __future__ import annotations

import argparse
import datetime as dt
import itertools
import os
import subprocess
import sys
import time
from typing import Dict, List, Any
import json
import re


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


def _slugify(text: str) -> str:
    s = re.sub(r"[^A-Za-z0-9._-]+", "_", text.strip())
    return s[:120]


def _split_for_jobs(seeds: List[int], jobs: int) -> List[List[int]]:
    if jobs <= 1 or len(seeds) <= 1:
        return [list(seeds)]
    chunks: List[List[int]] = [[] for _ in range(min(jobs, len(seeds)))]
    for i, s in enumerate(sorted(seeds)):
        chunks[i % len(chunks)].append(s)
    return chunks


def run_sweep_concurrent(label_prefix: str,
                         base_args: List[str],
                         cycle: int,
                         seeds_lists: List[List[int]],
                         out_base: str,
                         merge_into: str) -> None:
    # Launch N jobs of useless_ab_sweep.py in --direct mode with separate outdirs; then merge logs and tables.
    procs: List[subprocess.Popen] = []
    outs: List[str] = []
    labs: List[str] = []
    for idx, seed_list in enumerate(seeds_lists):
        if not seed_list:
            continue
        seeds_arg = format_seeds_arg(seed_list)
        job_label = f"{label_prefix} job={idx} seeds={seeds_arg}"
        labs.append(job_label)
        outdir = os.path.join(out_base, f"cycle-{cycle}", _slugify(label_prefix), f"job-{idx}")
        os.makedirs(outdir, exist_ok=True)
        args = [
            os.path.join('tools', 'useless_ab_sweep.py'),
            '--direct', '--outdir', outdir,
            '--seeds', seeds_arg,
        ] + base_args
        procs.append(subprocess.Popen([sys.executable] + args, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT))
    # Collect
    table_path = os.path.join('build', 'uselessTest', 'night_sweep_table.txt')
    for idx, p in enumerate(procs):
        if p.stdout is None:
            out = ''
        else:
            out = p.stdout.read()
        p.wait()
        outs.append(out)
        # Write table per job
        append(table_path, f"\n=== {now_str()} :: {labs[idx]} ===\n")
        append(table_path, out or '')
        # Merge this job's completion log into main
        try:
            # Parse outdir from command label
            job_dir = os.path.join(out_base, f"cycle-{cycle}", _slugify(label_prefix), f"job-{idx}")
            src_log = os.path.join(job_dir, 'completion_stats.log')
            if os.path.exists(src_log):
                with open(src_log, 'r', encoding='utf-8') as f:
                    data = f.read()
                if data:
                    append(merge_into, data if data.endswith('\n') else data + '\n')
        except Exception:
            pass
    # After all jobs, append a summary snapshot from the merged log
    prog = subprocess.run([sys.executable, os.path.join('tools', 'useless_progress.py'), merge_into], text=True, capture_output=True)
    summary_path = os.path.join('build', 'uselessTest', 'night_summary.txt')
    append(summary_path, f"\n[{now_str()}] {label_prefix} (concurrent jobs={len(seeds_lists)})\n")
    append(summary_path, (prog.stdout or ''))


def parse_seeds_arg(arg: str) -> List[int]:
    arg = (arg or '').strip()
    if not arg:
        return []
    if '-' in arg:
        a, b = arg.split('-', 1)
        return list(range(int(a), int(b) + 1))
    return [int(x) for x in arg.split(',') if x]


def format_seeds_arg(seeds: List[int]) -> str:
    if not seeds:
        return ''
    seeds = sorted(set(seeds))
    if seeds[-1] - seeds[0] + 1 == len(seeds):
        return f"{seeds[0]}-{seeds[-1]}"
    return ','.join(str(s) for s in seeds)


def make_chunks(items: List[int], chunk_size: int) -> List[List[int]]:
    if chunk_size <= 0 or chunk_size >= len(items):
        return [list(items)]
    return [items[i:i + chunk_size] for i in range(0, len(items), chunk_size)]


def load_json_objects(path: str) -> List[Dict[str, Any]]:
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
                    except Exception:
                        pass
                    start_idx = -1
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


def compute_seed_signals(records: List[Dict[str, Any]], min_end_ms: int | None = None) -> Dict[int, Dict[str, int]]:
    out: Dict[int, Dict[str, int]] = {}
    for o in records:
        end_ms = pick_int(o, 'end_ms', 0)
        if min_end_ms is not None and end_ms <= min_end_ms:
            continue
        seed = pick_int(o, 'rng_seed_belief', -1)
        if seed < 0:
            continue
        bnb = 1 if pick_int(o, 'micro_solver_bnb_nodes', 0) > 810_000 else 0
        contra = 1 if pick_int(o, 'restart_contradiction_count', 0) >= 1 else 0
        ms2 = 1 if pick_int(o, 'micro_solver2_attempts', 0) > 0 else 0
        adopt = 1 if pick_int(o, 'micro_solver_successes', 0) > 0 else 0
        prefix = 1 if pick_int(o, 'valid_prefix', 0) > 0 else 0
        cur = out.setdefault(seed, {'bnb': 0, 'contra': 0, 'ms2': 0, 'adopt': 0, 'prefix': 0})
        cur['bnb'] = max(cur['bnb'], bnb)
        cur['contra'] = max(cur['contra'], contra)
        cur['ms2'] = max(cur['ms2'], ms2)
        cur['adopt'] = max(cur['adopt'], adopt)
        cur['prefix'] = max(cur['prefix'], prefix)
    return out


def choose_seed_chunk(all_seeds: List[int], chunk_size: int, cycle: int) -> List[int]:
    chunks = make_chunks(list(sorted(set(all_seeds))), chunk_size)
    if not chunks:
        return []
    idx = cycle % len(chunks)
    return chunks[idx]


def main(argv: List[str]) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('--hours', type=float, default=8.0, help='Time budget in hours for follow-on')
    ap.add_argument('--seeds', default='4-12')
    ap.add_argument('--seed-chunk-size', type=int, default=0, help='Process seeds in chunks of this size (0=all)')
    ap.add_argument('--auto-prune', action='store_true', help='After first C1+C2 cycle, prune seeds by early signals')
    ap.add_argument('--bin', default=os.path.join('bin', 'uselessTest'))
    ap.add_argument('--jobs', type=int, default=1, help='Concurrent seed subsets per sweep (uses --direct mode)')
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
    all_seeds: List[int] = parse_seeds_arg(args.seeds)
    if not all_seeds:
        print('error: no seeds provided', file=sys.stderr)
        return 2
    prune_done = False
    build_dir = os.path.join('build', 'uselessTest')
    keep_path = os.path.join(build_dir, 'seed_keep_list.txt')
    drop_path = os.path.join(build_dir, 'seed_drop_list.txt')
    pool_path = os.path.join(build_dir, 'seed_pool.txt')
    log_path = os.path.join(build_dir, 'completion_stats.log')

    def current_last_end_ms() -> int:
        objs = load_json_objects(log_path)
        if not objs:
            return 0
        last = objs[-1]
        return pick_int(last, 'end_ms', 0)

    while time.monotonic() < end_ts:
        seeds_chunk = choose_seed_chunk(all_seeds, args.seed_chunk_size, cycle)
        seeds_arg = format_seeds_arg(seeds_chunk)
        append(pool_path, f"[{now_str()}] cycle={cycle} pool_size={len(all_seeds)} chunk_size={len(seeds_chunk)} seeds={seeds_arg}\n")
        cycle_start_end_ms = current_last_end_ms()
        # C1 cycle (contradictions) – tighter verify/audit, shorter RS window, broaden PAR_RS
        if time.monotonic() >= end_ts:
            break
        for u in [20, 24, 28]:
            if time.monotonic() >= end_ts:
                break
            for prs in [12, 16]:
                if time.monotonic() >= end_ts:
                    break
                label = f"C1 cycle={cycle} nearU={u} parRS={prs} seeds={seeds_arg}"
                if args.jobs > 1:
                    base = [
                        '--verifyTick', '24',
                        '--auditForce', '20', '--auditGrace', '3',
                        '--nearU', str(u), '--parRS', str(prs),
                        '--parMS', '15000' if prune_done else '10000', '--ambFB', '160',
                        '--msWindow', '128', '--msSwaps', '64',
                        '--bnbNodes', '2000000' if prune_done else '1500000',
                    ]
                    seeds_jobs = _split_for_jobs(seeds_chunk, args.jobs)
                    out_base = os.path.join('build', 'uselessTest', 'parallel')
                    run_sweep_concurrent(label, base, cycle, seeds_jobs, out_base, os.path.join('build', 'uselessTest', 'completion_stats.log'))
                else:
                    sweep = [
                        '--seeds', seeds_arg,
                        '--verifyTick', '24',
                        '--auditForce', '20', '--auditGrace', '3',
                        '--nearU', str(u), '--parRS', str(prs),
                        '--parMS', '15000' if prune_done else '10000', '--ambFB', '160',
                        '--msWindow', '128', '--msSwaps', '64',
                        '--bnbNodes', '2000000' if prune_done else '1500000',
                    ]
                    run_sweep(sweep, label)

        # C2 cycle (adoptions + MS2) – heavier MS2 windows and budgets
        if time.monotonic() >= end_ts:
            break
        for u in [28, 24]:
            if time.monotonic() >= end_ts:
                break
            for ms2nodes in ([2000000] if prune_done else [1500000, 2000000]):
                if time.monotonic() >= end_ts:
                    break
                for ms2ms in ([2500] if prune_done else [2000, 2500]):
                    if time.monotonic() >= end_ts:
                        break
                    label = f"C2 cycle={cycle} nearU={u} ms2Nodes={ms2nodes} ms2Ms={ms2ms} seeds={seeds_arg}"
                    if args.jobs > 1:
                        base = [
                            '--verifyTick', '48',
                            '--auditForce', '30', '--auditGrace', '10',
                            '--nearU', str(u), '--parRS', '8', '--parMS', '45000' if prune_done else '40000', '--ambFB', '160',
                            '--msWindow', '192' if prune_done else '160', '--msSwaps', '128' if prune_done else '96',
                            '--ms2-window', '128', '--ms2-max-ms', str(ms2ms), '--ms2-max-nodes', str(ms2nodes),
                            '--bnbMS', '6000', '--bnbNodes', '2000000',
                        ]
                        seeds_jobs = _split_for_jobs(seeds_chunk, args.jobs)
                        out_base = os.path.join('build', 'uselessTest', 'parallel')
                        run_sweep_concurrent(label, base, cycle, seeds_jobs, out_base, os.path.join('build', 'uselessTest', 'completion_stats.log'))
                    else:
                        sweep = [
                            '--seeds', seeds_arg,
                            '--verifyTick', '48',
                            '--auditForce', '30', '--auditGrace', '10',
                            '--nearU', str(u), '--parRS', '8', '--parMS', '45000' if prune_done else '40000', '--ambFB', '160',
                            '--msWindow', '192' if prune_done else '160', '--msSwaps', '128' if prune_done else '96',
                            '--ms2-window', '128', '--ms2-max-ms', str(ms2ms), '--ms2-max-nodes', str(ms2nodes),
                            '--bnbMS', '6000', '--bnbNodes', '2000000',
                        ]
                        run_sweep(sweep, label)

        if args.auto_prune and not prune_done:
            objs = load_json_objects(log_path)
            sigs = compute_seed_signals(objs, min_end_ms=cycle_start_end_ms)
            keep: List[int] = []
            drop: List[int] = []
            for s in all_seeds:
                v = sigs.get(s, {'bnb': 0, 'contra': 0, 'ms2': 0, 'adopt': 0, 'prefix': 0})
                strong = (v['adopt'] > 0) or (v['prefix'] > 0)
                weak = (v['bnb'] > 0) or (v['contra'] > 0) or (v['ms2'] > 0)
                if strong or weak:
                    keep.append(s)
                else:
                    drop.append(s)
            append(keep_path, f"[{now_str()}] cycle={cycle} kept {len(keep)} of {len(all_seeds)}\n")
            if keep:
                append(keep_path, ' ' + ' '.join(str(x) for x in keep) + '\n')
            append(drop_path, f"[{now_str()}] cycle={cycle} dropped {len(drop)}\n")
            if drop:
                append(drop_path, ' ' + ' '.join(str(x) for x in drop) + '\n')
            if keep:
                all_seeds = keep
            prune_done = True

        cycle += 1

    print(f"[{now_str()}] follow-on runner finished after {args.hours} hours")
    return 0


if __name__ == '__main__':
    raise SystemExit(main(sys.argv))
