#!/usr/bin/env python3
"""Python binding regressions. Set PYTHONPATH and the native library search path.

Usage: LD_LIBRARY_PATH=build PYTHONPATH=bindings/python python tests/test_python_binding.py MODEL.april
"""
import gc
import os
import sys
os.environ['ORT_DISABLE_TELEMETRY'] = '1'
import april_asr

errors = []
sys.unraisablehook = lambda value: errors.append(str(value.exc_value))
try:
    april_asr.Model(None)
except AttributeError:
    pass
model = april_asr.Model(sys.argv[1])
try:
    april_asr.Session(model, None)
except TypeError:
    pass
else:
    raise AssertionError('Non-callable callback accepted')
session = april_asr.Session(model, lambda *_: None, asynchronous=True, no_rt=True)
for data in (b'\x00', b'\x00\x00\x00'):
    try:
        session.feed_pcm16(data)
    except ValueError:
        pass
    else:
        raise AssertionError('Incomplete PCM16 sample accepted')
session.feed_pcm16(b'\x00\x00' * 100)
session.flush()
session.wait()
assert session.get_backlog_ms() == 0
assert april_asr.Session(model, lambda *_: None).get_backlog_ms() == 0
del session, model
gc.collect()
assert not errors, errors
print('PASS: Python invalid PCM/callback, partial initialization, async flush/wait and backlog')
