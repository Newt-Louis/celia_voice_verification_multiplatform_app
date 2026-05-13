@echo off
setlocal

set "BUILD_LABEL=[WINDOWS]_TEST_V1.0.0"
if not "%~1"=="" set "BUILD_LABEL=%~1"
for %%I in ("%~dp0..") do set "REPO_ROOT=%%~fI"

call "%~dp0windows-build-env.cmd"
if errorlevel 1 exit /b %errorlevel%

if defined CELIA_CMAKE_MAKE_PROGRAM (
  "%CELIA_CMAKE_EXE%" ^
    -S "%REPO_ROOT%" ^
    -B "%REPO_ROOT%\%CELIA_BUILD_DIR%" ^
    -G "%CELIA_CMAKE_GENERATOR%" ^
    -DCMAKE_BUILD_TYPE=Release ^
    "-DCMAKE_MAKE_PROGRAM=%CELIA_CMAKE_MAKE_PROGRAM%" ^
    -DCELIA_PACKAGE_LABEL="%BUILD_LABEL%"
) else (
  "%CELIA_CMAKE_EXE%" ^
    -S "%REPO_ROOT%" ^
    -B "%REPO_ROOT%\%CELIA_BUILD_DIR%" ^
    -G "%CELIA_CMAKE_GENERATOR%" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCELIA_PACKAGE_LABEL="%BUILD_LABEL%"
)
