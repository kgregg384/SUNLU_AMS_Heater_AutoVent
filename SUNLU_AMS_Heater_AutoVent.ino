// debug.ino - Enhanced ACS758 current sensor debugging with servo control
// Displays rolling 30-second average, offset-corrected readings, and device state
// Includes serial commands for manual vent control
//
// Board: Seeed XIAO SAMD21 (3.3V)
// Peripherals: ADS1115 (I2C), ACS758 connected to ADS channel 0, Servo on pin 8
  
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <Servo.h>
#include <math.h>

// ----------------- Constants -----------------
#define ADS_CH_CURRENT   0   // ACS758 -> ADS A0
#define ADS_CH_SERVO_FB  1   // Servo feedback -> ADS A1
#define SERVO_PWM_PIN    0   // Servo PWM output
#define LED_PIN         10   // Status LED (connected to ground)
#define BUTTON_PIN       3   // Calibration button (momentary, active LOW)

static const float ADS_LSB_V     = 0.000125f;   // ADS1115 @ GAIN_ONE (±4.096V range)
static const float SENS_V_PER_A  = 0.040f;      // ACS758 LCB-050B @ 5V ≈ 40 mV/A
static const uint32_t SAMPLE_MS  = 500;         // Sample for 500ms (~30 AC cycles @ 60Hz)
static const uint32_t OFFSET_MEASURE_MS = 5000; // 5 seconds to measure offset at startup
static const uint32_t ROLLING_AVG_MS = 30000;   // 30 second rolling average window
static const uint8_t MAX_SAMPLES = 60;          // Store 60 samples (30 seconds at 0.5s per sample)

// Detection thresholds - CALIBRATE THESE AFTER MEASURING YOUR SYSTEM!
// Run this debug program through a full dryer cycle to see actual current values
// Then adjust these thresholds to match your observations
//
// Note: ACS758 has ~50-75mA noise floor at low currents. Thresholds must be above this.
// For vent control, you likely only need "ACTIVITY" detection vs "IDLE"
// since low heat may not be distinguishable from fan-only operation.
static const float HEATER_ON_THRESHOLD  = 0.200f;  // Above this = heater on (200mA)
static const float HEATER_OFF_THRESHOLD = 0.120f;  // Below this = heater off (120mA)
static const float FAN_ON_THRESHOLD     = 0.120f;  // Above this = fan on (120mA, well above noise)
static const float FAN_OFF_THRESHOLD    = 0.080f;  // Below this = fan off (80mA, with hysteresis)

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

// Button state for edge detection and long press
bool g_lastButtonState = HIGH;
uint32_t g_buttonPressStart = 0;
bool g_buttonHandled = false;
static const uint32_t LONG_PRESS_MS = 2000;  // 2 second long press

// System power state (standby mode)
bool g_systemOn = true;

