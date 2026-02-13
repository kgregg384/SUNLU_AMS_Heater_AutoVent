@echo off
REM Dependency installer for SUNLU Serial Monitor
REM Run this FIRST before running serial_monitor.py

echo ========================================
echo SUNLU Serial Monitor - Dependency Installer
echo ========================================
echo.

REM Check if Python is installed
python --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Python is not installed or not in PATH
    echo.
    echo Please install Python from https://www.python.org/downloads/
    echo Make sure to check "Add Python to PATH" during installation
    echo.
    pause
    exit /b 1
)

echo Found Python:
python --version
echo.

REM Try different methods to install pyserial
echo [1/3] Attempting to install pyserial with pip...
pip install pyserial
if errorlevel 1 (
    echo.
    echo [2/3] First method failed, trying python -m pip...
    python -m pip install pyserial
    if errorlevel 1 (
        echo.
        echo [3/3] Second method failed, trying with --user flag...
        python -m pip install --user pyserial
        if errorlevel 1 (
            echo.
            echo ERROR: All installation methods failed
            echo.
            echo Try these steps manually:
            echo   1. Open Command Prompt as Administrator
            echo   2. Run: python -m pip install pyserial
            echo.
            pause
            exit /b 1
        )
    )
)

echo.
echo ========================================
echo SUCCESS!
echo ========================================
echo.
echo pyserial has been installed successfully
echo You can now run: python serial_monitor.py
echo.
pause
