/*
 * SUNLU AMS Heater Auto-Vent Controller
 *
 * Automated vent control system for 3D printer filament dryers based on current monitoring.
 * Detects heater and fan operation via AC current sensing and automatically controls a
 * servo-operated vent to manage humidity and heat dissipation.
 *
 * Features:
 * - Automatic current-based heater/fan detection
 * - Servo-controlled vent with position feedback
 * - Self-learning threshold calibration mode
 * - EEPROM storage for learned parameters
 * - LED status indication
 * - Button-based user interface
 * - 3-minute fan cooldown delay
 *
 * Hardware:
 * - Board: Seeed XIAO SAMD21 OR Seeed XIAO RP2040 (auto-detected)
 *   - SAMD21: 3.3V, 48MHz ARM Cortex-M0+, 256KB Flash, 32KB SRAM
 *   - RP2040: 3.3V, 133MHz Dual ARM Cortex-M0+, 2MB Flash, 264KB SRAM
 * - Current Sensor: ACS758 LCB-050B (AC current, 40mV/A sensitivity)
 * - ADC: ADS1115 16-bit I2C ADC (±4.096V range)
 * - Servo: Feedback-enabled servo motor
 * - LED: Status indicator (active low)
 * - Button: Momentary push button (active low, internal pullup)
 *
 * Pin Configuration (identical physical pins on both boards):
 * - D0: Servo PWM output
 * - D3: Button input (active low, internal pullup)
 * - D10: Status LED (active low)
 * - SDA/SCL: I2C bus for ADS1115
 *
 * ADS1115 Channel Mapping:
 * - A0: ACS758 current sensor output
 * - A1: Servo position feedback
 *
 * Author: Generated with Claude Code
 * License: MIT
 * Version: 1.1 (Added RP2040 support)
 */

// Board detection and conditional compilation
#if defined(ARDUINO_ARCH_SAMD)
  #define BOARD_SAMD21
  #include <FlashStorage.h>  // SAMD21 EEPROM emulation
  #define MY_BOARD_NAME "SAMD21"
#elif defined(ARDUINO_ARCH_RP2040)
  #define BOARD_RP2040
  #include <EEPROM.h>  // RP2040 EEPROM emulation
  #include <hardware/gpio.h>
  #include <hardware/pwm.h>
  #define MY_BOARD_NAME "RP2040"
#else
  #error "Unsupported board! This code requires Seeed XIAO SAMD21 or RP2040"
#endif

#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <Servo.h>
#include <math.h>

#ifdef BOARD_RP2040
  #include <Adafruit_NeoPixel.h>
#endif

// ----------------- Pin Definitions -----------------
// RP2040 requires D prefix, SAMD21 uses numeric pins
// Both boards have identical physical pinout on XIAO form factor
#ifdef BOARD_RP2040
  #define SERVO_PWM_PIN    D0   // Servo PWM output (GPIO26)
  #define LED_PIN          D10  // Status LED (GPIO3)
  #define BUTTON_PIN       D3   // Button input (GPIO29)
  // GPIO numbers for RP2040 hardware control
  #define GPIO_SERVO       26   // D0 = GPIO26
  #define GPIO_LED         3    // D10 = GPIO3
  #define GPIO_BTN         29   // D3 = GPIO29
#else  // SAMD21
  #define SERVO_PWM_PIN    0    // Servo PWM output
  #define LED_PIN          10   // Status LED
  #define BUTTON_PIN       3    // Button input
#endif

// ----------------- Constants -----------------
#define ADS_CH_CURRENT   0   // ACS758 -> ADS A0
#define ADS_CH_SERVO_FB  1   // Servo feedback -> ADS A1

static const float ADS_LSB_V     = 0.000125f;   // ADS1115 @ GAIN_ONE (±4.096V range)
static const float SENS_V_PER_A  = 0.040f;      // ACS758 LCB-050B @ 5V ≈ 40 mV/A
static const uint32_t SAMPLE_MS  = 500;         // Sample for 500ms (~30 AC cycles @ 60Hz)
static const uint32_t OFFSET_MEASURE_MS = 5000; // 5 seconds to measure offset at startup
static const uint32_t ROLLING_AVG_MS = 30000;   // 30 second rolling average window
static const uint8_t MAX_SAMPLES = 60;          // Store 60 samples (30 seconds at 0.5s per sample)

// Detection thresholds - Can be learned or set manually
// These defaults will be used if no learned values are stored in EEPROM
static float HEATER_ON_THRESHOLD  = 0.200f;  // Above this = heater on (200mA)
static float HEATER_OFF_THRESHOLD = 0.120f;  // Below this = heater off (120mA)
static float FAN_ON_THRESHOLD     = 0.025f;  // Above this = fan on (25mA, above baseline ~13mA)
static float FAN_OFF_THRESHOLD    = 0.020f;  // Below this = fan off (20mA, with hysteresis)

// Simple activity detection - anything running vs completely idle
// This is what you'll likely use for actual vent control
static const float ACTIVITY_THRESHOLD    = 0.100f;  // Above this = something is running (100mA)
static const float IDLE_THRESHOLD        = 0.080f;  // Below this = completely idle (80mA)

// Servo positions in DEGREES (Adafruit method)
static int SERVO_CLOSED_DEG = 20;   // Closed position (degrees)
static int SERVO_OPEN_DEG   = 160;  // Open position (degrees)

// Feedback calibration values (Adafruit method)
// These map feedback readings to servo positions
static float g_minFeedback = 0.0f;   // Feedback at closed position
static float g_maxFeedback = 1.0f;   // Feedback at open position

// ----------------- Globals -----------------
Adafruit_ADS1115 ads;
Servo servo;

// Offset tracking
float g_voltageOffset = 0.0f;

// Rolling average circular buffer
float g_currentSamples[MAX_SAMPLES];
uint8_t g_sampleIndex = 0;
uint8_t g_sampleCount = 0;
uint32_t g_lastUpdateMs = 0;

// Vent control state
bool g_ventOpen = false;
bool g_heaterWasOn = false;
uint32_t g_ventCloseDelayStart = 0;
bool g_ventClosePending = false;
static const uint32_t VENT_CLOSE_DELAY_MS = 180000;  // 3 minute delay before closing

// Device state detection (global so they can be reset during calibration)
bool g_fanOn = false;
bool g_heaterOn = false;
bool g_systemActive = false;

// Serial input buffer for numeric commands
char g_serialBuffer[8];
uint8_t g_serialIdx = 0;

// Calibration and LED state
bool g_calibrated = false;
uint32_t g_lastLedToggle = 0;
bool g_ledState = false;
static const uint32_t LED_FLASH_INTERVAL = 250;  // Flash every 250ms during calibration

