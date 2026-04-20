@echo off
setlocal

set "BUILD_LABEL=[WINDOWS]_TEST_V1.0.0"
if not "%~1"=="" set "BUILD_LABEL=%~1"

call "%~dp0configure-cpp-windows.cmd" "%BUILD_LABEL%"
if errorlevel 1 exit /b %errorlevel%

call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b %errorlevel%

"D:\Application\JetBrains\CLion 2026.1\bin\cmake\win\x64\bin\cmake.exe" --build cmake-build-msvc-release --target package_app
if errorlevel 1 exit /b %errorlevel%

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0make-windows-installer.ps1" -PackageDir "%CD%\builds\%BUILD_LABEL%"
