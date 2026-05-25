@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0.."

echo [Geai] Configure Debug build...
where cmake >nul 2>nul
if errorlevel 1 (
    echo [ERROR] CMake not found in PATH.
    echo Please install CMake or install Visual Studio C++ CMake tools.
    echo You can try: winget install Kitware.CMake
    pause
    exit /b 1
)

cmake -S . -B build -G "Visual Studio 17 2022" -A x64
if errorlevel 1 goto :fail

echo [Geai] Build Debug...
cmake --build build --config Debug
if errorlevel 1 goto :fail

echo [Geai] Run Debug executable...
"%cd%\build\Debug\Geai.exe"
exit /b 0

:fail
echo [ERROR] Build failed.
pause
exit /b 1
