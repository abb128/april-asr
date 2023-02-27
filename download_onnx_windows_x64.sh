#!/bin/sh

rm -rf lib
mkdir lib

set -e
cd lib
curl -O https://github.com/microsoft/onnxruntime/releases/download/v1.13.1/onnxruntime-win-x64-1.13.1.zip
unzip onnxruntime-win-x64-1.13.1.zip
mv onnxruntime-win-x64-1.13.1/* .
rm -r onnxruntime-win-x64-1.13.1
rm onnxruntime-win-x64-1.13.1.zip
