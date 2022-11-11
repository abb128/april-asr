#!/bin/sh

mkdir lib

set -e
cd lib
wget https://globalcdn.nuget.org/packages/microsoft.ml.onnxruntime.1.13.1.nupkg
mv microsoft.ml.onnxruntime.1.13.1.nupkg Microsoft.ML.OnnxRuntime.1.13.1.nupkg
unzip Microsoft.ML.OnnxRuntime.1.13.1.nupkg