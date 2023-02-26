#!/bin/bash
set -e
cp ../../../build/libaprilasr.so ./build/lib/linux-x64/
mcs -out:lib/netstandard2.0/AprilAsr.dll -target:library src/*.cs
nuget pack