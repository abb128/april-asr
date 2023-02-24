#!/bin/bash
set -e
mcs -out:lib/netstandard2.0/aprilasr.dll -target:library src/*.cs
nuget pack