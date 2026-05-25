@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0.."

set APP_NAME=Geai
set BUILD_DIR=build
set DIST_DIR=dist
set PACKAGE_DIR=%DIST_DIR%\%APP_NAME%-windows-x64
set ZIP_FILE=%DIST_DIR%\%APP_NAME%-windows-x64.zip

where cmake >nul 2>nul
if errorlevel 1 (
    echo [ERROR] CMake not found in PATH.
    pause
    exit /b 1
)

cmake -S . -B %BUILD_DIR% -G "Visual Studio 18 2026" -A x64
if errorlevel 1 goto :fail

cmake --build %BUILD_DIR% --config Release
if errorlevel 1 goto :fail

if exist "%PACKAGE_DIR%" rmdir /s /q "%PACKAGE_DIR%"
mkdir "%PACKAGE_DIR%"
copy "%BUILD_DIR%\Release\Geai.exe" "%PACKAGE_DIR%\Geai.exe" >nul
copy "README.md" "%PACKAGE_DIR%\README.md" >nul
copy "LICENSE" "%PACKAGE_DIR%\LICENSE" >nul
xcopy "deno-proxy" "%PACKAGE_DIR%\deno-proxy\" /E /I /Y >nul

if exist "%ZIP_FILE%" del "%ZIP_FILE%"
powershell -NoProfile -ExecutionPolicy Bypass -Command "Compress-Archive -Path '%PACKAGE_DIR%\*' -DestinationPath '%ZIP_FILE%' -Force"
if errorlevel 1 goto :fail

echo [OK] Release package created: %cd%\%ZIP_FILE%
pause
exit /b 0

:fail
echo [ERROR] VS 2026 release package failed. If your CMake does not list "Visual Studio 18 2026", use scripts\release-package.bat.
pause
exit /b 1
