@echo off

REM A simple build script similar to the one used by Casey

call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" x64
set path=Y:\handmade;%path%;

mkdir build 2> nul
pushd build

cl -Zi -FC -std:c++20 ..\src\win32_handmade.cpp user32.lib gdi32.lib
popd
