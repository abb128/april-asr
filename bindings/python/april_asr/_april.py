import ctypes
import struct
from enum import IntEnum
from . import _april_c_ffi as _c

class Token:
    def __init__(self, token):
        self.token = token.token.decode("utf-8")
        self.logprob = token.logprob
        self.word_boundary = (token.flags.value & 1) != 0

class Model:
    def __init__(self, path):
        self._handle = _c.ffi.aam_create_model(path)
        
        if self._handle == None:
            raise Exception("Failed to load model")
    
    def get_name(self):
        return _c.ffi.aam_get_name(self._handle)

    def get_description(self):
        return _c.ffi.aam_get_description(self._handle)
    
    def get_language(self):
        return _c.ffi.aam_get_language(self._handle)
    
    def get_sample_rate(self):
        return _c.ffi.aam_get_sample_rate(self._handle)
    
    def __del__(self):
        _c.ffi.aam_free(self._handle)
        self._handle = None


def _handle_result(userdata, result_type, num_tokens, tokens):
    self = ctypes.cast(userdata, ctypes.py_object).value
    
    a_tokens = []
    for i in range(num_tokens):
        a_tokens.append(Token(tokens[i]))
    
    self.callback(result_type, a_tokens)

_HANDLER = _c.AprilRecognitionResultHandler(_handle_result)

class Session:
    def __init__(self, model, callback, asynchronous=False, no_rt=False, speaker_name=""):
        config = _c.AprilConfig()
        config.flags = _c.AprilConfigFlagBits()

        if asynchronous and no_rt: config.flags.value = 2
        elif asynchronous: config.flags.value = 1
        else: config.flags.value = 0

        if speaker_name != "":
            spkr_data = struct.pack("@q", hash(speaker_name)) * 2
            config.speaker = _c.AprilSpeakerID.from_buffer_copy(spkr_data)

        config.handler = _HANDLER
        config.userdata = id(self)

        self.model = model
        self._handle = _c.ffi.aas_create_session(model._handle, config)
        if self._handle == None:
            raise Exception()

        self.callback = callback

    def get_rt_speedup(self):
        return _c.ffi.aas_realtime_get_speedup(self._handle)

    def feed_pcm16(self, data):
        _c.ffi.aas_feed_pcm16(self._handle, data)
    
    def flush(self):
        _c.ffi.aas_flush(self._handle)

    def __del__(self):
        _c.ffi.aas_free(self._handle)
        self.model = None
        self._handle = None


class Result(IntEnum):
    Unknown = 0,
    PartialRecognition = 1,
    FinalRecognition = 2,
    ErrorCantKeepUp = 3,
    Silence = 4