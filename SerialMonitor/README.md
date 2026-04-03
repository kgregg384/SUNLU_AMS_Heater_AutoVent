# SUNLU AMS Heater Serial Monitor

A dedicated Windows serial monitor application for the SUNLU AMS Heater Auto-Vent Controller.

## Features

- 🔍 **Auto-detect** Arduino/SAMD21 boards
- 🔄 **Auto-reconnect** on USB disconnect
- 🎨 **Color-coded output** for easy reading:
  - 🟢 Green: Opening vent, heater on
  - 🔵 Blue: Closing vent, heater off
  - 🟡 Yellow: Debug messages, warnings
  - 🔴 Red: Errors
  - 🔵 Cyan: Calibration messages
- ⌨️ **Send commands to Arduino** with quick buttons or manual input
- 🎓 **Learn Mode** - Start learning mode without pressing button on device
- 🖥️ **Clean, simple interface**
- 📋 **Real-time monitoring** at 115200 baud

## Quick Start (Windows)

### Option 1: Run Python Script (Requires Python)

1. **Install Python** (if not already installed):
   - Download from https://www.python.org/downloads/
   - Check "Add Python to PATH" during installation

2. **Install dependencies**:
   ```cmd
   pip install -r requirements.txt
   ```

3. **Run the monitor**:
   ```cmd
   python serial_monitor.py
   ```

### Option 2: Build Standalone .exe (No Python Required)

1. **Install PyInstaller**:
   ```cmd
   pip install pyinstaller
   ```

2. **Build the .exe**:
   ```cmd
   pyinstaller --onefile --windowed --name="SUNLU_SerialMonitor" serial_monitor.py
   ```

3. **Find the .exe**:
   - Located in `dist/SUNLU_SerialMonitor.exe`
   - Can be copied to any Windows PC
   - No Python installation needed!

4. **Run it**:
   - Double-click `SUNLU_SerialMonitor.exe`
   - Connect your SAMD21 board via USB
   - Monitor auto-detects and connects

## Usage

### Basic Operation

1. **Connect Board**:
   - Plug in SAMD21 via USB
   - App auto-detects and connects
   - Green "Connected" status appears

2. **Monitor Output**:
   - View real-time serial output
   - Color-coded messages for easy reading
   - Auto-scrolls to latest output

3. **Disconnect/Reconnect**:
   - Unplug USB or click "Disconnect"
   - Auto-reconnect when board is plugged back in
   - Manual reconnect with "Connect" button

### Controls

- **Port Dropdown**: Select COM port (auto-selected on startup)
- **Refresh Ports**: Rescan for new USB devices
- **Connect/Disconnect**: Manually control connection
- **Clear**: Clear the output window
- **Auto-reconnect**: Enable/disable automatic reconnection

### Sending Commands

#### Quick Command Buttons

Use the quick command buttons to send common commands without typing:

- **Learn Mode** (L): Start learning mode to calibrate thresholds
  - Initiates 3-phase learning process
  - Completely remote - no physical button needed!

- **Next Phase** (N): Advance to next learning phase
  - Click after each learning stage completes
  - Workflow: `Learn Mode` → wait → `Next Phase` → wait → `Next Phase` → wait → `Next Phase` → Done!
  - Replaces physical button press

- **Open Vent** (O): Manually open the vent

- **Close Vent** (C): Manually close the vent

- **Recalibrate** (R): Recalibrate servo positions

- **Standby** (S): Toggle standby/wake mode

- **Feedback** (F): Read current servo position feedback

#### Manual Input

For advanced commands:

1. Type command in the "Send:" text field
2. Press Enter or click "Send" button
3. Command is sent to Arduino

**Examples:**
- Type `90` to move servo to 90 degrees
- Type `L` to start learning mode
- Type `N` to advance learning phase
- Any single character or number supported by Arduino firmware

### Running Learning Mode Remotely

You can complete the entire learning process from your PC without touching the Arduino:

**Complete Workflow:**

1. **Click "Learn Mode"** button
   - Arduino responds: "ENTERING LEARNING MODE"
   - Shows: "PHASE 1: Ensure dryer is completely OFF"

2. **Turn off dryer completely** (if running)

3. **Click "Next Phase"** when ready
   - Arduino measures baseline for 10 seconds
   - LED flashes rapidly during measurement

