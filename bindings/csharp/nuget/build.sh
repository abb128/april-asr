#!/bin/bash
set -e

while getopts "s" opt; do
  case $opt in
    s)
      static=true
      ;;
    \?)
      echo "Invalid option: -$OPTARG" >&2
      exit 1
      ;;
  esac
done


rm ./build/lib/linux-x64/*.so
if [ "$static" = true ]; then
  cp ../../../build/libaprilasr.so ./build/lib/linux-x64/
else
  cp ../../../build/libaprilasr.so ./build/lib/linux-x64/
  cp ../../../lib/lib/*.so ./build/lib/linux-x64/
fi
mcs -out:lib/netstandard2.0/AprilAsr.dll -target:library src/*.cs
nuget pack