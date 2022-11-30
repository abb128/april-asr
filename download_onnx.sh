#!/bin/sh

mkdir lib

set -e
cd lib
wget https://globalcdn.nuget.org/packages/microsoft.ml.onnxruntime.1.13.1.nupkg
mv microsoft.ml.onnxruntime.1.13.1.nupkg Microsoft.ML.OnnxRuntime.1.13.1.nupkg
unzip Microsoft.ML.OnnxRuntime.1.13.1.nupkg

rm libonnxruntime.pc || :
touch libonnxruntime.pc
echo prefix=`pwd` >> libonnxruntime.pc
echo exec_prefix=\${prefix} >> libonnxruntime.pc
echo includedir=\${prefix}/build/native/include >> libonnxruntime.pc
# TODO x64 only
echo libdir=\${prefix}/runtimes/linux-x64/native >> libonnxruntime.pc
echo Name: onnxruntime >> libonnxruntime.pc
echo Description: The onnxruntime library >> libonnxruntime.pc
echo Version: 0.0.1 >> libonnxruntime.pc
echo Cflags: -I\${includedir} >> libonnxruntime.pc
echo Libs: -L\${libdir} -lonnxruntime >> libonnxruntime.pc


cd ..

mkdir build || :
cd build
rm libaprilasr.pc || :
touch libaprilasr.pc
echo prefix=`pwd` >> libaprilasr.pc
echo exec_prefix=\${prefix} >> libaprilasr.pc
echo includedir=\${prefix}/.. >> libaprilasr.pc
echo libdir=\${prefix} >> libaprilasr.pc
echo Name: aprilasr >> libaprilasr.pc
echo Description: The aprilasr library >> libaprilasr.pc
echo Version: 0.0.1 >> libaprilasr.pc
echo Cflags: -I\${includedir} >> libaprilasr.pc
echo Libs: -L\${libdir} -laprilasr >> libaprilasr.pc