// ----------------- Helper Functions -----------------

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

  // Sweep to find min/max feedback positions
  Serial.println(F("\nSweeping to find servo range..."));

  float minFb = 1.0f, maxFb = 0.0f;
  int minFbDeg = 0, maxFbDeg = 180;

  for (int deg = 0; deg <= 180; deg += 5) {
    servo.write(deg);
    delay(50);
    float fb = readServoFeedback01();

    if (deg % 30 == 0) {
      Serial.print(F("  "));
      Serial.print(deg);
      Serial.print(F("deg = "));
      Serial.println(fb, 4);
    }

    if (fb < minFb) { minFb = fb; minFbDeg = deg; }
    if (fb > maxFb) { maxFb = fb; maxFbDeg = deg; }
  }

  SERVO_CLOSED_DEG = minFbDeg;
  SERVO_OPEN_DEG = maxFbDeg;

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
  g_calibrated = false;  // Start flashing LED
  digitalWrite(LED_BUILTIN, LOW);

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

  // Sweep from 0 to 180 degrees to find working range
  Serial.println(F("\nSweeping to find servo range..."));

  float minFb = 1.0f, maxFb = 0.0f;
  int minFbDeg = 0, maxFbDeg = 180;

  for (int deg = 0; deg <= 180; deg += 5) {
    servo.write(deg);
    delayWithFlashing(50);
    float fb = readServoFeedback01();

    if (deg % 30 == 0) {
      Serial.print(F("  "));
      Serial.print(deg);
      Serial.print(F("deg = "));
      Serial.println(fb, 4);
    }

    if (fb < minFb) { minFb = fb; minFbDeg = deg; }
    if (fb > maxFb) { maxFb = fb; maxFbDeg = deg; }
  }

  SERVO_CLOSED_DEG = minFbDeg;
  SERVO_OPEN_DEG = maxFbDeg;

  Serial.println(F("\nSweep results:"));
  Serial.print(F("  Min feedback "));
  Serial.print(minFb, 4);
  Serial.print(F(" at "));
  Serial.print(minFbDeg);
  Serial.println(F(" deg"));
  Serial.print(F("  Max feedback "));
  Serial.print(maxFb, 4);
  Serial.print(F(" at "));
  Serial.print(maxFbDeg);
  Serial.println(F(" deg"));

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

  digitalWrite(LED_PIN, HIGH);
  digitalWrite(LED_BUILTIN, HIGH);

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
  digitalWrite(LED_PIN, LOW);
  digitalWrite(LED_BUILTIN, LOW);
  g_ledState = false;

  // Set system off
  g_systemOn = false;

  Serial.println(F("System is now in STANDBY"));
  Serial.println(F("Long press button to wake up"));
  Serial.println(F("========================================\n"));
}

// ----------------- Setup -----------------
void setup() {
  // Initialize LED and button pins first
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);  // LED off initially
  digitalWrite(LED_BUILTIN, LOW);  // LED off Initially

  Serial.begin(115200);

  // Wait for Serial with timeout
  uint32_t serialTimeout = millis();
  while (!Serial && (millis() - serialTimeout < 3000)) { }

  Serial.println(F("\n\n========================================"));
  Serial.println(F("ACS758 Current Sensor Debug - Enhanced"));
  Serial.println(F("Seeed XIAO SAMD21 + ADS1115 + Servo"));
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

  // Run full calibration (servo + current offset)
  runFullCalibration();

  // Print available commands
  Serial.println(F("Serial Commands:"));
  Serial.println(F("  O or o - Open vent"));
  Serial.println(F("  C or c - Close vent"));
  Serial.println(F("  R or r - Recalibrate servo only"));
  Serial.println(F("  F or f - Read current feedback (no movement)"));
  Serial.println(F("  S or s - Toggle standby mode"));
  Serial.println(F("  0-180  - Move servo to specified degrees"));
  Serial.println(F("Button:"));
  Serial.println(F("  Short press   - Full recalibration"));
  Serial.println(F("  Long press 2s - Toggle standby/wake"));
  Serial.println(F("========================================\n"));

  g_lastUpdateMs = millis();
}

// ----------------- Loop -----------------
void loop() {
  uint32_t now = millis();

  // Flash LED if not calibrated
  if (!g_calibrated) {
    if (now - g_lastLedToggle >= LED_FLASH_INTERVAL) {
      g_ledState = !g_ledState;
      digitalWrite(LED_PIN, g_ledState);
      g_lastLedToggle = now;
    }
  }

  // Check for button press (short = recalibrate, long = toggle standby)
  bool currentButtonState = digitalRead(BUTTON_PIN);

  // Button just pressed - start timing
  if (currentButtonState == LOW && g_lastButtonState == HIGH) {
    g_buttonPressStart = now;
    g_buttonHandled = false;
  }

  // Button is being held - check for long press
  if (currentButtonState == LOW && !g_buttonHandled) {
    if (now - g_buttonPressStart >= LONG_PRESS_MS) {
      g_buttonHandled = true;  // Prevent repeated triggers

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

  // Button just released - check for short press
  if (currentButtonState == HIGH && g_lastButtonState == LOW && !g_buttonHandled) {
    // Short press detected (released before long press threshold)
    Serial.println(F("\n*** SHORT PRESS DETECTED ***"));
    Serial.println(F("Running recalibration..."));
    runFullCalibration();
  }

  g_lastButtonState = currentButtonState;

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
