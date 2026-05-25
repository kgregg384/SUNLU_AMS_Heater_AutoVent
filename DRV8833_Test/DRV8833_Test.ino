/*
 * DRV8833 Isolated Bench Test for XIAO RP2040 / SAMD21
 *
 * Standalone sketch with no auto-vent, servo, ADS1115, or learning-mode
 * code. Drives only the DRV8833 actuator + fan pins. Use this to verify
 * the H-bridge wiring before flashing the full firmware.
 *
 * Wiring (same as full firmware):
 *   XIAO D1  -> DRV8833 IN1  (actuator extend)
 *   XIAO D2  -> DRV8833 IN2  (actuator retract)
 *   XIAO D8  -> DRV8833 IN3  (fan)
 *   DRV8833 IN4 -> GND (jumper, forces fan H-bridge to one direction)
 *   DRV8833 VCC -> 5V supply  | DRV8833 GND -> common GND
 *   DRV8833 EEP -> VCC (jumper) so the chip isn't asleep
 *
 * Serial commands (115200 baud):
 *   A   Extend actuator(s) for ACTUATOR_TRAVEL_MS
 *   Z   Retract actuator(s) for ACTUATOR_TRAVEL_MS
 *   X   Coast actuators immediately
 *   B   Brake actuators (both inputs HIGH)
 *   V   Toggle fan
 *   P   Pulse retract for 250 ms (kick a stuck motor past stiction)
 *   C   Auto-cycle: extend -> pause -> retract -> pause, repeating
 *   .   Stop auto-cycle
 *   ?   Print pin state snapshot
 */

#include <Arduino.h>

#ifdef ARDUINO_ARCH_RP2040
  #include <hardware/gpio.h>
  #define DRV_AIN1_PIN  D1
  #define DRV_AIN2_PIN  D2
  #define DRV_FAN_PIN   D8
  #define GPIO_AIN1     0   // D1 = GPIO0
  #define GPIO_AIN2     1   // D2 = GPIO1
  #define GPIO_FAN      2   // D8 = GPIO2
#else
  #define DRV_AIN1_PIN  1
  #define DRV_AIN2_PIN  2
  #define DRV_FAN_PIN   8
#endif

static const uint32_t ACTUATOR_TRAVEL_MS = 8000;
static const uint32_t CYCLE_PAUSE_MS     = 2000;

uint32_t g_actuatorStartMs = 0;
bool     g_actuatorMoving  = false;
bool     g_fanRunning      = false;

// Auto-cycle state machine
enum CyclePhase { CYCLE_OFF, CYCLE_EXTEND, CYCLE_PAUSE_A, CYCLE_RETRACT, CYCLE_PAUSE_B };
CyclePhase g_cyclePhase = CYCLE_OFF;
uint32_t   g_cyclePhaseStart = 0;

static void printPinSnapshot() {
  Serial.print(F("D1(IN1)="));
  Serial.print(digitalRead(DRV_AIN1_PIN));
  Serial.print(F(" D2(IN2)="));
  Serial.print(digitalRead(DRV_AIN2_PIN));
  Serial.print(F(" D8(IN3/fan)="));
  Serial.print(digitalRead(DRV_FAN_PIN));
  Serial.print(F(" moving="));
  Serial.print(g_actuatorMoving);
  Serial.print(F(" fan="));
  Serial.print(g_fanRunning);
  Serial.print(F(" cycle="));
  Serial.println((int)g_cyclePhase);
}

static void extendActuators() {
  digitalWrite(DRV_AIN1_PIN, HIGH);
  digitalWrite(DRV_AIN2_PIN, LOW);
  g_actuatorStartMs = millis();
  g_actuatorMoving = true;
  Serial.println(F("EXTEND  (D1=HIGH D2=LOW)"));
}

static void retractActuators() {
  digitalWrite(DRV_AIN1_PIN, LOW);
  digitalWrite(DRV_AIN2_PIN, HIGH);
  g_actuatorStartMs = millis();
  g_actuatorMoving = true;
  Serial.println(F("RETRACT (D1=LOW D2=HIGH)"));
}

static void coastActuators() {
  digitalWrite(DRV_AIN1_PIN, LOW);
  digitalWrite(DRV_AIN2_PIN, LOW);
  g_actuatorMoving = false;
  Serial.println(F("COAST   (D1=LOW D2=LOW)"));
}

static void brakeActuators() {
  digitalWrite(DRV_AIN1_PIN, HIGH);
  digitalWrite(DRV_AIN2_PIN, HIGH);
  g_actuatorMoving = false;
  Serial.println(F("BRAKE   (D1=HIGH D2=HIGH)"));
}

static void pulseRetract() {
  Serial.println(F("PULSE retract 250 ms (kick past stiction)"));
  digitalWrite(DRV_AIN1_PIN, LOW);
  digitalWrite(DRV_AIN2_PIN, HIGH);
  delay(250);
  digitalWrite(DRV_AIN1_PIN, LOW);
  digitalWrite(DRV_AIN2_PIN, LOW);
  g_actuatorMoving = false;
  Serial.println(F("pulse done, coasting"));
}

