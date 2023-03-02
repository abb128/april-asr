"""
Public interface for april_asr
"""

from typing import Callable, List
import ctypes
import struct
from enum import IntEnum
from . import _april_c_ffi as _c

class Result(IntEnum):
    """
    Result type that is passed to your handler
    """

    PARTIAL_RECOGNITION = 1,
    """A partial recognition. The next handler call will contain an updated
    list of tokens."""

    FINAL_RECOGNITION = 2,
    """A final recognition. The next handler call will start from an empty
    token list."""

    ERROR_CANT_KEEP_UP = 3,
    """In an asynchronous session, this may be called when the system can't
    keep up with the incoming audio, and samples have been dropped. The
    accuracy will be affected. An empty token list is given"""

    SILENCE = 4
    """Called after some silence. An empty token list is given"""

class Token:
    """
    A token may be a single letter, a word chunk, an entire word, punctuation,
    or other arbitrary set of characters.

    To convert a token array to a string, simply concatenate the strings from
    each token. You don't need to add spaces between tokens, the tokens
    contain their own formatting.

    Tokens also contain the log probability, and a boolean denoting whether or
    not it's a word boundary. In English, the word boundary value is equivalent
    to checking if the first character is a space.
    """

    token: str = ""
    logprob: float = 0.0
    word_boundary: bool = False

    def __init__(self, token):
        self.token = token.token.decode("utf-8")
        self.logprob = token.logprob
        self.word_boundary = (token.flags.value & 1) != 0

class Model:
    """
    Models end with the file extension `.april`. You need to pass a path to
    such a file to construct a Model type.

    Each model has its own sample rate in which it expects audio. There is a
    method to get the expected sample rate. Usually, this is 16000 Hz.

    Models also have additional metadata such as name, description, language.

    After loading a model, you can create one or more sessions that use the
    model.
    """
    def __init__(self, path: str):
        self._handle = _c.ffi.aam_create_model(path)

        if self._handle is None:
            raise Exception("Failed to load model")

    def get_name(self) -> str:
        """Get the name from the model's metadata"""
        return _c.ffi.aam_get_name(self._handle)

    def get_description(self) -> str:
        """Get the description from the model's metadata"""
        return _c.ffi.aam_get_description(self._handle)

    def get_language(self) -> str:
        """Get the language from the model's metadata"""
        return _c.ffi.aam_get_language(self._handle)

    def get_sample_rate(self) -> int:
        """Get the sample rate from the model's metadata"""
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
    """
    The session is what performs the actual speech recognition. It has
    methods to input audio, and it calls your given handler with decoded
    results.

    You need to pass a Model when constructing a Session.
    """
    def __init__(self,
            model: Model,
            callback: Callable[[Result, List[Token]], None],
            asynchronous: bool = False,
            no_rt: bool = False,
            speaker_name: str = ""
        ):
        config = _c.AprilConfig()
        config.flags = _c.AprilConfigFlagBits()

        if asynchronous and no_rt:
            config.flags.value = 2
        elif asynchronous:
            config.flags.value = 1
        else:
            config.flags.value = 0

        if speaker_name != "":
            spkr_data = struct.pack("@q", hash(speaker_name)) * 2
            config.speaker = _c.AprilSpeakerID.from_buffer_copy(spkr_data)

        config.handler = _HANDLER
        config.userdata = id(self)

        self.model = model
        self._handle = _c.ffi.aas_create_session(model._handle, config)
        if self._handle is None:
            raise Exception()

        self.callback = callback

    def get_rt_speedup(self) -> float:
        """
        If the session is asynchronous and realtime, this will return a
        positive float. A value below 1.0 means the session is keeping up, and
        a value greater than 1.0 means the input audio is being sped up by that
        factor in order to keep up. When the value is greater 1.0, the accuracy
        is likely to be affected.
        """
        return _c.ffi.aas_realtime_get_speedup(self._handle)

    def feed_pcm16(self, data: bytes) -> None:
        """
        Feed the given pcm16 samples in bytes to the session. If the session is
        asynchronous, this will return immediately and queue the data for the
        background thread to process. If the session is not asynchronous, this
        will block your thread and potentially call the handler before
        returning.
        """
        _c.ffi.aas_feed_pcm16(self._handle, data)

    def flush(self) -> None:
        """
        Flush any remaining samples and force the session to produce a final
        result.
        """
        _c.ffi.aas_flush(self._handle)

    def __del__(self):
        _c.ffi.aas_free(self._handle)
        self.model = None
        self._handle = None
