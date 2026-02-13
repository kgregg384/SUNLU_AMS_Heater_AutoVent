@echo off
REM Build script for SUNLU Serial Monitor Windows executable
REM This script installs dependencies and builds a standalone .exe file

echo ========================================
echo SUNLU Serial Monitor Builder
echo ========================================
echo.

REM Check if Python is installed
python --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Python is not installed or not in PATH
    echo Please install Python from https://www.python.org/downloads/
    echo Make sure to check "Add Python to PATH" during installation
    pause
    exit /b 1
)

echo [1/4] Installing dependencies...
python -m pip install --upgrade pip
pip install pyserial pyinstaller
if errorlevel 1 (
    echo ERROR: Failed to install dependencies
    pause
    exit /b 1
)

echo.
echo [2/4] Building executable...
pyinstaller --onefile --windowed --name="SUNLU_SerialMonitor" --icon=NONE serial_monitor.py
if errorlevel 1 (
    echo ERROR: Build failed
    pause
    exit /b 1
)

echo.
echo [3/4] Cleaning up build files...
rmdir /s /q build
del SUNLU_SerialMonitor.spec

echo.
echo [4/4] Build complete!
echo.
echo ========================================
echo SUCCESS!
echo ========================================
echo.
echo Your executable is ready:
echo   Location: dist\SUNLU_SerialMonitor.exe
echo   Size: ~10-15 MB
echo.
echo You can copy SUNLU_SerialMonitor.exe to any Windows PC
echo No Python installation needed!
echo.
pause
