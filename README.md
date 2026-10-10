# april-asr

[aprilasr](https://github.com/abb128/april-asr) is a minimal library that provides an API for offline streaming speech-to-text applications

[Documentation](https://abb128.github.io/april-asr/concepts.html)

## Streaming and accuracy updates

- Worker wakeups, queued-audio finalization and real-time load measurement are
  fixed, and state no longer leaks between flushed utterances.
- Feature extraction uses the model's requested frame geometry; historical
  geometry remains available via `APRIL_CORRECT_FBANK=0`.
- Utterances finalize after 1.2 s of inactivity instead of 2.2 s
  (`APRIL_SILENCE_MS`), and the final flush does less padding work.
- Model loading checks section bounds and ONNX tensor contracts, returning an
  error instead of aborting.
- The encoder uses 2 threads by default (1 when the process may only use one
  CPU), which gives most of the available speedup; override with
  `APRIL_ENCODER_THREADS`;
  `scripts/tune-cpu.py` helps choose a value.

### API changes

The model format is unchanged, but the loader now validates it: a model whose
sections overlap or run past the end of the file, whose token list is malformed,
or whose ONNX networks do not have the expected input and output names, types
and shapes (see `src/april_model.c`) is rejected with an error instead of
crashing. The C API has additions and changed contracts:

- `aas_flush` in an asynchronous session is now ordered with the audio queue.
  Audio fed before the call belongs to the flushed utterance and audio fed
  after it starts the next one, even while the background thread is behind,
  so callers may keep feeding (for example from an audio capture callback)
  without waiting.
- New `aas_wait(session)` blocks until queued asynchronous work and callbacks,
  including the final result of every queued flush, have finished. Call it
  when you need the final result before continuing, such as before reading
  results or tearing down. In an asynchronous session it returns 0 if called
  from the session's callback.
- New `aas_get_backlog_ms(session)` reports how much fed audio an asynchronous
  session has not processed yet. Use it to detect falling behind in
  `APRIL_CONFIG_FLAG_ASYNC_NO_RT_BIT` mode, where `aas_realtime_get_speedup`
  always returns 1.0.
- `APRIL_RESULT_ERROR_CANT_KEEP_UP` is now delivered from the background
  thread, at most once per processing batch while input continues, instead of
  synchronously from `aas_feed_pcm16` for every chunk that is dropped.

The Python (`Session.wait`, `Session.get_backlog_ms`), Java
(`Session.waitIdle`, `Session.getBacklogMs`) and C# (`AprilSession.Wait`,
`AprilSession.GetBacklogMs`) bindings expose the new functions.

### Accuracy

A preliminary comparison on 80 LibriSpeech test-clean and test-other clips
(1,601 words) with an English model measured word error rate going from 13.4%
to 12.6%. That run's clip manifest, model hash and settings have not been
published yet, so treat the figure as unverified until they are.

`tests/compare-asr.py` runs such a comparison against a JSON list of labelled
WAV files. It now records the SHA-256 of the library, model, reference list and
each clip, together with the profile, environment settings and chunking, so a
result file is enough to repeat the run exactly:

```
$ python tests/compare-asr.py old/libaprilasr.so MODEL.april clips.json --profile baseline --output before.json
$ python tests/compare-asr.py build/libaprilasr.so MODEL.april clips.json --profile balanced --output after.json
```

## Status
This library is currently facing some major rewrites over 2025 to improve efficiency and properly fulfill the API contract of multi-session support. The model format is going to change.

## Language support
The core library is written in C, and has a C API. [Python](https://abb128.github.io/april-asr/python.html) and [C#](https://abb128.github.io/april-asr/csharp.html) bindings are available.

## Example in Python

Install via `pip install april-asr`

```py
import april_asr as april
import librosa

# Change these values
model_path = "aprilv0_en-us.april"
audio_path = "audio.wav"

model = april.Model(model_path)


def handler(result_type, tokens):
    s = ""
    for token in tokens:
        s = s + token.token
    
    if result_type == april.Result.FINAL_RECOGNITION:
        print("@"+s)
    elif result_type == april.Result.PARTIAL_RECOGNITION:
        print("-"+s)
    else:
        print("")

session = april.Session(model, handler)

data, sr = librosa.load(audio_path, sr=model.get_sample_rate(), mono=True)
data = (data * 32767).astype("short").astype("<u2").tobytes()

session.feed_pcm16(data)
session.flush()
```

Read the [Python documentation here](https://abb128.github.io/april-asr/python.html).

## Example in C
An example use of this library is provided in `example.cpp`. It can perform speech recognition on a wave file, or do streaming recognition by reading stdin.

It's built as the target `main`. After building aprilasr, you can run it like so:
```
$ ./main /path/to/file.wav /path/to/model.april
```

For streaming recognition, you can pipe parec into it. The command below will live caption your desktop audio.
```
$ parec --format=s16 --rate=16000 --channels=1 --latency-msec=20 --device=@DEFAULT_MONITOR@ | ./main - /path/to/model.april
```

## Models
A few models are available, listed [here](https://abb128.github.io/april-asr/models.html). 

The English models are based on [csukuangfj's trained icefall model](https://huggingface.co/csukuangfj/icefall-asr-librispeech-lstm-transducer-stateless2-2022-09-03/tree/main/exp) as the base, and trained with some extra data.

To export your own models, check out `extra/exporting-howto.md`

## Building on Linux

Building requires ONNXRuntime. You can either try to build it from source or just download the release binaries.

### Downloading ONNXRuntime
Run `./download_onnx_linux_x64.sh` for linux-x64.

For other platforms the script should be very similar, or visit https://github.com/microsoft/onnxruntime/releases/tag/v1.30.0 and download the right zip/tgz file for your platform and extract the contents to a directory named `lib`.

You may also define the env variable `ONNX_ROOT` containing a path to where you extracted the archive, if placing it in `lib` isn't a choice.

### Building ONNXRuntime from source (untested)
You don't need to do this if you've downloaded ONNXRuntime.

Follow the instructions here: https://onnxruntime.ai/docs/how-to/build/inferencing.html#linux

then run
```
cd build/Linux/RelWithDebInfo/
sudo make install
```

### Building aprilasr
Run:
```
$ mkdir build
$ cd build
$ cmake -DCMAKE_BUILD_TYPE=Release ..
$ make -j4
```

You should now have `main`, `libaprilasr.so` and `libaprilasr_static.a`.

If running `main` fails because it can't find `libonnxruntime.so.1`, you may need to make the ONNX Runtime library accessible like so:
```
$ export ORT_DISABLE_TELEMETRY=1
$ export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`pwd`/../lib/lib/
```

## Building on Windows (msvc)
Create a folder called `lib` in the april-asr folder.

Download [onnxruntime-win-x64-1.30.0.zip](https://github.com/microsoft/onnxruntime/releases/download/v1.30.0/onnxruntime-win-x64-1.30.0.zip) and extract the insides of the onnxruntime-win-x64-1.30.0 folder to the `lib` folder

Run cmake to configure and generate Visual Studio project files. Make sure you select x64 as the target if you have downloaded the x64 version of ONNXRuntime.

Open the `ALL_BUILD.vcxproj` and everything should build. The output will be in the Release or Debug folders.

When running main.exe you may receive an error message like this:
> The application was unable to start correctly (0xc000007b)

To fix this, you need to make onnxruntime.dll available. One way to do this is to copy onnxruntime.dll from lib/lib/onnxruntime.dll to build/Debug and build/Release. You may need to distribute the dll together with your application.

## Applications
Currently I'm developing [Live Captions](https://github.com/abb128/LiveCaptions), a Linux desktop app that provides live captioning.

## Acknowledgements
Thanks to the [k2-fsa/icefall](https://github.com/k2-fsa/icefall) contributors for creating the speech recognition recipes and models.

This project makes use of a few libraries:
* pocketfft, authored by Martin Reinecke, Copyright (C) 2008-2018 Max-Planck-Society, licensed under BSD-3-Clause
* Sonic library, authored by Bill Cox, Copyright (C) 2010 Bill Cox, licensed under Apache 2.0 license
* tinycthread, authored by Marcus Geelnard and Evan Nemerson, licensed under zlib/libpng license


The bindings are based on the [Vosk API bindings](https://github.com/alphacep/vosk-api), which is another speech recognition library based on previous-generation Kaldi. Vosk is Copyright 2019 Alpha Cephei Inc. and licensed under the Apache 2.0 license.
