#!/usr/bin/env python3
"""Async overload regressions with a synthetic model slower than real time.

Requires onnx. Usage: python tests/test_async_ordering.py build/libaprilasr.so
Checks that overflow is reported while input continues, that flush boundaries
survive when audio is fed before the worker reaches the flush, and that the
reported backlog grows when recognition falls behind.
"""
import ctypes as c
import os
import struct
import sys
import tempfile
import threading
import time
from pathlib import Path
os.environ['ORT_DISABLE_TELEMETRY'] = '1'
import numpy as np
import onnx
from onnx import TensorProto as T, helper as h, numpy_helper

RATE, STRIDE_MS, WIDTH = 16000, 40, 512

def model(path, depth):
    """One emission per utterance: emit ' A' while the context is blank.

    The encoder runs `depth` matrix products on the features so that a frame
    costs a controllable amount of time; its output is multiplied away.
    """
    tensor = lambda name, value: numpy_helper.from_array(np.asarray(value, np.float32), name)
    info = lambda name, dtype, shape: h.make_tensor_value_info(name, dtype, shape)
    nodes = [h.make_node('Reshape', ['x', 'rows'], ['y0']),
             h.make_node('MatMul', ['y0', 'w_in'], ['z0'])]
    for i in range(depth):
        nodes += [h.make_node('MatMul', [f'z{i}', 'w'], [f'm{i}']),
                  h.make_node('Tanh', [f'm{i}'], [f'z{i + 1}'])]
    nodes += [h.make_node('ReduceMean', [f'z{depth}'], ['mean'], keepdims=0),
              h.make_node('Mul', ['mean', 'zero'], ['scalar']),
              h.make_node('Expand', ['scalar', 'eshape'], ['encoder_out']),
              h.make_node('Identity', ['h'], ['next_h']),
              h.make_node('Identity', ['c'], ['next_c'])]
    rng = np.random.default_rng(1)
    encoder = h.make_graph(nodes, 'encoder',
        [info('x', T.FLOAT, [1, 9, 80]), info('h', T.FLOAT, [1, 1, 2]), info('c', T.FLOAT, [1, 1, 2])],
        [info('encoder_out', T.FLOAT, [1, 1, 2]), info('next_h', T.FLOAT, [1, 1, 2]), info('next_c', T.FLOAT, [1, 1, 2])],
        [numpy_helper.from_array(np.array([9, 80], np.int64), 'rows'),
         numpy_helper.from_array(np.array([1, 1, 2], np.int64), 'eshape'),
         tensor('w_in', rng.standard_normal((80, WIDTH)) / 80),
         tensor('w', rng.standard_normal((WIDTH, WIDTH)) / WIDTH ** 0.5), tensor('zero', 0)])
    decoder = h.make_graph([
        h.make_node('Slice', ['context', 'one', 'two', 'one'], ['last']),
        h.make_node('Equal', ['last', 'blank'], ['is_blank']),
        h.make_node('Cast', ['is_blank'], ['flag'], to=T.FLOAT),
        h.make_node('Reshape', ['flag', 'dshape'], ['flat']),
        h.make_node('Expand', ['flat', 'dexpand'], ['decoder_out'])], 'decoder',
        [info('context', T.INT64, [1, 2])], [info('decoder_out', T.FLOAT, [1, 1, 2])],
        [numpy_helper.from_array(np.array([v], np.int64), n) for n, v in (('one', 1), ('two', 2), ('blank', 0))] +
        [numpy_helper.from_array(np.array(v, np.int64), n) for n, v in (('dshape', [1, 1, 1]), ('dexpand', [1, 1, 2]))])
    # logits = [10 - 10d, 10d, -10] + 0 * encoder_out, where d = blank context
    joiner = h.make_graph([
        h.make_node('Slice', ['decoder_out', 'zero_i', 'one_i', 'last_axis'], ['d']),
        h.make_node('Mul', ['d', 'scale'], ['scaled']),
        h.make_node('Add', ['scaled', 'bias'], ['raw']),
        h.make_node('ReduceSum', ['encoder_out'], ['esum'], keepdims=1),
        h.make_node('Mul', ['esum', 'zero'], ['ezero']),
        h.make_node('Add', ['raw', 'ezero'], ['logits'])], 'joiner',
        [info('encoder_out', T.FLOAT, [1, 1, 2]), info('decoder_out', T.FLOAT, [1, 1, 2])],
        [info('logits', T.FLOAT, [1, 1, 3])],
        [numpy_helper.from_array(np.array([v], np.int64), n) for n, v in (('zero_i', 0), ('one_i', 1), ('last_axis', 2))] +
        [tensor('scale', [[[-10, 10, 0]]]), tensor('bias', [[[10, 0, -10]]]), tensor('zero', 0)])
    networks = []
    for graph in (encoder, decoder, joiner):
        m = h.make_model(graph, opset_imports=[h.make_opsetid('', 13)], ir_version=8)
        onnx.checker.check_model(m)
        networks.append(m.SerializeToString())
    params = b'PARAMS\0\0' + struct.pack('<13i', 1, 9, 4, 80, RATE, 10, 25, 1, 20, 0, 1, 3, 0)
    for token in (b'<blk>', b' A', b' B'):
        params += struct.pack('<i', len(token)) + token
    meta = b'en\0\0\0\0\0\0' + struct.pack('<Q', 4) + b'slow' + struct.pack('<Q', 0) + struct.pack('<i', 1)
    start = 20 + len(meta) + 24 + 16 * len(networks)
    header = meta + struct.pack('<QQQ', start + sum(map(len, networks)), len(params), len(networks))
    for network in networks:
        header += struct.pack('<QQ', start, len(network)); start += len(network)
    path.write_bytes(b'APRILMDL' + struct.pack('<IQ', 1, len(header)) + header + b''.join(networks) + params)

