mkdir build
copy ..\..\build\Release\libaprilasr.dll build\Release\libaprilasr.dll
copy ..\..\lib\lib\onnxruntime.dll build\Release\onnxruntime.dll
npm i
npm run compile