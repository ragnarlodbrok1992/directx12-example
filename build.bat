@echo off

set BUILD_DIR="build"
set COMPILER="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.42.34433\bin\Hostx64\x64\cl.exe"

echo Checking for build directory...

IF NOT EXIST %BUILD_DIR% (
  echo Creating build directory...
  mkdir "%BUILD_DIR%"
)

pushd %BUILD_DIR%

echo Entering build directory..

%COMPILER% ../src/main.c^
  user32.lib gdi32.lib d3d12.lib dxgi.lib d3dcompiler.lib^
  /link /subsystem:windows /out:HelloTriangle.exe

popd

