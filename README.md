# april-asr

aprilasr is a minimal library that provides an API for offline streaming speech-to-text applications

## Language support
Currently only a C API is available.

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

## Building
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

## Applications
Currently I'm developing [Live Captions](https://github.com/abb128/LiveCaptions), a Linux desktop app that provides live captioning.

## Acknowledgements
Thanks to the [k2-fsa/icefall](https://github.com/k2-fsa/icefall) contributors for creating the speech recognition recipes and models.