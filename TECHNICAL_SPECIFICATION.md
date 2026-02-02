# SUNLU AMS Heater Auto-Vent Controller - Technical Specification

## 1. System Overview

The SUNLU AMS Heater Auto-Vent Controller is an intelligent automation system designed for 3D printer filament dryers. It monitors AC current consumption to detect heater and fan operation states, automatically controlling a servo-operated vent to optimize humidity removal and temperature management.

### 1.1 Key Features

- **Automatic Current Monitoring**: Real-time AC current sensing with offset compensation
- **Intelligent State Detection**: Distinguishes between heater-on, fan-only, and idle states
- **Self-Learning Calibration**: Adapts to different dryer models through automated threshold learning
- **Non-Volatile Memory**: Stores learned parameters in EEPROM for persistence across power cycles
- **Visual Feedback**: Multi-pattern LED status indication
- **User Interface**: Three-mode button control (short/long/very-long press)
- **Cooldown Management**: 3-minute delay for fan-only operation after heater shutoff

## 2. Hardware Specifications

### 2.1 Microcontroller

- **Model**: Seeed XIAO SAMD21
- **Processor**: ARM Cortex-M0+ @ 48MHz
- **Operating Voltage**: 3.3V
- **Flash Memory**: 256KB
- **SRAM**: 32KB
- **I/O Voltage**: 3.3V logic levels

### 2.2 Current Sensor

- **Model**: ACS758 LCB-050B
- **Type**: Hall-effect AC/DC current sensor
- **Measurement Range**: ±50A
- **Sensitivity**: 40mV/A @ 5V supply
- **Output Type**: Analog voltage (ratiometric)
- **Bandwidth**: DC to 100kHz

### 2.3 ADC Module

- **Model**: Adafruit ADS1115
- **Resolution**: 16-bit
- **Interface**: I2C (address 0x48)
- **Input Range**: ±4.096V (GAIN_ONE configuration)
- **LSB Size**: 0.125mV
- **Sample Rate**: Configurable, default 128 SPS
- **Channels Used**:
  - A0: Current sensor input
  - A1: Servo feedback input

### 2.4 Servo Motor

- **Type**: Feedback-enabled servo
- **Control**: PWM signal (standard servo protocol)
- **Position Range**: 0° to 180°
- **Feedback**: Analog voltage proportional to position
- **Operating Voltage**: 5V (separate power supply recommended)

### 2.5 User Interface

- **LED**: Single status indicator
  - Type: Standard LED
  - Configuration: Active low (cathode to GPIO, anode to VCC through resistor)
  - Pin: D10

- **Button**: Momentary push button
  - Type: SPST normally-open
  - Configuration: Active low with internal pullup resistor
  - Pin: D3

## 3. Pin Configuration

| Pin | Function | Direction | Configuration |
|-----|----------|-----------|---------------|
| D0 | Servo PWM | Output | Push-pull |
| D3 | Button | Input | Internal pullup, active low |
| D10 | Status LED | Output | Active low |
| SDA | I2C Data | I/O | Open-drain with external pullup |
| SCL | I2C Clock | Output | Open-drain with external pullup |

## 4. Software Architecture

### 4.1 Operating Modes

#### 4.1.1 Normal Operation Mode
- Continuous current monitoring
- State detection (heater, fan, idle)
- Automatic vent control
- LED status indication (solid when active)

#### 4.1.2 Standby Mode
- System disabled
- LED off
- Vent remains in current position
- Triggered by 2-5 second button press

#### 4.1.3 Learning Mode
- Three-phase automatic calibration
- LED pattern guidance for user
- EEPROM storage of learned thresholds
- Triggered by 5+ second button press

### 4.2 Current Measurement Algorithm

#### 4.2.1 Offset Compensation
At startup, the system measures the zero-current baseline for 5 seconds:

```
voltageOffset = average(ADC_readings) over 5 seconds
```

#### 4.2.2 RMS Calculation
AC current is measured using Welford's online algorithm for numerical stability:

```
For each sample over 500ms:
    voltage = ADC_reading - voltageOffset
    Update running mean and variance
RMS_voltage = sqrt(variance)
RMS_current = RMS_voltage / sensitivity
```