// Built-in LED breathing effect (RP2040 only)
#ifdef BOARD_RP2040
  uint32_t g_breathingStartMs = 0;
  static const uint32_t BREATHING_PERIOD_MS = 3000;  // 3 second breathing cycle
  static const uint8_t BREATHING_MIN = 2;   // Minimum brightness (never fully off)
  static const uint8_t BREATHING_MAX = 40;  // Maximum brightness (dimmer)
  static const uint8_t NEOPIXEL_POWER_PIN = 11;  // NeoPixel power pin (must be HIGH)
  static const uint8_t NEOPIXEL_DATA_PIN = 12;   // NeoPixel data pin (GPIO12 on XIAO RP2040)
  static const uint8_t NEOPIXEL_COUNT = 1;       // One NeoPixel LED
  Adafruit_NeoPixel neopixel(NEOPIXEL_COUNT, NEOPIXEL_DATA_PIN, NEO_GRB + NEO_KHZ800);
#endif

// Button state for edge detection and long press
bool g_lastButtonState = HIGH;
uint32_t g_buttonPressStart = 0;
bool g_buttonHandled = false;
bool g_ignoreNextRelease = false;  // Flag to ignore button release after entering learning mode
static const uint32_t LONG_PRESS_MS = 2000;   // 2 second long press
static const uint32_t VLONG_PRESS_MS = 5000;  // 5 second very long press for learning mode

// System power state (standby mode)
bool g_systemOn = true;

// Learning mode state
enum LearningPhase {
  LEARN_NONE = 0,
  LEARN_BASELINE,
  LEARN_HEATER,
  LEARN_FAN_ONLY,
  LEARN_COMPLETE
};
LearningPhase g_learningPhase = LEARN_NONE;
float g_learnedBaseline = 0.0f;
float g_learnedHeater = 0.0f;
float g_learnedFanOnly = 0.0f;
uint32_t g_learningStartMs = 0;  // When measurement started (0 = in prep phase)
uint32_t g_learningPhaseEntryMs = 0;  // When phase was entered
static const uint32_t LEARNING_MEASURE_MS = 10000;  // 10 seconds per phase
static const uint32_t LEARNING_PREP_MS = 5000;  // 5 seconds prep time before auto-start

// LED patterns
enum LEDPattern {
  LED_SOLID,
  LED_FAST_FLASH,      // 100ms - learning mode active
  LED_SLOW_FLASH,      // 250ms - calibration
  LED_DOUBLE_BLINK,    // Double blink pattern
  LED_BREATHING,       // Slow breathing pattern
  LED_SUCCESS_BLINK,   // 2 quick flashes
  LED_COMPLETE_BLINK,  // 3 long flashes
  LED_OFF
};
LEDPattern g_ledPattern = LED_SOLID;
uint32_t g_ledPatternStart = 0;

// EEPROM structure for storing learned thresholds
struct ThresholdData {
  uint32_t magic;  // Magic number to verify valid data
  float fanOnThreshold;
  float fanOffThreshold;
  float heaterOnThreshold;
  float heaterOffThreshold;
};
static const uint32_t EEPROM_MAGIC = 0xABCD1234;

#ifdef BOARD_SAMD21
  FlashStorage(thresholdStorage, ThresholdData);
#endif

// ----------------- Helper Functions -----------------

#ifdef BOARD_RP2040
// Update built-in NeoPixel with smooth breathing effect (RP2040 only)
// Uses NeoPixel library for WS2812B RGB LED control
// Color changes based on heater state: RED = heater on, BLUE = heater off
void updateBuiltinLEDBreathing() {
  uint32_t now = millis();
  uint32_t elapsed = now - g_breathingStartMs;
  uint32_t phase = elapsed % BREATHING_PERIOD_MS;

  // Calculate brightness using sine wave approximation
  // Phase goes from 0 to BREATHING_PERIOD_MS
  float t = (float)phase / (float)BREATHING_PERIOD_MS;  // 0.0 to 1.0

  // Use sine wave: sin(2*PI*t) ranges from -1 to 1
  // Convert to 0 to 1: (sin(2*PI*t) + 1) / 2
  float angle = t * 2.0f * 3.14159f;
  float brightness = (sin(angle) + 1.0f) / 2.0f;

  // Map to brightness range with minimum brightness
  uint8_t brightnessValue = BREATHING_MIN + (uint8_t)(brightness * (BREATHING_MAX - BREATHING_MIN));

  // Seeed XIAO RP2040: NeoPixel on pin 12 (data), pin 11 (power)
  // Color based on heater state: RED = heater on, BLUE = heater off
  if (g_heaterOn) {
    // Red when heater is on
    neopixel.setPixelColor(0, neopixel.Color(brightnessValue, 0, 0));
  } else {
    // Blue when heater is off
    neopixel.setPixelColor(0, neopixel.Color(0, 0, brightnessValue));
  }
  neopixel.show();
}
#endif

