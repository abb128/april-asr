# april-asr

aprilasr is a minimal library that provides an API for offline streaming speech-to-text applications

## Status
This library is currently under development. Some features are unimplemented, it may have bugs and crashes, and there may be significant changes to the API. It may not yet be production-ready.

Furthermore, there's only one model that only does English and has some accuracy issues at that.

### Language support
Currently only a C API is available, language bindings for other languages have not yet been developed.

## Example
An example use of this library is provided in `example.cpp`. It can perform speech recognition on a wave file, or do streaming recognition by reading stdin.

It's built as the target `main`. After building aprilasr, you can run it like so:
```
$ ./main /path/to/file.wav /path/to/model.april
```

For streaming recognition, you can pipe parec into it:
```
$ parec --format=s16 --rate=16000 --channels=1 --latency-ms=100 | ./main - /path/to/model.april
```

## Models
Currently only one model is available, the [English model](https://april.sapples.net/aprilv0_en-us.april), based on [csukuangfj's trained icefall model](https://huggingface.co/csukuangfj/icefall-asr-librispeech-lstm-transducer-stateless2-2022-09-03/tree/main/exp) as the base, and trained with some extra data.

To make your own models, check out `extra/exporting-howto.md`

## Building on Linux
Building requires ONNXRuntime v1.13.1. You can either try to build it from source or just download the release binaries.

### Downloading ONNXRuntime
Run `./download_onnx_linux_x64.sh` for linux-x64.

For other platforms the script should be very similar, or visit https://github.com/microsoft/onnxruntime/releases/tag/v1.13.1 and download the right zip/tgz file for your platform and extract the contents to a directory named `lib`.

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

You should now have `main`, `libaprilasr.so` and `libaprilasr_static.so`.

If running `main` fails because it can't find `libonnxruntime.so.1.13.1`, you may need to make `libonnxruntime.so.1.13.1` accessible like so:
```
$ export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`pwd`/../lib/lib/
```

## Building on Windows (msvc)
Create a folder called `lib` in the april-asr folder.

Download [onnxruntime-win-x64-1.13.1.zip](https://github.com/microsoft/onnxruntime/releases/download/v1.13.1/onnxruntime-win-x64-1.13.1.zip) and extract the insides of the onnxruntime-win-x64-1.13.1 folder to the `lib` folder

Run cmake to configure and generate Visual Studio project files. Make sure you select x64 as the target if you have downloaded the x64 version of ONNXRuntime.

Open the `ALL_BUILD.vcxproj` and everything should build. The output will be in the Release or Debug folders.

When running main.exe you may receive an error message like this:
> The application was unable to start correctly (0xc000007b)

To fix this, you need to make onnxruntime.dll available. One way to do this is to copy onnxruntime.dll from lib/lib/onnxruntime.dll to build/Debug and build/Release. You may need to distribute the dll together with your application.

## Applications
Currently I'm developing [Live Captions](https://github.com/abb128/LiveCaptions), a Linux desktop app that provides live captioning.

## Acknowledgements
Thanks to the [k2-fsa/icefall](https://github.com/k2-fsa/icefall) contributors for creating the speech recognition recipes and models.

The bindings are based on the [Vosk API bindings](https://github.com/alphacep/vosk-api), which is another speech recognition library based on previous-generation Kaldi. Vosk is Copyright 2019 Alpha Cephei Inc. and licensed under the Apache 2.0 license.