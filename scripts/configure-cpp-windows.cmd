@echo off
setlocal

set "BUILD_LABEL=[WINDOWS]_TEST_V1.0.0"
if not "%~1"=="" set "BUILD_LABEL=%~1"

call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b %errorlevel%

"D:\Application\JetBrains\CLion 2026.1\bin\cmake\win\x64\bin\cmake.exe" ^
  -S . ^
  -B cmake-build-msvc-release ^
  -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_MAKE_PROGRAM="D:\Application\JetBrains\CLion 2026.1\bin\ninja\win\x64\ninja.exe" ^
  -DCELIA_PACKAGE_LABEL="%BUILD_LABEL%"
