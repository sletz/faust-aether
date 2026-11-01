"""Read faustprobe's IEEE-float WAV and make a portable listening copy."""
import array
import math
from pathlib import Path
import struct
import sys
import wave


def read_float_wav(path):
    raw = Path(path).read_bytes()
    assert raw[:4] == b'RIFF' and raw[8:12] == b'WAVE'
    pos, fmt, data = 12, None, None
    while pos+8 <= len(raw):
        chunk, size = raw[pos:pos+4], struct.unpack_from('<I', raw, pos+4)[0]
        if chunk == b'fmt ':
            fmt = struct.unpack_from('<HHIIHH', raw, pos+8)
        elif chunk == b'data':
            data = raw[pos+8:pos+8+size]
        pos += 8+size+(size%2)
    assert fmt and data is not None
    encoding, channels, sr, _, _, bits = fmt
    assert encoding == 3 and bits in (32,64), f'expected float WAV, got {fmt}'
    samples = array.array('f' if bits == 32 else 'd')
    samples.frombytes(data)
    if sys.byteorder != 'little':
        samples.byteswap()
    assert all(math.isfinite(x) for x in samples)
    return sr, channels, samples


def listening_copy(source, target):
    sr, channels, samples = read_float_wav(source)
    peak = max(map(abs, samples), default=0)
    scale = 0.891250938/peak if peak > 1e-12 else 1
    pcm = array.array('h', (round(max(-1,min(1,x*scale))*32767) for x in samples))
    if sys.byteorder != 'little':
        pcm.byteswap()
    with wave.open(str(target), 'wb') as out:
        out.setnchannels(channels)
        out.setsampwidth(2)
        out.setframerate(sr)
        out.writeframes(pcm.tobytes())
    return {'source_peak':peak, 'preview_gain':scale, 'frames':len(samples)//channels,
            'sr':sr, 'channels':channels}
