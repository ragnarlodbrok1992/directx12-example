@echo off

set BUILD_DIR="build"
set COMPILER="D:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.42.34433\bin\Hostx64\x64\cl.exe"

set DIRECTX_HEADERS_INCLUDE_DIR="..\external\directx-headers\include"
set DIRECTX_HEADERS_LIB_DIR="..\external\directx-headers\"

echo Checking for build directory...

IF NOT EXIST %BUILD_DIR% (
  echo Creating build directory...
  mkdir "%BUILD_DIR%"
)

pushd %BUILD_DIR%

echo Entering build directory..

%COMPILER% /EHsc /Zi^
  /DEBUG:FULL^
  /INCREMENT:NO^
  ../src/main.cpp^
  /I%DIRECTX_HEADERS_INCLUDE_DIR%^
  user32.lib gdi32.lib d3d12.lib dxgi.lib d3dcompiler.lib^
  /link /subsystem:windows /out:HelloTriangle.exe

popd

