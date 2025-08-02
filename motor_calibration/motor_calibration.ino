// Include
#include <SPI.h>
#include <Wire.h>
#include <stdint.h>
#include <vector>
#include <utility>
#include <string.h>
#include <Arduino.h>
#include <ESP32Servo.h>

// Arduino MEGA as I2C subordinate
#define SUBORDINATE_ADDRESS 8  // Address of the subordinate device

// ############################################################
// #                       PIECE PICKER                       #
// ############################################################

#define PIECE_PICKER_PIN 23
#define PIECE_PICKER_UP_ANGLE 50
#define PIECE_PICKER_DOWN_ANGLE 180

Servo piece_picker;

// ############################################################
// #                       MOTOR CONTROL                      #
// ############################################################

int motor_x = 0;
int motor_y = 0;
int servo_angle = 0;
bool value_changed = false;
bool servo_value_changed = false;

// MOTOR CONTROL VARIABLES
const int8_t PUL_PIN[] = { 14, 13 };          // x, y
const int8_t DIR_PIN[] = { 27, 12 };          // x, y
const int8_t LIMIT_PIN[] = { 19, 3, 1, 23 };  // x-, x+, y-, y+
const int STEPS_PER_MM = 80;                  // measured value from testing, 3.95cm per rotation (1600 steps)
const int MM_PER_SQUARE = 66;                 // width of chessboard squares in mm
const int GRAVEYARD_GAP = 10;                 // gap between graveyard and chessboard in mm
const int ORIGIN_GAP = 10;                    // gap between real origin and where gantry rests at default (prevents holding down limit switches)
const int FAST_STEP_DELAY = 50;               // half the period of square wave pulse for stepper motor
const int SLOW_STEP_DELAY = 100;              // half the period of square wave pulse for stepper motor
const long ORIGIN_RESET_TIMEOUT = 50000;      // how many steps motor will attempt to recenter to origin before giving up

int motor_error = 0;                   // 0: normal operation, 1: misaligned origin, 2: cannot reach origin (stuck)
int motor_coordinates[2] = {-(MM_PER_SQUARE*3 + GRAVEYARD_GAP), 0};  // x, y coordinates of the motor in millimeters
// Need to offset by negative 3 squares (in mm)

void stepper_square_wave(int mode, int stepDelay) {
  // Modes: 0 -> x-axis, 1 -> y-axis, 2 -> x and y-axis
  // Generates a square wave of period (stepDelay*2)
  if (mode == 2) {
    digitalWrite(PUL_PIN[0], HIGH);
    digitalWrite(PUL_PIN[1], HIGH);
    delayMicroseconds(stepDelay);
    digitalWrite(PUL_PIN[0], LOW);
    digitalWrite(PUL_PIN[1], LOW);
    delayMicroseconds(stepDelay);
  } else {
    digitalWrite(PUL_PIN[mode], HIGH);
    delayMicroseconds(stepDelay);
    digitalWrite(PUL_PIN[mode], LOW);
    delayMicroseconds(stepDelay);
  }
}

