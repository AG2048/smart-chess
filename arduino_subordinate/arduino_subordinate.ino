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
const uint8_t STEPS_PER_MM = 80;
const float MM_PER_SQUARE = 66.7; // width of chessboard squares in mm
const uint8_t GRAVEYARD_GAP = 10.0;   // gap between graveyard and chessboard in mm
const uint8_t STEP_DELAY = 100; // in us, is half the period of square wave
const uint8_t PICKER_ANGLE[2] {}; // down angle, up angle
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

uint8_t motor_busy = 0;
int16_t motor_coord[2] = {-(floor(MM_PER_SQUARE*3) + GRAVEYARD_GAP), 0}; // x, y in mm

void setup() {
  for (uint8_t i = 0; i < 10; i++) pinMode(DPAD_PINS[i], INPUT_PULLUP); // Set pins as input with pull-up resistors
  for (uint8_t i = 0; i < 4; i++) pinMode(LIMIT_PINS[i], INPUT_PULLUP);
  for (uint8_t i = 0; i < 2; i++) pinMode(DIR_PINS[i], OUTPUT);
  for (uint8_t i = 0; i < 2; i++) pinMode(PUL_PINS[i], OUTPUT);
  pinMode(PICKER_PIN, OUTPUT);

  Wire.begin(SUBORDINATE_ADDR);
  Wire.onRequest(requestEvent);
  Wire.onReceive(receiveEvent);
}

void loop() {
  // Nothing needed here for I2C subordinate
}

void requestEvent() {
  if (motor_busy) {
    Wire.write(0x96);
    motor_busy = 0;
  } else {
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
}

void stepper_square_wave(uint8_t mode, uint8_t delay) { // delay in us
  if (mode == XY_AXIS) {
    digitalWrite(PUL_PIN[X_AXIS], HIGH);
    digitalWrite(PUL_PIN[Y_AXIS], HIGH);
    delayMicroseconds(delay);
    digitalWrite(PUL_PIN[X_AXIS], LOW);
    digitalWrite(PUL_PIN[Y_AXIS], LOW);
  } else if (mode == X_AXIS || mode == Y_AXIS){
    digitalWrite(PUL_PIN[mode], HIGH);
    delayMicroseconds(delay);
    digitalWrite(PUL_PIN[mode], LOW);
  }
  delayMicroseconds(delay);
}

void servo_bed_level(int8_t x, int8_t y, uint8_t angle) { // x and y in mm 
  piece_picker.write(angle + BED_LEVEL_OFFSET[floor(x/MM_PER_SQUARE)][floor(y/MM_PER_SQUARE)]);
}

void motor_move_line(int8_t x_d, int8_t y_d, uint16_t dist, uint8_t delay, uint8_t angle) { // (x, y) -> [-1, 1], dist in mm
  uint8_t mode = -1; // init to invalid value
  
  digitalWrite(DIR_PIN[X_AXIS], x_d < 0);
  digitalWrite(DIR_PIN[Y_AXIS], y_d > 0);

  if (x_d != 0 && y_d != 0) {
    mode = XY_AXIS;
  } else if (x_d != 0) {
    mode = X_AXIS;
  } else if (y_d != 0) {
    mode = Y_AXIS;
  }

  for (int i = 0; i < dist; i++) {
    stepper_square_wave(mode, delay);
    servo_bed_level(motor_coord[0]+i*x_d, motor_coord[1]+i*y_d, angle);
  }
}

void motor_move_piece(int8_t x0, int8_t y0, int8_t x1, int8_t y1, int8_t taxicab) {
  // Test
  motor_move_line(0, 1, 100, STEP_DELAY, PICKER_ANGLE[0]);
  motor_move_line(1, 0, 100, STEP_DELAY, PICKER_ANGLE[0]);
  motor_move_line(0, -1, 100, STEP_DELAY, PICKER_ANGLE[0]);
  motor_move_line(-1, 0, 100, STEP_DELAY, PICKER_ANGLE[0]);
}

void receiveEvent(int bytes) {
  // Read coordinate pair (x0, y0) to (xf, yf) and grid
  // Received x coordinates will be +3 of their true value
  // Receive motor argument in 17 bits
  int8_t x0, x1, y0, y1, taxicab;

  if (bytes < 3) return;

  motor_busy = 1;

  x0 = Wire.read();
  y0 = (x0 >> 4) & 0x0F;
  x0 &= 0x0F;
  x1 = Wire.read();
  y1 = (x1 >> 4) & 0x0F;
  x1 &= 0x0F;
  taxicab = Wire.read();

  motor_move_piece(x0 + 3, y0, x1 + 3, y1, taxicab);
}
