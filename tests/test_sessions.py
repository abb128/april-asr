#!/usr/bin/env python3
"""A reused session must recognize each flushed utterance like a fresh session.

Usage: python tests/test_sessions.py build/libaprilasr.so MODEL.april AUDIO.wav
Also checks empty flushes, partial/final time bounds and the async wait contract.
"""
import ctypes as c
import os
import sys
import wave
from pathlib import Path

os.environ['ORT_DISABLE_TELEMETRY'] = '1'
library, model_path, audio = map(Path, sys.argv[1:])
class Token(c.Structure):
    _fields_ = [('text', c.c_char_p), ('logprob', c.c_float), ('flags', c.c_int),
                ('time_ms', c.c_size_t), ('reserved', c.c_void_p)]
Callback = c.CFUNCTYPE(None, c.c_void_p, c.c_int, c.c_size_t, c.POINTER(Token))
class Config(c.Structure):
    _fields_ = [('speaker', c.c_uint8 * 16), ('handler', Callback),
                ('userdata', c.c_void_p), ('flags', c.c_int)]
lib = c.CDLL(str(library.resolve()))
for name, args, result in [
    ('aam_api_init', [c.c_int], None), ('aam_create_model', [c.c_char_p], c.c_void_p),
    ('aas_create_session', [c.c_void_p, Config], c.c_void_p),
    ('aas_feed_pcm16', [c.c_void_p, c.POINTER(c.c_short), c.c_size_t], None),
    ('aas_flush', [c.c_void_p], None), ('aas_wait', [c.c_void_p], c.c_int),
    ('aas_free', [c.c_void_p], None), ('aam_free', [c.c_void_p], None)]:
    fn = getattr(lib, name); fn.argtypes = args; fn.restype = result
with wave.open(str(audio)) as wav:
    assert wav.getnchannels() == 1 and wav.getsampwidth() == 2
    rate = wav.getframerate(); data = wav.readframes(wav.getnframes())
samples = (c.c_short * (len(data) // 2)).from_buffer_copy(data)
lib.aam_api_init(1)
model = lib.aam_create_model(os.fsencode(model_path)); assert model
failures = []
reference = None
for flags in (0, 2):
    finals, events = [], []
    @Callback
    def callback(_, kind, count, tokens):
        values = [(tokens[i].text.decode(), tokens[i].time_ms) for i in range(count)]
        events.extend(values)
        if kind == 2: finals.extend(values)
    config = Config(); config.handler = callback; config.flags = flags
    session = lib.aas_create_session(model, config); assert session
    for repetition in range(3):
        finals.clear(); events.clear()
        # Repeated empty flushes must not affect the next utterance.
        for _ in range(2):
            lib.aas_flush(session); assert lib.aas_wait(session)
        if events:
            failures.append(f'flags={flags} utterance={repetition}: empty flush emitted {events!r}')
        finals.clear(); events.clear()
        for offset in range(0, len(samples), 3200):
            count = min(3200, len(samples) - offset)
            lib.aas_feed_pcm16(session, c.cast(c.byref(samples, offset * 2), c.POINTER(c.c_short)), count)
            assert lib.aas_wait(session)
        lib.aas_flush(session); assert lib.aas_wait(session)
        start = repetition * len(samples) * 1000 // rate
        end = (repetition + 1) * len(samples) * 1000 // rate
        relative = [(text, time - start) for text, time in finals]
        if reference is None: reference = relative
        if relative != reference:
            failures.append(f'flags={flags} utterance={repetition}: expected {reference!r}; got {relative!r}')
        if not all(start <= time <= end for _, time in events):
            failures.append(f'flags={flags} utterance={repetition}: token outside source audio [{start}, {end}]')
    lib.aas_free(session)
lib.aam_free(model)
assert not failures, '\n'.join(failures)
print('PASS: sync/async session reuse, empty flushes and cumulative source timestamps')
