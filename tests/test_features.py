#!/usr/bin/env python3
"""Independent NumPy reference for the corrected feature geometry and edges.
Requires numpy; run: python tests/test_features.py build/libaprilasr.so
"""
import ctypes as c
import os
import sys
from pathlib import Path
os.environ['ORT_DISABLE_TELEMETRY'] = '1'
import numpy as np
class Options(c.Structure):
    _fields_ = [('sample_freq',c.c_int),('frame_shift_ms',c.c_int),('frame_length_ms',c.c_int),
                ('num_bins',c.c_int),('round_pow2',c.c_bool),('mel_low',c.c_int),('mel_high',c.c_int),
                ('snip_edges',c.c_bool),('corrected_window',c.c_bool),('pull_segment_count',c.c_int),
                ('pull_segment_step',c.c_int),('use_sonic',c.c_bool),('remove_dc_offset',c.c_bool),('preemph_coeff',c.c_float)]
lib = c.CDLL(str(Path(sys.argv[1]).resolve()))
lib.make_fbank.argtypes=[Options]; lib.make_fbank.restype=c.c_void_p
lib.fbank_accept_waveform.argtypes=[c.c_void_p,c.POINTER(c.c_float),c.c_size_t]
lib.fbank_pull_segments.argtypes=[c.c_void_p,c.POINTER(c.c_float),c.c_size_t]; lib.fbank_pull_segments.restype=c.c_bool
lib.fbank_finish.argtypes=[c.c_void_p]; lib.fbank_finish.restype=c.c_bool
lib.free_fbank.argtypes=[c.c_void_p]
def reference(wave, snip):
    low, high = np.float32(1127*np.log1p(20/700)), np.float32(1127*np.log1p(8000/700))
    delta = np.float32((high-low)/np.float32(81))
    mel = np.asarray(1127*np.log1p(np.arange(256)*16000/512/700),dtype=np.float32)
    filters=[]
    for i in range(80):
        left=np.float32(low+np.float32(i)*delta)
        centre=np.float32(left+delta); right=np.float32(centre+delta)
        filters.append(np.where((mel>left)&(mel<right),np.where(mel<=centre,(mel-left)/(centre-left),(right-mel)/(right-centre)),0).astype(np.float32))
    filters=np.asarray(filters)
    window=(0.5-0.5*np.cos(2*np.pi*np.arange(400)/399))**0.85
    frames=[]
    count=max(0,1+(len(wave)-400)//160) if snip else (len(wave)+80)//160
    for index in range(count):
        start=index*160+(0 if snip else -120)
        indices=np.arange(start,start+400)
        while np.any((indices<0)|(indices>=len(wave))):
            indices=np.where(indices<0,-indices-1,indices)
            indices=np.where(indices>=len(wave),2*len(wave)-1-indices,indices)
        frame=wave[indices].astype(np.float64)
        frame-=frame.mean()
        frame[1:]-=np.float32(.97)*frame[:-1].copy()
        frame[0]*=1-np.float32(.97)
        power=np.abs(np.fft.rfft(frame*window,n=512)[:256])**2
        frames.append(np.log(np.maximum(np.finfo(np.float32).eps,filters@power)))
    return np.asarray(frames)
def native(wave,snip):
    o=Options(16000,10,25,80,True,20,0,snip,True,1,1,False,True,.97)
    bank=lib.make_fbank(o); result=[]
    out=np.zeros(80,dtype=np.float32)
    for pos in range(0,len(wave),37):
        packet=wave[pos:pos+37].copy()
        lib.fbank_accept_waveform(bank,packet.ctypes.data_as(c.POINTER(c.c_float)),len(packet))
        while lib.fbank_pull_segments(bank,out.ctypes.data_as(c.POINTER(c.c_float)),out.nbytes): result.append(out.copy())
    while lib.fbank_finish(bank):
        while lib.fbank_pull_segments(bank,out.ctypes.data_as(c.POINTER(c.c_float)),out.nbytes): result.append(out.copy())
    lib.free_fbank(bank)
    return np.asarray(result)
rng=np.random.default_rng(77)
maximum=0
for n in [100,400,801,4800]:
    waveform=rng.uniform(-.5,.5,n).astype(np.float32)
    for snip in [True,False]:
        want=reference(waveform,snip); got=native(waveform,snip)
        assert len(want)==len(got),(n,snip,want.shape,got.shape)
        if len(want):
            error=float(np.max(np.abs(want-got)))
            maximum=max(maximum,error)
            assert error < 0.0002,(n,snip,error)
print(f'Corrected FFT/mel features match NumPy reference; maximum log-energy error {maximum:.8f}')