class Token(c.Structure):
    _fields_ = [('text', c.c_char_p), ('logprob', c.c_float), ('flags', c.c_int),
                ('time_ms', c.c_size_t), ('reserved', c.c_void_p)]
Callback = c.CFUNCTYPE(None, c.c_void_p, c.c_int, c.c_size_t, c.POINTER(Token))
class Config(c.Structure):
    _fields_ = [('speaker', c.c_uint8 * 16), ('handler', Callback),
                ('userdata', c.c_void_p), ('flags', c.c_int)]
lib = c.CDLL(str(Path(sys.argv[1]).resolve()))
for name, args, result in [
    ('aam_api_init', [c.c_int], None), ('aam_create_model', [c.c_char_p], c.c_void_p),
    ('aas_create_session', [c.c_void_p, Config], c.c_void_p),
    ('aas_feed_pcm16', [c.c_void_p, c.POINTER(c.c_short), c.c_size_t], None),
    ('aas_flush', [c.c_void_p], None), ('aas_wait', [c.c_void_p], c.c_int),
    ('aas_get_backlog_ms', [c.c_void_p], c.c_size_t),
    ('aas_free', [c.c_void_p], None), ('aam_free', [c.c_void_p], None)]:
    fn = getattr(lib, name); fn.argtypes = args; fn.restype = result
lib.aam_api_init(1)

class Session:
    def __init__(self, handle, flags):
        self.events, self.lock = [], threading.Lock()
        @Callback
        def callback(_, kind, count, tokens):
            with self.lock:
                self.events.append((time.monotonic(), kind, [tokens[i].text.decode() for i in range(count)]))
        self.callback = callback
        config = Config(); config.handler = callback; config.flags = flags
        self.handle = lib.aas_create_session(handle, config); assert self.handle
    def feed(self, ms, chunk_ms=20, paced=False):
        samples = (c.c_short * (RATE * chunk_ms // 1000))(*([3000, -3000] * (RATE * chunk_ms // 2000)))
        start = time.monotonic()
        for i in range(ms // chunk_ms):
            lib.aas_feed_pcm16(self.handle, samples, len(samples))
            if paced:
                delay = start + (i + 1) * chunk_ms / 1000 - time.monotonic()
                if delay > 0: time.sleep(delay)
    def kinds(self, kind):
        with self.lock: return [e for e in self.events if e[1] == kind]
    def free(self):
        assert lib.aas_wait(self.handle)
        lib.aas_free(self.handle)

with tempfile.TemporaryDirectory() as directory:
    path = Path(directory) / 'slow.april'
    # Grow the encoder until a sync session runs well below real time.
    depth = 4
    while True:
        model(path, depth)
        handle = lib.aam_create_model(os.fsencode(path)); assert handle
        probe = Session(handle, 0)
        begin = time.monotonic(); probe.feed(1000, chunk_ms=200); speed = 1 / (time.monotonic() - begin)
        probe.free()
        if speed < 0.4: break
        lib.aam_free(handle)
        depth = int(depth * max(2.0, speed / 0.3))
    print(f'synthetic encoder depth {depth}: {speed:.2f}x real time')

    # 1. Under sustained overload the overflow callback must arrive while
    #    audio is still being fed, not after input stops.
    session = Session(handle, 2)
    feeding = 8
    started = time.monotonic()
    backlog = []
    sampler = threading.Thread(target=lambda: [backlog.append(lib.aas_get_backlog_ms(session.handle)) or time.sleep(0.25) for _ in range(feeding * 4 - 2)])
    sampler.start()
    session.feed(feeding * 1000, paced=True)
    stopped = time.monotonic()
    sampler.join()
    overflows = session.kinds(3)
    lib.aas_wait(session.handle)
    assert overflows, 'no overflow reported while overloaded input continued'
    assert overflows[0][0] < stopped, f'first overflow {overflows[0][0] - stopped:.2f}s after input stopped'
    print(f'overflow after {overflows[0][0] - started:.2f}s of {stopped - started:.2f}s input; {len(overflows)} reports')
    assert max(backlog) >= 2000, f'backlog did not grow under overload: {backlog}'
    assert lib.aas_get_backlog_ms(session.handle) == 0
    session.free()

    # 2. Audio fed after aas_flush starts a new utterance even when the worker
    #    has not reached the flush yet. Each utterance emits exactly one ' A',
    #    as long as it is shorter than the 1200 ms silence timeout.
    for utterances in (2, 4):
        session = Session(handle, 2)
        for _ in range(utterances):
            session.feed(600, chunk_ms=100)
            lib.aas_flush(session.handle)
            lib.aas_flush(session.handle)  # repeated flushes stay empty
        assert lib.aas_get_backlog_ms(session.handle) > 0, 'worker was not behind; test cannot observe merging'
        assert lib.aas_wait(session.handle)
        finals = [tokens for _, _, tokens in session.kinds(2)]
        assert finals == [[' A']] * utterances, f'{utterances} flushed utterances produced finals {finals!r}'
        session.free()

    # 3. Sync sessions report no backlog.
    probe = Session(handle, 0)
    probe.feed(1000, chunk_ms=500)
    assert lib.aas_get_backlog_ms(probe.handle) == 0
    probe.free()
    lib.aam_free(handle)
print('PASS: timely overflow, ordered async flush boundaries and backlog reporting')