4. **Turn dryer ON** (heater + fan)

5. **Click "Next Phase"** when dryer is fully running
   - Arduino measures heater current for 10 seconds
   - LED flashes rapidly during measurement

6. **Turn heater OFF** (fan only remains running)

7. **Click "Next Phase"** when only fan is running
   - Arduino measures fan-only current for 10 seconds
   - LED flashes rapidly during measurement

8. **Done!** Arduino saves thresholds to EEPROM
   - Shows: "LEARNING COMPLETE!"
   - LED blinks 3 times

**Benefits:**
- No physical access to Arduino needed
- See real-time feedback in serial monitor
- Perfect for initial setup and troubleshooting
- Can be done anywhere with USB connection

## Troubleshooting

### "No module named serial" Error

This is the most common error when running for the first time. It means pyserial isn't installed.

**Quick Fix:**
```cmd
cd SerialMonitor
install_dependencies.bat
```

**Or manually:**
```cmd
pip install pyserial
```

**Still not working? Try these:**

1. **Test your setup:**
   ```cmd
   python test_imports.py
   ```
   This will show exactly what's missing

2. **Install with different methods:**
   ```cmd
   python -m pip install pyserial
   ```
   Or:
   ```cmd
   python -m pip install --user pyserial
   ```

3. **Check Python version:**
   ```cmd
   python --version
   ```
   Needs Python 3.7 or higher

4. **Multiple Python installations?**
   - Try `python3` instead of `python`
   - Or `py -3` on Windows
   ```cmd
   py -3 -m pip install pyserial
   py -3 serial_monitor.py
   ```

5. **Run as Administrator:**
   - Right-click Command Prompt
   - "Run as Administrator"
   - Then: `pip install pyserial`

6. **Last resort - Build .exe (no Python needed):**
   ```cmd
   build_windows_exe.bat
   ```
   Then use `dist/SUNLU_SerialMonitor.exe`

### Port Not Detected

- Click "Refresh Ports"
- Check USB cable connection
- Verify driver installation (Windows should auto-install)
- Try different USB port

### Connection Fails

- Board may be in bootloader mode (orange LED pulsing)
- Press reset button once to exit bootloader
- Wait for board to boot up (~3 seconds)
- Click "Connect" again

### Output Stops

- Check "Auto-reconnect" is enabled
- Try clicking "Disconnect" then "Connect"
- Unplug and replug USB cable
- Click "Clear" to reset display

### No Output Appears

- Verify board is running firmware (not in bootloader)
- Check baud rate is 115200 (set in code)
- Press reset button on board to restart
- Board may be in standby mode (long press button to wake)

## Color Coding Reference

| Color | Message Type | Examples |
|-------|-------------|----------|
| 🟢 Green | Vent opening, heater on | ">>> HEATER ON", "OPENING VENT" |
| 🔵 Blue | Vent closing, heater off | ">>> HEATER & FAN OFF", "CLOSING VENT" |
| 🟡 Yellow | Debug info, warnings | "DEBUG: heaterOn=1", "WARNING:" |
| 🔴 Red | Errors | "ERROR:", "Connection failed" |
| 🔵 Cyan | Calibration | "CALIBRATION", "Learning Mode" |
| ⚪ White | General status | Normal output lines |

## Technical Details

- **Baud Rate**: 115200
- **Data Bits**: 8
- **Parity**: None
- **Stop Bits**: 1
- **Auto-detect**: Filters for VID 0x2341 (Arduino) and 0x2886 (Seeed)
- **Reconnect Delay**: 2 seconds
- **Read Timeout**: 100ms

## Requirements

- **Python**: 3.7 or higher
- **Libraries**: pyserial (see requirements.txt)
- **OS**: Windows 7/8/10/11 (also works on Mac/Linux)

## Building from Source

```cmd
# Clone the repository
git clone https://github.com/kgregg384/SUNLU_AMS_Heater_AutoVent.git
cd SUNLU_AMS_Heater_AutoVent

# Install dependencies
pip install -r requirements.txt

# Run directly
python serial_monitor.py

# Or build .exe
pip install pyinstaller
pyinstaller --onefile --windowed --name="SUNLU_SerialMonitor" serial_monitor.py
```

## License

MIT License - See main project LICENSE file

