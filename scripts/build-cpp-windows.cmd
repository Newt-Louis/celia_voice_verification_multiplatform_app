@echo off
setlocal

for %%I in ("%~dp0..") do set "REPO_ROOT=%%~fI"

call "%~dp0windows-build-env.cmd"
if errorlevel 1 exit /b %errorlevel%

"%CELIA_CMAKE_EXE%" --build "%REPO_ROOT%\%CELIA_BUILD_DIR%" --target Voice_Embedded_Verification
