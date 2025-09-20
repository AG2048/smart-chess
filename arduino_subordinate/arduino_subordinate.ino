#include <Wire.h>

#define SUBORDINATE_ADDR 8

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

const uint8_t DPAD_PINS[10] = {31, 33, 35, 37, 39, 41, 43, 45, 47, 49};
const uint8_t LIMIT_PINS[4] = {}; // x-, x+, y-, y+
const uint8_t DIR_PINS[2] = {};   // x, y
const uint8_t PUL_PINS[2] = {};   // x, y
const uint8_t PICKER_PIN = ;

Servo piece_picker;
const float MM_PER_SQUARE = 66.7; // width of chessboard squares in mm
const float GRAVEYARD_GAP = 10.0;   // gap between graveyard and chessboard in mm
const uint8_t PICKER_ANGLE[2] {}; // up angle, down angle
const int8_t BED_LEVEL_OFFSET[8][8] = {
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0}
}; // x, y
enum {
  X_AXIS,
  Y_AXIS,
  XY_AXIS
};

int motor_coordinates[2] = {-(MM_PER_SQUARE*3 + GRAVEYARD_GAP), 0};

void setup() {
  for (uint8_t i = 0; i < 10; i++) {
    pinMode(DPAD_PINS[i], INPUT_PULLUP); // Set pins as input with pull-up resistors
  }
  Wire.begin(SUBORDINATE_ADDR);
  Wire.onRequest(requestEvent);
  Wire.onReceive(receiveEvent);
}

void loop() {
  // Nothing needed here for I2C subordinate
}

void requestEvent() {
  // Read pin states and pack into 10 bits (LSB = pin 31)
  uint16_t value = 0;
  for (uint8_t i = 0; i < 10; i++) {
    if (digitalRead(DPAD_PINS[i])) {
      value |= (1 << i);
    }
  }
  // Send as 2 bytes, LSB first
  Wire.write(value & 0xFF);        // Send LSB
  Wire.write((value >> 8) & 0xFF); // Send MSB
}

void motor_move_piece(int8_t x0, int8_t y0, int8_t x1, int8_t y1, int8_t taxicab) {

}

void receiveEvent() {
  // Read coordinate pair (x0, y0) to (xf, yf) and grid
  // Received x coordinates will be +3 of their true value
  // Receive motor argument in 17 bits
  int8_t x0, x1, y0, y1, taxicab;

  if (Wire.available() < 3) return;

  x0 = Wire.read();
  y0 = (x0 >> 4) & 0x0F;
  x0 &= 0x0F;
  x1 = Wire.read();
  y1 = (x1 >> 4) & 0x0F;
  x1 &= 0x0F;
  taxicab = Wire.read();

  motor_move_piece(x0-3, y0, x1-3, y1, taxicab);
}
