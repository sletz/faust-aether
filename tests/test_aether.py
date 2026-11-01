#!/usr/bin/env python3
"""Offline audible-output, stability and reproducibility tests, using faustprobe."""
import array
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import shlex
import shutil
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
OUT = Path(os.environ.get('AETHER_RESULTS', str(ROOT / 'test-results')))
OUT.mkdir(parents=True, exist_ok=True)
PROBE = os.environ.get('FAUSTPROBE', 'faustprobe')
FAUST = os.environ.get('FAUST', 'faust')
PATCHES = json.loads((ROOT / 'presets/patches.json').read_text())
REPORT = []

def run(cmd, expected=0):
    print('$ ' + shlex.join(map(str, cmd)), flush=True)
    p = subprocess.run(list(map(str, cmd)), capture_output=True, text=True)
    # Preserve both channels, including expected comparison failures.
    print(p.stdout, end='')
    print(p.stderr, end='', file=sys.stderr)
    assert p.returncode == expected, f'exit {p.returncode}, expected {expected}'
    return p.stdout, p.stderr

# Identity is printed before running any comparison.
for tool in (FAUST, PROBE):
    stdout, stderr = run([tool, '--version'])
    resolved = Path(shutil.which(tool) or tool).resolve()
    REPORT.append({'tool':str(resolved), 'version':stdout+stderr,
                   'sha256':hashlib.sha256(resolved.read_bytes()).hexdigest()})
    if tool == FAUST:
        assert 'Source commit:' in stdout, 'compiler must identify its sources'
print(platform.platform(), platform.machine(), flush=True)
run(['clang++', '--version'])

def case(name, flags, file='aether.dsp', audible=True, single=False, frames=24000, expect_silent=False, excitation='zero'):
    cmd = [PROBE, '--single' if single else '--double', '-I', ROOT,
           '--in', excitation, '--quiet', '--format', 'json', '--fail-above', '0.921',
           '-n', frames, *flags, ROOT/file]
    text, err = run(cmd)
    document = json.loads(text)
    d = document['runs'][0] if 'runs' in document else document
    assert len(d['channels']) == 2, 'stereo output required'
    for c in d['channels']:
        assert all(math.isfinite(c[k]) for k in ('peak','rms','dc'))
        assert c['peak'] <= 0.921
    if audible:
        assert min(c['rms'] for c in d['channels']) > 1e-6, 'silent or broken patch'
    if expect_silent:
        assert max(c['peak'] for c in d['channels']) < 1e-12, 'idle leakage'
    record = {'name':name, 'command':shlex.join(map(str,cmd)), 'result':d, 'document':document, 'stderr':err}
    REPORT.append(record)
    (OUT/(name+'.json')).write_text(json.dumps(record,indent=2)+'\n')
    return d

note = ['--set','gate=1','--set','freq=220']
poly = ['--nvoices','12','--chord','45,52,57,60,64,67:110@0..18000']

# A deliberately altered sample must make the actual verdict tool fail.
# Tests use a tiny deterministic fixture, separate from instrument comparisons.
fixture = OUT/'verdict-fixture.dsp'
fixture.write_text('process = 0.125, 0.25;\n')
reference = OUT/'verdict-reference.wav'
run([PROBE,'--double','--in','zero','-n','32','--out',reference,fixture])
raw = bytearray(reference.read_bytes())
pos = 12
while raw[pos:pos+4] != b'data':
    length = struct.unpack_from('<I',raw,pos+4)[0]
    pos += 8+length+(length%2)
# A float64 WAV has 16 bytes/frame; float32 has 8. Change a value, not metadata.
length = struct.unpack_from('<I',raw,pos+4)[0]
fmt = '<d' if length//32//2 == 8 else '<f'
struct.pack_into(fmt,raw,pos+8,0.625)
reference.write_bytes(raw)
stdout, stderr = run([PROBE,'--double','--in','zero','-n','32','--quiet',
                     '--ref',reference,fixture], expected=1)
assert 'mismatch' in (stdout+stderr).lower() or 'differ' in (stdout+stderr).lower() or 'fail' in (stdout+stderr).lower()
REPORT.append({'name':'comparison-negative-control','expected_exit':1,'stdout':stdout,'stderr':stderr})

