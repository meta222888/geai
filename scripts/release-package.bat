@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0.."

set APP_NAME=Geai
set BUILD_DIR=build
set DIST_DIR=dist
set PACKAGE_DIR=%DIST_DIR%\%APP_NAME%-windows-x64
set ZIP_FILE=%DIST_DIR%\%APP_NAME%-windows-x64.zip

echo [Geai] Check CMake...
where cmake >nul 2>nul
if errorlevel 1 (
    echo [ERROR] CMake not found in PATH.
    echo Please install CMake or install Visual Studio C++ CMake tools.
    echo You can try: winget install Kitware.CMake
    pause
    exit /b 1
)

echo [Geai] Configure Release build...
cmake -S . -B %BUILD_DIR% -G "Visual Studio 17 2022" -A x64
if errorlevel 1 goto :fail

echo [Geai] Build Release...
cmake --build %BUILD_DIR% --config Release
if errorlevel 1 goto :fail

echo [Geai] Prepare package directory...
if exist "%PACKAGE_DIR%" rmdir /s /q "%PACKAGE_DIR%"
mkdir "%PACKAGE_DIR%"

copy "%BUILD_DIR%\Release\Geai.exe" "%PACKAGE_DIR%\Geai.exe" >nul
copy "README.md" "%PACKAGE_DIR%\README.md" >nul
copy "LICENSE" "%PACKAGE_DIR%\LICENSE" >nul
xcopy "deno-proxy" "%PACKAGE_DIR%\deno-proxy\" /E /I /Y >nul

echo [Geai] Create zip package...
if exist "%ZIP_FILE%" del "%ZIP_FILE%"
powershell -NoProfile -ExecutionPolicy Bypass -Command "Compress-Archive -Path '%PACKAGE_DIR%\*' -DestinationPath '%ZIP_FILE%' -Force"
if errorlevel 1 goto :fail

echo.
echo [OK] Release package created:
echo %cd%\%ZIP_FILE%
pause
exit /b 0

:fail
echo [ERROR] Release package failed.
pause
exit /b 1
