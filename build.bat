@echo off

mkdir build 2> nul
pushd build

cl -Zi ..\src\win32_handmade.cpp user32.lib gdi32.lib
popd