#### 4.2.3 Rolling Average
A 30-second rolling average smooths transient variations:

```
samples[] = circular buffer of 60 samples (0.5s each)
average_current = mean(samples[])
```

### 4.3 State Detection

The system uses hysteresis-based thresholds to prevent chattering:

| State | Entry Condition | Exit Condition |
|-------|----------------|----------------|
| Heater ON | Current > HEATER_ON_THRESHOLD | Current < HEATER_OFF_THRESHOLD |
| Fan ON | Current > FAN_ON_THRESHOLD | Current < FAN_OFF_THRESHOLD |
| Idle | Current < FAN_OFF_THRESHOLD | Current > FAN_ON_THRESHOLD |

Default thresholds (before learning):
- HEATER_ON_THRESHOLD: 200mA
- HEATER_OFF_THRESHOLD: 120mA
- FAN_ON_THRESHOLD: 25mA
- FAN_OFF_THRESHOLD: 20mA

### 4.4 Vent Control Logic

```
IF (heater_detected OR fan_detected):
    OPEN vent

IF (heater_off AND fan_detected):
    Keep vent OPEN
    Start cooldown timer

IF (heater_off AND fan_off AND cooldown_expired):
    CLOSE vent after 3 minutes

IF (heater_off AND fan_off AND NOT in_cooldown):
    CLOSE vent immediately
```

### 4.5 Learning Mode Algorithm

#### Phase 1: Baseline Measurement
1. LED: SOLID (5 second prep time)
2. User ensures dryer is OFF
3. LED: FAST FLASH (10 second measurement)
4. Records: `baseline_current = avg(measurements)`
5. LED: SUCCESS BLINK

#### Phase 2: Heater + Fan Measurement
1. LED: BREATHING pattern (5 second prep time)
2. User turns dryer ON (heater + fan)
3. LED: FAST FLASH (10 second measurement)
4. Records: `heater_current = max(measurements)`
5. LED: SUCCESS BLINK

#### Phase 3: Fan-Only Measurement
1. LED: DOUBLE BLINK pattern (5 second prep time)
2. User turns heater OFF (fan continues)
3. LED: FAST FLASH (10 second measurement)
4. Records: `fan_current = avg(measurements)`
5. LED: COMPLETE BLINK (3 long flashes)

#### Threshold Calculation
```
FAN_ON_THRESHOLD = (baseline + fan_current) / 2 + 2mA margin
FAN_OFF_THRESHOLD = FAN_ON_THRESHOLD - 5mA hysteresis

HEATER_ON_THRESHOLD = (fan_current + heater_current) / 2
HEATER_OFF_THRESHOLD = (baseline + fan_current) / 2 + 5mA margin
```

Thresholds are saved to EEPROM with magic number validation.

### 4.6 EEPROM Storage

```c
struct ThresholdData {
    uint32_t magic;  // 0xABCD1234 for validation
    float fanOnThreshold;
    float fanOffThreshold;
    float heaterOnThreshold;
    float heaterOffThreshold;
};
```

Storage uses SAMD21 FlashStorage library for EEPROM emulation.

### 4.7 LED Patterns

| Pattern | Timing | Meaning |
|---------|--------|---------|
| SOLID | Continuous on | Normal operation / Phase 1 prep |
| OFF | Continuous off | Standby mode |
| FAST FLASH | 100ms on/off | Measuring (learning mode) |
| SLOW FLASH | 250ms on/off | Calibrating servo |
| BREATHING | 2s cycle fade | Phase 2 prep (turn dryer ON) |
| DOUBLE BLINK | 2 blinks, pause, repeat | Phase 3 prep (turn heater OFF) |
| SUCCESS BLINK | 2 quick flashes | Phase complete |
| COMPLETE BLINK | 3 long flashes | Learning complete |

### 4.8 Button Interface

| Press Duration | Action | Result |
|---------------|--------|--------|
| < 2 seconds | Short press | Recalibrate servo |
| 2-5 seconds | Long press | Toggle standby mode |
| > 5 seconds | Very long press | Enter learning mode |

