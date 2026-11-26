@echo off
rem ============================================================
rem  Build DEEP SPACE OUTPOST on Windows
rem  Usage: double-click, or run  build.bat  in a terminal.
rem  Order: MSVC (Visual Studio) -> gcc in PATH -> Dev-C++ MinGW
rem ============================================================
setlocal
cd /d "%~dp0"

rem remove stale exe so a failed build never leaves an old binary behind
if exist game.exe del game.exe

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSDIR="
if exist "%VSWHERE%" (
  for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
)

if defined VSDIR if exist "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" (
  echo [1/3] Trying MSVC in "%VSDIR%"
  call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
  cl /nologo /utf-8 /std:c11 /O2 /W4 game.c /Fe:game.exe
  if not errorlevel 1 goto ok
  echo [..]  MSVC failed, trying the next compiler
)

where gcc >nul 2>&1
if not errorlevel 1 (
  echo [2/3] Compiler: gcc (from PATH)
  gcc -std=c11 -O2 -Wall -Wextra -o game.exe game.c
  if not errorlevel 1 goto ok
  echo [..]  gcc failed, trying the next compiler
)

set "DEVGCC=%ProgramFiles(x86)%\Dev-Cpp\MinGW64\bin\gcc.exe"
if exist "%DEVGCC%" (
  echo [3/3] Compiler: Dev-C++ MinGW
  "%DEVGCC%" -std=c11 -O2 -Wall -Wextra -o game.exe game.c
  if not errorlevel 1 goto ok
)

echo [X] Build FAILED - no working compiler.
echo     Need either:
echo       - Visual Studio with "Desktop development with C++"
echo         ^(that workload includes the Windows SDK headers^), or
echo       - MinGW-w64 / Dev-C++ with gcc on PATH.
exit /b 1

:ok
echo [OK] Built game.exe
echo.
echo   Play     : game.exe
echo   Self test: game.exe --selftest 30000
echo   Snapshot : game.exe --snapshot
echo   Jet lab  : game.exe --jetlab
echo.
exit /b 0