// LED pattern handler - call this frequently from loop()
void updateLEDPattern() {
  uint32_t now = millis();
  uint32_t elapsed = now - g_ledPatternStart;

  switch (g_ledPattern) {
    case LED_SOLID:
      digitalWrite(LED_PIN, HIGH);
      #ifdef BOARD_SAMD21
        digitalWrite(LED_BUILTIN, HIGH);
      #endif
      break;

    case LED_FAST_FLASH:  // 100ms on/off for learning mode
      if (now - g_lastLedToggle >= 100) {
        g_ledState = !g_ledState;
        digitalWrite(LED_PIN, g_ledState);
        #ifdef BOARD_SAMD21
          digitalWrite(LED_BUILTIN, g_ledState);
        #endif
        g_lastLedToggle = now;
      }
      break;

    case LED_SLOW_FLASH:  // 250ms on/off for calibration
      if (now - g_lastLedToggle >= 250) {
        g_ledState = !g_ledState;
        digitalWrite(LED_PIN, g_ledState);
        #ifdef BOARD_SAMD21
          digitalWrite(LED_BUILTIN, g_ledState);
        #endif
        g_lastLedToggle = now;
      }
      break;

    case LED_DOUBLE_BLINK:  // Double blink every 2 seconds
      if (elapsed < 150) {
        digitalWrite(LED_PIN, HIGH);
        #ifdef BOARD_SAMD21
          digitalWrite(LED_BUILTIN, HIGH);
        #endif
      } else if (elapsed < 300) {
        digitalWrite(LED_PIN, LOW);
        #ifdef BOARD_SAMD21
          digitalWrite(LED_BUILTIN, LOW);
        #endif
      } else if (elapsed < 450) {
        digitalWrite(LED_PIN, HIGH);
        #ifdef BOARD_SAMD21
          digitalWrite(LED_BUILTIN, HIGH);
        #endif
      } else if (elapsed < 2000) {
        digitalWrite(LED_PIN, LOW);
        #ifdef BOARD_SAMD21
          digitalWrite(LED_BUILTIN, LOW);
        #endif
      } else {
        g_ledPatternStart = now;
      }
      break;

    case LED_BREATHING:  // Slow breathing pattern using PWM
      {
        // Simple approximation using on/off timing
        uint32_t cycle = elapsed % 2000;
        if (cycle < 1000) {
          // Fade in - use simple threshold
          digitalWrite(LED_PIN, cycle > 500 ? HIGH : (cycle % 100 < 50 ? HIGH : LOW));
          #ifdef BOARD_SAMD21
            digitalWrite(LED_BUILTIN, cycle > 500 ? HIGH : (cycle % 100 < 50 ? HIGH : LOW));
          #endif
        } else {
          // Fade out
          uint32_t fadeOut = cycle - 1000;
          digitalWrite(LED_PIN, fadeOut < 500 ? HIGH : (fadeOut % 100 < 50 ? HIGH : LOW));
          #ifdef BOARD_SAMD21
            digitalWrite(LED_BUILTIN, fadeOut < 500 ? HIGH : (fadeOut % 100 < 50 ? HIGH : LOW));
          #endif
        }
      }
      break;

    case LED_SUCCESS_BLINK:  // 2 quick flashes then return to solid
      if (elapsed < 150) {
        digitalWrite(LED_PIN, HIGH);
        #ifdef BOARD_SAMD21
          digitalWrite(LED_BUILTIN, HIGH);
        #endif
      } else if (elapsed < 300) {
        digitalWrite(LED_PIN, LOW);
        #ifdef BOARD_SAMD21
          digitalWrite(LED_BUILTIN, LOW);
        #endif
      } else if (elapsed < 450) {
        digitalWrite(LED_PIN, HIGH);
        #ifdef BOARD_SAMD21
          digitalWrite(LED_BUILTIN, HIGH);
        #endif
      } else if (elapsed < 600) {
        digitalWrite(LED_PIN, LOW);
        #ifdef BOARD_SAMD21
          digitalWrite(LED_BUILTIN, LOW);
        #endif
      } else {
        g_ledPattern = LED_SOLID;
      }
      break;

    case LED_COMPLETE_BLINK:  // 3 long flashes then return to solid
      if (elapsed < 500) {
        digitalWrite(LED_PIN, HIGH);
        #ifdef BOARD_SAMD21
          digitalWrite(LED_BUILTIN, HIGH);
        #endif
      } else if (elapsed < 1000) {
        digitalWrite(LED_PIN, LOW);
        #ifdef BOARD_SAMD21
          digitalWrite(LED_BUILTIN, LOW);
        #endif
      } else if (elapsed < 1500) {
        digitalWrite(LED_PIN, HIGH);
        #ifdef BOARD_SAMD21
          digitalWrite(LED_BUILTIN, HIGH);
        #endif
      } else if (elapsed < 2000) {
        digitalWrite(LED_PIN, LOW);
        #ifdef BOARD_SAMD21
          digitalWrite(LED_BUILTIN, LOW);
        #endif
      } else if (elapsed < 2500) {
        digitalWrite(LED_PIN, HIGH);
        #ifdef BOARD_SAMD21
          digitalWrite(LED_BUILTIN, HIGH);
        #endif
      } else if (elapsed < 3000) {
        digitalWrite(LED_PIN, LOW);
        #ifdef BOARD_SAMD21
          digitalWrite(LED_BUILTIN, LOW);
        #endif
      } else {
        g_ledPattern = LED_SOLID;
      }
      break;

    case LED_OFF:
      digitalWrite(LED_PIN, LOW);
      #ifdef BOARD_SAMD21
        digitalWrite(LED_BUILTIN, LOW);
      #endif
      break;
  }
}

// Set LED pattern
void setLEDPattern(LEDPattern pattern) {
  g_ledPattern = pattern;
  g_ledPatternStart = millis();
  g_lastLedToggle = millis();
}

// Delay while flashing LED (for calibration)
void delayWithFlashing(uint32_t ms) {
  uint32_t start = millis();
  while (millis() - start < ms) {
    if (millis() - g_lastLedToggle >= LED_FLASH_INTERVAL) {
      g_ledState = !g_ledState;
      digitalWrite(LED_PIN, g_ledState);
      g_lastLedToggle = millis();
    }
  }
}

inline float readVolts(uint8_t ch) {
  int16_t raw = ads.readADC_SingleEnded(ch);
  return raw * ADS_LSB_V;
}

// Median filter for servo feedback (reduce noise)
static inline float median3(float a, float b, float c) {
  float x = a, y = b, z = c;
  if (x > y) { float t = x; x = y; y = t; }
  if (y > z) { float t = y; y = z; z = t; }
  if (x > y) { float t = x; x = y; y = t; }
  return y;
}

// Read servo feedback position (0.0 to 1.0)
float readServoFeedback01() {
  float v1 = readVolts(ADS_CH_SERVO_FB);
  float v2 = readVolts(ADS_CH_SERVO_FB);
  float v3 = readVolts(ADS_CH_SERVO_FB);
  float v  = median3(v1, v2, v3);
  float x  = v / 3.3f;
  if (x < 0) x = 0; if (x > 1) x = 1;
  return x;
}

// Convert feedback reading to degrees (using calibration)
int feedbackToDeg(float fb) {
  // Linear interpolation: map feedback range to degree range
  float t = (fb - g_minFeedback) / (g_maxFeedback - g_minFeedback);
  if (t < 0) t = 0;
  if (t > 1) t = 1;
  return SERVO_CLOSED_DEG + (int)(t * (SERVO_OPEN_DEG - SERVO_CLOSED_DEG));
}

// Convert degrees to expected feedback (using calibration)
float degToFeedback(int deg) {
  float t = (float)(deg - SERVO_CLOSED_DEG) / (float)(SERVO_OPEN_DEG - SERVO_CLOSED_DEG);
  if (t < 0) t = 0;
  if (t > 1) t = 1;
  return g_minFeedback + t * (g_maxFeedback - g_minFeedback);
}