int move_motor_to_coordinate(int x, int y, int axisAligned, int stepDelay) {
  // Move the motor to a specific coordinate
  // x, y: x, y coordinates of the square to move to (IN MILLIMETERS)
  // Move the motor to (x, y)
  // This function should be used to move the motor to a specific coordinate without any checks, it should be used AFTER calibrating the motor
  // The "starting" coordinate is assumed to be motor_coordinates variable (x, y)

  // TODO
  Serial.println("Move motor code running");
  long dx = x - motor_coordinates[0];
  long dy = y - motor_coordinates[1];

  Serial.print("Starting to (");
  Serial.print(motor_coordinates[0]);
  Serial.print(", ");
  Serial.print(motor_coordinates[1]);
  Serial.println(") in mm");

  Serial.print("Moving to (");
  Serial.print(x);
  Serial.print(", ");
  Serial.print(y);
  Serial.println(") in mm");

  Serial.print("Differential (");
  Serial.print(dx);
  Serial.print(", ");
  Serial.print(dy);
  Serial.println(") in mm");

  // Setting direction
  digitalWrite(DIR_PIN[0], dx < 0);
  digitalWrite(DIR_PIN[1], dy > 0);

  dx = abs(dx);
  dy = abs(dy);

  // Check if movement should be axis-aligned or diagonal
  if (axisAligned || dx == 0 || dy == 0) {
    // Move x axis
    Serial.println("Move x axis");
    for (long i = 0; i < dx * STEPS_PER_MM; i++) {
      // if (digitalRead(LIMIT_PIN[0])) {
      //   motor_coordinates[0] = -3;
      //   break;
      // }
      // if (digitalRead(LIMIT_PIN[1])) {
      //   motor_coordinates[0] = 10;
      //   break;
      // }
      stepper_square_wave(0, stepDelay);
    }
    // Move y axis
    Serial.println("Move y axis");
    for (long i = 0; i < dy * STEPS_PER_MM; i++) {
      // if (digitalRead(LIMIT_PIN[2])) {
      //   motor_coordinates[1] = -3;
      //   break;
      // }
      // if (digitalRead(LIMIT_PIN[3])) {
      //   motor_coordinates[1] = 7;
      //   break;
      // }
      stepper_square_wave(1, stepDelay);
    }
  } else if (dx == dy) {
    // Move diagonal axis
    Serial.println("Move diag dx == dy");
    for (long i = 0; i < dx * STEPS_PER_MM; i++) {
      //if (digitalRead(LIMIT_PIN[0]) || digitalRead(LIMIT_PIN[1]) || digitalRead(LIMIT_PIN[2]) || digitalRead(LIMIT_PIN[3])) return 1;
      stepper_square_wave(2, stepDelay);
    }
  } else if (dx > dy) {
    Serial.println("Move dx > dy");
    for (long i = 0; i < dx * STEPS_PER_MM; i++) {
      if (i < dy * STEPS_PER_MM) {
        //if (digitalRead(LIMIT_PIN[0]) || digitalRead(LIMIT_PIN[1]) || digitalRead(LIMIT_PIN[2]) || digitalRead(LIMIT_PIN[3])) return 1;
        stepper_square_wave(2, stepDelay);
      } else {
        //if (digitalRead(LIMIT_PIN[0]) || digitalRead(LIMIT_PIN[1]) || digitalRead(LIMIT_PIN[2]) || digitalRead(LIMIT_PIN[3])) return 1;
        stepper_square_wave(0, stepDelay);
      }
    }
  } else {
    Serial.println("Move diag dy > dx");
    for (long i = 0; i < dy * STEPS_PER_MM; i++) {
      if (i < dx * STEPS_PER_MM) {
        //if (digitalRead(LIMIT_PIN[0]) || digitalRead(LIMIT_PIN[1]) || digitalRead(LIMIT_PIN[2]) || digitalRead(LIMIT_PIN[3])) return 1;
        stepper_square_wave(2, stepDelay);
      } else {
        //if (digitalRead(LIMIT_PIN[0]) || digitalRead(LIMIT_PIN[1]) || digitalRead(LIMIT_PIN[2]) || digitalRead(LIMIT_PIN[3])) return 1;
        stepper_square_wave(1, stepDelay);
      }
    }
  }

  // Update motor coordinates
  motor_coordinates[0] = x;
  motor_coordinates[1] = y;

  // Serial.print("Moving to (");
  // Serial.print(x);
  // Serial.print(", ");
  // Serial.print(y);
  // Serial.println(") in mm");

  return 0;
}

// TODO: motor function need a "safe move" parameter, to tell if the motor need to move the piece along an edge, or just straight to the destination
int move_piece_by_motor(int from_x, int from_y, int to_x, int to_y, int gridAligned) {
  Serial.println("MOVING CODE RUNNING");
  // Move the motor from one square to another
  // from_x, from_y: x, y coordinates of the square to move from
  // to_x, to_y: x, y coordinates of the square to move to
  // stepDelay: time in microseconds to wait between each step (half a period)
  // fastMove: if true, move piece in a straight line to destination, else, move along edges
  // Move the motor from (from_x, from_y) to (to_x, to_y)

  // TODO
  // TODO: don't write the motor moving code here, just write the logic to move the motor and call motor_move_to_coordinate function

  // Convert square coordinates to mm coordinates
  if (from_x < 0) {
    from_x = from_x * MM_PER_SQUARE - GRAVEYARD_GAP;
  } else if (from_x > 7) {
    from_x = from_x * MM_PER_SQUARE + GRAVEYARD_GAP;
  } else {
    from_x = from_x * MM_PER_SQUARE;
  }

  if (to_x < 0) {
    to_x = to_x * MM_PER_SQUARE - GRAVEYARD_GAP;
  } else if (to_x > 7) {
    to_x = to_x * MM_PER_SQUARE + GRAVEYARD_GAP;
  } else {
    to_x = to_x * MM_PER_SQUARE;
  }

  from_y = from_y * MM_PER_SQUARE;
  to_y = to_y * MM_PER_SQUARE;

  int ret;
  int corner_x, corner_y;

  // Move motor to starting location
  move_motor_to_coordinate(from_x, from_y, false, FAST_STEP_DELAY);

  delay(1000);  // pick up piece
  Serial.println("Pick up piece");
  piece_picker.write(PIECE_PICKER_UP_ANGLE);
  delay(1000);

  if (gridAligned) {
    // Move motor onto edges instead of centers, then move to destination
    corner_x = from_x < to_x ? from_x + (MM_PER_SQUARE) / 2 : from_x - (MM_PER_SQUARE) / 2;
    corner_y = from_y < to_y ? from_y + (MM_PER_SQUARE) / 2 : from_y - (MM_PER_SQUARE) / 2;
    if (ret = move_motor_to_coordinate(corner_x, corner_y, false, FAST_STEP_DELAY)) return ret;

    corner_x = from_x < to_x ? to_x - (MM_PER_SQUARE) / 2 : to_x + (MM_PER_SQUARE) / 2;
    corner_y = from_y < to_y ? to_y - (MM_PER_SQUARE) / 2 : to_y + (MM_PER_SQUARE) / 2;
    if (ret = move_motor_to_coordinate(corner_x, corner_y, true, FAST_STEP_DELAY)) return ret;
  }

  if (ret = move_motor_to_coordinate(to_x, to_y, false, FAST_STEP_DELAY)) return ret;

  delay(1000);  // release piece
  Serial.println("Release piece");
  piece_picker.write(PIECE_PICKER_DOWN_ANGLE);
  delay(1000);

  return 0;
}

