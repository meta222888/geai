@echo off
setlocal

cd /d "%~dp0.."

echo [Geai] Cleaning build and dist directories...
if exist build rmdir /s /q build
if exist dist rmdir /s /q dist

echo [OK] Clean complete.
pause