# Confirm automatic extraction: a silently omitted effect is a broken instrument.
stdout, stderr = run([PROBE,'--double','--nvoices','12','-I',ROOT,'--list-params',ROOT/'aether.dsp'])
assert '0 input(s)/voice' in stdout and 'effect: yes (2 -> 2)' in stdout
fxparams, _ = run([PROBE,'--double','-I',ROOT,'--list-params',ROOT/'effects.dsp'])
assert '/Effects/' in fxparams
(OUT/'parameters.txt').write_text(stdout+'\n'+fxparams)
REPORT.append({'name':'structure','controls':sum('slider' in line or 'button' in line or 'checkbox' in line for line in (stdout+'\n'+fxparams).splitlines())})

case('idle', ['--nvoices','12'], audible=False, expect_silent=True)
for name, patch in PATCHES.items():
    controls = [x for k,v in patch.items() for x in ('--set',f'{k}={v}')]
    case('preset-'+name, poly+controls, frames=48000)
case('twelve-voices', ['--nvoices','12','--chord','36,43,48,52,55,59,60,64,67,71,74,79:127@0..18000'])
case('voice-stealing', ['--nvoices','3','--chord','48,55,60:100@0..16000',
                      '--chord','62,65,69:127@4000..20000','--note','80:100@8000..14000'])
case('single-precision',poly,single=True)
for sr in (32000,44100,48000,96000,192000):
    case('rate-'+str(sr),poly+['--sr',str(sr),'--set','resonance=0.9',
         '--set','cutoff=18000','--set','filter_env_octaves=7','--set','drive=8',
         '--set','fold=1','--set','fm_index=12','--set','brightness=1',
         '--set','delay_feedback=0.87','--set','reverb_seconds=8'],frames=sr)
# Every modulation source and destination is active in the same patch.
matrix = []
for i in range(8):
    for k,v in [('source',i+1),('destination',i%6+1),('depth',(-1 if i%2 else 1)*0.35)]:
        matrix += ['--set',f'Route_{i}/{k}={v}']
case('matrix-and-automation',poly+matrix+['--at','6000','macro1=1','--at','9000','macro2=1',
     '--at','12000','filter_type=2','--at','15000','cutoff=30'])
for mode in (0,1,2):
    case('filter-'+str(mode),poly+['--set',f'filter_type={mode}','--set','resonance=0.9'])
case('low-and-high-notes',['--nvoices','4','--note','0:100@0..12000','--note','127:127@0..12000'])
case('sequencer', ['--set','sequencer=1'],file='audition.dsp',frames=96000)
case('voice-invariants',note+['--check','block=1,7,512','--check','reset','--check','determinism'],frames=8192)
case('effect-invariants',['--check','block=1,7,512','--check','reset','--check','determinism'],
     file='effects.dsp',frames=32768,excitation='impulse')
# With a short release and dry bus, silence must return after note-off.
dry = [x for k in ('chorus_mix','phaser_mix','delay_mix','reverb_mix') for x in ('--set',k+'=0')]
case('release-to-silence',['--nvoices','4','--note','60:100@0..2000','--set','amp_release=0.02',
     '--skip','18000',*dry],frames=24000,audible=False,expect_silent=True)
# An audible shared effect tail after voice reclamation is expected.
case('shared-tail',['--nvoices','4','--chord','48,55,60:100@0..4000','--set','amp_release=0.02',
     '--set','reverb_mix=0.8','--skip','16000'],frames=48000)

# Export both backends; all outputs stay in the ignored results directory.
for entry in ('aether','effects','audition'):
    for lang,ext in (('cpp','cpp'),('rust','rs')):
        run([FAUST,'-double','-I',ROOT,'-lang',lang,ROOT/(entry+'.dsp'),'-o',OUT/(entry+'.'+ext)])
REPORT.append({'name':'backend-exports','programs':3,'backends':['cpp','rust'],'precision':'double'})
(OUT/'summary.json').write_text(json.dumps({'machine':platform.platform(),'records':REPORT},indent=2)+'\n')
print(f'PASS: {sum("result" in r for r in REPORT)} audio cases, 6 backend exports, structure and negative control')