## 5. Timing Specifications

| Parameter | Value | Notes |
|-----------|-------|-------|
| ADC Sample Period | 500ms | ~30 AC cycles @ 60Hz |
| Rolling Average Window | 30 seconds | 60 samples |
| Offset Measurement Time | 5 seconds | At startup |
| Learning Phase Duration | 10 seconds | Per phase |
| Learning Prep Time | 5 seconds | Between phases |
| Fan Cooldown Delay | 3 minutes | After heater turns off |
| Servo Settle Time | 1 second | After position change |
| Update Display Interval | 5 seconds | Serial output |
| Button Debounce | 50ms | Implicit in timing |

## 6. Memory Usage

- **Flash**: ~57KB / 256KB (22%)
- **SRAM**: Estimated ~4KB / 32KB (12%)
- **EEPROM**: 20 bytes (threshold storage)

## 7. Communication Interfaces

### 7.1 I2C Bus
- **Speed**: 100kHz (standard mode)
- **Device**: ADS1115 @ 0x48

### 7.2 Serial Console (Debug)
- **Baud Rate**: 115200
- **Format**: 8N1
- **Purpose**: Debug output and manual control commands

### 7.3 Serial Commands (Debug Mode)

| Command | Action |
|---------|--------|
| `O` or `o` | Open vent |
| `C` or `c` | Close vent |
| `R` or `r` | Recalibrate servo |
| `F` or `f` | Read servo feedback |
| `S` or `s` | Toggle standby mode |
| `L` or `l` | Enter learning mode |
| `0-180` | Move servo to specific angle |

## 8. Power Requirements

- **Microcontroller**: 3.3V @ ~50mA (active), ~15mA (idle)
- **ADS1115**: 3.3V @ ~1mA
- **Servo**: 5V @ 100-500mA (load dependent)
- **Total System**: 5V @ 200-600mA typical

**Recommended**: Use separate 5V power supply for servo, common ground with microcontroller.

## 9. Environmental Specifications

- **Operating Temperature**: 0°C to 50°C
- **Storage Temperature**: -20°C to 70°C
- **Humidity**: 10% to 90% non-condensing
- **Altitude**: Up to 2000m

## 10. Safety Features

- **Offset Compensation**: Eliminates DC bias errors
- **Rolling Average**: Filters transient spikes
- **Hysteresis**: Prevents rapid state changes
- **Cooldown Delay**: Protects dryer fan bearing
- **Watchdog**: None (consider adding for production)
- **Overcurrent Protection**: Relies on ACS758 internal protection

## 11. Calibration Procedure

### 11.1 Servo Calibration
Performed automatically at startup:
1. Move to closed position (180°)
2. Record feedback voltage
3. Move to open position (0°)
4. Record feedback voltage
5. Create linear mapping between feedback and position

### 11.2 Current Threshold Learning
See Section 4.5 for detailed algorithm.

## 12. Error Handling

- **ADS1115 Not Found**: System halts with error message
- **Servo Feedback Out of Range**: Warning message, continues operation
- **EEPROM Read Invalid**: Uses default thresholds
- **Button Stuck**: No specific handling (could add timeout)

## 13. Future Enhancements

Potential improvements for future versions:

1. **Watchdog Timer**: Auto-reset on system hang
2. **Temperature Sensing**: Direct dryer temperature monitoring
3. **WiFi Connectivity**: Remote monitoring and control
4. **Data Logging**: Historical current and state data
5. **Adjustable Cooldown**: User-configurable delay time
6. **Multiple Profiles**: Store settings for different dryers
7. **OLED Display**: On-device status display
8. **Emergency Stop**: Hardware override for safety

## 14. Compliance and Standards

This device is intended for personal/hobbyist use. For commercial deployment, consider:

- FCC Part 15 (EMI/RFI emissions)
- CE marking (if selling in EU)
- UL/CSA certification (electrical safety)
- RoHS compliance (hazardous substances)

## 15. Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2024-01-27 | Initial release with learning mode |

---

**Document Version**: 1.0
**Last Updated**: January 27, 2024
**Author**: Generated with Claude Code
