# Python

## Installation

TODO: Publish on PyPi


## Getting Started

To get started, import AprilASR

```py
import april_asr as april
```


## Model

You can load a model like so:

```py
your_model_path = "/path/to/model.april"
model = april.Model(your_model_path)
```

Models have a few metadata methods:
```py
name: str = model.get_name()
description: str = model.get_description()
language: str = model.get_language()
sample_rate: int = model.get_sample_rate()
```

## Session

Before creating a session, define a handler callback. Here is an example handler function that concatenates the tokens to a string and prints it:

```py
def handler(result_type, tokens):
    s = ""
    for token in tokens:
        s = s + token.token
    
    if result_type == april.Result.FinalRecognition:
        print("@"+s)
    elif result_type == april.Result.PartialRecognition:
        print("-"+s)
    else:
        print("")
```

Now, a session may be created:

```py
session = april.Session(model, handler)
```

### Session Options

There are more options when it comes to creating a session, here is the initializer signature:
```py
def __init__(self, model, callback, asynchronous=False, no_rt=False, speaker_name=""):
```

Refer to the General Concepts page for an explanation on asynchronous, non-realtime, and speaker name options

## Feed data

Most of the examples use a very simple method like this to load and feed audio:

```py
with open(wav_file_path, "rb") as f:
    data = f.read()

session.feed_pcm16(data)
```

This works only if the wav file is PCM16 and sampled in the correct sample rate. When you attempt to load an mp3, non-PCM16/non-16kHz wav file, or any other audio file in this way, you will likely get gibberish or no results.

To load more arbitrary audio files, you can use a Python library that handles audio loading (make sure librosa is installed: `pip install librosa`):

```py
import librosa

data, sr = librosa.load("/path/to/anything.mp3", sr=model.get_sample_rate(), mono=True)
data = (data * 32767).astype("short").astype("<u2").tobytes()

session.feed_pcm16(data)
```


You can flush the session once the end of the file has been reached to force a final result:
```py
session.flush()
```


## Complete example

```py
import april_asr as april
import librosa

# Change these values
model_path = "aprilv0_en-us.april"
audio_path = "audio.wav"

model = april.Model(model_path)


def handler(result_type: april.Result, tokens: april.Token):
    s = ""
    for token in tokens:
        s = s + token.token
    
    if result_type == april.Result.FinalRecognition:
        print("@"+s)
    elif result_type == april.Result.PartialRecognition:
        print("-"+s)
    else:
        print("")

session = april.Session(model, handler)

data, sr = librosa.load(audio_path, sr=model.get_sample_rate(), mono=True)
data = (data * 32767).astype("short").astype("<u2").tobytes()

session.feed_pcm16(data)
session.flush()

```

Congratulations! You have just performed speech recognition using AprilASR!