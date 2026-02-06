# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an Arduino-based automated vent control system for 3D printer filament dryers. It monitors AC current to detect heater/fan operation and controls a servo-operated vent to manage humidity and heat dissipation.

**Key Features:**
- Current-based heater/fan detection via ACS758 current sensor
- Servo-controlled vent with position feedback
- Self-learning threshold calibration mode
- EEPROM storage for learned parameters
- 3-minute fan cooldown delay before closing vent

## Hardware

**Supported Boards:**
- Seeed XIAO SAMD21 (3.3V, 48MHz ARM Cortex-M0+, 256KB Flash, 32KB SRAM)
- Seeed XIAO RP2040 (3.3V, 133MHz Dual ARM Cortex-M0+, 2MB Flash, 264KB SRAM)
- Board type is auto-detected via conditional compilation

**Components:**
- ACS758 LCB-050B AC current sensor (40mV/A sensitivity)
- ADS1115 16-bit I2C ADC (±4.096V range)
- Feedback-enabled servo motor
- Status LED and momentary button

**Critical Pin Mapping:**
- D0: Servo PWM output
- D3: Button input (active low, internal pullup)
- D10: Status LED (active low)
- SDA/SCL: I2C bus for ADS1115
- ADS1115 A0: Current sensor input
- ADS1115 A1: Servo position feedback

## Build Commands

This is an Arduino sketch that must be compiled using the Arduino IDE or CLI:

```bash
# Compile for SAMD21
arduino-cli compile --fqbn Seeeduino:samd:seeed_XIAO_m0 SUNLU_AMS_Heater_AutoVent.ino

# Compile for RP2040
arduino-cli compile --fqbn Seeeduino:rp2040:seeed_xiao_rp2040 SUNLU_AMS_Heater_AutoVent.ino

# Upload to SAMD21
arduino-cli upload -p /dev/ttyACM0 --fqbn Seeeduino:samd:seeed_XIAO_m0 SUNLU_AMS_Heater_AutoVent.ino

# Upload to RP2040
arduino-cli upload -p /dev/ttyACM0 --fqbn Seeeduino:rp2040:seeed_xiao_rp2040 SUNLU_AMS_Heater_AutoVent.ino
```

**Required Arduino Libraries:**
- Wire (built-in I2C)
- Servo (built-in)
- Adafruit_ADS1X15
- FlashStorage (SAMD21 only)
- EEPROM (RP2040 only)

## Architecture

### Board Abstraction Layer

The code uses conditional compilation to support both SAMD21 and RP2040:
- `BOARD_SAMD21` / `BOARD_RP2040` macros control board-specific code
- RP2040 requires explicit GPIO function configuration (`gpio_set_function`)
- EEPROM emulation differs between boards (FlashStorage vs EEPROM library)

### State Machine Design

**System States:**
- Normal operation (monitoring and vent control)
- Standby mode (system off, vent closed, LED off)
- Calibration mode (LED flashing, measuring offsets)
- Learning mode (multi-phase threshold learning)

**Vent Control Logic:**
- Heater ON → Open vent immediately
- Heater OFF + Fan OFF → Close vent immediately
- Heater OFF + Fan ON → Start 3-minute countdown, then close vent
- Uses hysteresis thresholds to prevent oscillation

### Current Sensing Architecture

The system measures AC RMS current using Welford's online variance algorithm:
1. ADS1115 reads differential voltage from ACS758 sensor
2. Voltage offset is calibrated at startup (no load condition)
3. RMS current is calculated from variance over 500ms sample window
4. Rolling 30-second average provides stable readings

**Three detection levels:**
- Fan detection: ~25mA threshold
- Heater detection: ~200mA threshold
- Activity detection: ~100mA threshold (simple ON/OFF)

### Servo Control with Feedback

The servo system uses position feedback for verification:
- Calibration records feedback voltage at open/closed positions
- Linear interpolation maps feedback to degrees
- Servo detaches after movement to reduce noise/power
- Physical servo is reversed: 180° = closed, 0° = open

### Learning Mode

Three-phase automatic threshold learning:
1. **Baseline**: Measures idle current (dryer OFF)
2. **Heater**: Measures current with heater + fan ON
3. **Fan Only**: Measures current with only fan running

Calculated thresholds are saved to EEPROM with magic number validation.

### LED Patterns

Multiple LED patterns indicate system state:
- `LED_SOLID`: Normal operation
- `LED_SLOW_FLASH` (250ms): Calibration in progress
- `LED_FAST_FLASH` (100ms): Learning mode measuring
- `LED_DOUBLE_BLINK`: Repeating double blink
- `LED_BREATHING`: PWM-simulated breathing effect
- `LED_SUCCESS_BLINK`: 2 quick flashes (success)
- `LED_COMPLETE_BLINK`: 3 long flashes (learning complete)
- `LED_OFF`: Standby mode

### Button Interface

**Press Types:**
- Short press (<2s): Full recalibration
- Long press (2-5s): Toggle standby/wake mode
- Very long press (>5s): Enter learning mode

Edge detection prevents repeated triggers and handles transitions between modes.

## Serial Commands

Available at runtime via Serial (115200 baud):
- `O` or `o`: Open vent
- `C` or `c`: Close vent
- `R` or `r`: Recalibrate servo only
- `F` or `f`: Read current feedback position
- `S` or `s`: Toggle standby mode
- `L` or `l`: Enter learning mode
- `0-180`: Move servo to specific degrees

## Important Implementation Details

**RP2040 GPIO Initialization:**
GPIO3 (D10/LED) defaults to SPI MOSI function on RP2040 and MUST be explicitly set to GPIO mode using `gpio_set_function()` before pinMode() or digitalWrite() will work. This is a critical requirement not present on SAMD21.

**Servo Detachment:**
The servo is detached after movement to prevent EMI noise from affecting the sensitive current measurements. Attach only when moving.

**Offset Calibration:**
The ACS758 current sensor has a voltage offset that varies with temperature and supply voltage. Always calibrate offset at startup with no load on the dryer.

**Hysteresis Thresholds:**
All detection thresholds use hysteresis (separate ON/OFF values) to prevent chattering when current fluctuates near threshold values.

**EEPROM Magic Number:**
Learned thresholds are stored with a magic number (0xABCD1234) to verify valid data. Without valid data, defaults are used.