// Calibrate servo feedback at two positions (Adafruit method)
void calibrateServo() {
  Serial.println(F("\n========================================"));
  Serial.println(F("SERVO CALIBRATION (Adafruit method)"));
  Serial.println(F("========================================"));

  servo.attach(SERVO_PWM_PIN);
  Serial.println(F("Servo ttached"));
  delay(100);

  // Step 1: Go to closed position and read feedback
  Serial.println(F("\nStep 1: Reading CLOSED position feedback..."));
  servo.write(SERVO_CLOSED_DEG);
  delay(1000);  // Wait for servo to settle
  g_minFeedback = readServoFeedback01();
  Serial.print(F("  CLOSED ("));
  Serial.print(SERVO_CLOSED_DEG);
  Serial.print(F("deg): feedback = "));
  Serial.println(g_minFeedback, 4);

  // Step 2: Move to open position and read feedback
  Serial.println(F("\nStep 2: Moving to OPEN position..."));
  servo.write(SERVO_OPEN_DEG);
  delay(1000);  // Wait for servo to settle
  g_maxFeedback = readServoFeedback01();
  Serial.print(F("  OPEN ("));
  Serial.print(SERVO_OPEN_DEG);
  Serial.print(F("deg): feedback = "));
  Serial.println(g_maxFeedback, 4);

  // Verify the mapping makes sense
  if (g_maxFeedback <= g_minFeedback + 0.05f) {
    Serial.println(F("WARNING: Feedback range too small!"));
  }

  // Print the mapping
  Serial.println(F("\nCalibration complete:"));
  Serial.print(F("  "));
  Serial.print(SERVO_CLOSED_DEG);
  Serial.print(F("deg (closed) = "));
  Serial.print(g_minFeedback, 4);
  Serial.print(F(" feedback\n  "));
  Serial.print(SERVO_OPEN_DEG);
  Serial.print(F("deg (open)   = "));
  Serial.print(g_maxFeedback, 4);
  Serial.println(F(" feedback"));

  // Return to closed
  Serial.println(F("\nReturning to CLOSED..."));
  servo.write(SERVO_CLOSED_DEG);
  delay(500);

  servo.detach();
  Serial.println(F("========================================\n"));
}

// Measure RMS current over a time period using Welford's online algorithm
float measureIrmsOver(uint32_t ms) {
  uint32_t t0 = millis();
  double mean = 0.0, M2 = 0.0;
  uint32_t n = 0;

  while ((millis() - t0) < ms) {
    float v = readVolts(ADS_CH_CURRENT) - g_voltageOffset;  // Apply offset correction
    n++;
    double d  = v - mean;
    mean     += d / n;
    double d2 = v - mean;
    M2       += d * d2;
  }

  if (n < 2) return 0.0f;

  double var = M2 / (n - 1);
  double vrms_ac = sqrt(fmax(0.0, var));

  return (float)(vrms_ac / SENS_V_PER_A);
}

// Calculate voltage offset by averaging readings over a period
float calculateOffset(uint32_t ms) {
  uint32_t t0 = millis();
  double sum = 0.0;
  uint32_t n = 0;

  Serial.println(F("Measuring voltage offset (ensure no load on ACS758)..."));

  while ((millis() - t0) < ms) {
    float v = readVolts(ADS_CH_CURRENT);
    sum += v;
    n++;

    // Flash LED during offset measurement
    if (millis() - g_lastLedToggle >= LED_FLASH_INTERVAL) {
      g_ledState = !g_ledState;
      digitalWrite(LED_PIN, g_ledState);
      g_lastLedToggle = millis();
    }

    // Print progress every 500ms
    if (n % 50 == 0) {
      Serial.print(F("."));
    }
  }

  Serial.println();

  if (n == 0) return 0.0f;

  float offset = (float)(sum / n);
  Serial.print(F("Offset calculated: "));
  Serial.print(offset, 6);
  Serial.print(F("V from "));
  Serial.print(n);
  Serial.println(F(" samples"));

  return offset;
}

// Add sample to rolling average buffer
void addSampleToRollingAvg(float current) {
  g_currentSamples[g_sampleIndex] = current;
  g_sampleIndex = (g_sampleIndex + 1) % MAX_SAMPLES;
  if (g_sampleCount < MAX_SAMPLES) {
    g_sampleCount++;
  }
}

// Calculate rolling average from buffer
float getRollingAverage() {
  if (g_sampleCount == 0) return 0.0f;

  double sum = 0.0;
  for (uint8_t i = 0; i < g_sampleCount; i++) {
    sum += g_currentSamples[i];
  }

  return (float)(sum / g_sampleCount);
}

// Move servo to position (degrees) and detach
void moveServo(int degrees) {
  if (!servo.attached()) {
    servo.attach(SERVO_PWM_PIN);
    delay(50);
  }

  servo.write(degrees);
  Serial.print(F("Servo moved to "));
  Serial.print(degrees);
  Serial.println(F(" degrees"));

  delay(500);  // Allow servo to reach position
  servo.detach();
  Serial.println(F("Servo detached"));
}

// Close vent
void closeVent() {
  Serial.println(F("CLOSING VENT"));
  moveServo(SERVO_CLOSED_DEG);
}

// Open vent
void openVent() {
  Serial.println(F("OPENING VENT"));
  moveServo(SERVO_OPEN_DEG);
}

// Recalibrate servo using Adafruit method
// Sweeps to find min/max positions, then records feedback at each
void calibrateFromCurrentPosition() {
  Serial.println(F("\n========================================"));
  Serial.println(F("RECALIBRATION (Adafruit method)"));
  Serial.println(F("========================================"));

  float currentFb = readServoFeedback01();
  Serial.print(F("Current feedback: "));
  Serial.print(currentFb, 4);
  Serial.print(F(" (estimated "));
  Serial.print(feedbackToDeg(currentFb));
  Serial.println(F(" degrees)"));

  if (!servo.attached()) {
    servo.attach(SERVO_PWM_PIN);
    delay(50);
  }

  // Use fixed servo positions (no slow sweep to avoid sticking)
  Serial.println(F("\nUsing fixed servo positions..."));

  // Physical servo is reversed, so we use swapped values
  SERVO_CLOSED_DEG = 180;  // Max position for closed
  SERVO_OPEN_DEG = 0;      // Min position for open

  Serial.println(F("  CLOSED position: 180 deg"));
  Serial.println(F("  OPEN position: 0 deg"));

  // Calibrate feedback at each position
  Serial.println(F("\nCalibrating feedback..."));

  servo.write(SERVO_CLOSED_DEG);
  delay(1000);
  g_minFeedback = readServoFeedback01();
  Serial.print(F("  CLOSED ("));
  Serial.print(SERVO_CLOSED_DEG);
  Serial.print(F("deg): "));
  Serial.println(g_minFeedback, 4);

  servo.write(SERVO_OPEN_DEG);
  delay(1000);
  g_maxFeedback = readServoFeedback01();
  Serial.print(F("  OPEN ("));
  Serial.print(SERVO_OPEN_DEG);
  Serial.print(F("deg): "));
  Serial.println(g_maxFeedback, 4);

  // Return to closed
  Serial.println(F("\n========================================"));
  Serial.print(F("CLOSED: "));
  Serial.print(SERVO_CLOSED_DEG);
  Serial.print(F("deg = "));
  Serial.println(g_minFeedback, 4);
  Serial.print(F("OPEN:   "));
  Serial.print(SERVO_OPEN_DEG);
  Serial.print(F("deg = "));
  Serial.println(g_maxFeedback, 4);
  Serial.println(F("========================================\n"));

  servo.write(SERVO_CLOSED_DEG);
  delay(500);
  servo.detach();
}

