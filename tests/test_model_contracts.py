#!/usr/bin/env python3
"""Generate tiny ONNX models to test loading errors without large fixtures.

Requires onnx. Usage: python tests/test_model_contracts.py build/libaprilasr.so
"""
import ctypes as c
import os
import struct
import sys
import tempfile
from pathlib import Path
os.environ['ORT_DISABLE_TELEMETRY'] = '1'
import onnx
from onnx import TensorProto as T, helper as h

lib = c.CDLL(str(Path(sys.argv[1]).resolve()))
lib.aam_api_init.argtypes = [c.c_int]
lib.aam_create_model.argtypes = [c.c_char_p]; lib.aam_create_model.restype = c.c_void_p
lib.aam_free.argtypes = [c.c_void_p]
lib.aam_api_init(1)

def graph(inputs, outputs):
    nodes = []
    for name, dtype, shape in outputs:
        size = 1
        for d in shape: size *= d
        tensor = h.make_tensor(name + '_value', dtype, shape, [0] * size)
        nodes.append(h.make_node('Constant', [], [name], value=tensor))
    infos = lambda specs: [h.make_tensor_value_info(*spec) for spec in specs]
    model = h.make_model(h.make_graph(nodes, 'test', infos(inputs), infos(outputs)),
                         opset_imports=[h.make_opsetid('', 13)], ir_version=8)
    onnx.checker.check_model(model)
    return model.SerializeToString()

def container(networks):
    params = b'PARAMS\0\0' + struct.pack('<13i', 1, 9, 4, 80, 16000, 10, 25, 1, 20, 0, 1, 3, 0)
    for token in (b'<blk>', b' A', b' B'):
        params += struct.pack('<i', len(token)) + token
    meta = b'en\0\0\0\0\0\0' + struct.pack('<Q', 4) + b'test' + struct.pack('<Q', 0) + struct.pack('<i', 1)
    start = 20 + len(meta) + 24 + 16 * len(networks)
    header = meta + struct.pack('<QQQ', start + sum(map(len, networks)), len(params), len(networks))
    for network in networks:
        header += struct.pack('<QQ', start, len(network)); start += len(network)
    return b'APRILMDL' + struct.pack('<IQ', 1, len(header)) + header + b''.join(networks) + params

def networks(change=None):
    specs = [
        [[['x', T.FLOAT, [1, 9, 80]], ['h', T.FLOAT, [1, 1, 2]], ['c', T.FLOAT, [1, 1, 2]]],
         [['encoder_out', T.FLOAT, [1, 1, 2]], ['next_h', T.FLOAT, [1, 1, 2]], ['next_c', T.FLOAT, [1, 1, 2]]]],
        [[['context', T.INT64, [1, 2]]], [['decoder_out', T.FLOAT, [1, 1, 2]]]],
        [[['encoder_out', T.FLOAT, [1, 1, 2]], ['decoder_out', T.FLOAT, [1, 1, 2]]],
         [['logits', T.FLOAT, [1, 1, 3]]]]]
    if change: change(specs)
    return [graph(*spec) for spec in specs]

cases = {
    'wrong input name': lambda s: s[0][0][0].__setitem__(0, 'audio'),
    'wrong input type': lambda s: s[0][0][0].__setitem__(1, T.DOUBLE),
    'wrong input rank': lambda s: s[0][0][0].__setitem__(2, [9, 80]),
    'dynamic context': lambda s: s[1][0][0].__setitem__(2, [1, 'context_size']),
    'float context': lambda s: s[1][0][0].__setitem__(1, T.FLOAT),
    'state mismatch': lambda s: s[0][1][1].__setitem__(2, [1, 1, 3]),
    'joiner mismatch': lambda s: s[2][0][0].__setitem__(2, [1, 1, 4]),
    'wrong output type': lambda s: s[2][1][0].__setitem__(1, T.DOUBLE),
    'wrong vocabulary': lambda s: s[2][1][0].__setitem__(2, [1, 1, 4]),
}
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory) / 'test.april'
    path.write_bytes(container(networks()))
    model = lib.aam_create_model(os.fsencode(path)); assert model, 'valid control rejected'
    lib.aam_free(model)
    for name, change in cases.items():
        path.write_bytes(container(networks(change)))
        model = lib.aam_create_model(os.fsencode(path))
        if model: lib.aam_free(model)
        assert not model, name
    bad = networks(); bad[0] = b'not an ONNX model'
    path.write_bytes(container(bad))
    assert not lib.aam_create_model(os.fsencode(path))
print(f'PASS: valid model control and {len(cases) + 1} invalid ONNX contracts')
