@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0.."

echo [Geai] Configure Debug build with Visual Studio 2026 generator...
where cmake >nul 2>nul
if errorlevel 1 (
    echo [ERROR] CMake not found in PATH.
    pause
    exit /b 1
)

cmake -S . -B build -G "Visual Studio 18 2026" -A x64
if errorlevel 1 goto :fail

cmake --build build --config Debug
if errorlevel 1 goto :fail

"%cd%\build\Debug\Geai.exe"
exit /b 0

:fail
echo [ERROR] VS 2026 build failed. If your CMake does not list "Visual Studio 18 2026", use scripts\dev-run.bat or open the folder directly in Visual Studio.
pause
exit /b 1
