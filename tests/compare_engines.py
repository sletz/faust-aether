#!/usr/bin/env python3
"""Compare actual polyphonic samples from two faustprobe engines."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from wav_utils import read_float_wav

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('probe_a')
p.add_argument('probe_b')
a = p.parse_args()
out = ROOT/'test-results/cross-engine'
out.mkdir(parents=True,exist_ok=True)
records = []
compiler = os.environ.get('FAUST','faust')
print(subprocess.check_output([compiler,'--version'],text=True),flush=True)
for probe in (a.probe_a,a.probe_b):
    print(subprocess.check_output([probe,'--version'],text=True),flush=True)
    print('sha256:',hashlib.sha256(Path(probe).read_bytes()).hexdigest(),flush=True)
for name,flags in [('chord',[]),('modulated',['--set','fm_index=8','--set','ring_mod=0.5',
                         '--set','fold=0.5','--at','16384','cutoff=600'])]:
    files = []
    commands = []
    for i,probe in enumerate((a.probe_a,a.probe_b)):
        path = out/f'{name}-{i}.wav'
        cmd = [probe,'--double','-I',str(ROOT),'--in','zero','--nvoices','8',
               '--chord','48,55,60,63:100@0..22050','--sr','44100','-n','65536',
               '--quiet','--out',str(path),*flags,str(ROOT/'aether.dsp')]
        print('$ '+shlex.join(cmd),flush=True)
        subprocess.run(cmd,check=True)
        commands.append(shlex.join(cmd))
        files.append(read_float_wav(path))
    assert files[0][:2] == files[1][:2] == (44100,2)
    x,y = files[0][2],files[1][2]
    assert len(x)==len(y)==65536*2
    deltas = [max(abs(x[i]-y[i]) for i in range(c,len(x),2)) for c in range(2)]
    assert max(deltas) < 1e-8, deltas
    records.append({'name':name,'commands':commands,'frames':65536,'channels':2,
                    'max_abs_difference':deltas,'absolute_tolerance':1e-8})
    print(name,deltas,flush=True)
(out/'summary.json').write_text(json.dumps(records,indent=2)+'\n')
print('PASS: 2 programs/renders, 65536 frames × 2 channels each')
