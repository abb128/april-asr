import ctypes
from . import _april_c_ffi as _c

class AprilToken:
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
    
    if result_type == 1:
        self.result_final = False
    elif result_type == 2:
        self.result_final = True
    else:
        return
    
    self.tokens = []
    for i in range(num_tokens):
        self.tokens.append(AprilToken(tokens[i]))
    
    self.callback(self.result_final, self.tokens)

class Session:
    def __init__(self, model, callback, asynchronous=False):
        config = _c.AprilConfig()
        config.flags = _c.AprilConfigFlagBits()
        config.flags.value = 1 if not asynchronous else 0 # TODO: Maybe synchronous mode should be default in april api
        config.handler = _c.AprilRecognitionResultHandler(_handle_result)
        config.userdata = id(self)

        self.model = model
        self._handle = _c.ffi.aas_create_session(model._handle, config)
        if self._handle == None:
            raise Exception()

        self.result_final = False
        self.tokens = []

        self.callback = callback

    def feed_pcm16(self, data):
        _c.ffi.aas_feed_pcm16(self._handle, data)
    
    def flush(self):
        _c.ffi.aas_flush(self._handle)

    def __del__(self):
        _c.ffi.aas_free(self._handle)
        self.model = None
        self._handle = None
