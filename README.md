# april-asr

[aprilasr](https://github.com/abb128/april-asr) is a minimal library that provides an API for offline streaming speech-to-text applications

[Documentation](https://abb128.github.io/april-asr/concepts.html)

## Status
This library is currently under development. Some features are unimplemented, it may have bugs and crashes, and there may be significant changes to the API. It may not yet be production-ready.

Furthermore, there's only one model that only does English and has some accuracy issues at that.

### Language support
The library has a C API, and there are C# and Python bindings available, but these may not be stable yet.

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
Run:
```
$ mkdir build
$ cd build
$ cmake -DCMAKE_BUILD_TYPE=Release ..
$ make -j4
```

You should now have `main` and `libaprilasr.so`

## Building on Windows (msvc)
Run cmake to configure and generate Visual Studio project files.

Open the `ALL_BUILD.vcxproj` and everything should build. The output will be in the Release or Debug folders.

## Applications
Currently I'm developing [Live Captions](https://github.com/abb128/LiveCaptions), a Linux desktop app that provides live captioning.

## Acknowledgements
Thanks to the [k2-fsa/icefall](https://github.com/k2-fsa/icefall) contributors for creating the speech recognition recipes and models.

Thanks to the developers of [ggml](https://github.com/ggerganov/ggml) and [llama.cpp](https://github.com/ggerganov/llama.cpp), who have created a performant, lightweight and flexible C tensor library.

This project makes use of a few libraries:
* pocketfft, authored by Martin Reinecke, Copyright (C) 2008-2018 Max-Planck-Society, licensed under BSD-3-Clause
* Sonic library, authored by Bill Cox, Copyright (C) 2010 Bill Cox, licensed under Apache 2.0 license
* tinycthread, authored by Marcus Geelnard and Evan Nemerson, licensed under zlib/libpng license


The bindings are based on the [Vosk API bindings](https://github.com/alphacep/vosk-api), which is another speech recognition library based on previous-generation Kaldi. Vosk is Copyright 2019 Alpha Cephei Inc. and licensed under the Apache 2.0 license.