static void fanOn() {
  digitalWrite(DRV_FAN_PIN, HIGH);
  g_fanRunning = true;
  Serial.println(F("FAN ON  (D8=HIGH)"));
}

static void fanOff() {
  digitalWrite(DRV_FAN_PIN, LOW);
  g_fanRunning = false;
  Serial.println(F("FAN OFF (D8=LOW)"));
}

static void startCycle() {
  Serial.println(F("AUTO-CYCLE started (extend/pause/retract/pause, repeat). '.' to stop."));
  g_cyclePhase = CYCLE_EXTEND;
  g_cyclePhaseStart = millis();
  extendActuators();
}

static void stopCycle() {
  if (g_cyclePhase != CYCLE_OFF) {
    Serial.println(F("AUTO-CYCLE stopped"));
  }
  g_cyclePhase = CYCLE_OFF;
  coastActuators();
}

static void serviceCycle(uint32_t now) {
  if (g_cyclePhase == CYCLE_OFF) return;
  uint32_t elapsed = now - g_cyclePhaseStart;
  switch (g_cyclePhase) {
    case CYCLE_EXTEND:
      if (elapsed >= ACTUATOR_TRAVEL_MS) {
        coastActuators();
        g_cyclePhase = CYCLE_PAUSE_A;
        g_cyclePhaseStart = now;
      }
      break;
    case CYCLE_PAUSE_A:
      if (elapsed >= CYCLE_PAUSE_MS) {
        retractActuators();
        g_cyclePhase = CYCLE_RETRACT;
        g_cyclePhaseStart = now;
      }
      break;
    case CYCLE_RETRACT:
      if (elapsed >= ACTUATOR_TRAVEL_MS) {
        coastActuators();
        g_cyclePhase = CYCLE_PAUSE_B;
        g_cyclePhaseStart = now;
      }
      break;
    case CYCLE_PAUSE_B:
      if (elapsed >= CYCLE_PAUSE_MS) {
        extendActuators();
        g_cyclePhase = CYCLE_EXTEND;
        g_cyclePhaseStart = now;
      }
      break;
    default: break;
  }
}

void setup() {
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && (millis() - t0 < 3000)) { }

  #ifdef ARDUINO_ARCH_RP2040
    // Force the new pins to plain GPIO mode (RP2040 GPIO0/GPIO1 default to UART0)
    gpio_set_function(GPIO_AIN1, GPIO_FUNC_SIO);
    gpio_set_function(GPIO_AIN2, GPIO_FUNC_SIO);
    gpio_set_function(GPIO_FAN,  GPIO_FUNC_SIO);
  #endif

  pinMode(DRV_AIN1_PIN, OUTPUT);
  pinMode(DRV_AIN2_PIN, OUTPUT);
  pinMode(DRV_FAN_PIN,  OUTPUT);
  digitalWrite(DRV_AIN1_PIN, LOW);
  digitalWrite(DRV_AIN2_PIN, LOW);
  digitalWrite(DRV_FAN_PIN,  LOW);

  Serial.println(F("\n\n========================================"));
  Serial.println(F("DRV8833 Isolated Bench Test"));
  Serial.println(F("========================================"));
  Serial.println(F("A=extend  Z=retract  X=coast  B=brake"));
  Serial.println(F("V=toggle fan  P=pulse retract"));
  Serial.println(F("C=auto-cycle  .=stop cycle  ?=pin state"));
  Serial.println(F("========================================"));
  printPinSnapshot();
}

void loop() {
  uint32_t now = millis();

  // Coast actuators after their travel time (non-blocking)
  if (g_actuatorMoving && g_cyclePhase == CYCLE_OFF &&
      (now - g_actuatorStartMs >= ACTUATOR_TRAVEL_MS)) {
    coastActuators();
  }

  // Run the auto-cycle state machine if active
  serviceCycle(now);

  // Process serial commands
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '\r' || ch == '\n' || ch == ' ') continue;
    switch (ch) {
      case 'A': case 'a': stopCycle(); extendActuators();  break;
      case 'Z': case 'z': stopCycle(); retractActuators(); break;
      case 'X': case 'x': stopCycle(); coastActuators();   break;
      case 'B': case 'b': stopCycle(); brakeActuators();   break;
      case 'V': case 'v': if (g_fanRunning) fanOff(); else fanOn(); break;
      case 'P': case 'p': stopCycle(); pulseRetract();     break;
      case 'C': case 'c': startCycle(); break;
      case '.':           stopCycle();  break;
      case '?':           printPinSnapshot(); break;
      default:
        Serial.print(F("Unknown command: "));
        Serial.println(ch);
        break;
    }
  }
}
