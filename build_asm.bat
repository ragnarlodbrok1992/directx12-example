@echo off



set BUILD_DIR="build_asm"
set COMPILER="D:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.42.34433\bin\Hostx64\x64\cl.exe"
set ASSEMBLER="D:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.42.34433\bin\Hostx64\x64\ml64.exe"
set LINKER="D:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.42.34433\bin\Hostx64\x64\link.exe"

echo Checking for build directory...

IF NOT EXIST %BUILD_DIR% (
  echo Creating build directory...
  mkdir %BUILD_DIR%
)

pushd %BUILD_DIR%

echo Entering build directory...

echo Assembling...

%ASSEMBLER% /c /Foadd.obj ../src/add.asm

echo Compiling...

%COMPILER% /c /Fomain.obj ../src/main_asm.c

echo Linking...

%LINKER% main.obj add.obj /out:program.exe

popd
