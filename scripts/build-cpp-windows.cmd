@echo off
setlocal

call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b %errorlevel%

"D:\Application\JetBrains\CLion 2026.1\bin\cmake\win\x64\bin\cmake.exe" --build cmake-build-msvc-release --target Voice_Embedded_Verification
