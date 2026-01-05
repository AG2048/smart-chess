#include <Wire.h>
#include <Servo.h>

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

// GPIO pins
// const uint8_t DPAD_PINS[10] = {31, 33, 35, 37, 39, 41, 43, 45, 47, 49};
const uint8_t DPAD_PINS[10] = {41, 39, 45, 43, 33, 31, 37, 35, 49, 47}; // swapped white and black
// x+, x+, y+, y+, x-, x-, y-, y-, confirm, confirm (white/black)
// const uint8_t DPAD_PINS[10] = {33, 31, 37, 35, 41, 39, 45, 43, 49, 47}; // wrong
const uint8_t LIMIT_PINS[4] = {4, 5, 2, 3}; // x-, x+, y-, y+
const uint8_t DIR_PINS[2] = {9, 11};   // x, y
const uint8_t PUL_PINS[2] = {10, 12};   // x, y
const uint8_t PICKER_PIN = 7;

// Piece picker
Servo piece_picker; 
const uint8_t STEPS_PER_MM = 80;
const float MM_PER_SQUARE = 66.66; // width of chessboard squares in mm
const uint8_t GRAVEYARD_GAP = 10.0;   // gap between graveyard and chessboard in mm
const uint8_t STEP_DELAY = 100; // in us, is half the period of square wave
const uint8_t MOVE_DELAY = 1000; // in ms, delay between piece picker and gantry movement
const uint8_t PICKER_ANGLE[2] {10, 120}; // down angle, up angle  
const int16_t CALIBRATE_COORD[2] = {(-3*MM_PER_SQUARE)-17, -22}; // actual calibrate coordinate relative to board origin
// const int8_t BED_LEVEL_OFFSET[8][8] = {
//   {0, 0, 0, 0, 0, 0, 0, 0},
//   {0, 0, 0, 0, 0, 0, 0, 0},
//   {0, 0, 0, 0, 0, 0, 0, 0},
//   {0, 0, 0, 0, 0, 0, 0, 0},
//   {0, 0, 0, 0, 0, 0, 0, 0},
//   {0, 0, 0, 0, 0, 0, 0, 0},
//   {0, 0, 0, 0, 0, 0, 0, 0},
//   {0, 0, 0, 0, 0, 0, 0, 0}
// }; // x, y
enum {
  X_AXIS,
  Y_AXIS,
  XY_AXIS
};

uint8_t x_0, x_1, y_0, y_1, motor_mode; // DO NOT use except for passing into motor_move_piece

uint8_t state = 0; // 0: idle/button poll, 1: motor, 2: motor done
int16_t motor_coord[2] = {-(floor(MM_PER_SQUARE*3) + GRAVEYARD_GAP), 0}; // x, y in mm

void stepper_square_wave(uint8_t mode, uint8_t delay) { // delay in us
  if (mode == XY_AXIS) {
    digitalWrite(PUL_PINS[X_AXIS], HIGH);
    digitalWrite(PUL_PINS[Y_AXIS], HIGH);
    delayMicroseconds(delay);
    digitalWrite(PUL_PINS[X_AXIS], LOW);
    digitalWrite(PUL_PINS[Y_AXIS], LOW);
  } else if (mode == X_AXIS || mode == Y_AXIS){
    digitalWrite(PUL_PINS[mode], HIGH);
    delayMicroseconds(delay);
    digitalWrite(PUL_PINS[mode], LOW);
  }
  delayMicroseconds(delay);
}

// void servo_bed_level(int8_t x, int8_t y, uint8_t angle) { // x and y in mm 
//   piece_picker.write(angle + BED_LEVEL_OFFSET[floor(x/MM_PER_SQUARE)][floor(y/MM_PER_SQUARE)]);
// }

void motor_move_line(int8_t x_dir, int8_t y_dir, uint16_t dist, uint8_t delay) { // (x, y) -> [-1, 1], dist in mm, both x, y, cannot be 0
  uint8_t mode = -1; // init to invalid value

  digitalWrite(DIR_PINS[X_AXIS], x_dir < 0);
  digitalWrite(DIR_PINS[Y_AXIS], y_dir > 0);

  if (x_dir != 0 && y_dir != 0) {
    mode = XY_AXIS;
  } else if (x_dir != 0) {
    mode = X_AXIS;
  } else if (y_dir != 0) {
    mode = Y_AXIS;
  }

  for (int i = 0; i < dist*STEPS_PER_MM; i++) {
    stepper_square_wave(mode, delay);
    // servo_bed_level(motor_coord[0]+i*x_dir, motor_coord[1]+i*y_dir, angle);
  }
}

