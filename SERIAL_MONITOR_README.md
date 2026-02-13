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

## Troubleshooting

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

## Credits

Generated with [Claude Code](https://claude.com/claude-code)
