@echo off
setlocal

call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b %errorlevel%

"D:\Application\JetBrains\CLion 2026.1\bin\cmake\win\x64\bin\cmake.exe" ^
  -S . ^
  -B cmake-build-msvc-debug ^
  -G Ninja ^
  -DCMAKE_BUILD_TYPE=Debug ^
  -DCMAKE_MAKE_PROGRAM="D:\Application\JetBrains\CLion 2026.1\bin\ninja\win\x64\ninja.exe"
