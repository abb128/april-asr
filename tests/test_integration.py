#!/usr/bin/env python3
"""Real-model regressions: PCM pipe EOF, WAV validation and async flush/wait.
Usage: python tests/test_integration.py build/main MODEL.april recording.wav
The recording must be mono PCM16, and use the model's sample rate.
"""
import ctypes as c
import io
import json
import os
import subprocess
import sys
import threading
import wave
from pathlib import Path
os.environ['ORT_DISABLE_TELEMETRY']='1'
runner, model, audio=map(Path,sys.argv[1:])
def run(source,*flags,data=None):
    return subprocess.run([str(runner.resolve()),str(source),str(model),'--json',*flags],input=data,capture_output=True,timeout=90)
def benchmark(result):
    assert result.returncode==0,result.stderr.decode()
    events=[json.loads(line) for line in result.stdout.decode().splitlines()]
    assert not any(e['type']=='overflow' for e in events)
    return next(e for e in events if e['type']=='benchmark')
with wave.open(str(audio)) as f:
    rate=f.getframerate(); data=f.readframes(f.getnframes())
file_result=benchmark(run(audio))
pipe_result=benchmark(run('-',data=data))
assert file_result['transcript']==pipe_result['transcript']
assert file_result['audio_seconds']==pipe_result['audio_seconds']
async_result=benchmark(run(audio,'--async'))
assert async_result['transcript']==file_result['transcript']
assert async_result['audio_seconds']==file_result['audio_seconds']
# EOF must terminate, not spin forever; odd byte counts must fail explicitly.
assert run('-',data=b'\x00').returncode==3
stream=io.BytesIO()
with wave.open(stream,'wb') as f:
    f.setnchannels(2); f.setsampwidth(2); f.setframerate(rate); f.writeframes(b'\x00'*64)
bad_wav=audio.parent/'invalid-stereo-test.wav'
bad_wav.write_bytes(stream.getvalue())
try: assert run(bad_wav).returncode==2
finally: bad_wav.unlink()
# Overflow callbacks are delivered on the worker; callback wait is rejected.
from ctypes import c_void_p as Ptr
class Token(c.Structure):
    _fields_=[('text',c.c_char_p),('logprob',c.c_float),('flags',c.c_int),('time_ms',c.c_size_t),('reserved',Ptr)]
Callback=c.CFUNCTYPE(None,Ptr,c.c_int,c.c_size_t,c.POINTER(Token))
class Config(c.Structure):
    _fields_=[('speaker',c.c_uint8*16),('handler',Callback),('userdata',Ptr),('flags',c.c_int)]
lib=c.CDLL(str(runner.parent.resolve()/'libaprilasr.so'))
for name,args,result in [('aam_api_init',[c.c_int],None),('aam_create_model',[c.c_char_p],Ptr),('aas_create_session',[Ptr,Config],Ptr),
    ('aas_feed_pcm16',[Ptr,c.POINTER(c.c_short),c.c_size_t],None),('aas_wait',[Ptr],c.c_int),('aas_free',[Ptr],None),('aam_free',[Ptr],None)]:
    fn=getattr(lib,name); fn.argtypes=args; fn.restype=result
lib.aam_api_init(1)
handle=lib.aam_create_model(os.fsencode(model)); assert handle
calls=[]; owner=threading.get_ident()
@Callback
def callback(user,kind,count,tokens):
    calls.append((kind,threading.get_ident(),lib.aas_wait(session)))
config=Config(); config.handler=callback; config.flags=2
session=lib.aas_create_session(handle,config); assert session
samples=(c.c_short*48001)()
lib.aas_feed_pcm16(session,samples,len(samples))
assert lib.aas_wait(session)==1
assert calls and all(kind==3 and thread!=owner and wait==0 for kind,thread,wait in calls),calls
lib.aas_free(session); lib.aam_free(handle)
print('PASS: file/pipe/async transcripts agree; EOF and invalid audio handled; callback thread and wait contract checked.')