// Run full calibration (servo + current offset)
// Called at startup and when button is pressed
void runFullCalibration() {
  g_calibrated = false;  // Mark as not calibrated
  setLEDPattern(LED_SLOW_FLASH);  // Flash LED during calibration

  Serial.println(F("\n========================================"));
  Serial.println(F("FULL CALIBRATION STARTING"));
  Serial.println(F("========================================"));

  // === SERVO CALIBRATION ===
  Serial.println(F("\n--- SERVO CALIBRATION ---"));

  g_minFeedback = readServoFeedback01();
  Serial.print(F("Startup feedback (CLOSED): "));
  Serial.println(g_minFeedback, 4);

  servo.attach(SERVO_PWM_PIN);
  delayWithFlashing(100);

  // Use fixed servo positions (no slow sweep to avoid sticking)
  Serial.println(F("\nUsing fixed servo positions..."));

  // Physical servo is reversed, so we use swapped values
  SERVO_CLOSED_DEG = 180;  // Max position for closed
  SERVO_OPEN_DEG = 0;      // Min position for open

  Serial.println(F("  CLOSED position: 180 deg"));
  Serial.println(F("  OPEN position: 0 deg"));

  // Calibrate feedback at each position
  Serial.println(F("\nCalibrating feedback at each position..."));

  servo.write(SERVO_CLOSED_DEG);
  delayWithFlashing(1000);
  g_minFeedback = readServoFeedback01();
  Serial.print(F("  CLOSED ("));
  Serial.print(SERVO_CLOSED_DEG);
  Serial.print(F("deg): "));
  Serial.println(g_minFeedback, 4);

  servo.write(SERVO_OPEN_DEG);
  delayWithFlashing(1000);
  g_maxFeedback = readServoFeedback01();
  Serial.print(F("  OPEN ("));
  Serial.print(SERVO_OPEN_DEG);
  Serial.print(F("deg): "));
  Serial.println(g_maxFeedback, 4);

  // Print calibration summary
  Serial.println(F("\nSERVO CALIBRATION COMPLETE"));
  Serial.print(F("  CLOSED: "));
  Serial.print(SERVO_CLOSED_DEG);
  Serial.print(F("deg = "));
  Serial.print(g_minFeedback, 4);
  Serial.println(F(" feedback"));
  Serial.print(F("  OPEN:   "));
  Serial.print(SERVO_OPEN_DEG);
  Serial.print(F("deg = "));
  Serial.print(g_maxFeedback, 4);
  Serial.println(F(" feedback"));

  // Return to closed position
  servo.write(SERVO_CLOSED_DEG);
  delayWithFlashing(500);
  servo.detach();

  // === CURRENT OFFSET CALIBRATION ===
  Serial.println(F("\n--- CURRENT OFFSET CALIBRATION ---"));
  Serial.println(F("IMPORTANT: Ensure dryer is OFF"));
  delayWithFlashing(1000);

  g_voltageOffset = calculateOffset(OFFSET_MEASURE_MS);

  // Clear rolling average buffer to remove any stale data
  g_sampleIndex = 0;
  g_sampleCount = 0;
  for (uint8_t i = 0; i < MAX_SAMPLES; i++) {
    g_currentSamples[i] = 0.0f;
  }

  // Mark calibration complete - LED stays solid, vent is closed
  g_calibrated = true;
  g_ventOpen = false;
  g_ventClosePending = false;
  g_heaterWasOn = false;  // Reset heater state tracking
  g_systemOn = true;

  // Reset all device state detection flags (assume everything OFF after calibration)
  g_fanOn = false;
  g_heaterOn = false;
  g_systemActive = false;

  setLEDPattern(LED_SOLID);  // Set LED to solid on after calibration

  // Allow sensor to settle after calibration
  Serial.println(F("\nAllowing sensor to settle (3 seconds)..."));
  delay(3000);

  Serial.println(F("\n========================================"));
  Serial.println(F("CALIBRATION COMPLETE - System Ready"));
  Serial.println(F("========================================\n"));
}

// Enter standby mode - close vent, turn off LED, stop monitoring
void enterStandbyMode() {
  Serial.println(F("\n========================================"));
  Serial.println(F("ENTERING STANDBY MODE"));
  Serial.println(F("========================================"));

  // Close the vent
  closeVent();
  g_ventOpen = false;
  g_ventClosePending = false;

  // Turn off LED
  setLEDPattern(LED_OFF);

  // Set system off
  g_systemOn = false;

  Serial.println(F("System is now in STANDBY"));
  Serial.println(F("Long press button to wake up"));
  Serial.println(F("========================================\n"));
}

// Load thresholds from EEPROM
bool loadThresholdsFromEEPROM() {
  ThresholdData data;

  #ifdef BOARD_SAMD21
    data = thresholdStorage.read();
  #else  // RP2040
    EEPROM.begin(512);  // Initialize EEPROM with 512 bytes
    EEPROM.get(0, data);
  #endif

  if (data.magic == EEPROM_MAGIC) {
    FAN_ON_THRESHOLD = data.fanOnThreshold;
    FAN_OFF_THRESHOLD = data.fanOffThreshold;
    HEATER_ON_THRESHOLD = data.heaterOnThreshold;
    HEATER_OFF_THRESHOLD = data.heaterOffThreshold;

    Serial.println(F("\n=== Loaded thresholds from EEPROM ==="));
    Serial.print(F("  Fan ON:  ")); Serial.print(FAN_ON_THRESHOLD, 4); Serial.println(F("A"));
    Serial.print(F("  Fan OFF: ")); Serial.print(FAN_OFF_THRESHOLD, 4); Serial.println(F("A"));
    Serial.print(F("  Heat ON: ")); Serial.print(HEATER_ON_THRESHOLD, 4); Serial.println(F("A"));
    Serial.print(F("  Heat OFF:")); Serial.print(HEATER_OFF_THRESHOLD, 4); Serial.println(F("A"));
    Serial.println(F("=======================================\n"));
    return true;
  }

  Serial.println(F("No learned thresholds found, using defaults"));
  return false;
}

// Save thresholds to EEPROM
void saveThresholdsToEEPROM() {
  ThresholdData data;
  data.magic = EEPROM_MAGIC;
  data.fanOnThreshold = FAN_ON_THRESHOLD;
  data.fanOffThreshold = FAN_OFF_THRESHOLD;
  data.heaterOnThreshold = HEATER_ON_THRESHOLD;
  data.heaterOffThreshold = HEATER_OFF_THRESHOLD;

  #ifdef BOARD_SAMD21
    thresholdStorage.write(data);
  #else  // RP2040
    EEPROM.put(0, data);
    EEPROM.commit();  // RP2040 requires explicit commit
  #endif

  Serial.println(F("\n=== Saved thresholds to EEPROM ==="));
  Serial.print(F("  Fan ON:  ")); Serial.print(FAN_ON_THRESHOLD, 4); Serial.println(F("A"));
  Serial.print(F("  Fan OFF: ")); Serial.print(FAN_OFF_THRESHOLD, 4); Serial.println(F("A"));
  Serial.print(F("  Heat ON: ")); Serial.print(HEATER_ON_THRESHOLD, 4); Serial.println(F("A"));
  Serial.print(F("  Heat OFF:")); Serial.print(HEATER_OFF_THRESHOLD, 4); Serial.println(F("A"));
  Serial.println(F("===================================\n"));
}

