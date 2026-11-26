@echo off
rem ============================================================
rem  Build DEEP SPACE OUTPOST on Windows
rem  Usage: double-click, or run  build.bat  in a terminal.
rem  Tries MSVC first (Visual Studio), then MinGW/gcc.
rem ============================================================
setlocal
cd /d "%~dp0"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSDIR="
if exist "%VSWHERE%" (
  for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
)

if defined VSDIR if exist "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" (
  echo [1/2] Compiler: MSVC in "%VSDIR%"
  call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
  cl /nologo /utf-8 /std:c11 /O2 /W4 game.c /Fe:game.exe
  if errorlevel 1 goto fail
  goto ok
)

where gcc >nul 2>&1
if not errorlevel 1 (
  echo [1/2] Compiler: gcc
  gcc -std=c11 -O2 -Wall -Wextra -o game.exe game.c
  if errorlevel 1 goto fail
  goto ok
)

echo [X] No compiler found. Install Visual Studio (C++ tools) or MinGW-w64.
exit /b 1

:fail
echo [X] Build FAILED.
exit /b 1

:ok
echo [2/2] Built game.exe
echo.
echo   Play     : game.exe
echo   Self test: game.exe --selftest 30000
echo   Snapshot : game.exe --snapshot
echo   Jet lab  : game.exe --jetlab
echo.
exit /b 0
