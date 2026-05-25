# Plan: Add DRV8833 driving 2 linear actuators + 5V fan, tied to vent state

## Context

The existing controller drives a single servo flap to vent moisture from a SUNLU AMS filament dryer, with vent state determined by sensed heater/fan current (see `openVent()` / `closeVent()` at `SUNLU_AMS_Heater_AutoVent.ino:535-545` and the decision logic at `:1250-1279`).

The user wants to add a **DRV8833** dual H-bridge to:

1. Drive **two 5 V linear actuators in parallel** that lift the dryer lid whenever the vent opens.
2. Drive a **2-wire 5 V fan** that runs whenever the vent is open.

Both new outputs should mirror the existing vent state — extending/turning on with `openVent()` and retracting/turning off with `closeVent()`. No new sensing or new trigger logic is needed; we piggyback on the existing vent decision.

Confirmed with the user:
- Actuators are 5 V (within DRV8833's 2.7–10.8 V VM range — DRV8833 is the right chip).
- Fan runs whenever vent is open (on/off, no PWM needed).
- Actuators wired **in parallel on one H-bridge** (always move together; frees the other bridge for the fan).
- Fan is a simple 2-wire DC fan.

---

## Wiring

### DRV8833 module connections

This wiring targets the common low-cost DRV8833 breakout sold on Amazon as the [WWZMDiB 6-pack (B0DB8CX8LK)](https://www.amazon.com/dp/B0DB8CX8LK). That board uses **a single `VCC` pin for both motor and logic supply** (2.7–10.8 V) — there's no separate logic rail. The TI chip's logic inputs are happy with 3.3 V signaling regardless of what VCC sits at, so a 5 V VCC works fine with the XIAO's 3.3 V GPIO output.

Silkscreen labels on this board: `VCC`, `GND`, `IN1`, `IN2`, `IN3`, `IN4`, `OUT1`, `OUT2`, `OUT3`, `OUT4`, `EEP`, `ULT`. (Pin labels `AIN1/AIN2/BIN1/BIN2/AOUT1/.../nSLEEP/nFAULT` you may see in tutorials are the same pins under the chip's datasheet names.)

| Board pin | Connects to | Purpose |
|---|---|---|
| `VCC` | +5 V rail (shared with servo supply) | Motor + logic supply for the DRV8833 (single rail on this board) |
| `GND` | Common ground with XIAO and 5 V rail | Return |
| `EEP` (sleep) | **Leave alone** — jumper on the board pulls it high already | Keeps the driver awake |
| `ULT` (fault) | Leave unconnected | Open-drain fault output, needs external pull-up to read |
| `IN1` (= AIN1) | XIAO **D1** (GPIO0) | Actuator extend |
| `IN2` (= AIN2) | XIAO **D2** (GPIO1) | Actuator retract |
| `IN3` (= BIN1) | XIAO **D8** (GPIO2) | Fan on/off |
| `IN4` (= BIN2) | `GND` (jumper wire) | Forces fan H-bridge to one direction |
| `OUT1` (= AOUT1) | Actuator #1 (+) **and** actuator #2 (+) in parallel | Both lid actuators move together |
| `OUT2` (= AOUT2) | Actuator #1 (–) **and** actuator #2 (–) in parallel | Both lid actuators move together |
| `OUT3` (= BOUT1) | Fan (+) | Drive high to spin fan |
| `OUT4` (= BOUT2) | Fan (–) | Tied to GND via IN4 |

> **Verify against your actual board before soldering.** Some clone boards have known silkscreen errors (e.g., `BIN1` printed where `AIN1` should be). When the boards arrive, trace one input pin from the screw terminal to the chip leg and confirm which IC pin it lands on before assuming the silkscreen is right.

### Wiring diagram

```
                          ┌─────────────────────────────┐
                          │   EXTERNAL 5V 2A+ SUPPLY    │
                          └──────┬──────────────┬───────┘
                                 │+5V           │GND
                                 │              │
                                 ▼              ▼
        ┌──────────────────────────┐     ┌──────────────────────────────────────┐
        │    XIAO RP2040 / SAMD21  │     │  DRV8833 module (WWZMDiB B0DB8CX8LK) │
        │                          │     │                                      │
        │ 5V  ◄── from +5V rail    │     │ VCC ◄── from +5V rail                │
        │ GND ◄── common GND       │     │ GND ◄── common GND                   │
        │                          │     │                                      │
        │ D1  ─────────────────────┼────►│ IN1                                  │
        │ D2  ─────────────────────┼────►│ IN2                                  │
        │ D8  ─────────────────────┼────►│ IN3                                  │
        │                          │     │ IN4 ◄── short wire to GND            │
        │                          │     │                                      │
        │ (existing - unchanged:)  │     │ EEP   on-board jumper, leave default │
        │  D0  → servo signal      │     │ ULT   leave unconnected              │
        │  D3  → button            │     │                                      │
        │  D10 → status LED        │     │ OUT1 ─┬─ Actuator #1 (+)             │
        │  SDA/SCL → ADS1115       │     │       └─ Actuator #2 (+)             │
        │                          │     │ OUT2 ─┬─ Actuator #1 (−)             │
        │                          │     │       └─ Actuator #2 (−)             │
        │                          │     │ OUT3 ──── Fan (+)                    │
        │                          │     │ OUT4 ──── Fan (−)                    │
        └──────────────────────────┘     └──────────────────────────────────────┘

   All grounds tie together: XIAO GND, DRV8833 GND, PSU GND, actuator (−)/fan (−) returns.
```

#### Connection list

| # | From | To | Wire |
|---|---|---|---|
| 1 | 5V PSU `+` | XIAO `5V` pin | red |
| 2 | 5V PSU `+` | DRV8833 `VCC` | red |
| 3 | 5V PSU `–` | XIAO `GND` | black |
| 4 | 5V PSU `–` | DRV8833 `GND` | black |
| 5 | XIAO `D1` | DRV8833 `IN1` | signal |
| 6 | XIAO `D2` | DRV8833 `IN2` | signal |
| 7 | XIAO `D8` | DRV8833 `IN3` | signal |
| 8 | DRV8833 `IN4` | DRV8833 `GND` (jumper) | short |
| 9 | DRV8833 `OUT1` | Actuator #1 (+) **and** Actuator #2 (+) | motor wire (paralleled) |
| 10 | DRV8833 `OUT2` | Actuator #1 (–) **and** Actuator #2 (–) | motor wire (paralleled) |
| 11 | DRV8833 `OUT3` | Fan (+) | motor wire |
| 12 | DRV8833 `OUT4` | Fan (–) | motor wire |

> **Power tip:** the XIAO's USB-C input alone (≤500 mA) won't reliably feed the actuators + fan + servo through the board. Power the whole thing from an **external 5 V 2 A+ supply** wired to both the XIAO's `5V` pin and the DRV8833's `VCC`, with all grounds tied common. USB-C can still be plugged in for programming/serial — the XIAO accepts whichever source is highest.

### H-bridge voltage drop — minor caveat

The DRV8833 drops ~0.7 V total across the high-side + low-side FETs at 1 A. With `VCC = 5 V`, your 5 V actuators and fan see ~4.3 V under load — they'll run, but slightly slower than nameplate. For lid-lift duty this should be fine. If the actuators feel too sluggish in practice, you can raise `VCC` toward 6 V (still well inside the 10.8 V max) and use 6 V actuators / fan instead.

### Pin choices on XIAO (RP2040)

From an earlier pinout audit, the in-use pins are: **D0** (servo PWM / GPIO26), **D3** (button / GPIO29), **D10** (LED / GPIO3), **SDA/SCL** (ADS1115). Everything else is free. The chosen new pins — **D1, D2, D8** — are all on different PWM slices from the servo (servo uses slice 13 / GPIO26), so there's no PWM-timing conflict, and all three are plain GPIOs on both SAMD21 and RP2040 (the repo supports both boards on the same physical pinout).

### Power note (worth flagging before purchase)

The README currently recommends a **5 V 1 A+** supply for the servo + electronics. Adding two parallel 5 V linear actuators (typically 100–500 mA each, varies widely by model) plus a small 5 V fan (often 100–250 mA) can easily push total draw past 1 A. I'd recommend a **5 V 2–3 A** supply to leave margin, and verifying the parallel-actuator current draw stays under the DRV8833's ~1.2 A continuous per-channel rating. *(Specific actuator current draw varies — check your actuator's datasheet for stall and no-load current before committing.)*

A common ground between the 5 V supply, the DRV8833 `GND`, and the XIAO `GND` is mandatory.

### Linear-actuator stall caveat

5 V hobby linear actuators usually have internal limit switches that disconnect the motor at end-of-travel — but not all do. To be safe, the firmware drives each direction for a fixed `ACTUATOR_TRAVEL_MS` and then **coasts** (both `AIN1` and `AIN2` LOW), rather than holding power against an endstop. Tune this constant once you have the actuators in hand and know the full stroke time.

---

## Branching

The work landed on a feature branch off `main` so it can be tested in isolation and merged only if the hardware integration succeeds:

```bash
git checkout -b feature/drv8833-actuators-fan
```

Merge back to `main` after the bench + in-system verification steps pass.

## Firmware changes

All changes are in **`SUNLU_AMS_Heater_AutoVent.ino`**. The repo supports both SAMD21 and RP2040 from the same source — D1/D2/D8 are free on both, so the new defines live under both branches of the existing `#ifdef BOARD_RP2040` block.

### 1. Pin defines (around `:65-83`)

In both the `BOARD_RP2040` and SAMD21 branches:

```c
#define DRV_AIN1_PIN  D1   // Actuator extend
#define DRV_AIN2_PIN  D2   // Actuator retract
#define DRV_FAN_PIN   D8   // Fan on/off (BIN1; BIN2 tied to GND on board)
```

### 2. Constants and state (around `:137-141`)

```c
static const uint32_t ACTUATOR_TRAVEL_MS = 8000;  // Tune to full stroke time
uint32_t g_actuatorStartMs = 0;
bool     g_actuatorMoving  = false;
bool     g_fanRunning      = false;
```

### 3. Helper functions (near `closeVent()` / `openVent()`)

```c
static void extendActuators() {
  digitalWrite(DRV_AIN1_PIN, HIGH);
  digitalWrite(DRV_AIN2_PIN, LOW);
  g_actuatorStartMs = millis();
  g_actuatorMoving = true;
  Serial.println(F("Actuators extending"));
}

static void retractActuators() {
  digitalWrite(DRV_AIN1_PIN, LOW);
  digitalWrite(DRV_AIN2_PIN, HIGH);
  g_actuatorStartMs = millis();
  g_actuatorMoving = true;
  Serial.println(F("Actuators retracting"));
}

static void stopActuators() {
  digitalWrite(DRV_AIN1_PIN, LOW);
  digitalWrite(DRV_AIN2_PIN, LOW);
  g_actuatorMoving = false;
  Serial.println(F("Actuators coasting"));
}

static void fanOn()  { digitalWrite(DRV_FAN_PIN, HIGH); g_fanRunning = true;  Serial.println(F("Fan ON"));  }
static void fanOff() { digitalWrite(DRV_FAN_PIN, LOW);  g_fanRunning = false; Serial.println(F("Fan OFF")); }
```

### 4. Hook into existing vent functions

```c
void closeVent() {
  Serial.println(F("CLOSING VENT"));
  moveServo(SERVO_CLOSED_DEG);
  retractActuators();
  fanOff();
}

void openVent() {
  Serial.println(F("OPENING VENT"));
  moveServo(SERVO_OPEN_DEG);
  extendActuators();
  fanOn();
}
```

That's the entire integration with the vent state machine — `:1250-1279` continues to call `openVent()` / `closeVent()` exactly as it did before.

### 5. Setup additions (in `setup()`)

```c
pinMode(DRV_AIN1_PIN, OUTPUT);
pinMode(DRV_AIN2_PIN, OUTPUT);
pinMode(DRV_FAN_PIN,  OUTPUT);
digitalWrite(DRV_AIN1_PIN, LOW);
digitalWrite(DRV_AIN2_PIN, LOW);
digitalWrite(DRV_FAN_PIN,  LOW);
```

### 6. Non-blocking actuator stop (top of `loop()`)

Runs every iteration so the actuators coast at the end of their stroke time even during standby or learning mode:

```c
if (g_actuatorMoving && (millis() - g_actuatorStartMs >= ACTUATOR_TRAVEL_MS)) {
  stopActuators();
}
```

### 7. Serial bench-test commands

Added next to the existing `O`/`C` handlers:

- `A` / `a` → `extendActuators()`
- `Z` / `z` → `retractActuators()`
- `X` / `x` → `stopActuators()`
- `V` / `v` → toggle fan

---

## Verification

1. **Compile** in Arduino IDE for both XIAO SAMD21 and XIAO RP2040 board targets — no new libraries needed.
2. **Bench test, DRV8833 powered but actuators/fan disconnected**, with a scope or multimeter on AOUT1/AOUT2 and BOUT1:
   - Send `O` over serial → verify AOUT1≈5 V, AOUT2≈0 V for ~8 s, then both coast to 0 V. Verify BOUT1≈5 V continuously.
   - Send `C` → verify AOUT1≈0 V, AOUT2≈5 V for ~8 s, then both coast. Verify BOUT1 drops to 0 V.
3. **Connect actuators + fan**, repeat. Confirm both actuators extend together, fan spins; reverse direction on close.
4. **Tune `ACTUATOR_TRAVEL_MS`** to match the actual full stroke time.
5. **In-system test**: run a real dry cycle. Confirm vent flap, lid actuators, and fan all transition together at heater-on, and that the fan + actuator-retract happen on the 3-minute cooldown close just like the servo does today.
6. **Monitor 5 V rail** with a multimeter during the first run to confirm no brownout when actuators energize simultaneously with the servo.
