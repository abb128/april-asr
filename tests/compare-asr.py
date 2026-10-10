#!/usr/bin/env python3
"""Compare an April shared library against labelled WAVs with the same chunks.
Run each configuration in its own process: ORT models are immutable after load.
The output records SHA-256 hashes of the library, model, reference list and
every clip, and the exact settings, so that a result can be reproduced.
"""
import argparse
import ctypes as c
import hashlib
import json
import os
import re
import time
import wave
from pathlib import Path
os.environ['ORT_DISABLE_TELEMETRY'] = '1'
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('library', type=Path)
p.add_argument('model', type=Path)
p.add_argument('references', type=Path, help='JSON array of {audio,text}; relative audio paths use this directory')
p.add_argument('--output', type=Path, required=True)
p.add_argument('--profile', choices=['baseline', 'legacy', 'balanced', 'strict'], default='balanced')
p.add_argument('--limit', type=int)
p.add_argument('--threads', type=int, default=1)
a = p.parse_args()
os.environ['APRIL_ENCODER_THREADS'] = str(a.threads)
os.environ['APRIL_CORRECT_FBANK'] = '0' if a.profile in ['baseline', 'legacy'] else '1'
os.environ['APRIL_EARLY_EMIT'] = '0' if a.profile == 'strict' else '1'
os.environ['APRIL_PUNCTUATION_BIAS'] = '0' if a.profile == 'strict' else '3.5'
os.environ['APRIL_MAX_SYMBOLS'] = '6' if a.profile == 'strict' else '3'
os.environ['APRIL_SPECULATIVE'] = '0' if a.profile == 'strict' else '1'
os.environ['APRIL_SILENCE_MS'] = '2200' if a.profile in ['baseline', 'legacy'] else '1200'
SETTINGS = ['APRIL_ENCODER_THREADS', 'APRIL_CORRECT_FBANK', 'APRIL_EARLY_EMIT', 'APRIL_PUNCTUATION_BIAS',
            'APRIL_MAX_SYMBOLS', 'APRIL_SPECULATIVE', 'APRIL_SILENCE_MS']
def sha256(path):
    digest = hashlib.sha256()
    with open(path, 'rb') as f:
        for block in iter(lambda: f.read(1 << 20), b''): digest.update(block)
    return digest.hexdigest()
class Token(c.Structure):
    _fields_ = [('token', c.c_char_p), ('logprob', c.c_float), ('flags', c.c_int), ('time_ms', c.c_size_t), ('reserved', c.c_void_p)]
Callback = c.CFUNCTYPE(None, c.c_void_p, c.c_int, c.c_size_t, c.POINTER(Token))
class Config(c.Structure):
    _fields_ = [('speaker', c.c_uint8 * 16), ('handler', Callback), ('userdata', c.c_void_p), ('flags', c.c_int)]
library = c.CDLL(str(a.library.resolve()))
for name, args, result in [
    ('aam_api_init', [c.c_int], None), ('aam_create_model', [c.c_char_p], c.c_void_p),
    ('aam_get_sample_rate', [c.c_void_p], c.c_size_t), ('aas_create_session', [c.c_void_p, Config], c.c_void_p),
    ('aas_feed_pcm16', [c.c_void_p, c.POINTER(c.c_short), c.c_size_t], None),
    ('aas_flush', [c.c_void_p], None), ('aas_free', [c.c_void_p], None), ('aam_free', [c.c_void_p], None)]:
    fn = getattr(library, name); fn.argtypes = args; fn.restype = result
library.aam_api_init(1)
model = library.aam_create_model(os.fsencode(a.model))
if not model: raise SystemExit('Model load failed')
rate = library.aam_get_sample_rate(model)
def words(s):
    return re.findall(r"[a-z0-9]+(?:'[a-z0-9]+)?", s.lower())
def distance(x, y):
    row = list(range(len(y) + 1))
    for i, w in enumerate(x, 1):
        new = [i]
        for j, v in enumerate(y, 1): new.append(min(new[-1]+1, row[j]+1, row[j-1]+(w != v)))
        row = new
    return row[-1]
rows = json.loads(a.references.read_text())
if a.limit: rows = rows[:a.limit]
output, durations = [], []
for i, row in enumerate(rows):
    audio = Path(row['audio'])
    if not audio.is_absolute(): audio = a.references.parent / audio
    with wave.open(str(audio)) as wav:
        assert wav.getnchannels() == 1 and wav.getsampwidth() == 2 and wav.getframerate() == rate
        data = wav.readframes(wav.getnframes())
    samples = (c.c_short * (len(data)//2)).from_buffer_copy(data)
    finals, first_token, calls, probabilities = [], [], [], []
    @Callback
    def callback(_userdata, kind, count, tokens):
        if kind == 2: finals.append(''.join(tokens[j].token.decode() for j in range(count)))
        if count and not first_token: first_token.append(tokens[0].time_ms)
        if count: probabilities.extend(tokens[j].logprob for j in range(count))
    config = Config(); config.handler = callback
    session = library.aas_create_session(model, config)
    assert session
    start = time.perf_counter()
    for offset in range(0, len(samples), rate//50):
        count = min(rate//50, len(samples)-offset)
        ptr = c.cast(c.byref(samples, offset * 2), c.POINTER(c.c_short))
        tick = time.perf_counter()
        library.aas_feed_pcm16(session, ptr, count)
        calls.append((time.perf_counter()-tick)*1000)
    library.aas_flush(session)
    elapsed = time.perf_counter()-start
    library.aas_free(session)
    text = ''.join(finals)
    reference = words(row['text']); prediction = words(text)
    calls.sort()
    entry = {'audio': row['audio'], 'audio_sha256': sha256(audio), 'reference': row['text'], 'transcript': text,
             'word_errors': distance(reference, prediction), 'words': len(reference),
             'audio_seconds': len(samples)/rate, 'wall_seconds': elapsed,
             'feed_p95_ms': calls[int(.95*(len(calls)-1))],
             'first_text_audio_ms': first_token[0] if first_token else None}
    if a.profile != 'baseline': assert all(prob <= 0.00001 for prob in probabilities)
    output.append(entry)
    if (i+1)%10 == 0: print(f'{a.profile}: {i+1}/{len(rows)} clips', flush=True)
library.aam_free(model)
errors = sum(row['word_errors'] for row in output)
num_words = sum(row['words'] for row in output)
seconds = sum(row['audio_seconds'] for row in output)
summary = {'profile': a.profile, 'threads': a.threads,
           'library_sha256': sha256(a.library), 'model': a.model.name, 'model_sha256': sha256(a.model),
           'references_sha256': sha256(a.references), 'limit': a.limit,
           'settings': {name: os.environ[name] for name in SETTINGS},
           'session': 'synchronous', 'chunk_ms': 20, 'sample_rate': rate,
           'clips': len(output), 'words': num_words,
           'word_errors': errors, 'wer': errors/num_words, 'audio_seconds': seconds,
           'wall_seconds': sum(row['wall_seconds'] for row in output),
           'rtf': sum(row['wall_seconds'] for row in output)/seconds,
           'normalization': "lowercase, ignore punctuation, keep within-word apostrophes; digits not expanded",
           'items': output}
a.output.write_text(json.dumps(summary, indent=2)+'\n')
print(json.dumps({k:v for k,v in summary.items() if k!='items'}), flush=True)
