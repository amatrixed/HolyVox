@echo off
REM Requires: Visual Studio 2022 (or Build Tools) with "Desktop development with C++", CMake and Git in PATH.
cmake -B build -G "Visual Studio 17 2022" -A x64 || goto :err
cmake --build build --config Release --parallel || goto :err
echo.
echo Done: build\HolyVox_artefacts\Release\VST3\HolyVox.vst3
echo Copy the HolyVox.vst3 folder to C:\Program Files\Common Files\VST3\
goto :eof
:err
echo Build failed.
exit /b 1
