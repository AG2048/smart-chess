#include <Wire.h>

#define SUBORDINATE_ADDR 8

const uint8_t pins[10] = {31, 33, 35, 37, 39, 41, 43, 45, 47, 49};
// Order of pins follow:
/*
enum {
  JOYSTICK_0_POS_X_INDEX,
  JOYSTICK_1_POS_X_INDEX,
  JOYSTICK_0_POS_Y_INDEX,
  JOYSTICK_1_POS_Y_INDEX,
  JOYSTICK_0_NEG_X_INDEX,
  JOYSTICK_1_NEG_X_INDEX,
  JOYSTICK_0_NEG_Y_INDEX,
  JOYSTICK_1_NEG_Y_INDEX,
  JOYSTICK_0_BUTTON_INDEX,
  JOYSTICK_1_BUTTON_INDEX,
  JOYSTICK_PINS_COUNT
};
*/
volatile uint16_t pinStates = 0;

void setup() {
  for (uint8_t i = 0; i < 10; i++) {
    pinMode(pins[i], INPUT_PULLUP); // Set pins as input with pull-up resistors
  }
  Wire.begin(SUBORDINATE_ADDR);
  Wire.onRequest(requestEvent);
  // Serial.begin(9600);
}

void loop() {
  // Nothing needed here for I2C subordinate
}

void requestEvent() {
  // Read pin states and pack into 10 bits (LSB = pin 31)
  uint16_t value = 0;
  for (uint8_t i = 0; i < 10; i++) {
    if (digitalRead(pins[i])) {
      value |= (1 << i);
    }
  }
  // Send as 2 bytes, LSB first
  Wire.write(value & 0xFF);        // Send LSB
  Wire.write((value >> 8) & 0xFF); // Send MSB
  // Serial.print("Printed Value: ");
  // Serial.println(value);
}
