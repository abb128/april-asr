Requires ONNXRuntime v1.13.1

put onnx v1.13.1 in lib


To compile:
```
$ mkdir build
$ cd build
$ cmake -DCMAKE_BUILD_TYPE=Release ..
$ make
```

To run example test:
```
$ export LD_LIBRARY_PATH=`pwd`/../lib
$ ./main ../example.wav
```


If you get this error:
```
error: expected ‘)’ before ‘__FILE_NAME__’
   33 | #define LOCATION __FILE_NAME__ ":" S2(__LINE__)
      |                  ^~~~~~~~~~~~~
```
you are running an old version of gcc. Please install gcc 12+ or clang 13+



TODO:
* Better error handling
* fbank snip_edges and kaldifeat difference :(
* Could save hidden states
* Could batch many inputs into one call
* Figure out licensing
* Language model
* VAD
* Tensors are calloc'ed when could be malloc'ed
* linebreak when silence for a while
* longer partial/final rather than single tokens
* could speed/slow audio for performance and accuracy
* raspberry pi support, NCNN?
* __FILE_NAME__ alternative
* findonnxruntime only does linux-x64, need arm64 as well