@echo off

mkdir ..\..\build
pushd ..\..\build
call "C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\vcvarsall.bat" x64
cl -Zi "..\Adventure Time\code\win32_adventure_time.cpp" user32.lib gdi32.lib
popd