int move_motor_to_origin(int offset) {
  // resets the motor to origin (0, 0) and re-calibrate the motor
  // fastMove: if true, it will move fast until the last 50mm until "supposed" origin and move slowly until it hits the limit switch
  //           if false, it will move slowly from the beginning until it hits the limit switch
  // The function should reference motor_coordinates variable to estimate where it is
  int dx = -(MM_PER_SQUARE*3 + GRAVEYARD_GAP)-motor_coordinates[0]; // Need to offset by negative 3 squares (in mm)
  int dy = -motor_coordinates[1];

  // Sets direction
  digitalWrite(DIR_PIN[0], dx > 0);
  digitalWrite(DIR_PIN[1], dy > 0);
  int ret;

  // Moves to a bit short of where origin is
  move_motor_to_coordinate(-(MM_PER_SQUARE*3 + GRAVEYARD_GAP)+offset, offset, false, FAST_STEP_DELAY);

  // if (ret = move_motor_to_coordinate(offset, offset, false, FAST_STEP_DELAY)) return ret;

  // // Slowly moves towards origin until hits x limit switch
  // int counter = 0;
  // while (!digitalRead(LIMIT_PIN[0])) {
  //   stepper_square_wave(0, SLOW_STEP_DELAY);
  //   counter++;
  //   if (counter > ORIGIN_RESET_TIMEOUT) return 2;  // Motor unable to recenter to origin
  // }

  // // Slowly moves towards origin until hits y limit switch
  // counter = 0;
  // while (!digitalRead(LIMIT_PIN[2])) {
  //   stepper_square_wave(1, SLOW_STEP_DELAY);
  //   counter++;
  //   if (counter > ORIGIN_RESET_TIMEOUT) return 2;  // Motor unable to recenter to origin
  // }

  // // Move motor slightly off origin, so limit switches are not held
  // if (ret = move_motor_to_coordinate(offset, offset, false, FAST_STEP_DELAY)) return ret;

  // Updates current motor coordinates
  motor_coordinates[0] = offset;
  motor_coordinates[1] = offset;

  return 0;
}

// ############################################################
// #                     JOYSTICK CONTROL                     #
// ############################################################

// JOYSTICK CONTROL (each corresponds to the pin number) (first index is for white's joystick, second index is for black's joystick)
// Joystick is active LOW, so connect the other pin to GND
// Changes made: Now this just stores the value read by digital pin on Arduino Mega
// The arduino mega will periodically send values back via I2C
// Active LOW, so initialize to 1
int8_t JOYSTICK_POS_X_VALUE[] = { 1, 1 };
int8_t JOYSTICK_POS_Y_VALUE[] = { 1, 1 };
int8_t JOYSTICK_NEG_X_VALUE[] = { 1, 1 };
int8_t JOYSTICK_NEG_Y_VALUE[] = { 1, 1 };
int8_t JOYSTICK_BUTTON_VALUE[] = { 1, 1 };

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