// Start learning mode
void startLearningMode() {
  Serial.println(F("\n========================================"));
  Serial.println(F("ENTERING LEARNING MODE"));
  Serial.println(F("========================================"));
  Serial.println(F("This will learn optimal thresholds"));
  Serial.println(F(""));
  Serial.println(F("LED patterns:"));
  Serial.println(F("  FLASHING = Measuring now (don't touch)"));
  Serial.println(F("  SOLID = Waiting - make change, press button"));
  Serial.println(F("  3 LONG BLINKS = All done!"));
  Serial.println(F(""));
  Serial.println(F("PHASE 1: Ensure dryer is completely OFF"));
  Serial.println(F("Press button when ready..."));
  Serial.println(F("========================================\n"));

  g_learningPhase = LEARN_BASELINE;
  g_learnedBaseline = 0.0f;
  g_learnedHeater = 0.0f;
  g_learnedFanOnly = 0.0f;
  g_learningStartMs = 0;  // Not measuring yet
  g_learningPhaseEntryMs = millis();

  // Start with solid LED - waiting for user to press button
  setLEDPattern(LED_SOLID);
}

// Process learning phase
void processLearningPhase() {
  uint32_t now = millis();

  // Check if we're in learning mode
  if (g_learningPhase != LEARN_NONE) {
    // Update LED pattern
    updateLEDPattern();

    // Check if measurement period started
    if (g_learningStartMs > 0) {
      if (now - g_learningStartMs >= LEARNING_MEASURE_MS) {
        // Measurement complete - stop measuring and wait for user
        g_learningStartMs = 0;

        // Move to next phase
        switch (g_learningPhase) {
          case LEARN_BASELINE:
            Serial.print(F("Baseline learned: "));
            Serial.print(g_learnedBaseline, 4);
            Serial.println(F("A\n"));

            g_learningPhase = LEARN_HEATER;
            setLEDPattern(LED_SOLID);
            Serial.println(F("PHASE 2: Turn dryer ON (heater + fan)"));
            Serial.println(F("Press button when ready..."));
            break;

          case LEARN_HEATER:
            Serial.print(F("Heater+Fan learned: "));
            Serial.print(g_learnedHeater, 4);
            Serial.println(F("A\n"));

            g_learningPhase = LEARN_FAN_ONLY;
            setLEDPattern(LED_SOLID);
            Serial.println(F("PHASE 3: Turn heater OFF (fan only)"));
            Serial.println(F("Press button when ready..."));
            break;

          case LEARN_FAN_ONLY:
            Serial.print(F("Fan only learned: "));
            Serial.print(g_learnedFanOnly, 4);
            Serial.println(F("A\n"));

            // Calculate optimal thresholds
            Serial.println(F("Calculating thresholds..."));

            // Fan threshold = halfway between baseline and fan-only
            FAN_ON_THRESHOLD = (g_learnedBaseline + g_learnedFanOnly) / 2.0f + 0.002f;  // Add 2mA margin
            FAN_OFF_THRESHOLD = FAN_ON_THRESHOLD - 0.005f;  // 5mA hysteresis

            // Heater threshold = halfway between fan-only and heater
            HEATER_ON_THRESHOLD = (g_learnedFanOnly + g_learnedHeater) / 2.0f;
            HEATER_OFF_THRESHOLD = (g_learnedBaseline + g_learnedFanOnly) / 2.0f + 0.005f;

            // Save to EEPROM
            saveThresholdsToEEPROM();

            // Show completion blink
            g_learningPhase = LEARN_COMPLETE;
            setLEDPattern(LED_COMPLETE_BLINK);
            Serial.println(F("========================================"));
            Serial.println(F("LEARNING COMPLETE!"));
            Serial.println(F("Thresholds saved to EEPROM"));
            Serial.println(F("========================================\n"));

            delay(3500);  // Wait for complete blink

            // Return to normal operation
            g_learningPhase = LEARN_NONE;
            setLEDPattern(LED_SOLID);
            break;

          default:
            break;
        }
      } else {
        // Still measuring - accumulate current reading
        float Irms = measureIrmsOver(SAMPLE_MS);

        switch (g_learningPhase) {
          case LEARN_BASELINE:
            // Running average
            if (g_learnedBaseline == 0.0f) {
              g_learnedBaseline = Irms;
            } else {
              g_learnedBaseline = g_learnedBaseline * 0.9f + Irms * 0.1f;
            }
            break;

          case LEARN_HEATER:
            // Take maximum reading (heater may cycle)
            if (Irms > g_learnedHeater) {
              g_learnedHeater = Irms;
            }
            break;

          case LEARN_FAN_ONLY:
            // Running average
            if (g_learnedFanOnly == 0.0f) {
              g_learnedFanOnly = Irms;
            } else {
              g_learnedFanOnly = g_learnedFanOnly * 0.9f + Irms * 0.1f;
            }
            break;

          default:
            break;
        }

        // Print progress every second
        static uint32_t lastProgress = 0;
        if (now - lastProgress >= 1000) {
          uint32_t remaining = (LEARNING_MEASURE_MS - (now - g_learningStartMs)) / 1000;
          Serial.print(F("  Measuring... "));
          Serial.print(remaining);
          Serial.print(F("s remaining ("));
          Serial.print(Irms, 4);
          Serial.println(F("A)"));
          lastProgress = now;
        }
      }
    }
  }
}

// Handle learning mode button press
void handleLearningButtonPress() {
  if (g_learningPhase != LEARN_NONE && g_learningPhase != LEARN_COMPLETE) {
    if (g_learningStartMs == 0) {
      // Start measurement - switch to flashing LED
      Serial.println(F("Starting measurement..."));
      g_learningStartMs = millis();
      setLEDPattern(LED_FAST_FLASH);
    }
  }
}

