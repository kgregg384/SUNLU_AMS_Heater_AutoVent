#!/usr/bin/env python3
"""
Test script to verify all dependencies are installed correctly
Run this before running serial_monitor.py
"""

import sys

print("=" * 50)
print("SUNLU Serial Monitor - Dependency Check")
print("=" * 50)
print()

print(f"Python Version: {sys.version}")
print(f"Python Path: {sys.executable}")
print()

# Test imports
failed = []

print("Testing imports...")
print()

try:
    import tkinter
    print("✓ tkinter - OK")
except ImportError as e:
    print("✗ tkinter - FAILED")
    print(f"  Error: {e}")
    failed.append("tkinter")

try:
    import serial
    print(f"✓ pyserial - OK (version {serial.__version__})")
except ImportError as e:
    print("✗ pyserial - FAILED")
    print(f"  Error: {e}")
    failed.append("pyserial")

try:
    import serial.tools.list_ports
    print("✓ serial.tools.list_ports - OK")
except ImportError as e:
    print("✗ serial.tools.list_ports - FAILED")
    print(f"  Error: {e}")
    failed.append("serial.tools.list_ports")

print()
print("=" * 50)

if failed:
    print("FAILED - Missing dependencies:")
    for dep in failed:
        print(f"  - {dep}")
    print()
    print("To fix:")
    if "pyserial" in failed or "serial.tools.list_ports" in failed:
        print("  Run: pip install pyserial")
        print("  Or:  python -m pip install pyserial")
    if "tkinter" in failed:
        print("  tkinter should come with Python.")
        print("  You may need to reinstall Python with tkinter support.")
    print()
    sys.exit(1)
else:
    print("SUCCESS - All dependencies are installed!")
    print("You can now run: python serial_monitor.py")
    print()

input("Press Enter to exit...")
