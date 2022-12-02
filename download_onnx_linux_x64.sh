#!/bin/sh

rm -rf lib
mkdir lib

set -e
cd lib
wget https://github.com/microsoft/onnxruntime/releases/download/v1.13.1/onnxruntime-linux-x64-1.13.1.tgz
tar -xvzf onnxruntime-linux-x64-1.13.1.tgz -C .
mv onnxruntime-linux-x64-1.13.1/* .
rm -r onnxruntime-linux-x64-1.13.1
rm onnxruntime-linux-x64-1.13.1.tgz