// ----------------- Setup -----------------
void setup() {
  // Initialize LED and button pins first
  #ifdef BOARD_RP2040
    // CRITICAL: RP2040 requires explicit GPIO function setup
    // GPIO3 (D10) defaults to SPI MOSI, must set to GPIO mode
    // GPIO26 (D0) needs PWM function for servo
    // GPIO29 (D3) needs GPIO mode for button
    gpio_set_function(GPIO_LED, GPIO_FUNC_SIO);    // D10/GPIO3 to GPIO mode
    gpio_set_function(GPIO_SERVO, GPIO_FUNC_PWM);  // D0/GPIO26 to PWM mode
    gpio_set_function(GPIO_BTN, GPIO_FUNC_SIO);    // D3/GPIO29 to GPIO mode

    // Set maximum drive strength for LED (makes it brighter)
    gpio_set_drive_strength(GPIO_LED, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_slew_rate(GPIO_LED, GPIO_SLEW_RATE_FAST);
  #endif

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);  // LED off initially

  #ifdef BOARD_RP2040
    // Initialize NeoPixel power and breathing effect
    pinMode(NEOPIXEL_POWER_PIN, OUTPUT);
    digitalWrite(NEOPIXEL_POWER_PIN, HIGH);  // Power on the NeoPixel
    neopixel.begin();
    neopixel.setBrightness(255);  // Full brightness control in code
    neopixel.show(); // Initialize all pixels to 'off'
    g_breathingStartMs = millis();
  #else
    digitalWrite(LED_BUILTIN, LOW);  // LED off Initially (SAMD21)
  #endif

  Serial.begin(115200);

  // Wait for Serial with timeout
  uint32_t serialTimeout = millis();
  while (!Serial && (millis() - serialTimeout < 3000)) { }

  Serial.println(F("\n\n========================================"));
  Serial.println(F("SUNLU AMS Heater Auto-Vent Controller"));
  Serial.print(F("Board: Seeed XIAO "));
  Serial.println(F(MY_BOARD_NAME));
  Serial.println(F("ADS1115 + ACS758 + Servo"));
  Serial.println(F("========================================\n"));

  Serial.println(F("Initializing ADS1115..."));
  if (!ads.begin()) {
    Serial.println(F("ERROR: ADS1115 not found! Check I2C wiring."));
    while(1) {
      delay(1000);
      Serial.println(F("ERROR: ADS1115 not found!"));
    }
  }

  ads.setGain(GAIN_ONE);  // ±4.096V range
  ads.setDataRate(RATE_ADS1115_128SPS);  // 128 samples per second
  Serial.println(F("ADS1115 initialized successfully\n"));

  // Load learned thresholds from EEPROM (if available)
  loadThresholdsFromEEPROM();

  // Run full calibration (servo + current offset)
  runFullCalibration();

  // Print available commands
  Serial.println(F("Serial Commands:"));
  Serial.println(F("  O or o - Open vent"));
  Serial.println(F("  C or c - Close vent"));
  Serial.println(F("  R or r - Recalibrate servo only"));
  Serial.println(F("  F or f - Read current feedback (no movement)"));
  Serial.println(F("  S or s - Toggle standby mode"));
  Serial.println(F("  L or l - Enter learning mode"));
  Serial.println(F("  0-180  - Move servo to specified degrees"));
  Serial.println(F("Button:"));
  Serial.println(F("  Short press       - Full recalibration"));
  Serial.println(F("  Long press 2s     - Toggle standby/wake"));
  Serial.println(F("  Very long press 5s - Enter learning mode"));
  Serial.println(F("========================================\n"));

  g_lastUpdateMs = millis();
  setLEDPattern(LED_SOLID);
}