bool update_joystick_values() {
  // Read joystick values from I2C
  // Return true if successful, false otherwise
  Wire.requestFrom(SUBORDINATE_ADDRESS, 2);  // Request 2 bytes from slave
  if (Wire.available() < 2) {
    Serial.println("NO DATA");
    return false;  // Not enough data received
  }
  uint8_t data1 = Wire.read();  // Read first byte
  uint8_t data2 = Wire.read();  // Read second byte
  // Combine the two bytes into a single 16-bit value
  // Note, data1 is lower byte and data2 is higher byte
  // So LSB of data2 is the 8th bit of the combined value
  uint16_t combinedData = data1 | (data2 << 8);  // Combine the two bytes
  // Print the received data for debugging
  // Serial.print("Received data: ");
  // Serial.print(combinedData, BIN); // Print in binary format
  // Serial.print(" (0x");
  // Serial.print(combinedData, HEX); // Print in hexadecimal format
  // Serial.println(")");
  // Update joystick values based on the received data
  JOYSTICK_POS_X_VALUE[0] = (combinedData >> JOYSTICK_0_POS_X_INDEX) & 1;
  JOYSTICK_POS_X_VALUE[1] = (combinedData >> JOYSTICK_1_POS_X_INDEX) & 1;
  JOYSTICK_POS_Y_VALUE[0] = (combinedData >> JOYSTICK_0_POS_Y_INDEX) & 1;
  JOYSTICK_POS_Y_VALUE[1] = (combinedData >> JOYSTICK_1_POS_Y_INDEX) & 1;
  JOYSTICK_NEG_X_VALUE[0] = (combinedData >> JOYSTICK_0_NEG_X_INDEX) & 1;
  JOYSTICK_NEG_X_VALUE[1] = (combinedData >> JOYSTICK_1_NEG_X_INDEX) & 1;
  JOYSTICK_NEG_Y_VALUE[0] = (combinedData >> JOYSTICK_0_NEG_Y_INDEX) & 1;
  JOYSTICK_NEG_Y_VALUE[1] = (combinedData >> JOYSTICK_1_NEG_Y_INDEX) & 1;
  JOYSTICK_BUTTON_VALUE[0] = (combinedData >> JOYSTICK_0_BUTTON_INDEX) & 1;
  JOYSTICK_BUTTON_VALUE[1] = (combinedData >> JOYSTICK_1_BUTTON_INDEX) & 1;
  return true;
}

// ############################################################
// #                           MAIN                           #
// ############################################################

void setup() {
  Serial.begin(9600);
  Wire.begin();
  delay(1000);  // Wait for serial monitor to open
  Serial.println("Starting up...");
  // MOTOR pins
  pinMode(PUL_PIN[0], OUTPUT);
  pinMode(DIR_PIN[0], OUTPUT);
  pinMode(PUL_PIN[1], OUTPUT);
  pinMode(DIR_PIN[1], OUTPUT);
  piece_picker.attach(PIECE_PICKER_PIN);
  move_motor_to_origin(0); // Move motor to origin with offset 0
  motor_x = 0;
  motor_y = 0;
  servo_angle = 0;
  value_changed = false;
  servo_value_changed = false;
}


void loop() {
  value_changed = false;  // Reset value changed flag
  servo_value_changed = false;  // Reset servo value changed flag
  update_joystick_values();  // Update joystick values from I2C
  if (JOYSTICK_POS_X_VALUE[0] == 0) {
    // Move motor to right. 
    Serial.println("Moving motor to right");
    motor_x += 33;
    value_changed = true;
  } else if (JOYSTICK_NEG_X_VALUE[0] == 0) {
    // Move motor to left.
    Serial.println("Moving motor to left");
    motor_x -= 33;
    value_changed = true;
  }

  if (JOYSTICK_POS_Y_VALUE[0] == 0) {
    // Move motor up.
    Serial.println("Moving motor up");
    motor_y += 33;   
    value_changed = true;
  } else if (JOYSTICK_NEG_Y_VALUE[0] == 0) {
    // Move motor down.
    Serial.println("Moving motor down");
    motor_y -= 33;
    value_changed = true;
  }

  if (JOYSTICK_POS_Y_VALUE[1] == 0) {
    // Move servo up.
    Serial.println("Moving servo up");
    servo_angle += 10;
    if (servo_angle > 180) {
      servo_angle = 180;
    }
    servo_value_changed = true;
  } else if (JOYSTICK_NEG_Y_VALUE[1] == 0) {
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
    move_motor_to_coordinate(motor_x, motor_y, false, FAST_STEP_DELAY);  // Move motor to new coordinates
  }

  if (servo_value_changed) {
    Serial.print("Moving servo to ");
    Serial.print(servo_angle);
    Serial.println(" degrees");
    piece_picker.write(servo_angle);  // Move servo to new angle
  }
}
