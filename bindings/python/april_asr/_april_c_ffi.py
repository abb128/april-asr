"""
Internal module for interfacing with the C API
"""

import ctypes
import sys
import os

class AprilSpeakerID(ctypes.Structure):
    """Equivalent to AprilSpeakerID struct in C header"""
    _fields_ = [("data", ctypes.c_uint8 * 16)]

class AprilTokenFlagBits(ctypes.Structure):
    """Equivalent to AprilTokenFlagBits type in C header"""
    _fields_ = [("value", ctypes.c_uint32)]

class AprilToken(ctypes.Structure):
    """Equivalent to AprilToken struct in C header"""
    _fields_ = [("token", ctypes.c_char_p),
                ("logprob", ctypes.c_float),
                ("flags", AprilTokenFlagBits)]

AprilRecognitionResultHandler = ctypes.CFUNCTYPE(None,
    ctypes.c_void_p, ctypes.c_int, ctypes.c_size_t, ctypes.POINTER(AprilToken))

class AprilConfigFlagBits(ctypes.Structure):
    """Equivalent to AprilConfigFlagBits type in C header"""
    _fields_ = [("value", ctypes.c_uint32)]

class AprilConfig(ctypes.Structure):
    """Equivalent to AprilConfig struct in C header"""
    _fields_ = [("speaker", AprilSpeakerID),
                ("handler", AprilRecognitionResultHandler),
                ("userdata", ctypes.c_void_p),
                ("flags", AprilConfigFlagBits)]

def _init_library_functions(lib):
    lib.aam_api_init.argtypes = []
    lib.aam_api_init.restype = None

    lib.aam_create_model.argtypes = [ctypes.c_char_p]
    lib.aam_create_model.restype = ctypes.c_void_p

    lib.aam_get_name.argtypes = [ctypes.c_void_p]
    lib.aam_get_name.restype = ctypes.c_char_p

    lib.aam_get_description.argtypes = [ctypes.c_void_p]
    lib.aam_get_description.restype = ctypes.c_char_p

    lib.aam_get_language.argtypes = [ctypes.c_void_p]
    lib.aam_get_language.restype = ctypes.c_char_p

    lib.aam_get_sample_rate.argtypes = [ctypes.c_void_p]
    lib.aam_get_sample_rate.restype = ctypes.c_size_t

    lib.aam_free.argtypes = [ctypes.c_void_p]
    lib.aam_free.restype = None

    lib.aas_create_session.argtypes = [ctypes.c_void_p, AprilConfig]
    lib.aas_create_session.restype = ctypes.c_void_p

    lib.aas_feed_pcm16.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_short), ctypes.c_size_t]
    lib.aas_feed_pcm16.restype = None

    lib.aas_flush.argtypes = [ctypes.c_void_p]
    lib.aas_flush.restype = None

    lib.aas_realtime_get_speedup.argtypes = [ctypes.c_void_p]
    lib.aas_realtime_get_speedup.restype = ctypes.c_float

    lib.aas_free.argtypes = [ctypes.c_void_p]
    lib.aas_free.restype = None


class AprilFFI:
    """Provides all of the C functions to interact with the nativel ibrary"""
    def __init__(self, path):
        try:
            self.lib = ctypes.cdll.LoadLibrary(path)
        except OSError:
            self.lib = ctypes.cdll.LoadLibrary(os.path.basename(path))

        _init_library_functions(self.lib)

        self.lib.aam_api_init(1)

        self.aam_get_sample_rate       = self.lib.aam_get_sample_rate
        self.aam_free                  = self.lib.aam_free
        self.aas_create_session        = self.lib.aas_create_session
        self.aas_flush                 = self.lib.aas_flush
        self.aas_realtime_get_speedup  = self.lib.aas_realtime_get_speedup
        self.aas_free                  = self.lib.aas_free

    def aam_create_model(self, path):
        """Equivalent to aam_create_model in the C header"""
        return self.lib.aam_create_model(path.encode("utf-8"))

    def aam_get_name(self, model):
        """Equivalent to aam_get_name in the C header"""
        return self.lib.aam_get_name(model).decode("utf-8")

    def aam_get_description(self, model):
        """Equivalent to aam_get_description in the C header"""
        return self.lib.aam_get_description(model).decode("utf-8")

    def aam_get_language(self, model):
        """Equivalent to aam_get_language in the C header"""
        return self.lib.aam_get_language(model).decode("utf-8")

    def aas_feed_pcm16(self, session, data):
        """Equivalent to aas_feed_pcm16 in the C header"""
        return self.lib.aas_feed_pcm16(session,
            ctypes.cast(data, ctypes.POINTER(ctypes.c_short)), len(data) // 2)


def _load_library():
    if os.environ.get('GH_DOCS_CI_DONT_LOAD_APRIL_ASR') is not None:
        return None
    
    dlldir = os.path.abspath(os.path.dirname(__file__))
    if sys.platform == "win32":
        # We want to load dependencies too
        os.environ["PATH"] = dlldir + os.pathsep + os.environ["PATH"]
        if hasattr(os, "add_dll_directory"):
            os.add_dll_directory(dlldir)
        return AprilFFI(os.path.join(dlldir, "libaprilasr.dll"))
    elif sys.platform == "linux":
        return AprilFFI(os.path.join(dlldir, "libaprilasr.so"))
    elif sys.platform == "darwin":
        return AprilFFI(os.path.join(dlldir, "libaprilasr.dyld"))
    else:
        raise Exception("Unsupported platform")

ffi = _load_library()