void motor_move(int16_t x, int16_t y, uint8_t delay, uint8_t taxicab) { // in mm
  uint16_t x_mag = abs(x-motor_coord[0]), y_mag = abs(y-motor_coord[1]);
  int8_t x_dir = (x-motor_coord[0]) > 0 ? 1 : -1, y_dir = (y-motor_coord[1]) > 0 ? 1 : -1;

  if (taxicab) {
    Serial.println("taxicab");
    motor_move_line(x_dir, 0, x_mag, delay);
    motor_move_line(0, y_dir, y_mag, delay);
  } else if (x_mag == y_mag) {
    Serial.println("x == y");
    motor_move_line(x_dir, y_dir, x_mag, delay);
  } else if (x_mag > y_mag) {
    Serial.println("x > y");
    motor_move_line(x_dir, y_dir, y_mag, delay);
    motor_move_line(x_dir, 0, x_mag-y_mag, delay);
  } else {
    Serial.println("x < y");

    Serial.println(x);
    Serial.println(y);
    Serial.println(x_dir);
    Serial.println(y_dir);
    Serial.println(x_mag);
    Serial.println(y_mag);

    motor_move_line(x_dir, y_dir, x_mag, delay);
    motor_move_line(0, y_dir, y_mag-x_mag, delay);
  }

  motor_coord[0] = x;
  motor_coord[1] = y;
}

void motor_move_piece(int16_t x_0, int16_t y_0, int16_t x_1, int16_t y_1, int8_t taxicab) { // in squares, but converted to mm
  if (x_0 < 0) {
    x_0 *= MM_PER_SQUARE;
    x_0 -= GRAVEYARD_GAP;
  } else if (x_0 > 7) {
    x_0 *= MM_PER_SQUARE;
    x_0 += GRAVEYARD_GAP;
  } else {
    x_0 *= MM_PER_SQUARE;
  }

  if (x_1 < 0) {
    x_1 *= MM_PER_SQUARE;
    x_1 -= GRAVEYARD_GAP;
  } else if (x_1 > 7) {
    x_1 *= MM_PER_SQUARE;
    x_1 += GRAVEYARD_GAP;
  } else {
    x_1 *= MM_PER_SQUARE;
  }

  y_0 *= MM_PER_SQUARE;
  y_1 *= MM_PER_SQUARE;

  piece_picker.write(PICKER_ANGLE[0]);
  delay(MOVE_DELAY);
  Serial.println("First move");
  Serial.println(motor_coord[0]);
  Serial.println(motor_coord[1]);
  Serial.println(x_0);
  Serial.println(y_0);
  motor_move(x_0, y_0, STEP_DELAY, false);
  delay(MOVE_DELAY);
  piece_picker.write(PICKER_ANGLE[1]);
  delay(MOVE_DELAY);

  if (taxicab) {
    int x_a, y_a, x_b, y_b;
    x_a = x_0 < x_1 ? x_0 + (MM_PER_SQUARE/2) : x_0 - (MM_PER_SQUARE/2);
    y_a = y_0 < y_1 ? y_0 + (MM_PER_SQUARE/2) : y_0 - (MM_PER_SQUARE/2);
    x_b = x_0 < x_1 ? x_1 - (MM_PER_SQUARE/2) : x_1 + (MM_PER_SQUARE/2);
    y_b = y_0 < y_1 ? y_1 - (MM_PER_SQUARE/2) : y_1 + (MM_PER_SQUARE/2);

    x_a = min(max(x_a, (int)(-2.5*MM_PER_SQUARE)), (int)(13.5*MM_PER_SQUARE));
    y_a = min(max(y_a, (int)(MM_PER_SQUARE/2)), (int)(7.5*MM_PER_SQUARE));
    x_b = min(max(x_b, (int)(-2.5*MM_PER_SQUARE)), (int)(13.5*MM_PER_SQUARE));
    y_b = min(max(y_b, (int)(MM_PER_SQUARE/2)), (int)(7.5*MM_PER_SQUARE));

    motor_move(x_a, y_a, STEP_DELAY, false);
    motor_move(x_b, y_b, STEP_DELAY, true);
  }

  Serial.println("Second move");
  Serial.println(motor_coord[0]);
  Serial.println(motor_coord[1]);
  Serial.println(x_1);
  Serial.println(y_1);
  motor_move(x_1, y_1, STEP_DELAY, false);
  delay(MOVE_DELAY);
  piece_picker.write(PICKER_ANGLE[0]);
  delay(MOVE_DELAY);
}