// ----------------- Loop -----------------
void loop() {
  static bool longPressExecuted = false;  // Declare at function scope
  uint32_t now = millis();

  // Check for button press FIRST (before learning mode processing)
  bool currentButtonState = digitalRead(BUTTON_PIN);

  // Button just pressed - start timing
  if (currentButtonState == LOW && g_lastButtonState == HIGH) {
    g_buttonPressStart = now;
    g_buttonHandled = false;
  }

  // Button is being held - check for very long press first, then long press
  // Skip long/very long press detection if already in learning mode
  if (currentButtonState == LOW && g_learningPhase == LEARN_NONE) {
    uint32_t pressDuration = now - g_buttonPressStart;

    // Check for very long press (>5s) - takes priority, overrides long press
    if (pressDuration >= VLONG_PRESS_MS) {
      if (!g_buttonHandled) {
        g_buttonHandled = true;  // Prevent repeated triggers
        Serial.println(F("\n*** VERY LONG PRESS DETECTED ***"));

        // Cancel standby if it was triggered
        if (!g_systemOn) {
          g_systemOn = true;
          setLEDPattern(LED_FAST_FLASH);
        }

        startLearningMode();
        g_ignoreNextRelease = true;  // Ignore the button release after entering learning mode
      }
    }
    // Check for long press (2s) - but only if very long press hasn't been handled yet
    else if (pressDuration >= LONG_PRESS_MS && !g_buttonHandled) {
      // Don't mark as handled yet - allow very long press to override
      // Just execute the long press action once
      if (!longPressExecuted) {
        longPressExecuted = true;

        if (g_systemOn) {
          // System is ON - enter standby mode
          Serial.println(F("\n*** LONG PRESS DETECTED ***"));
          enterStandbyMode();
        } else {
          // System is OFF - wake up with full calibration
          Serial.println(F("\n*** LONG PRESS DETECTED ***"));
          Serial.println(F("Waking up from standby..."));
          runFullCalibration();
        }
      }
    }
  }

  // Reset long press executed flag when button is released
  if (currentButtonState == HIGH && g_lastButtonState == LOW) {
    longPressExecuted = false;
  }

  // Button just released - check for short press
  if (currentButtonState == HIGH && g_lastButtonState == LOW) {
    // Check if we're in learning mode waiting for button press
    if (g_learningPhase != LEARN_NONE) {
      // Ignore the first button release after entering learning mode
      if (g_ignoreNextRelease) {
        Serial.println(F("Ignoring button release from learning mode entry"));
        g_ignoreNextRelease = false;
      } else {
        Serial.println(F("Button released during learning mode"));
        handleLearningButtonPress();
      }
    } else if (!g_buttonHandled) {
      // Short press detected (released before long press threshold)
      Serial.println(F("\n*** SHORT PRESS DETECTED ***"));
      Serial.println(F("Running recalibration..."));
      runFullCalibration();
    }
  }

  g_lastButtonState = currentButtonState;

  // Handle learning mode processing (if active)
  if (g_learningPhase != LEARN_NONE) {
    processLearningPhase();
    return;  // Skip normal operation when in learning mode
  }

  // Update LED pattern (handles all LED states now)
  updateLEDPattern();

  // Update built-in LED breathing effect (RP2040 only)
  #ifdef BOARD_RP2040
    if (g_systemOn) {  // Only breathe when system is active
      updateBuiltinLEDBreathing();
    } else {
      neopixel.setPixelColor(0, 0);  // Turn off in standby
      neopixel.show();
    }
  #endif

  // Check for serial commands
  while (Serial.available()) {
    char ch = Serial.read();

    // Handle newline - process buffered command
    if (ch == '\n' || ch == '\r') {
      if (g_serialIdx > 0) {
        g_serialBuffer[g_serialIdx] = '\0';  // Null terminate

        // Check if it's a number
        bool isNumber = true;
        for (uint8_t i = 0; i < g_serialIdx; i++) {
          if (!isdigit(g_serialBuffer[i])) {
            isNumber = false;
            break;
          }
        }

        if (isNumber) {
          int degrees = atoi(g_serialBuffer);
          if (degrees >= 0 && degrees <= 180) {
            Serial.print(F("Moving to "));
            Serial.print(degrees);
            Serial.println(F(" degrees..."));
            moveServo(degrees);

            // Read and display feedback at new position
            delay(100);
            float fb = readServoFeedback01();
            Serial.print(F("Feedback at position: "));
            Serial.println(fb, 4);
          } else {
            Serial.println(F("Invalid degrees (use 0-180)"));
          }
        }
        g_serialIdx = 0;  // Reset buffer
      }
    }
    // Handle single character commands immediately
    else if (g_serialIdx == 0 && !isdigit(ch)) {
      if (ch == 'O' || ch == 'o') {
        openVent();
      } else if (ch == 'C' || ch == 'c') {
        closeVent();
      } else if (ch == 'R' || ch == 'r') {
        calibrateFromCurrentPosition();
      } else if (ch == 'F' || ch == 'f') {
        // Just read feedback without moving anything
        float fb = readServoFeedback01();
        Serial.print(F("Current feedback: "));
        Serial.print(fb, 4);
        Serial.print(F(" = "));
        Serial.print(feedbackToDeg(fb));
        Serial.println(F(" degrees (calibrated)"));
      } else if (ch == 'S' || ch == 's') {
        // Toggle standby mode
        if (g_systemOn) {
          enterStandbyMode();
        } else {
          Serial.println(F("\nWaking up from standby..."));
          runFullCalibration();
        }
      } else if (ch == 'L' || ch == 'l') {
        // Enter learning mode
        startLearningMode();
      }
    }
    // Buffer digits for numeric input
    else if (isdigit(ch) && g_serialIdx < sizeof(g_serialBuffer) - 1) {
      g_serialBuffer[g_serialIdx++] = ch;
    }
  }

  // Skip all monitoring when in standby mode
  if (!g_systemOn) {
    delay(100);
    return;
  }

  // Only update and display every 5 seconds
  if (now - g_lastUpdateMs >= 5000) {
    g_lastUpdateMs = now;

    // Measure current
    float Irms = measureIrmsOver(SAMPLE_MS);

    // Add to rolling average buffer
    addSampleToRollingAvg(Irms);

    // Calculate rolling average
    float rollingAvgIrms = getRollingAverage();

    // Determine fan state (informational only - may not be accurate)
    // Uses global g_fanOn variable with hysteresis
    if (Irms >= FAN_ON_THRESHOLD) {
      g_fanOn = true;
    } else if (Irms <= FAN_OFF_THRESHOLD) {
      g_fanOn = false;
    }

    // Determine heater state (informational only - may not distinguish low heat from fan)
    // Uses global g_heaterOn variable with hysteresis
    if (Irms >= HEATER_ON_THRESHOLD) {
      g_heaterOn = true;
    } else if (Irms <= HEATER_OFF_THRESHOLD) {
      g_heaterOn = false;
    }

    // Simple activity detection (use this for vent control)
    // Uses global g_systemActive variable with hysteresis
    if (Irms >= ACTIVITY_THRESHOLD) {
      g_systemActive = true;
    } else if (Irms <= IDLE_THRESHOLD) {
      g_systemActive = false;
    }

    // Vent control logic based on heater and fan state
    // Debug: show state before decision
    Serial.print(F("DEBUG: heaterOn="));
    Serial.print(g_heaterOn);
    Serial.print(F(" fanOn="));
    Serial.print(g_fanOn);
    Serial.print(F(" g_ventOpen="));
    Serial.print(g_ventOpen);
    Serial.print(F(" Irms="));
    Serial.println(Irms, 4);

    if (g_heaterOn && !g_ventOpen) {
      // Heater is ON and vent is closed - open it, cancel any pending close
      g_ventClosePending = false;
      Serial.println(F(">>> HEATER ON - Opening vent"));
      openVent();
      g_ventOpen = true;
    } else if (!g_heaterOn && !g_fanOn && g_ventOpen) {
      // Both heater AND fan are OFF - close immediately
      Serial.println(F(">>> HEATER & FAN OFF - Closing vent immediately"));
      closeVent();
      g_ventOpen = false;
      g_ventClosePending = false;
    } else if (!g_heaterOn && g_heaterWasOn && g_fanOn && g_ventOpen) {
      // Heater just turned OFF but fan still running - start delay timer
      g_ventCloseDelayStart = now;
      g_ventClosePending = true;
      Serial.println(F(">>> HEATER OFF, FAN ON - Vent close in 3min"));
    }
    g_heaterWasOn = g_heaterOn;

    // Check if pending close delay has expired (fan still running case)
    if (g_ventClosePending && !g_heaterOn && g_fanOn) {
      uint32_t elapsed = now - g_ventCloseDelayStart;
      if (elapsed >= VENT_CLOSE_DELAY_MS) {
        Serial.println(F("DELAY EXPIRED - Closing vent"));
        closeVent();
        g_ventOpen = false;
        g_ventClosePending = false;
      }
    }

    // Display debug information - single condensed line
    float rawV = readVolts(ADS_CH_CURRENT);
    Serial.print(now / 1000);
    Serial.print(F("s | Offset: "));
    Serial.print(g_voltageOffset, 4);
    Serial.print(F("V | Raw: "));
    Serial.print(rawV, 4);
    Serial.print(F("V | Irms: "));
    Serial.print(Irms, 4);
    Serial.print(F("A | Avg: "));
    Serial.print(rollingAvgIrms, 4);
    Serial.print(F("A ("));
    Serial.print(g_sampleCount);
    Serial.print(F("/"));
    Serial.print(MAX_SAMPLES);
    Serial.print(F(") | Fan: "));
    Serial.print(g_fanOn ? F("ON ") : F("OFF"));
    Serial.print(F(" | Heat: "));
    Serial.print(g_heaterOn ? F("ON ") : F("OFF"));
    Serial.print(F(" | Vent: "));
    Serial.print(g_ventOpen ? F("OPEN") : F("CLOSED"));
    if (g_ventClosePending) {
      uint32_t remaining = (VENT_CLOSE_DELAY_MS - (now - g_ventCloseDelayStart)) / 1000;
      Serial.print(F(" ("));
      Serial.print(remaining);
      Serial.print(F("s)"));
    }
    Serial.println();
  }

  // Small delay to prevent tight looping
  delay(100);
}