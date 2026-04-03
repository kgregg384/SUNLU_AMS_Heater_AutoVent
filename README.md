# SUNLU AMS Heater Auto-Vent Controller

An intelligent, automatic vent controller for SUNLU AMS 3D printer filament dryers. It monitors AC current draw to detect heater and fan states, then drives a servo-operated vent to manage moisture removal, temperature, and energy consumption — all without any manual intervention.

## How It Works

The controller uses a hall-effect current sensor (ACS758) to non-invasively monitor your dryer's power consumption. A 16-bit ADC (ADS1115) provides precise current measurements, allowing the system to distinguish between three states:

- **Heater + Fan running** — vent opens immediately
- **Fan only** — vent stays open for cooling
- **Idle** — vent closes after a 3-minute cooldown delay

A self-learning calibration mode lets the controller adapt to your specific dryer model by measuring the actual current signatures.

## Features

- **Automatic operation** — no manual vent management needed
- **Self-learning calibration** — adapts to any compatible dryer
- **Non-volatile storage** — learned thresholds survive power cycles
- **Servo position feedback** — closed-loop vent positioning
- **Dual board support** — works with Seeed XIAO SAMD21 or RP2040
- **Single-button interface** — short/long/very-long press for different functions
- **LED status indication** — see what the controller is doing at a glance
- **Serial debug console** — full diagnostic output and manual control commands
- **Serial Monitor app** — GUI tool with color-coded output, quick command buttons, and remote learning mode
- **Pre-built UF2 firmware** — flash without compiling using drag-and-drop

## Hardware

### Components

| Component | Model | Purpose |
|-----------|-------|---------|
| Microcontroller | Seeed XIAO SAMD21 **or** RP2040 | Main controller (identical pinout) |
| Current Sensor | ACS758 LCB-050B | AC current measurement (±50A) |
| ADC | ADS1115 | 16-bit I2C analog-to-digital converter |
| Servo | Feedback-enabled servo | Vent actuation with position feedback |
| PCB | Custom (from [3dcreator.shop](https://3dcreator.shop)) | Bare board or kit with components |

You'll also need a momentary push button, an LED with resistor, a USB-C cable, and a 5V 1A+ power source.

### Pin Configuration

| Pin | Function | Notes |
|-----|----------|-------|
| D0 | Servo PWM | Output |
| D3 | Button | Input, active low, internal pullup |
| D10 | Status LED | Active low |
| SDA/SCL | I2C | ADS1115 @ address 0x48 |

ADS1115 channels: A0 = current sensor, A1 = servo feedback.

### 3D Printed Parts

STL files for the vent mechanism and mounting hardware are available in this repository. Approximate print time: ~4 hours.

## Getting Started

### Prerequisites

- [Arduino IDE](https://www.arduino.cc/ide) or [PlatformIO](https://platformio.org/)
- Board package for your XIAO variant:
  - **SAMD21**: Seeed SAMD Boards
  - **RP2040**: Seeed RP2040 Boards (via [Earle Philhower's core](https://github.com/earlephilhower/arduino-pico))
- Required libraries:
  - `Adafruit ADS1X15`
  - `Servo`
  - `Wire`
  - `FlashStorage` (SAMD21) or `EEPROM` (RP2040)

### Upload Firmware

**Option A: Pre-built UF2 (RP2040 only — no Arduino IDE needed)**

1. Connect your XIAO RP2040 while holding the BOOT button to enter bootloader mode
2. A USB drive named `RPI-RP2` will appear
3. Drag and drop the appropriate UF2 file onto the drive:
   - `SUNLU_AMS_Heater_AutoVent_v1.uf2` — for the v1 3D printed design
   - `SUNLU_AMS_Heater_AutoVent_v2.uf2` — for the v2 3D printed design
   
   The v1 and v2 designs drive the vent from opposite sides, so the servo travel direction for open/close is reversed between them. Choose the file that matches your printed parts.
4. The board reboots automatically and starts running

**Option B: Compile from source**

1. Connect your XIAO board via USB-C
2. Select the correct board in Arduino IDE
3. Open `SUNLU_AMS_Heater_AutoVent.ino`
4. Upload

The firmware automatically detects which board it's running on at compile time — no configuration needed.

### First-Time Setup

1. Assemble the PCB and 3D printed parts, mount onto your dryer
2. Connect AC power to the AutoVent and USB-C power to the controller
3. Wait for the LED to go solid after the 10-second startup calibration
4. Run **Learning Mode** to teach the controller your dryer's current signature:
   - Ensure dryer is OFF
   - Hold the button for 6–7 seconds to enter learning mode
   - Follow the LED prompts through 3 measurement phases (~45 seconds total)
   - Settings are saved automatically

See the [User Guide](USER_GUIDE.md) for detailed step-by-step instructions.

## Button Controls

| Press Duration | Action |
|---------------|--------|
| < 2 seconds | Recalibrate servo |
| 2–5 seconds | Toggle standby mode |
| > 5 seconds | Enter learning mode |

## LED Patterns

| Pattern | Meaning |
|---------|---------|
| Solid | Normal operation / waiting for input |
| Off | Standby mode |
| Slow flash | Servo calibrating |
| Fast flash | Measuring (learning mode) |
| 3 long blinks | Learning complete |

## Serial Monitor App

A dedicated GUI serial monitor is included in the `SerialMonitor/` folder. It provides color-coded output, quick command buttons, auto-detection of the board, and the ability to run the full learning mode remotely without pressing the physical button.

See the **[Serial Monitor Setup Guide](SerialMonitor/README.md)** for installation and usage instructions.

## Serial Debug

You can also use any serial terminal at 115200 baud. Available commands:

| Command | Action |
|---------|--------|
| `O` / `o` | Open vent |
| `C` / `c` | Close vent |
| `R` / `r` | Recalibrate servo |
| `F` / `f` | Read servo feedback |
| `S` / `s` | Toggle standby |
| `L` / `l` | Enter learning mode |
| `0`–`180` | Move servo to angle |

## Documentation

- **[User Guide](USER_GUIDE.md)** — Setup, operation, troubleshooting, and maintenance
- **[Technical Specification](TECHNICAL_SPECIFICATION.md)** — Algorithms, timing, hardware details, and architecture

## Project Structure

```
SUNLU_AMS_Heater_AutoVent.ino        # Main firmware (SAMD21 + RP2040)
SUNLU_AMS_Heater_AutoVent_v1.uf2     # Pre-built firmware for hardware v1 (RP2040)
SUNLU_AMS_Heater_AutoVent_v2.uf2     # Pre-built firmware for hardware v2 (RP2040)
SerialMonitor/                        # GUI serial monitor app (Python/tkinter)
  serial_monitor.py                   #   Main application
  requirements.txt                    #   Python dependencies
  install_dependencies.bat            #   Windows dependency installer
  build_windows_exe.bat               #   Build standalone .exe
build/                                # Arduino CLI board FQBNs
USER_GUIDE.md                         # End-user documentation
TECHNICAL_SPECIFICATION.md            # Technical reference
```

## PCBs and Kits

Custom PCBs (bare boards and assembled kits) are available from **[3dcreator.shop](https://3dcreator.shop)**.

## License

MIT

## Contributing

Issues and pull requests are welcome. If you've adapted this for a different dryer model, feel free to share your experience.
