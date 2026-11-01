#!/usr/bin/env python3
"""Render a preset with faustprobe. No Python dependencies."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
from wav_utils import listening_copy

ROOT = Path(__file__).resolve().parents[1]
PATCHES = json.loads((ROOT / 'presets/patches.json').read_text())
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--preset', choices=PATCHES, default='nebula')
p.add_argument('--out', type=Path, default=ROOT / 'renders/aether-demo.wav')
p.add_argument('--seconds', type=float, default=20)
p.add_argument('--sr', type=int, default=48000)
p.add_argument('--probe', default=os.environ.get('FAUSTPROBE', 'faustprobe'))
a = p.parse_args()
if not 2 <= a.seconds <= 600 or not 32000 <= a.sr <= 192000:
    p.error('seconds: 2..600; sr: 32000..192000')
a.out.parent.mkdir(parents=True, exist_ok=True)
sr = a.sr
cmd = [a.probe, '--double', '-I', str(ROOT), '--in', 'zero', '--nvoices', '12',
       '--sr', str(sr), '-n', str(round(a.seconds*sr)), '--quiet',
       '--fail-above', '0.921', '--out', str(a.out)]
for key, value in PATCHES[a.preset].items():
    cmd += ['--set', f'{key}={value}']
# Leave at least 40% of short renders for releases and the shared reverb.
end = a.seconds*0.6
chords = [(45,52,57,60,64), (41,48,53,57,60), (48,55,60,64,67), (43,50,55,59,62)]
for i, chord in enumerate(chords):
    on, off = round(i*end/4*sr), round((i+0.9)*end/4*sr)
    cmd += ['--chord', f'{",".join(map(str,chord))}:{86+i*5}@{on}..{off}']
# A modest amount of automation, with every change on an exact frame.
cmd += ['--at', str(round(end*0.5*sr)), 'macro1=0.8',
        '--at', str(round(end*0.7*sr)), 'cutoff=4200', str(ROOT / 'aether.dsp')]
print(shlex.join(cmd), flush=True)
subprocess.run(cmd, check=True)
print(a.out.resolve())
preview = a.out.with_name(a.out.stem+'-preview.wav')
print('PCM16 listening copy:', listening_copy(a.out,preview))
print(preview.resolve())
