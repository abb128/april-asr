cp ..\..\..\build\Release\libaprilasr.dll .\build\lib\win-x64\
csc /t:library /out:lib/netstandard2.0/AprilAsr.dll src/*.cs 
nuget pack
