#!/usr/bin/env python3
"""
Render a quick ASCII sparkline of unknown_history from the last JSON
object in an appended completion_stats.log file.

Usage:
  python3 tools/uh_spark.py [path]

Default path: build/uselessTest/completion_stats.log
"""
from __future__ import annotations

import json
import os
import sys
from typing import List


def load_last(path: str):
    s = open(path,'r',encoding='utf-8').read()
    depth=0; ins=False; esc=False; st=-1; last=None
    for i,ch in enumerate(s):
        if ins:
            if esc: esc=False
            elif ch=='\\': esc=True
            elif ch=='"': ins=False
            continue
        else:
            if ch=='"': ins=True; continue
            if ch=='{':
                if depth==0: st=i
                depth+=1
            elif ch=='}':
                depth-=1
                if depth==0 and st!=-1:
                    try:
                        last=json.loads(s[st:i+1])
                    except Exception:
                        pass
                    st=-1
    return last


def spark(values: List[float], width=80):
    if not values:
        return '(no data)'
    # downsample to width
    n=len(values)
    if n>width:
        step=n/width
        v=[values[int(i*step)] for i in range(width)]
    else:
        v=values[:]
    lo=min(v); hi=max(v)
    chars=' .:-=+*#%@'
    if hi==lo:
        return chars[-1]*len(v)
    out=[]
    for x in v:
        t=(x-lo)/(hi-lo)
        idx=int(t*(len(chars)-1)+0.5)
        out.append(chars[idx])
    return ''.join(out)


def main(argv):
    p=argv[1] if len(argv)>1 else os.path.join('build','uselessTest','completion_stats.log')
    if not os.path.exists(p):
        print(f'error: not found: {p}', file=sys.stderr)
        return 2
    o=load_last(p)
    if not o:
        print('no record')
        return 0
    uh=o.get('unknown_history',[])
    print('unknown_history len=',len(uh))
    print('spark:', spark(uh))
    return 0


if __name__=='__main__':
    raise SystemExit(main(sys.argv))

