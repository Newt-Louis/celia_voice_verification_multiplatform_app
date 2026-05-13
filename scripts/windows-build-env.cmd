@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "FOUND_BUILD_DIR=%CELIA_BUILD_DIR%"
if not defined FOUND_BUILD_DIR set "FOUND_BUILD_DIR=cmake-build-msvc-release"

set "FOUND_VCVARS=%CELIA_VCVARS64%"
if defined FOUND_VCVARS if not exist "!FOUND_VCVARS!" (
    echo [celia-build] CELIA_VCVARS64 points to a missing file: !FOUND_VCVARS!
    exit /b 1
)
if not defined FOUND_VCVARS call :find_vcvars
if not defined FOUND_VCVARS (
    echo [celia-build] Could not find MSVC vcvars64.bat.
    echo [celia-build] Install Visual Studio Build Tools with the Desktop development with C++ workload,
    echo [celia-build] or set CELIA_VCVARS64 to your VC\Auxiliary\Build\vcvars64.bat path.
    exit /b 1
)

call :set_vs_root

set "FOUND_CMAKE=%CELIA_CMAKE_EXE%"
if defined FOUND_CMAKE if not exist "!FOUND_CMAKE!" (
    echo [celia-build] CELIA_CMAKE_EXE points to a missing file: !FOUND_CMAKE!
    exit /b 1
)
if not defined FOUND_CMAKE call :find_cmake
if not defined FOUND_CMAKE (
    echo [celia-build] Could not find cmake.exe.
    echo [celia-build] Install standalone CMake, install the Visual Studio CMake tools component,
    echo [celia-build] add cmake.exe to PATH, or set CELIA_CMAKE_EXE.
    exit /b 1
)

set "FOUND_NINJA=%CELIA_NINJA_EXE%"
if defined FOUND_NINJA if not exist "!FOUND_NINJA!" (
    echo [celia-build] CELIA_NINJA_EXE points to a missing file: !FOUND_NINJA!
    exit /b 1
)
if not defined FOUND_NINJA call :find_ninja

set "FOUND_GENERATOR=%CELIA_CMAKE_GENERATOR%"
set "FOUND_MAKE_PROGRAM=%CELIA_CMAKE_MAKE_PROGRAM%"
if not defined FOUND_GENERATOR (
    if /I "%CELIA_PREFER_NINJA%"=="1" if defined FOUND_NINJA (
        set "FOUND_GENERATOR=Ninja"
        set "FOUND_MAKE_PROGRAM=!FOUND_NINJA!"
    )
)
if not defined FOUND_GENERATOR (
    set "FOUND_GENERATOR=NMake Makefiles"
    set "FOUND_MAKE_PROGRAM="
)

if /I not "!FOUND_GENERATOR!"=="Ninja" set "FOUND_MAKE_PROGRAM="
if /I "!FOUND_GENERATOR!"=="Ninja" if not defined FOUND_MAKE_PROGRAM if defined FOUND_NINJA set "FOUND_MAKE_PROGRAM=!FOUND_NINJA!"
if /I "!FOUND_GENERATOR!"=="Ninja" if not defined FOUND_MAKE_PROGRAM (
    echo [celia-build] Ninja generator was requested, but ninja.exe was not found.
    echo [celia-build] Install Ninja, add it to PATH, or set CELIA_NINJA_EXE.
    exit /b 1
)

endlocal & set "CELIA_BUILD_DIR=%FOUND_BUILD_DIR%" & set "CELIA_VCVARS64=%FOUND_VCVARS%" & set "CELIA_VS_ROOT=%FOUND_VS_ROOT%" & set "CELIA_CMAKE_EXE=%FOUND_CMAKE%" & set "CELIA_NINJA_EXE=%FOUND_NINJA%" & set "CELIA_CMAKE_GENERATOR=%FOUND_GENERATOR%" & set "CELIA_CMAKE_MAKE_PROGRAM=%FOUND_MAKE_PROGRAM%"

call "%CELIA_VCVARS64%" >nul
if errorlevel 1 exit /b %errorlevel%