void motor_move_calibrate() {
  piece_picker.write(PICKER_ANGLE[0]);
  delay(MOVE_DELAY);
  motor_move(-3*MM_PER_SQUARE, 0, STEP_DELAY, false);

  digitalWrite(DIR_PINS[X_AXIS], 1); // default x, y, to go to origin (to fix mis-wired drivers)
  digitalWrite(DIR_PINS[Y_AXIS], 0);

  while (!digitalRead(LIMIT_PINS[0])) {
    stepper_square_wave(X_AXIS, STEP_DELAY);
  }

  while (!digitalRead(LIMIT_PINS[1])) {
    stepper_square_wave(Y_AXIS, STEP_DELAY);
  }

  motor_coord[0] = CALIBRATE_COORD[0];
  motor_coord[1] = CALIBRATE_COORD[1];
  motor_move(-3*MM_PER_SQUARE, 0, STEP_DELAY, false);
}

double motor_x, motor_y, servo_angle;
bool value_changed, servo_value_changed;

void setup() {
  for (uint8_t i = 0; i < 10; i++) pinMode(DPAD_PINS[i], INPUT_PULLUP); // Set pins as input with pull-up resistors
  for (uint8_t i = 0; i < 4; i++) pinMode(LIMIT_PINS[i], INPUT_PULLUP);
  for (uint8_t i = 0; i < 2; i++) pinMode(DIR_PINS[i], OUTPUT);
  for (uint8_t i = 0; i < 2; i++) pinMode(PUL_PINS[i], OUTPUT);
  pinMode(PICKER_PIN, OUTPUT);

  Serial.begin(9600);
  Serial.println("Starting up...");

  piece_picker.attach(PICKER_PIN);
  motor_move_calibrate();

  motor_x = -3*MM_PER_SQUARE;
  motor_y = 0;
  servo_angle = 0;
}

void loop() {
  value_changed = false;  // Reset value changed flag
  servo_value_changed = false;  // Reset servo value changed flag

  if (digitalRead(DPAD_PINS[0]) == 0) {
    // Move motor to right. 
    Serial.println("Moving motor to right");
    motor_x += MM_PER_SQUARE;
    value_changed = true;
  } else if (digitalRead(DPAD_PINS[4]) == 0) {
    // Move motor to left.
    Serial.println("Moving motor to left");
    motor_x -= MM_PER_SQUARE;
    value_changed = true;
  }

  if (digitalRead(DPAD_PINS[2]) == 0) {
    // Move motor up.
    Serial.println("Moving motor up");
    motor_y += MM_PER_SQUARE;   
    value_changed = true;
  } else if (digitalRead(DPAD_PINS[6]) == 0) {
    // Move motor down.
    Serial.println("Moving motor down");
    motor_y -= MM_PER_SQUARE;
    value_changed = true;
  }

  if (digitalRead(DPAD_PINS[3]) == 0) {
    // Move servo up.
    Serial.println("Moving servo up");
    servo_angle += 10;
    if (servo_angle > 180) {
      servo_angle = 180;
    }
    servo_value_changed = true;
  } else if (digitalRead(DPAD_PINS[7]) == 0) {
    // Move servo down.
    Serial.println("Moving servo down");
    servo_angle -= 10;
    if (servo_angle < 0) {
      servo_angle = 0;
    }
    servo_value_changed = true;
  }

  if (value_changed) {
    Serial.print("Moving motor to (");
    Serial.print(motor_x);
    Serial.print(", ");
    Serial.print(motor_y);
    Serial.println(") in mm");
    motor_move(motor_x, motor_y, STEP_DELAY, false); // Move motor to new coordinates
  }

  if (servo_value_changed) {
    Serial.print("Moving servo to ");
    Serial.print(servo_angle);
    Serial.println(" degrees");
    piece_picker.write(servo_angle);  // Move servo to new angle
  }

  delay(200); // Debounce delay
}
