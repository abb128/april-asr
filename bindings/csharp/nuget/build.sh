#!/bin/bash
set -e
mcs -out:lib/netstandard2.0/AprilAsr.dll -target:library src/*.cs
nuget pack