where.exe cl >nul 2>nul
if errorlevel 1 (
    echo [celia-build] MSVC cl.exe is not available after calling vcvars64.bat.
    exit /b 1
)

if /I "%CELIA_CMAKE_GENERATOR%"=="NMake Makefiles" (
    where.exe nmake >nul 2>nul
    if errorlevel 1 (
        echo [celia-build] nmake.exe is not available. Install the MSVC build tools or install Ninja.
        exit /b 1
    )
)

echo [celia-build] CMake: %CELIA_CMAKE_EXE%
echo [celia-build] Generator: %CELIA_CMAKE_GENERATOR%
if defined CELIA_CMAKE_MAKE_PROGRAM echo [celia-build] Make program: %CELIA_CMAKE_MAKE_PROGRAM%
exit /b 0

:find_vcvars
if defined VSINSTALLDIR if exist "%VSINSTALLDIR%VC\Auxiliary\Build\vcvars64.bat" (
    set "FOUND_VCVARS=%VSINSTALLDIR%VC\Auxiliary\Build\vcvars64.bat"
    exit /b 0
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "!VSWHERE!" (
    for /f "usebackq delims=" %%I in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2^>nul`) do (
        if not defined FOUND_VCVARS if exist "%%I\VC\Auxiliary\Build\vcvars64.bat" set "FOUND_VCVARS=%%I\VC\Auxiliary\Build\vcvars64.bat"
    )
)
if defined FOUND_VCVARS exit /b 0

for %%R in ("%ProgramFiles%\Microsoft Visual Studio" "%ProgramFiles(x86)%\Microsoft Visual Studio") do (
    for %%V in (18 17 16) do (
        for %%E in (BuildTools Community Professional Enterprise Preview) do (
            if not defined FOUND_VCVARS if exist "%%~R\%%V\%%E\VC\Auxiliary\Build\vcvars64.bat" set "FOUND_VCVARS=%%~R\%%V\%%E\VC\Auxiliary\Build\vcvars64.bat"
        )
    )
)
exit /b 0

:set_vs_root
set "FOUND_VS_ROOT="
if defined FOUND_VCVARS (
    for %%I in ("!FOUND_VCVARS!") do (
        for %%R in ("%%~dpI..\..\..") do set "FOUND_VS_ROOT=%%~fR"
    )
)
exit /b 0

:find_cmake
for /f "delims=" %%I in ('where.exe cmake 2^>nul') do (
    if not defined FOUND_CMAKE set "FOUND_CMAKE=%%I"
)
if defined FOUND_CMAKE exit /b 0

if defined FOUND_VS_ROOT if exist "!FOUND_VS_ROOT!\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
    set "FOUND_CMAKE=!FOUND_VS_ROOT!\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    exit /b 0
)
if exist "%ProgramFiles%\CMake\bin\cmake.exe" (
    set "FOUND_CMAKE=%ProgramFiles%\CMake\bin\cmake.exe"
    exit /b 0
)
if exist "%ProgramFiles(x86)%\CMake\bin\cmake.exe" (
    set "FOUND_CMAKE=%ProgramFiles(x86)%\CMake\bin\cmake.exe"
    exit /b 0
)
if exist "D:\Application\JetBrains\CLion 2026.1\bin\cmake\win\x64\bin\cmake.exe" (
    set "FOUND_CMAKE=D:\Application\JetBrains\CLion 2026.1\bin\cmake\win\x64\bin\cmake.exe"
    exit /b 0
)
exit /b 0

:find_ninja
for /f "delims=" %%I in ('where.exe ninja 2^>nul') do (
    if not defined FOUND_NINJA set "FOUND_NINJA=%%I"
)
if defined FOUND_NINJA exit /b 0

if defined FOUND_VS_ROOT if exist "!FOUND_VS_ROOT!\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe" (
    set "FOUND_NINJA=!FOUND_VS_ROOT!\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
    exit /b 0
)
if exist "D:\Application\JetBrains\CLion 2026.1\bin\ninja\win\x64\ninja.exe" (
    set "FOUND_NINJA=D:\Application\JetBrains\CLion 2026.1\bin\ninja\win\x64\ninja.exe"
    exit /b 0
)
exit /b 0
