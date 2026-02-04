/*
 * RP2040 Hardware Test for XIAO RP2040
 * Tests LED brightness and Servo control on specific pins
 */

#include <Servo.h>
#include <hardware/gpio.h>
#include <hardware/pwm.h>

// Pin definitions (Arduino pin numbers) - RP2040 needs D prefix!
#define LED_PIN    D10  // D10
#define SERVO_PIN  D8   // D8 (GPIO2) - testing if PWM works on different pin
#define BUTTON_PIN D3   // D3

// GPIO numbers for RP2040
#define GPIO_LED    3  // D10 = GPIO3
#define GPIO_SERVO  2  // D8 = GPIO2
#define GPIO_BTN   29  // D3 = GPIO29

Servo servo;

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("\n\n========================================");
  Serial.println("RP2040 Hardware Test");
  Serial.println("========================================");
  Serial.println("Testing LED on D10 (GPIO3)");
  Serial.println("Testing Servo on D0 (GPIO26)");
  Serial.println("Testing Button on D3 (GPIO29)");
  Serial.println("========================================\n");

  // CRITICAL: Set pins to GPIO function first (RP2040 defaults some to SPI/I2C/etc)
  Serial.println("Setting pins to GPIO function...");
  gpio_set_function(GPIO_LED, GPIO_FUNC_SIO);    // D10/GPIO3 to GPIO mode (was SPI MOSI!)
  gpio_set_function(GPIO_SERVO, GPIO_FUNC_PWM);  // D0/GPIO26 to PWM mode for servo
  gpio_set_function(GPIO_BTN, GPIO_FUNC_SIO);    // D3/GPIO29 to GPIO mode

  // Setup LED
  pinMode(LED_PIN, OUTPUT);
  gpio_set_dir(GPIO_LED, GPIO_OUT);  // Force output direction
  digitalWrite(LED_PIN, LOW);

  // Set maximum drive strength for LED
  gpio_set_drive_strength(GPIO_LED, GPIO_DRIVE_STRENGTH_12MA);
  gpio_set_slew_rate(GPIO_LED, GPIO_SLEW_RATE_FAST);

  Serial.println("LED configured with 12mA drive strength, GPIO mode");

  // Setup button
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Serial.println("Pin functions configured!");

  // Test LED brightness
  Serial.println("\nLED Test - turning ON for 5 seconds...");
  digitalWrite(LED_PIN, HIGH);
  delay(5000);
  digitalWrite(LED_PIN, LOW);
  Serial.println("LED Test complete. Was it bright?");

  delay(2000);

  // First try Servo library with D8 constant
  Serial.print("\nTrying Servo library with D8 (GPIO2)...");
  servo.attach(SERVO_PIN);
  delay(100);

  if (servo.attached()) {
    Serial.println(" SUCCESS");
    Serial.println("Testing with Servo library...");
    servo.write(0);
    delay(2000);
    servo.write(90);
    delay(2000);
    servo.write(180);
    delay(2000);
    Serial.println("Did servo move with Servo library?");
    servo.detach();
    delay(1000);
  } else {
    Serial.println(" FAILED");
  }

  // Now try raw PWM
  Serial.print("\nUsing raw PWM for servo on D8 (GPIO");
  Serial.print(GPIO_SERVO);
  Serial.println(")...");

  // Configure GPIO2 for PWM
  gpio_set_function(GPIO_SERVO, GPIO_FUNC_PWM);
  uint slice_num = pwm_gpio_to_slice_num(GPIO_SERVO);
  uint channel = pwm_gpio_to_channel(GPIO_SERVO);

  Serial.print("PWM Slice: ");
  Serial.print(slice_num);
  Serial.print(", Channel: ");
  Serial.println(channel);

  // 50Hz for servo: 125MHz / (125 * 20000) = 50Hz
  pwm_set_clkdiv(slice_num, 125.0f);  // 125MHz / 125 = 1MHz
  pwm_set_wrap(slice_num, 20000);     // 1MHz / 20000 = 50Hz (20ms period)
  pwm_set_enabled(slice_num, true);

  Serial.println("PWM configured: 50Hz, 20ms period");

  // Helper function to set servo position (0-180 degrees)
  auto setServoPos = [slice_num, channel](int degrees) {
    // 0° = 1ms (5% duty), 90° = 1.5ms (7.5%), 180° = 2ms (10% duty)
    // At 20000 wrap: 1ms=1000, 1.5ms=1500, 2ms=2000
    uint16_t pulse_width = 1000 + (degrees * 1000 / 180);
    pwm_set_chan_level(slice_num, channel, pulse_width);
  };

  // Try to move servo
  Serial.println("Moving servo to 0 degrees...");
  setServoPos(0);
  delay(2000);

  Serial.println("Moving servo to 90 degrees...");
  setServoPos(90);
  delay(2000);

  Serial.println("Moving servo to 180 degrees...");
  setServoPos(180);
  delay(2000);

  Serial.println("Raw PWM servo test complete. Did it move?");

  Serial.println("\n========================================");
  Serial.println("Test complete!");
  Serial.println("Press button (D3) to repeat LED blink");
  Serial.println("========================================\n");
}

void loop() {
  // Check button
  if (digitalRead(BUTTON_PIN) == LOW) {
    Serial.println("Button pressed - blinking LED");

    // Blink LED 3 times
    for (int i = 0; i < 3; i++) {
      digitalWrite(LED_PIN, HIGH);
      delay(500);
      digitalWrite(LED_PIN, LOW);
      delay(500);
    }

    delay(1000); // Debounce
  }

  delay(100);
}
