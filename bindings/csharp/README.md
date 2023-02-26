# AprilAsr for C#

## Building NuPkg on Windows

First, make sure you have built release libaprilasr.dll.

Open Developer PowerShell for VS 2022 and cd into the nuget directory. Run `.\build.bat`. It will build AprilAsr.x.x.x.x.nupkg.


## Building NuPkg on Linux

Make sure you've built libaprilasr.so.

If you've done a build with onnxruntime statically linked to libaprilasr.so, run `./build.sh -s`. Otherwise, run `./build.sh`


## Example

There is an example on running this in AprilAsrDemo. To run it, make sure you've built the nupkg, then cd into AprilAsrDemo and run `dotnet run`.