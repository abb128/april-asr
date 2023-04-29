#!/bin/bash
mkdir -p build
cp ../../build/Release/libaprilasr.so build/Release/libaprilasr.so
cp ../../lib/lib/onnxruntime.so build/Release/onnxruntime.so
npm i
npm run compile