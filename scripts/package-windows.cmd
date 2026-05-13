@echo off
setlocal

set "BUILD_LABEL=[WINDOWS]_TEST_V1.1.0"
if not "%~1"=="" set "BUILD_LABEL=%~1"

call "%~dp0configure-cpp-windows.cmd" "%BUILD_LABEL%"
if errorlevel 1 exit /b %errorlevel%

for %%I in ("%~dp0..") do set "REPO_ROOT=%%~fI"

call "%~dp0windows-build-env.cmd"
if errorlevel 1 exit /b %errorlevel%

"%CELIA_CMAKE_EXE%" --build "%REPO_ROOT%\%CELIA_BUILD_DIR%" --target package_app
if errorlevel 1 exit /b %errorlevel%

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0make-windows-installer.ps1" -PackageDir "%REPO_ROOT%\builds\%BUILD_LABEL%"
