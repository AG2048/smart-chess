// Include
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "FastLED.h"
#include "ArduinoSTL.h"
#include "Board.h"
#include "MemoryFree.h"
#include "Piece.h"
#include "PieceType.h"
#include "Timer.h"

// OLED DEFINES
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library. 
// On an arduino MEGA 2560: 20(SDA), 21(SCL)

#define OLED_RESET     -1 // Reset pin (-1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS_ONE 0x3C // Address is 0x3D for 128x64
#define SCREEN_ADDRESS_TWO 0x3D  // Apparently the next address is 0x3D not 0x7A on the board.

Adafruit_SSD1306 * display_one;
Adafruit_SSD1306 * display_two;

// ############################################################
// #                    GAME STATE CONTROL                    #
// ############################################################

enum GameState {
  GAME_POWER_ON,    // Power on the board, motors calibrate
  GAME_IDLE,        // No game is being played yet, waiting for a game to start
  GAME_INITIALIZE,  // Game started, initialize the board
  GAME_BEGIN_TURN,  // Begin a turn - generate moves, remove illegal moves,
                    // check for checkmate/draw
  GAME_WAIT_FOR_SELECT,  // Joystick moving, before pressing select button on
                         // valid piece
  GAME_WAIT_FOR_MOVE,  // Joystick moving, after pressing select button on valid
                       // destination
  GAME_MOVE_MOTOR,     // Motors moving pieces
  GAME_END_MOVE,       // Update chess board, see if a pawn can promote
  GAME_WAIT_FOR_SELECT_PAWN_PROMOTION,  // Joystick moving, before pressing
                                        // select button on promotion piece
  GAME_PAWN_PROMOTION_MOTOR,  // Motors moving pieces for pawn promotion
  GAME_END_TURN,              // End a turn - switch player
  GAME_OVER_WHITE_WIN,        // White wins
  GAME_OVER_BLACK_WIN,        // Black wins
  GAME_OVER_DRAW,             // Draw
  GAME_RESET                  // Reset the game
};

// Game state
GameState game_state;

Timer game_timer;

// ############################################################
// #                  GAME STATUS VARIABLES                   #
// ############################################################

// 0 for white to move, 1 for black to move
bool player_turn;

// 0 for player is not under check, 1 for player is under check
bool current_player_under_check;

// Sources of check, if any (x, y coordinates of the piece that is checking the
// king)
std::vector<std::pair<int8_t, int8_t>> sources_of_check;

// User selection memory
int8_t selected_x;
int8_t selected_y;
int8_t destination_x;
int8_t destination_y;
int8_t capture_x;
int8_t capture_y;
int8_t promotable_pawn_x;
int8_t promotable_pawn_y;
int8_t promotion_type;  // 0 for queen, 1 for rook, 2 for bishop, 3 for knight
int8_t previous_destination_x = 0;
int8_t previous_destination_y = 0;
int8_t previous_selected_x = 0;
int8_t previous_selected_y = 0;
int number_of_turns = 0;

bool draw_three_fold_repetition;  // If true, the game is a draw due to three fold repetition
bool draw_fifty_move_rule;        // If true, the game is a draw due to fifty move rule
bool draw_stalemate;              // If true, the game is a draw due to stalemate
bool draw_insufficient_material;  // If true, the game is a draw due to insufficient material

// Graveyard will be updated when a piece is captured - must consider cases of: 
// 1. normal piece captured - it can immediately replace a temp piece (update graveyard by moving the pawn to the graveyard)
// 2. temp piece captured - move temp piece to graveyard...
// 3. pawn promoted - move pawn to graveyard, and replace with promoted piece (it could be a temp piece or piece from graveyard)
int8_t graveyard[12];  // Graveyard, each for one piece type. 0 is queen, 1 is
                       // rook, 2 is bishop, 3 is knight, 4 is pawn, next values
                       // are same for black. (10 and 11 are for temp pieces,
                       // they start at 8 and decrease to 0)
                       // This records how many pieces of each type are in the graveyard

// This records the position of promoted pawns that are using temp pieces
std::vector<std::pair<int8_t, int8_t>> promoted_pawns_using_temp_pieces; 

// Initialize Memory for the Board Object and Moves Vector (keeping track of possible moves)
Board *p_board;
std::vector<std::pair<int8_t, int8_t>>
    all_moves[8][8];

std::pair<int8_t, int8_t> get_graveyard_empty_coordinate(int8_t piece_type,
                                                   bool color) {
  // Get the graveyard coordinate for a piece type (THE FIRST EMPTY SPACE)
  // piece_type: 1 for queen, 2 for rook, 3 for bishop, 4 for knight, 
  //    5 for pawn, 6 for temp piece 
  // color: 0 for white, 1 for black 
  // Returns the x, y coordinate for this piece in the graveyard as a pair

  // This function should be called BEFORE the graveyard is updated 
  //    (since a value of 0 is used for the first piece of each type) 
  // We treat the white queen moves to coordinate (8,7), 
  //    rook to (8,6) and (8,5), bishop to (8,4) and (8,3), 
  //    knight to (8,2) and (8,1), pawn to (9,7 to 9,0) 
  // We treat the black queen moves to coordinate (-1,0), 
  //    rook to (-1,1) and (-1,2), bishop to (-1,3) and (-1,4), knight to (-1,5) and (-1,6), 
  //    pawn to (-2,0 to -2,7) 
  // The specific coordinate is decided by graveyard[index] and index is decided by piece_type and color

  int8_t graveyard_index = 0;
  if (piece_type == 1) {
    graveyard_index = 0;
  } else if (piece_type == 2) {
    graveyard_index = 1;
  } else if (piece_type == 3) {
    graveyard_index = 2;
  } else if (piece_type == 4) {
    graveyard_index = 3;
  } else if (piece_type == 5) {
    graveyard_index = 4;
  }

  // If black, add 5 to the index
  if (color == 1) {
    graveyard_index += 5;
  }

  // If piece is temp, 10 index is for white, 11 index is for black
  if (piece_type == 6) {
    if (color == 0) {
      graveyard_index = 10;
    } else {
      graveyard_index = 11;
    }
  }

  if (color == 0) {
    // White
    if (piece_type == 1) { // Queen
      return std::make_pair(8, 7);  // only 1 queen can be captured
    } else if (piece_type == 2) { // Rook
      return std::make_pair(8, 6 - graveyard[graveyard_index]);
    } else if (piece_type == 3) { // Bishop
      return std::make_pair(8, 4 - graveyard[graveyard_index]);
    } else if (piece_type == 4) { // Knight
      return std::make_pair(8, 2 - graveyard[graveyard_index]);
    } else if (piece_type == 5) { // Pawn
      return std::make_pair(9, 7 - graveyard[graveyard_index]);
    } else if (piece_type == 6) { // Temp piece
      return std::make_pair(10, 7 - graveyard[graveyard_index]);
    }
  } else {
    // Black
    if (piece_type == 1) {
      return std::make_pair(-1, 0);
    } else if (piece_type == 2) {
      return std::make_pair(-1, 1 + graveyard[graveyard_index]);
    } else if (piece_type == 3) {
      return std::make_pair(-1, 3 + graveyard[graveyard_index]);
    } else if (piece_type == 4) {
      return std::make_pair(-1, 5 + graveyard[graveyard_index]);
    } else if (piece_type == 5) {
      return std::make_pair(-2, graveyard[graveyard_index]);
    } else if (piece_type == 6) {
      return std::make_pair(11, graveyard[graveyard_index]);
    }
  }
}

// ############################################################
// #                       MOTOR CONTROL                      #
// ############################################################

// MOTOR CONTROL VARIABLES
const int8_t PUL_PIN[] = {9, 9}; // x, y
const int8_t DIR_PIN[] = {8, 8}; // x, y
const int8_t LIMIT_PIN[] = {11, 11, 11, 11}; // x-, x+, y-, y+
const int STEPS_PER_MM = 40; // measured value from testing, 3.95cm per rotation (1600 steps)
const int MM_PER_SQUARE = 50; // width of chessboard squares in mm
const int FAST_STEP_DELAY = 50; // half the period of square wave pulse for stepper motor
const int SLOW_STEP_DELAY = 100; // half the period of square wave pulse for stepper motor
const int ORIGIN_RESET_TIMEOUT = 50000; // how many steps motor will attempt to recenter to origin before giving up

int motor_error = 0; // 0: normal operation, 1: misaligned origin, 2: cannot reach origin (stuck)
int motor_coordinates[2] = {0, 0};  // x, y coordinates of the motor in millimeters

// TODO: motor function need a "safe move" parameter, to tell if the motor need to move the piece along an edge, or just straight to the destination
int move_piece_by_motor(int from_x, int from_y, int to_x, int to_y) {
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
  from_x = from_x * MM_PER_SQUARE;
  from_y = from_y * MM_PER_SQUARE;
  to_x = to_x * MM_PER_SQUARE;
  to_y = to_y * MM_PER_SQUARE;
  int ret;

  // Move motor to starting location
  if (ret = move_motor_to_coordinate(from_x, from_y, false, FAST_STEP_DELAY)) return ret;

  delay(1000); // pick up piece
  Serial.println("Pick up piece");

  // Move motor onto edges instead of centers, then move to destination
  if (ret = move_motor_to_coordinate(from_x+(MM_PER_SQUARE/2), from_y+(MM_PER_SQUARE/2), false, FAST_STEP_DELAY)) return ret;
  if (ret = move_motor_to_coordinate(to_x+(MM_PER_SQUARE/2), to_y+(MM_PER_SQUARE/2), true, FAST_STEP_DELAY)) return ret;

  delay(1000); // release piece
  Serial.println("Release piece");

  // Move motor back from edge to centers at final location
  if (ret = move_motor_to_coordinate(to_x, to_y, false, FAST_STEP_DELAY)) return ret;

  return 0;
}

int move_motor_to_coordinate(int x, int y, int axisAligned, int stepDelay) {
  // Move the motor to a specific coordinate
  // x, y: x, y coordinates of the square to move to (IN MILLIMETERS)
  // Move the motor to (x, y)
  // This function should be used to move the motor to a specific coordinate without any checks, it should be used AFTER calibrating the motor
  // The "starting" coordinate is assumed to be motor_coordinates variable (x, y)

  // TODO
  Serial.println("Move motor code running");
  const int dx = x-motor_coordinates[0];
  const int dy = y-motor_coordinates[1];

  // Setting direction
  digitalWrite(DIR_PIN[0], dx > 0);
  digitalWrite(DIR_PIN[1], dy > 0);

  // Check if movement should be axis-aligned or diagonal
  if (axisAligned || dx == 0 || dy == 0) {
    // Move x axis
    for (int i = 0; i < abs(dx)*STEPS_PER_MM; i++) {
      if (digitalRead(LIMIT_PIN[0]) || digitalRead(LIMIT_PIN[1]) || digitalRead(LIMIT_PIN[2]) || digitalRead(LIMIT_PIN[3])) return 1;
      stepper_square_wave(0, stepDelay);
    }

    // Move y axis
    for (int i = 0; i < abs(dy)*STEPS_PER_MM; i++) {
      if (digitalRead(LIMIT_PIN[0]) || digitalRead(LIMIT_PIN[1]) || digitalRead(LIMIT_PIN[2]) || digitalRead(LIMIT_PIN[3])) return 1;
      stepper_square_wave(1, stepDelay);
    }
  } else {
    float error = 0.0; // Track error between expected slope and actual slope
    // Check if x or y has greater change
    if (abs(dx) >= abs(dy)) {
      // Moves the x axis normally, while occasionally incrementing the y axis
      for (int i = 0; i < abs(dx)*STEPS_PER_MM; i++) {
        if (digitalRead(LIMIT_PIN[0]) || digitalRead(LIMIT_PIN[1]) || digitalRead(LIMIT_PIN[2]) || digitalRead(LIMIT_PIN[3])) return 1;
        stepper_square_wave(0, stepDelay);
        error += abs((float)dy/dx); // accumulates the error

        // if error is bigger than what can be adjusted in one step, fix the error
        if (error >= 1.0) { 
          if (digitalRead(LIMIT_PIN[0]) || digitalRead(LIMIT_PIN[1]) || digitalRead(LIMIT_PIN[2]) || digitalRead(LIMIT_PIN[3])) return 1;
          stepper_square_wave(1, stepDelay);
          error -= 1.0;
        }
      }
    } else {
      // Moves the x axis normally, while occasionally incrementing the y axis
      for (int i = 0; i < abs(dy)*STEPS_PER_MM; i++) {
        if (digitalRead(LIMIT_PIN[0]) || digitalRead(LIMIT_PIN[1]) || digitalRead(LIMIT_PIN[2]) || digitalRead(LIMIT_PIN[3])) return 1;
        stepper_square_wave(1, stepDelay);
        error += abs((float)dx/dy); // accumulates the error

        // if error is bigger than what can be adjusted in one step, fix the error
        if (error >= 1.0) { 
          if (digitalRead(LIMIT_PIN[0]) || digitalRead(LIMIT_PIN[1]) || digitalRead(LIMIT_PIN[2]) || digitalRead(LIMIT_PIN[3])) return 1;
          stepper_square_wave(0, stepDelay);
          error -= 1.0;
        }
      }
    }
  }
  
  // Update motor coordinates
  motor_coordinates[0] = x;
  motor_coordinates[1] = y;

  Serial.print("Moving to (");
  Serial.print(x);
  Serial.print(", ");
  Serial.print(y);
  Serial.println(") in mm");

  return 0;
}

int move_motor_to_origin(int offset) {
  // resets the motor to origin (0, 0) and re-calibrate the motor
  // fastMove: if true, it will move fast until the last 50mm until "supposed" origin and move slowly until it hits the limit switch
  //           if false, it will move slowly from the beginning until it hits the limit switch
  // The function should reference motor_coordinates variable to estimate where it is
  const int dx = -motor_coordinates[0];
  const int dy = -motor_coordinates[1];
  
  // Sets direction
  digitalWrite(DIR_PIN[0], dx > 0);
  digitalWrite(DIR_PIN[1], dy > 0);
  int ret;

  // Moves to a bit short of where origin is
  if (ret = move_motor_to_coordinate(offset, offset, false, FAST_STEP_DELAY)) return ret;

  // Slowly moves towards origin until hits x limit switch
  int counter = 0;
  while (!digitalRead(LIMIT_PIN[0])) {
    stepper_square_wave(0, SLOW_STEP_DELAY);
    counter++;
    if (counter > ORIGIN_RESET_TIMEOUT) return 2; // Motor unable to recenter to origin
  }

  // Slowly moves towards origin until hits y limit switch
  counter = 0;
  while (!digitalRead(LIMIT_PIN[2])) {
    stepper_square_wave(1, SLOW_STEP_DELAY);
    counter++;
    if (counter > ORIGIN_RESET_TIMEOUT) return 2; // Motor unable to recenter to origin
  }

  // Move motor slightly off origin, so limit switches are not held
  if (ret = move_motor_to_coordinate(offset, offset, false, FAST_STEP_DELAY)) return ret;

  // Updates current motor coordinates
  motor_coordinates[0] = offset;
  motor_coordinates[1] = offset;

  return 0;
}

void stepper_square_wave(int axis, int stepDelay) {
  // Generates a square wave of period (stepDelay*2)
  digitalWrite(PUL_PIN[axis], HIGH);
  delayMicroseconds(stepDelay);
  digitalWrite(PUL_PIN[axis], LOW);
  delayMicroseconds(stepDelay);
}

// ############################################################
// #                     JOYSTICK CONTROL                     #
// ############################################################

// JOYSTICK CONTROL (each corresponds to the pin number) (first index is for white's joystick, second index is for black's joystick)
// Joystick is active LOW, so connect the other pin to GND
const int8_t JOYSTICK_POS_X_PIN[] = {2, 2};
const int8_t JOYSTICK_POS_Y_PIN[] = {3, 3};
const int8_t JOYSTICK_NEG_X_PIN[] = {4, 4};
const int8_t JOYSTICK_NEG_Y_PIN[] = {5, 5};
const int8_t JOYSTICK_BUTTON_PIN[] = {6, 45};
// User Joystick Location - keeping track of white x, black x, white y, black y
int8_t joystick_x[2];
int8_t joystick_y[2];
bool confirm_button_pressed[2];
// Previous joystick input -- for edge detection (true if the button / joystick was pressed in the previous detection cycle)
bool prev_confirm_button_pressed[2];
bool prev_joystick_neutral[2];
// Promotion Joystick Selection - index of promotion piece selected (0,1,2,3)
int8_t promotion_joystick_selection;  // only happening on one player's turn, so no need for 2
// Joystick selection for idle screen difficulty selection
int8_t idle_joystick_x[2];
int8_t idle_joystick_y[2];
// Memory to see if a player is ready (game starts when both players ready)
bool player_ready[2];
// Remember if the game is in full idle screen or not
#define IDLE_SCREEN_TIMEOUT 6 // 10 seconds
bool in_idle_screen;
uint32_t last_idle_change_time; // last time a button was pressed.

void move_user_joystick_x_y(bool color) {
  // Get input from the joystick and modify the corresponding joystick coord for the color (player)
  // color: 0 for white, 1 for black
  // Update the joystick coordinates and confirm button pressed flag
  // Result: <color>_joystick_<x,y> ++ or -- (mod 8),
  // <color>_confirm_button_pressed = true if confirm button is pressed from
  // neutral state

  // Read the joystick values -- low is pressed, high is not pressed
  int8_t x_val = digitalRead(JOYSTICK_POS_X_PIN[color]);
  int8_t y_val = digitalRead(JOYSTICK_POS_Y_PIN[color]);
  int8_t neg_x_val = digitalRead(JOYSTICK_NEG_X_PIN[color]);
  int8_t neg_y_val = digitalRead(JOYSTICK_NEG_Y_PIN[color]);
  int8_t button_val = digitalRead(JOYSTICK_BUTTON_PIN[color]);

  // Update the joystick values if joystick WAS neutral and now has a value.
  // Update neutral flag. Note active low for button (pressed is low 0)
  if (prev_joystick_neutral[color]) {
    // If previous joystick WAS neutral
    bool change_happened =
        false;  // For displaying, only serial printif change happened.
    if (x_val == 0) {
      joystick_x[color]++;
      prev_joystick_neutral[color] = false;
      change_happened = true;
    } else if (neg_x_val == 0) {
      joystick_x[color]--;
      prev_joystick_neutral[color] = false;
      change_happened = true;
    } else if (y_val == 0) {
      joystick_y[color]++;
      prev_joystick_neutral[color] = false;
      change_happened = true;
    } else if (neg_y_val == 0) {
      joystick_y[color]--;
      prev_joystick_neutral[color] = false;
      change_happened = true;
    }
    // joystick x and y loop from 0 to 7
    joystick_x[color] = (joystick_x[color] + 8) % 8;
    joystick_y[color] = (joystick_y[color] + 8) % 8;
    // if no joystick movement, joystick_neutral stays true

    // TEMP DISPLAY CODE:
    if (change_happened) {
      serial_display_board_and_selection();

      // Display OLED
      display_turn_select(color, joystick_x[color], joystick_y[color], selected_x, selected_y, destination_x, destination_y, display_one, display_two);
    }
  } else {
    // If previous joystick was NOT neutral
    if (x_val == 1 && neg_x_val == 1 && y_val == 1 && neg_y_val == 1) {
      prev_joystick_neutral[color] = true;
    }
  }

  // Update the confirm button pressed flag
  // Note active low for button (pressed is low 0)
  // Flag is true if button was pressed from previous state
  if (button_val == 0 && prev_confirm_button_pressed[color] == 0) {
    confirm_button_pressed[color] = true;
  } else {
    confirm_button_pressed[color] = false;  
    // Note: this also automatically reset the confirm_button_pressed flag
    // If your state machine requires multiple cycles to "load in" the confirm
    // button press, you need to remove this line from this function
  }
  // Update the previous confirm button pressed flag (true if button was
  // pressed, but button is active low)
  prev_confirm_button_pressed[color] = !button_val;  // active low
}

void move_user_joystick_promotion(bool color) {
  // Get input from joystick, but the promotion_joystick_selection just loops from 0 to 3 
  // color: 0 for white, 1 for black 
  // Update the promotion_joystick_selection, and confirm button pressed flag 
  // Result: promotion_joystick_selection ++ or --, confirm_button_pressed = true if confirm button is pressed from neutral state

  // Read the joystick values -- low is pressed, high is not pressed
  int8_t x_val = digitalRead(JOYSTICK_POS_X_PIN[color]);
  int8_t y_val = digitalRead(JOYSTICK_POS_Y_PIN[color]);
  int8_t neg_x_val = digitalRead(JOYSTICK_NEG_X_PIN[color]);
  int8_t neg_y_val = digitalRead(JOYSTICK_NEG_Y_PIN[color]);
  int8_t button_val = digitalRead(JOYSTICK_BUTTON_PIN[color]);

  // Update the joystick values if joystick WAS neutral and now has a value.
  // Update neutral flag. Note active low for button (pressed is low 0)
  bool change_happened = false;  // For displaying, only serial printif change happened.

  if (prev_joystick_neutral[color]) {
    // If previous joystick WAS neutral
    if (x_val == 0) {
      promotion_joystick_selection = (promotion_joystick_selection + 1) % 4;
      prev_joystick_neutral[color] = false;
      change_happened = true;
    } else if (neg_x_val == 0) {
      promotion_joystick_selection = (promotion_joystick_selection - 1) % 4;
      prev_joystick_neutral[color] = false;
      change_happened = true;
    }
    // if no joystick movement, joystick_neutral stays true
  } else {
    // If previous joystick was NOT neutral
    if (x_val == 1 && neg_x_val == 1 && y_val == 1 && neg_y_val == 1) {
      prev_joystick_neutral[color] = true;
    }
  }

  if (change_happened) {
    // TEMP DISPLAY CODE:
    Serial.print("color: ");
    Serial.print(color);
    Serial.print(" promotion_joystick_selection: ");
    Serial.println(promotion_joystick_selection);

    display_promotion(color, promotion_joystick_selection, display_one, display_two);
  }

  // Update the confirm button pressed flag
  // Note active low for button (pressed is low 0)
  // Flag is true if button was pressed from previous state
  if (button_val == 0 && prev_confirm_button_pressed[color] == 0) {
    confirm_button_pressed[color] = true;
  } else {
    confirm_button_pressed[color] = false;
  }

  // Update the previous confirm button pressed flag (true if button was
  // pressed, but button is active low)
  prev_confirm_button_pressed[color] = !button_val;  // active low
}

void move_user_joystick_idle(bool color, bool update_y, int8_t max_y) {
  // Get input from the joystick and modify the corresponding joystick coord for the color (player)
  // update_y == true: then update y. Else don't update y.
  // max_y is usually the highest difficulty level before looping back.
  // color: 0 for white, 1 for black
  // Update the joystick coordinates and confirm button pressed flag
  // Result: <color>_joystick_<x,y> ++ or -- (no longer mod 8, depends on the selection data)
  // <color>_confirm_button_pressed = true if confirm button is pressed from
  // neutral state

  // Read the joystick values -- low is pressed, high is not pressed
  int8_t x_val = digitalRead(JOYSTICK_POS_X_PIN[color]);
  int8_t y_val = digitalRead(JOYSTICK_POS_Y_PIN[color]);
  int8_t neg_x_val = digitalRead(JOYSTICK_NEG_X_PIN[color]);
  int8_t neg_y_val = digitalRead(JOYSTICK_NEG_Y_PIN[color]);
  int8_t button_val = digitalRead(JOYSTICK_BUTTON_PIN[color]);

  // Update the joystick values if joystick WAS neutral and now has a value.
  // Update neutral flag. Note active low for button (pressed is low 0)
  bool change_happened = false;  // For displaying, only serial printif change happened.
  if (prev_joystick_neutral[color]) {
    // a joystick movement happened, reset the idle timer

    // If previous joystick WAS neutral
    if (x_val == 0) {
      idle_joystick_x[color]++;
      prev_joystick_neutral[color] = false;
      change_happened = true;
    } else if (neg_x_val == 0) {
      idle_joystick_x[color]--;
      prev_joystick_neutral[color] = false;
      change_happened = true;
    } else if (y_val == 0 && update_y) {
      idle_joystick_y[color]++;
      prev_joystick_neutral[color] = false;
      change_happened = true;
    } else if (neg_y_val == 0 && update_y) {
      idle_joystick_y[color]--;
      prev_joystick_neutral[color] = false;
      change_happened = true;
    }
    // joystick x and y loop from 0 to 7
    idle_joystick_x[color] = (idle_joystick_x[color]+2) % 2;
    idle_joystick_y[color] = (idle_joystick_y[color]+max_y) % max_y;
    // if no joystick movement, joystick_neutral stays true
  } else {
    // If previous joystick was NOT neutral, set to neutral if all joystick values are high
    if (x_val == 1 && neg_x_val == 1 && y_val == 1 && neg_y_val == 1) {
      prev_joystick_neutral[color] = true;
    }
  }

  if (change_happened) {
    if (in_idle_screen) {
      // We are currently in idle screen, just break out of the idle state. Don't update the joystick values
      in_idle_screen = false;
    }
    last_idle_change_time = game_timer.read();
    display_idle_screen(game_timer, in_idle_screen, idle_joystick_x[1], idle_joystick_y[1], display_two, 1);
    display_idle_screen(game_timer, in_idle_screen, idle_joystick_x[0], idle_joystick_y[0], display_one, 0);
  }

  // Update the confirm button pressed flag
  // Note active low for button (pressed is low 0)
  // Flag is true if button was pressed from previous state
  if (button_val == 0 && prev_confirm_button_pressed[color] == 0) {
    last_idle_change_time = game_timer.read(); // a button press happened, reset the idle timer
    if (in_idle_screen) {
      // We are currently in idle screen, just break out of the idle state. Don't update the joystick values
      in_idle_screen = false; // Issue: 
      display_idle_screen(game_timer, in_idle_screen, idle_joystick_x[1], idle_joystick_y[1], display_two, 1);
      display_idle_screen(game_timer, in_idle_screen, idle_joystick_x[0], idle_joystick_y[0], display_one, 0);
    } else {
      confirm_button_pressed[color] = true;
    }
  } else {
    confirm_button_pressed[color] = false;  
    // Note: this also automatically reset the confirm_button_pressed flag
    // If your state machine requires multiple cycles to "load in" the confirm
    // button press, you need to remove this line from this function
  }
  // Update the previous confirm button pressed flag (true if button was
  // pressed, but button is active low)
  prev_confirm_button_pressed[color] = !button_val;  // active low
}

// ############################################################
// #                        LED CONTROL                       #
// ############################################################
// LED variables
const int stripLen = 64;
const int8_t LED_BOARD_PIN = 12;
const int LED_BRIGHTNESS = 10;
// Array of CRGB objects corresponding to LED colors / brightness (0 indexed)
struct CRGB led_display[stripLen];
// If we wish to add cosmetic things, add another array of "previous states", or "pre-set patterns" or other stuff
// Also we may need a separate LED strip timer variable to keep track "how long ago was the last LED update"
// A function to update LED strip in each situation

// ############################################################
// #                       OLED DISPLAY                       #
// ############################################################
// Keep track of when IDLE is supposed to switch
uint32_t last_interacted_time, last_idle_change;

// So we don't display idle or waiting screen multiple times...
bool idle_one, idle_two;

void free_displays() {
  // Serial.println("Freeing displays");
  // Serial.println(freeMemory());
  delete display_one;
  // Serial.println(freeMemory());
  delete display_two;
  // Serial.println(freeMemory());
  display_one = nullptr;
  display_two = nullptr;
  // Serial.println("After free displays");
  // Serial.println(freeMemory());

  // void* testAlloc = malloc(1024);
  // if (testAlloc) {
  //   Serial.println("Memory was freed and is reusable!");
  //   free(testAlloc);
  // } else {
  //   Serial.println("Memory is fragmented and unusable.");
  // }
}

void display_init() {

  // Initializing flags
  last_interacted_time = 0;
  last_idle_change = 0;
  idle_one = 1; // Display idle screen one first by default
  idle_two = 0;

  // Print space before initializing displays
  // Serial.print("Before display init: ");
  // Serial.println(freeMemory());

  display_one = new Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
  display_two = new Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

  // Serial.print("After display init: ");
  // Serial.println(freeMemory());

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display_one->begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS_ONE)) {
    for(;;); // Don't proceed, loop forever
  }

  if(!display_two->begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS_TWO)) {
    for(;;); // Don't proceed, loop forever
  }

  // Serial.print("After display begin: ");
  // Serial.println(freeMemory());

  // Serial.println("OLEDOK");

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  // display_one->display();
  display_one->setTextColor(SSD1306_WHITE);

  // display_two->display();
  display_two->setTextColor(SSD1306_WHITE);
}

void display_idle_screen(Timer timer, bool is_idle, bool is_display_computer, uint8_t comp_diff, Adafruit_SSD1306 *display, int8_t display_id) {
  /* display_idle_screen arguments
  Timer timer, passed in so we can poll and see the current time and hence how much time has passed

  is_idle, true if we are currently displaying an idle screen

  screen_select to know which screen we are on; 1 for choosing player opponent, 0 for changing computer opponent difficulty

  comp_diff to know the current difficulty to be displayed when setting up computer opponents (0 to 19)

  display to display to a specific display
  */

  if (!is_idle) {
    // Stop the scrolling that may be happening
    display->stopscroll();

    if (!is_display_computer) {
      // Player
      display_select_player(display);
    } else {
      display_select_computer(display, comp_diff);
    }

  }

}

// Displayes who is making what moves
// This function should only be called when the joystick actually is moved
// OR after "selected_x" or "destination_x" is changed
void display_turn_select(int8_t player_id, int8_t joystick_x, int8_t joystick_y, int8_t selected_x, int8_t selected_y, int8_t destination_x, int8_t destination_y, Adafruit_SSD1306 *display_one, Adafruit_SSD1306 *display_two) {
  /* display_turn_select args

    selector ; indicates which player is selecting a move right now (1 for P1, 2 for P2)

    joystick_x, joystick_y ; coordinate currently selected via joystick. will be constantly updated

    selected_x, selected_y ; the first move coordinate that player selects
      both assumed to be -1 if not set

    destination_x, destination_y ; the second move coordinate that player selects
      both assumed to be -1 if not set
  */
  char msg[100];
  // Convert joystick_x and joystick_y into correct ASCII values
  // joystick_x + 'a'
  // joystick_y + '1'

  // Display what the player's cursor is ON

  // Clear the display
  display_one->clearDisplay();
  display_one->setTextSize(1);
  display_one->setCursor(0, 0);
  display_two->clearDisplay();
  display_two->setTextSize(1);
  display_two->setCursor(0, 0);

  // If we have not yet selected
  if (selected_x == -1 && selected_y == -1) { // if the user is still selecting their first move
    sprintf(msg, "Moving %c%c...", (joystick_x + 'a'), (joystick_y + '1'));

    if (player_id == 1) {
      // Player id 1 is selecting, corresponds to screen id 2... cuz index...

      display_two->print(msg);
      msg[0] = 'm'; // display lower case m
      display_one->println(F("Your opponent is "));
      display_one->print(msg);

    } else {
      // Display on the other screen
      display_one->print(msg);
      msg[0] = 'm'; // display lower case m
      display_two->println(F("Your opponent is "));
      display_two->print(msg);
    }

  // A piece has been successfully selected, now selecting second move
  } else if (destination_x == -1 && destination_y == -1) { // if the user is selecting their second move
    // Display the Second coordinate player is selecting

    sprintf(msg, "Moving %c%c to %c%c...", (selected_x + 'a'), (selected_y + '1'), (joystick_x + 'a'), (joystick_y + '1'));

    // msg: Moving from selected coords to joystick coords (all converted into board coords)

    if (player_id == 1) {
      display_two->print(msg);
      msg[0] = 'm';
      display_one->println(F("Your opponent is "));
      display_one->print(msg);  

    } else {
      display_one->print(msg);
      msg[0] = 'm';
      display_two->println(F("Your opponent is "));
      display_two->print(msg);
    }
  }

  display_one->display();
  display_two->display();
}

// This part is run by motor code once
// Same argument as display_turn_select, but no joystick_x and joystick_y. Show "you are moving" or "opponent is moving" from xxx to yyy
void display_while_motor_moving(int8_t player_id, int8_t selected_x, int8_t selected_y, int8_t destination_x, int8_t destination_y, Adafruit_SSD1306 *display_one, Adafruit_SSD1306 *display_two) {
  /* display_while_motor_moving args

    selector ; indicates which player is selecting a move right now (1 for P1, 2 for P2)

    selected_x, selected_y ; the first move coordinate that player selects
      both assumed to be -1 if not set

    destination_x, destination_y ; the second move coordinate that player selects
      both assumed to be -1 if not set
  */
  char msg[100];

  // Clear the display
  display_one->clearDisplay();
  display_one->setTextSize(1);
  display_one->setCursor(0, 0);
  display_two->clearDisplay();
  display_two->setTextSize(1);
  display_two->setCursor(0, 0);

  // At this time of code, we always have a valid selected_x and selected_y, and destination_x and destination_y
  // player_id is whoever is moving
  
  sprintf(msg, "Moving %c%c to %c%c...", (selected_x + 'a'), (selected_y + '1'), (destination_x + 'a'), (destination_y + '1'));

  if (player_id == 1) {
    display_two->print(msg);
    msg[0] = 'm';
    display_one->println(F("Your opponent is "));
    display_one->print(msg);

  } else {
    display_one->print(msg);
    msg[0] = 'm';
    display_two->println(F("Your opponent is "));
    display_two->print(msg);
  }

  display_one->display();
  display_two->display();
}

// Only call in the promotion_joystick when joystick is moving, or right before entering the promotion select state
void display_promotion(int8_t player_promoting, int8_t piece, Adafruit_SSD1306 *display_one, Adafruit_SSD1306 *display_two) {
  /* display_promotion arguments

    player_promoting to know which player is promoting; that way, we know which player to display "opponent is promoting..." message
    
    piece to know which piece the promoting player is currently hovering, so we can print it on their display->

  */
  display_one->clearDisplay();
  display_one->setTextSize(1);
  display_one->setCursor(0, 0);
  display_two->clearDisplay();
  display_two->setTextSize(1);
  display_two->setCursor(0, 0);

  if (player_promoting == 1) {

    // Player black is promoting   
    display_two->println(F("You are currently promoting to a..."));

    switch(piece) {
      // Order is Queen Rook Bishop Knight
      case 0:
        display_two->print(F("QUEEN"));
        break;
      case 3:
        display_two->print(F("KNIGHT"));
        break;
      case 1:
        display_two->print(F("ROOK"));
        break;
      case 2:
        display_two->print(F("BISHOP"));
        break;
    }
    // Player 2 is waiting
    display_one->print(F("Your opponent is promoting..."));
    display_one->display();
    display_two->display();
    
  } else {

    // Player 2 is promoting   
    display_one->println(F("You are currently promoting to a..."));

    switch(piece) {
      case 0:
        display_one->print(F("QUEEN"));
        break;
      case 3:
        display_one->print(F("KNIGHT"));
        break;
      case 1:
        display_one->print(F("ROOK"));
        break;
      case 2:
        display_one->print(F("BISHOP"));
        break;
    }
    // Player 1 is waiting
    display_two->print(F("Your opponent is promoting..."));
    display_one->display();
    display_two->display();

  }
}

void display_game_over(int8_t winner, int8_t draw, Adafruit_SSD1306 *display_one, Adafruit_SSD1306 *display_two) {
  /* display_game_over arguments

    winner to know which player won: 
    0 for white
    1 for black

    draw to know what kind of draw it is
      0 for no draw
      1 for 50 move draw
      2 for 3 fold repetition    
      3 for draw via insufficient material
    
    depending on winner value, the corresponding helper functions will be called to display information to each display

  */
  if (draw == 0){
    // display winner
    if (winner == 0) {
      display_winner(0, display_one);
      display_loser(1, display_two);
    } else {
      display_winner(1, display_two);
      display_loser(0, display_one);
    }

  } else {
    // Display draw
    display_draw(draw, display_one);
    display_draw(draw, display_two);
  }
}


void display_select_player(Adafruit_SSD1306 *display) {
  display->clearDisplay();
  display->setTextSize(1);
  // Draw bitmap
  display->setCursor(0, 0);
  display->println(F(      "PLAYER"));
  display->println(F("HUMAN  Comp>"));
  display->print(F("MODE"));
  display->display();
}

void display_select_computer(Adafruit_SSD1306 *display, uint8_t comp_diff) {
  display->clearDisplay();
  display->setTextSize(1);
  display->setCursor(0, 0);
  display->println(F("STOCKFISH"));
  display->print(F("<Human  LV:"));
  // Draw bitmap
  display->print(comp_diff+1, DEC);
  display->display();
}

void display_idle_scroll(Adafruit_SSD1306 *display) {
  display->clearDisplay();
  display->setTextSize(2);
  display->setCursor(0, 0);
  display->println(F(" > Spark <"));
  display->print(F("SMARTCHESS"));
  display->display();
  // display->startscrollleft(0, 1); // (row 1, row 2). Scrolls just the first row of text.
  display->startscrollright(2, 3); // SSD1306 can't handle two concurrent scroll directions.

  Serial.print("Free memory: ");
  Serial.println(freeMemory());
}

void display_draw(int8_t draw, Adafruit_SSD1306 *display) {

  /*
    0 for no draw
    1 for 50 move draw
    2 for 3 fold repetition    
    3 for draw via insufficient material 

  */

  // Display to player 1
  display->clearDisplay();
  display->setTextSize(1);
  display->setCursor(0, 0);

  if (draw == 1) {

    display->println(F("50 move!"));

  } else if (draw == 2) {

    display->println(F("3 fold!"));

  } else if (draw == 3) {

    display->println(F("Stalemate!"));

  } else if (draw == 4) {

    display->println(F("Insufficient material!"));

  } else {
    return;
  }

  display->display();
}

void display_winner(int8_t winner, Adafruit_SSD1306 *display) {

  display->clearDisplay();
  display->setTextSize(1);
  display->setCursor(0, 0);

  if (winner == 0) display->print("White");
  else display->print("Black");
  display->println(F(" wins!"));

  display->display();

}

void display_loser(int8_t loser, Adafruit_SSD1306 *display) {

  display->clearDisplay();
  display->setTextSize(1);
  display->setCursor(0, 0);

  if (loser == 0) display->print("White");
  else display->print("Black");
  display->print(F(" loses!"));

  display->display();
}
// ############################################################
// #                           UTIL                           #
// ############################################################

// Display in serial monitor
const char CHESS_PIECE_CHAR[] = {' ', 'K', 'Q', 'B', 'N', 'R', 'P',
                                 'k', 'q', 'b', 'n', 'r', 'p'};

void serial_display_board_and_selection() {
  // print the "-------..." in for loop
  for (uint8_t i = 0; i < 51; i++) {
    Serial.print("-");
  }
  Serial.println("");
  for (int8_t row = 7; row >= 0; row--) {
    for (int8_t col = 0; col < 8; col++) {
      bool showed = false;
      for (int i = 0; i < all_moves[selected_y][selected_x].size(); i++) {
        if (selected_x == -1 || selected_y == -1) {
          break;
        }
        if (all_moves[selected_y][selected_x][i].second%8 == col &&
            all_moves[selected_y][selected_x][i].second/8 == row) {
          Serial.print("X");
          Serial.print("\t");
          showed = true;
          break;
        }
        if (all_moves[selected_y][selected_x][i].first%8 == col &&
            all_moves[selected_y][selected_x][i].first/8 == row) {
          Serial.print("O");
          Serial.print("\t");
          showed = true;
          break;
        }
      }
      if (showed) {
        continue;
      }
      if (p_board->pieces[row][col]->get_type() == EMPTY) {
        Serial.print(" ");
      } else {
        // Print CHESS_PIECE_CHAR[p_board->pieces[row][col]->get_type() +
        // 6*p_board->pieces[row][col]->get_color()];
        Serial.print(
            CHESS_PIECE_CHAR[p_board->pieces[row][col]->get_type() +
                             6 * p_board->pieces[row][col]->get_color()]);
      }
      Serial.print("\t");
    }
    Serial.println();
    if (joystick_y[player_turn] != row) {
      Serial.println();
    } else {
      // print "^" below the row
      for (uint8_t i = 0; i < 8; i++) {
        if (joystick_x[player_turn] == i) {
          Serial.print("^");
        } else {
          Serial.print(" ");
        }
        Serial.print("\t");
      }
      Serial.println();
    }
  }
}

int coordinate_to_index(int x, int y) {
  // Returns equivalent index of LED of an (x, y) coordinate
  if (y % 2 == 0) {
    return y * 8 + x;
  } else {
    return y * 8 + 8 - x - 1;
  }
}

// ############################################################
// #                           MAIN                           #
// ############################################################

void setup() {
  Serial.begin(9600);
  Serial.println(freeMemory());

  // Initial game state
  game_state = GAME_POWER_ON;
}

void loop() {
  // Serial.print("Current State: ");
  // Serial.println(game_state);
  // Serial.print("Free memory: ");
  // Serial.println(freeMemory());

  FastLED.show();  // Display board via LEDs

  delay(50);  // Delay for 50ms - just a standard delay (although not necessary)

  // State machine, during each loop, we are in a certain state, each state handles the transition to the next state
  // Chained if block below does not have an else case
  if (game_state == GAME_POWER_ON) {
    // Power on the board, motors calibrate, initialize pins, set LED to blank / off

    game_timer.start();  // Start the game timer

    // LED pin initialize:
    LEDS.addLeds<WS2812B, LED_BOARD_PIN, GRB>(led_display, stripLen);
    FastLED.setBrightness(LED_BRIGHTNESS);

    // Pins initialization
    // JOYSTICK are active low, so set them as INPUT_PULLUP (default HIGH)
    pinMode(JOYSTICK_POS_X_PIN[0], INPUT_PULLUP);
    pinMode(JOYSTICK_POS_Y_PIN[0], INPUT_PULLUP);
    pinMode(JOYSTICK_NEG_X_PIN[0], INPUT_PULLUP);
    pinMode(JOYSTICK_NEG_Y_PIN[0], INPUT_PULLUP);
    pinMode(JOYSTICK_BUTTON_PIN[0], INPUT_PULLUP);
    pinMode(JOYSTICK_POS_X_PIN[1], INPUT_PULLUP);
    pinMode(JOYSTICK_POS_Y_PIN[1], INPUT_PULLUP);
    pinMode(JOYSTICK_NEG_X_PIN[1], INPUT_PULLUP);
    pinMode(JOYSTICK_NEG_Y_PIN[1], INPUT_PULLUP);
    pinMode(JOYSTICK_BUTTON_PIN[1], INPUT_PULLUP);
    // MOTOR pins
    pinMode(PUL_PIN[0], OUTPUT);
    pinMode(DIR_PIN[0], OUTPUT);
    pinMode(PUL_PIN[1], OUTPUT);
    pinMode(DIR_PIN[1], OUTPUT);
    // Set LED to blank initially
    fill_solid(led_display, stripLen, CRGB(0, 0, 0));

    joystick_x[0] = 4;
    joystick_y[0] = 0;
    joystick_x[1] = 4;
    joystick_y[1] = 7;
    confirm_button_pressed[0] = false;
    confirm_button_pressed[1] = false;
    prev_confirm_button_pressed[0] = true;  // Assume the button is pressed before the game starts, so force user to release the button
    prev_confirm_button_pressed[1] = true;
    prev_joystick_neutral[0] = true;
    prev_joystick_neutral[1] = true;
    
    idle_joystick_x[0] = 0;
    idle_joystick_y[0] = 0;
    idle_joystick_x[1] = 1;
    idle_joystick_y[1] = 1;

    player_ready[0] = false;
    player_ready[1] = false;

    in_idle_screen = false;
    last_idle_change_time = game_timer.read();

    // Initialize the OLED display
    display_init();
    display_idle_screen(game_timer, in_idle_screen, idle_joystick_x[1], idle_joystick_y[1], display_two, 1);
    display_idle_screen(game_timer, in_idle_screen, idle_joystick_x[0], idle_joystick_y[0], display_one, 0);

    // TODO
    game_state = GAME_IDLE;
  } else if (game_state == GAME_IDLE) {
    // No game is being played yet, waiting for a game to start
    // Wait for game mode select (player / computer / difficulty)
    // Wait for a button press to start the game
    // If button pressed, initialize the game
    // If button not pressed, stay in this state

    // Enter idle screen if no button is pressed for 60 seconds
    // Debug prints:
    // Serial.print("Time since last change: ");
    // Serial.println(game_timer.read() - last_idle_change_time);
    // Serial.print("Last idle change time: ");
    // Serial.println(last_idle_change_time);
    // Serial.print("In idle screen: ");
    // Serial.println(in_idle_screen);
    // Serial.print("Player 1 ready: ");
    // Serial.println(player_ready[0]);
    // Serial.print("Player 2 ready: ");
    // Serial.println(player_ready[1]);

    uint32_t time_since_last_change = game_timer.read() - last_idle_change_time;
    // Serial.print("Time since last change: ");
    // Serial.println(time_since_last_change);
    // Serial.print("DID WE TIMEOUT? ");
    // Serial.println(time_since_last_change > IDLE_SCREEN_TIMEOUT * 1000);
    if (time_since_last_change > IDLE_SCREEN_TIMEOUT * 1000 && !in_idle_screen) {
      in_idle_screen = true;
      // reset confirm button pressed
      confirm_button_pressed[0] = false;
      confirm_button_pressed[1] = false;
      // reset player ready
      player_ready[0] = false;
      player_ready[1] = false;
      // reset idle screen joystick
      idle_joystick_x[0] = 0;
      idle_joystick_y[0] = 0;
      idle_joystick_x[1] = 1;
      idle_joystick_y[1] = 1;

      // First time we got into idle screen, display the idle screen
      display_idle_scroll(display_one);
      display_idle_scroll(display_two);
    }

    // Collect both joystick inputs. base on if joystick x == 0 or 1.
    // x==0 means player, x==1 means computer
    // only move_y if computer is selected
    // If we are in idle_screen, nothing happens, we only break out of idle screen if a button is pressed
    
    move_user_joystick_idle(0, idle_joystick_x[0] == 1, 20); // max stockfish level is 20
    move_user_joystick_idle(1, idle_joystick_x[1] == 1, 20);
    

    // Check button presses: if button pressed, toggle player_ready
    if (confirm_button_pressed[0]) {
      player_ready[0] = !player_ready[0];
      confirm_button_pressed[0] = false;
    }
    if (confirm_button_pressed[1]) {
      player_ready[1] = !player_ready[1];
      confirm_button_pressed[1] = false;
    }
    
    // Board LED IDLE animation
    fill_solid(led_display, stripLen, CRGB(0, 0, 0));

    // OLED display: show the current selection
    // TODO:
    // display_idle(timer=game_timer, screen=0, player_ready[0], idle_joystick_x[0], idle_joystick_y[0])
    // display_idle(timer=game_timer, screen=1, player_ready[1], idle_joystick_x[1], idle_joystick_y[1])

    // If both players are ready, start the game
    if (player_ready[0] && player_ready[1]) {
      game_state = GAME_INITIALIZE;
      // reset confirm button pressed
      confirm_button_pressed[0] = false;
      confirm_button_pressed[1] = false;
      // reset player ready
      player_ready[0] = false;
      player_ready[1] = false;
      // reset idle screen joystick
      idle_joystick_x[0] = 0;
      idle_joystick_y[0] = 0;
      idle_joystick_x[1] = 0;
      idle_joystick_y[1] = 0;
    }

    // game_state = GAME_INITIALIZE;
  } else if (game_state == GAME_INITIALIZE) {
    // Game started, initialize the board

    // Initialize the board
    p_board = new Board();  // Initialize the board object to a new board
    player_turn = 0;  // White to move first
    current_player_under_check = 0;  // No player is under check initially
    sources_of_check.clear();  // clean memory for sources of check

    // Draw reason is false by default
    draw_three_fold_repetition = false;  // If true, the game is a draw due to three fold repetition
    draw_fifty_move_rule = false;        // If true, the game is a draw due to fifty move rule
    draw_stalemate = false;              // If true, the game is a draw due to stalemate
    draw_insufficient_material = false;  // If true, the game is a draw due to insufficient material

    // Currently no piece is selected, so initialize to -1, -1
    selected_x = -1;
    selected_y = -1;
    destination_x = -1;
    destination_y = -1;
    capture_x = -1;
    capture_y = -1;
    // The followings are for LED display to show previously "from" and "to"
    previous_destination_x = 0;
    previous_destination_y = 0;
    previous_selected_x = 0;
    previous_selected_y = 0;

    number_of_turns = 0;

    // Graveyard count all sets to 0, except for temp pieces which is 8
    for (int8_t i = 0; i < 10; i++) {
      graveyard[i] = 0;  // initialize graveyard
    }
    graveyard[10] = 8;  // initialize temp pieces
    graveyard[11] = 8;
    // clear the promoted pawn vector (memory)
    promoted_pawns_using_temp_pieces.clear();

    // Joystick location (set default to king's square)
    joystick_x[0] = 4;
    joystick_y[0] = 0;
    joystick_x[1] = 4;
    joystick_y[1] = 7;
    confirm_button_pressed[0] = false;
    confirm_button_pressed[1] = false;
    prev_confirm_button_pressed[0] = true;  // Assume the button is pressed before the game starts, so force user to release the button
    prev_confirm_button_pressed[1] = true;
    prev_joystick_neutral[0] = true;
    prev_joystick_neutral[1] = true;

    // Promotion joystick selection - default is 0 which is queen
    promotion_joystick_selection = 0;

    // TODO: motor calibration can be done here
    game_state = GAME_BEGIN_TURN;
  } else if (game_state == GAME_BEGIN_TURN) {
    // Free display memory
    free_displays();

    // Begin a turn - generate moves, remove illegal moves, check for
    // checkmate/draw
    // This is a software state, one cycle happens here

    // TODO: this is temporary, just to see the board state
    serial_display_board_and_selection();
    // Check if 50 move rule is reached
    if (p_board->draw_move_counter >= 50) {
      game_state = GAME_OVER_DRAW;
      draw_fifty_move_rule = true;
      return;
    }

    // Check if 3-fold repetition is reached
    if (p_board->is_three_fold_repetition()) {
      game_state = GAME_OVER_DRAW;
      draw_three_fold_repetition = true;
      return;
    }

    // Check if insufficient material
    if (p_board->is_insufficient_material()) {
      game_state = GAME_OVER_DRAW;
      draw_insufficient_material = true;
      return;
    }

    // Generate all possible moves for the player, and remove illegal moves
    bool no_moves = true;
    for (int8_t i = 0; i < 8; i++) {
      for (int8_t j = 0; j < 8; j++) {
        // Non-player's piece should have empty moves
        all_moves[i][j].clear();
        if (p_board->pieces[i][j]->get_type() == EMPTY ||
            p_board->pieces[i][j]->get_color() != player_turn) {
          continue;
        }
        all_moves[i][j] = p_board->pieces[i][j]->get_possible_moves(p_board);
        p_board->remove_illegal_moves_for_a_piece(
            j, i, all_moves[i][j]);  // NOTE it's j, i!!!!!
        if (all_moves[i][j].size() > 0) {
          no_moves = false;
        }
      }
    }

    // Find if we are under check
    current_player_under_check = p_board->under_check(player_turn);
    sources_of_check.clear();
    if (current_player_under_check) {
      sources_of_check = p_board->sources_of_check(player_turn);
    }

    // If no more moves, checkmate or stalemate
    if (no_moves) {
      if (current_player_under_check) {
        // Checkmate - whoever made the previous move wins
        if (player_turn == 0) {
          game_state = GAME_OVER_BLACK_WIN;
        } else {
          game_state = GAME_OVER_WHITE_WIN;
        }
      } else {
        // Stalemate
        game_state = GAME_OVER_DRAW;
        draw_stalemate = true;
      }
      return;
    }

    // TEMP DISPLAY CODE:
    serial_display_board_and_selection();

    // Before every turn, set unspecified values to -1
    selected_x = -1;
    selected_y = -1;
    destination_x = -1;
    destination_y = -1;
    capture_x = -1;
    capture_y = -1;
    // Display screen once before moving to next state
    display_init();
    display_turn_select(player_turn, joystick_x[player_turn], joystick_y[player_turn], selected_x, selected_y, destination_x, destination_y, display_one, display_two);
    // free_displays();

    // Move to the next state
    game_state = GAME_WAIT_FOR_SELECT;
  } else if (game_state == GAME_WAIT_FOR_SELECT) {
    // Joystick moving, before pressing select button on valid piece
    // Wait for joystick input
    // If there is input, check if it's a valid piece, if not, ignore
    // If it's a valid piece, indicate the piece is selected, and move to
    // GAME_WAIT_FOR_MOVE

    // Get button input, update joystick location, update confirm button pressed
    // display_init();
    move_user_joystick_x_y(player_turn);
    // free_displays();

    // LED display
    // HERE, highlight the "previous move" that just happened by highlighting
    // pieces... (orange) Also display the cursor location - which is
    // joystick_x[player_turn], joystick_y[player_turn] (green - overwrites
    // orange) Also display sources of check, if any (red) (will be overwritten
    // by green if the cursor is on the same square) red on king square if under
    // check
    fill_solid(led_display, stripLen, CRGB(0, 0, 0));
    led_display[coordinate_to_index(joystick_x[player_turn],
                                    joystick_y[player_turn])] = CRGB(0, 0, 255);
    // Also light up previous move of opponent

    // OLED display: show the current selection
    // TODO
    // display_turn_select()...
    
    if(number_of_turns != 0){
      led_display[coordinate_to_index(previous_selected_x, previous_selected_y)] = CRGB(255, 255, 0);
      led_display[coordinate_to_index(previous_destination_x, previous_destination_y)] = CRGB(255, 255, 0);
    }


    // Don't move until the confirm button is pressed
    if (confirm_button_pressed[player_turn]) {
      confirm_button_pressed[player_turn] = false; 
      // This line is unnecessary, as the confirm_button_pressed flag is reset in the move_user_joystick_x_y function
      // And all subsequent confirm_button_pressed checks are done after the move_user_joystick_x_y function is called, which resets the flag as well
    } else {
      return;
    }

    // When confirm button is pressed, update the selected piece location using the global variables joystick_x, joystick_y
    selected_x = joystick_x[player_turn];
    selected_y = joystick_y[player_turn];

    display_turn_select(player_turn, joystick_x[player_turn], joystick_y[player_turn], selected_x, selected_y, destination_x, destination_y, display_one, display_two);

    // Check for valid input
    if (selected_x < 0 || selected_x > 7 || selected_y < 0 || selected_y > 7) {
      // Invalid input
      return;
    }
    if (p_board->pieces[selected_y][selected_x]->get_type() == EMPTY ||
        p_board->pieces[selected_y][selected_x]->get_color() != player_turn) {
      // Not player's piece
      return;
    }

    // TEMP DISPLAY TO SERIAL:
    serial_display_board_and_selection();

    // PROCEED TO NEXT STATE
    game_state = GAME_WAIT_FOR_MOVE;
  } else if (game_state == GAME_WAIT_FOR_MOVE) {
    // Joystick moving, after pressing select button on valid destination
    // Wait for joystick input
    // If there is input, check if it's a valid destination, if not, ignore
    // If the input is the selected_x, selected_y, move back to GAME_WAIT_FOR_SELECT 
    // If it's another of your own piece, replace selected_x, selected_y with the new piece, 
    // stay in this state If it's a valid destination, move the piece, and move to GAME_MOVE_MOTOR

    // Get button input, update joystick location, update confirm button pressed
    // display_init();
    move_user_joystick_x_y(player_turn);
    // free_displays();

    // LED display
    // HERE, highlight the "prevoius move" that just happened by highlighting pieces... (in orange)
    // Also display the cursor location - which is joystick_x[player_turn], joystick_y[player_turn] (green - overwrites orange) 
    // Also highlight the "selected piece" with a different color (selected_x, selected_y) (blue) overwrites green 
    // Also display sources of check, if any (red) (will be overwritten by green if the cursor is on the same square) red on king square if under check, unless selected piece is on the king square
    // ALSO, display the possible moves of the selected piece (yellow) (overwrites orange)
    fill_solid(led_display, stripLen, CRGB(0, 0, 0));
    led_display[coordinate_to_index(joystick_x[player_turn],
                                    joystick_y[player_turn])] = CRGB(0, 0, 255);
    led_display[coordinate_to_index(selected_x, selected_y)] = CRGB(0, 255, 0);
    // Also light up previous move of opponent

    // OLED display: show the current selection
    // TODO
    // display_turn_select()...

    // Don't move until the confirm button is pressed
    if (confirm_button_pressed[player_turn]) {
      confirm_button_pressed[player_turn] = false;
      // Again, this line is unnecessary, as the confirm_button_pressed flag is reset in the move_user_joystick_x_y function
      // And all subsequent confirm_button_pressed checks are done after the move_user_joystick_x_y function is called, which resets the flag as well
    } else {
      return;
    }

    // When confirm button is pressed, update the destination piece location using the global variables joystick_x, joystick_y (which has been updated in the move_user_joystick_x_y function)
    destination_x = joystick_x[player_turn];
    destination_y = joystick_y[player_turn];

    // Check for valid input (within the board)
    if (destination_x < 0 || destination_x > 7 || destination_y < 0 ||
        destination_y > 7) {
      // Invalid input
      return;
    }
    // If we selected the same piece, move back to select (cancel selection)
    if (destination_x == selected_x && destination_y == selected_y) {
      // Move back to select
      game_state = GAME_WAIT_FOR_SELECT;
      destination_x = -1;
      destination_y = -1;
      selected_x = -1;
      selected_y = -1;
      // Display on OLED
      // display_init();
      display_turn_select(player_turn, joystick_x[player_turn], joystick_y[player_turn], selected_x, selected_y, destination_x, destination_y, display_one, display_two);
      // free_displays();
      return;
    }
    // If we selected another piece of our own, replace selected piece
    if (p_board->pieces[destination_y][destination_x]->get_type() != EMPTY &&
        p_board->pieces[destination_y][destination_x]->get_color() ==
            player_turn) {
      // Replace selected piece, and repeat this state
      selected_x = destination_x;
      selected_y = destination_y;
      destination_x = -1;
      destination_y = -1;
      // display_init();
      display_turn_select(player_turn, joystick_x[player_turn], joystick_y[player_turn], selected_x, selected_y, destination_x, destination_y, display_one, display_two);
      // free_displays();
      return;
    }
    // Check if the move is valid (if the destination is in the list of possible moves)
    bool valid_move = false;
    for (int8_t i = 0; i < all_moves[selected_y][selected_x].size(); i++) {
      if (all_moves[selected_y][selected_x][i].first%8 == destination_x &&
          all_moves[selected_y][selected_x][i].first/8 == destination_y) {
        valid_move = true;
        capture_x = all_moves[selected_y][selected_x][i].second%8;
        capture_y = all_moves[selected_y][selected_x][i].second/8;
        break;
      }
    }
    if (!valid_move) {
      // Invalid move
      return;
    }

    // Move is valid, move the piece
    game_state = GAME_MOVE_MOTOR;
  } else if (game_state == GAME_MOVE_MOTOR) {
    /*
    1. check if this move involves any capture
    2. If move involves capture, see if the captured piece can be used to
    replace a currently promoted pawn
    3. if yes -> don't move captured piece to graveyard, but instead use it to
    replace the pawn. (temp piece off the board -> captured piece to pawn ->
    moving piece to destination)
    4. if no -> move captured piece to graveyard (captured piece -> graveyard,
    moving piece to destination)
    5. just update the board regularly - since the captured piece is now removed (if there was any)
    6. castling... special move. move king first (and rook needs to be moving via the "safe move" parameter)
    */

    // LED: just display the new "move" that just happened - which is seleced_x,
    // selected_y, destination_x, destination_y, (capture_x, capture_y -- don't
    // need to display this, but ok if you want to)
    // Make sure to update the LED display to show the new move - WHILE THE MOTORS ARE MOVING

    // OLED display: show the current selection
    // TODO
    // display_turn_select()...
    // display_init();
    display_while_motor_moving(player_turn, selected_x, selected_y, destination_x, destination_y, display_one, display_two);
    // free_displays();
    Serial.println("Moving piece by motor");
    Serial.println(freeMemory());

    // Check if capture:
    if (capture_x != -1) {
      // See if the captured piece is a temp piece
      bool captured_piece_is_temp = false;
      int8_t captured_temp_piece_x = -1;
      int8_t captured_temp_piece_y = -1;
      for (int8_t i = 0; i < promoted_pawns_using_temp_pieces.size(); i++) {
        if (promoted_pawns_using_temp_pieces[i].first == capture_x &&
            promoted_pawns_using_temp_pieces[i].second == capture_y) {
          captured_piece_is_temp = true;
          break;
        }
      }

      if (captured_piece_is_temp) {
        // The captured piece is a temp piece, so just move to graveyard (6 == temp piece)
        std::pair<int8_t, int8_t> graveyard_coordinate =
            get_graveyard_empty_coordinate(6, p_board->pieces[capture_y][capture_x]->get_color());
        // Motor move the piece from capture_x, capture_y to graveyard_coordinate
        move_piece_by_motor(capture_x, capture_y, graveyard_coordinate.first,
                   graveyard_coordinate.second);
        // Update the graveyard memory
        graveyard[10 + p_board->pieces[capture_y][capture_x]->get_color()]++;
        // Remove the promoted pawn from the vector
        for (int8_t i = 0; i < promoted_pawns_using_temp_pieces.size(); i++) {
          if (promoted_pawns_using_temp_pieces[i].first == capture_x &&
              promoted_pawns_using_temp_pieces[i].second == capture_y) {
            promoted_pawns_using_temp_pieces.erase(promoted_pawns_using_temp_pieces.begin() + i);
            break;
          }
        }
      } else {
        // The captured piece is not a temp, do the regular replacement
        bool capture_piece_can_replace_promoted_pawn = false;
        // This part is about seeing if the captured piece can replace a
        // promoted pawn. So, we can add a check to see if captured-piece is a temp piece
        for (int8_t i = 0; i < promoted_pawns_using_temp_pieces.size(); i++) {
          // Check for all promoted pawns, if colour matches captured_x,
          // captured_y's piece AND type matches If they do match, tell the
          // motor to make a replacement.
          int8_t pawn_x = promoted_pawns_using_temp_pieces[i].first;
          int8_t pawn_y = promoted_pawns_using_temp_pieces[i].second;
          if (p_board->pieces[capture_y][capture_x]->get_color() == p_board->pieces[pawn_y][pawn_x]->get_color() &&
              p_board->pieces[capture_y][capture_x]->get_type() == p_board->pieces[pawn_y][pawn_x]->get_type()) {
            // There exists a promoted pawn that can be replaced by the captured
            // piece Move temp piece to graveyard (6 == temp piece)
            std::pair<int8_t, int8_t> graveyard_coordinate =
                get_graveyard_empty_coordinate(6, p_board->pieces[pawn_y][pawn_x]->get_color());
            move_piece_by_motor(pawn_x, pawn_y, graveyard_coordinate.first, graveyard_coordinate.second);

            // Move captured piece to the pawn's location (use safe move)
            move_piece_by_motor(capture_x, capture_y, pawn_x, pawn_y);

            // Remove the promoted pawn from the vector - since it's replaced,
            // and can be treated as a normal piece
            promoted_pawns_using_temp_pieces.erase(promoted_pawns_using_temp_pieces.begin() + i);
            // Update the graveyard memory for the temp piece
            graveyard[10 + p_board->pieces[pawn_y][pawn_x]->get_color()]++;
            capture_piece_can_replace_promoted_pawn = true;
            break;
          }
        }

        if (!capture_piece_can_replace_promoted_pawn) {
          // If the captured piece cannot replace a promoted pawn, move the
          // captured piece to the graveyard
          PieceType captured_piece_type =
              p_board->pieces[capture_y][capture_x]->get_type();
          int8_t graveyard_index = 0;
          if (captured_piece_type == QUEEN) {
            graveyard_index = 0;
          } else if (captured_piece_type == ROOK) {
            graveyard_index = 1;
          } else if (captured_piece_type == BISHOP) {
            graveyard_index = 2;
          } else if (captured_piece_type == KNIGHT) {
            graveyard_index = 3;
          } else if (captured_piece_type == PAWN) {
            graveyard_index = 4;
          }

          // Move the captured piece to graveyard (graveyard_index is 0 to 4,
          // plus 1 for get_graveyard_empty_coordinate function as it takes 1 to 5,
          // and 6 for temp piece)
          std::pair<int8_t, int8_t> graveyard_coordinate =
              get_graveyard_empty_coordinate(graveyard_index + 1, p_board->pieces[capture_y][capture_x]->get_color());
          move_piece_by_motor(capture_x, capture_y, graveyard_coordinate.first, graveyard_coordinate.second);

          // If colour is black, add 5 to the index, and update the graveyard
          graveyard_index += 5 * p_board->pieces[capture_y][capture_x]->get_color();
          graveyard[graveyard_index]++;
        }
      }
    }
    // Move the piece - regardless of capture
    // If the piece that's moving is a temp piece, edit its coordinates in the
    // promoted_pawns_using_temp_pieces vector
    for (int8_t i = 0; i < promoted_pawns_using_temp_pieces.size(); i++) {
      if (promoted_pawns_using_temp_pieces[i].first == selected_x &&
          promoted_pawns_using_temp_pieces[i].second == selected_y) {
        promoted_pawns_using_temp_pieces[i].first = destination_x;
        promoted_pawns_using_temp_pieces[i].second = destination_y;
        break;
      }
    }
    // Move the piece (fast move except knight)
    move_piece_by_motor(selected_x, selected_y, destination_x, destination_y);

    // If the piece is a king that moved 2 squares, move the rook (castling)
    if (p_board->pieces[selected_y][selected_x]->get_type() == KING &&
        abs(destination_x - selected_x) == 2) {
      // Castling move
      int8_t rook_x = 0;
      int8_t rook_y = 0;
      if (destination_x - selected_x == 2) {
        // King side castling
        rook_x = 7;
        rook_y = selected_y;
      } else {
        // Queen side castling
        rook_x = 0;
        rook_y = selected_y;
      }
      // Move the rook (BEWARE, THIS ROOK MOVE NEEDS TO MOVE ALONG THE EDGE, NOT LIKE ANY REGULAR ROOK MOVE)
      // HAVE TO GO AROUND THE KING
      move_piece_by_motor(rook_x, rook_y, (selected_x + destination_x) / 2, selected_y);
    }

    // TODO: motor should move back to origin and calibrate

    // Since motor movements are blocking, we can update the board state right after the motor movement
    game_state = GAME_END_MOVE;
  } else if (game_state == GAME_END_MOVE) {
    // Update chess board, see if a pawn can promote
    // Before updating the board, see if the pi
    p_board->move_piece(selected_x, selected_y, destination_x, destination_y,
                        capture_x, capture_y);
    // Check if a pawn can promote
    if (p_board->can_pawn_promote(destination_x, destination_y)) {
      game_state = GAME_WAIT_FOR_SELECT_PAWN_PROMOTION;
      promotion_joystick_selection = 0; // default to queen
      // display_init();
      display_promotion(player_turn, 0, display_one, display_two);
      // free_displays();
    } else {
      game_state = GAME_END_TURN;
    }
  } else if (game_state == GAME_WAIT_FOR_SELECT_PAWN_PROMOTION) {
    // Joystick moving, before pressing select button on promotion piece
    // Wait for joystick input
    // If there is input, check if it's a valid promotion piece, if not, ignore
    // If it's a valid promotion piece, indicate the piece is selected, and move
    // to GAME_PAWN_PROMOTION_MOTOR

    // LED display
    // HERE (Display the promotion_joystick_selection, which has value 0,1,2,3)
    // (separate LED maybe at the edge of board? - the "promotion" indicator
    // squares are basically same as other squares, but with a "thicker" top
    // custom shape that does not allow a "queen-shaped light" to go thru, so
    // when this light is on... queen-shaped black line or queen-shaped light
    // goes thru) Also display the "move" that just happened by highlight
    // pieces... (orange colour)

    // OLED display: show the current selection
    // TODO
    // display_promotion()...

    // Get button input, update joystick location (Special for pawn promotion,
    // use promotion_joystick_selection) (there's only 4 possible values, 0, 1, 2, 3)
    // display_init();
    move_user_joystick_promotion(player_turn);
    // free_displays();

    // Don't move until the confirm button is pressed
    if (confirm_button_pressed[player_turn]) {
      confirm_button_pressed[player_turn] = false;
    } else {
      return;
    }

    // 0, 1, 2, 3 are the possible promotion pieces that promotion_joystick_selection can take, updated by move_user_joystick_promotion
    promotion_type = promotion_joystick_selection;

    // Check for valid input
    if (promotion_type < 0 || promotion_type > 3) {
      // Invalid input
      return;
    }

    // Valid promotion piece, promote the pawn in software
    switch (promotion_type) {
      case 0:
        p_board->promote_pawn(destination_x, destination_y, QUEEN);
        break;
      case 1:
        p_board->promote_pawn(destination_x, destination_y, ROOK);
        break;
      case 2:
        p_board->promote_pawn(destination_x, destination_y, BISHOP);
        break;
      case 3:
        p_board->promote_pawn(destination_x, destination_y, KNIGHT);
        break;
    }
    // Move to motor moving
    game_state = GAME_PAWN_PROMOTION_MOTOR;
  } else if (game_state == GAME_PAWN_PROMOTION_MOTOR) {
    /*
    1. promotion is happning, definitely, happening at destination_x,
    destination_y
    2. see if the promotion CAN be done with a piece in graveyard (see if
    graveyard[index] > 0)
    3. if yes -> move graveyard[index] piece to destination_x, destination_y,
    AND move pawn to graveyard, update graveyard memory (graveyard[index]--, but
    graveyard[pawn]++)
    4. if no -> move a temp piece, and KEEP TRACK OF ITS COORDINATES (temp piece
    to destination_x, destination_y, pawn to graveyard, update graveyard memory
    (graveyard[pawn]++, graveyard[temp_piece]--), and keep track of temp piece's
    coordinates in the vector: promoted_pawns_using_temp_pieces)
    5. add reset motor to origin logic from previous game_state again
    */

    // LED: keep the "promotion light" on, but with a different color (maybe
    // purple?)
    // Make sure the LED is on throughout the duration of the motor movement

    // OLED display: show the current selection
    // TODO
    // display_promotion()...
  
    // Check if there's a valid piece in the graveyard to be used for promotion
    bool valid_graveyard_piece = false;
    PieceType promotion_piece_type =
        p_board->pieces[destination_y][destination_x]->get_type();
    int8_t graveyard_index = 0;
    if (promotion_piece_type == QUEEN) {
      graveyard_index = 0;
    } else if (promotion_piece_type == ROOK) {
      graveyard_index = 1;
    } else if (promotion_piece_type == BISHOP) {
      graveyard_index = 2;
    } else if (promotion_piece_type == KNIGHT) {
      graveyard_index = 3;
    }
    // If colour is black, add 5 to the index
    graveyard_index +=
        5 * p_board->pieces[destination_y][destination_x]->get_color();
    if (graveyard[graveyard_index] > 0) {
      valid_graveyard_piece = true;
    }

    // Move pawn to graveyard regardless of promotion (5 == pawn in function,
    // but index is 4 + 5*color)
    std::pair<int8_t, int8_t> graveyard_coordinate = get_graveyard_empty_coordinate(
        5, p_board->pieces[destination_y][destination_x]->get_color());
    move_piece_by_motor(destination_x, destination_y, graveyard_coordinate.first,
               graveyard_coordinate.second);
    graveyard[4 + 5 * p_board->pieces[destination_y][destination_x]->get_color()]++;

    // If there is a valid piece in the graveyard, use that piece for promotion
    if (valid_graveyard_piece) {
      // Move the graveyard piece to the destination (the graveyard index is now
      // 0,1,2,3 or 5,6,7,8, convert back to 1,2,3,4 for the
      // get_graveyard_empty_coordinate function)

      // Update the graveyard memory before moving the piece, since the get_graveyard_empty_coordinate function only gets the next empty spot, but we want the PIECE spot
      graveyard[graveyard_index]--;

      // Obtain the graveyard coordinate for the piece, and move the piece
      graveyard_coordinate = get_graveyard_empty_coordinate(
          graveyard_index - p_board->pieces[destination_y][destination_x]->get_color() * 5 + 1, p_board->pieces[destination_y][destination_x]->get_color());
      move_piece_by_motor(graveyard_coordinate.first, graveyard_coordinate.second,
                 destination_x, destination_y);
    } else {
      // There isn't a valid piece in the graveyard, use a temp piece

      // Update the graveyard memory so the get_graveyard_empty_coordinate returns the coordinate of the first available temp piece
      graveyard[10 + p_board->pieces[destination_y][destination_x]->get_color()]--;

      // Move a temp piece to the destination (6 == temp piece)
      graveyard_coordinate = get_graveyard_empty_coordinate(
          6, p_board->pieces[destination_y][destination_x]->get_color());
      move_piece_by_motor(graveyard_coordinate.first, graveyard_coordinate.second,
                 destination_x, destination_y);

      // Update the promoted pawns using temp pieces vector (add this promoted pawn)
      promoted_pawns_using_temp_pieces.push_back(
          std::make_pair(destination_x, destination_y));
    }

    // TODO: motor should move back to origin and calibrate

    // Proceed to next state, since this state is blocking
    game_state = GAME_END_TURN;
  } else if (game_state == GAME_END_TURN) {
    // Record the move that just happened (selected_x, selected_y, destination_x, destination_y) as the "previous move"
    // This "previous move" will be displayed by LED
    
    previous_destination_x = destination_x;
    previous_destination_y = destination_y;
    previous_selected_x = selected_x;
    previous_selected_y = selected_y;

    number_of_turns++;

    // End a turn - switch player
    player_turn = !player_turn;

    // Turn off promotion LED light if that was on. (if you have a separate LED
    // for promotion indicator)

    game_state = GAME_BEGIN_TURN;
  } else if (game_state == GAME_OVER_WHITE_WIN) {
    // White wins

    // OLED:
    // display_game_over()
    display_init();
    display_game_over(0, 0, display_one, display_two);
    free_displays();

    // TODO: some sort of display, and reset the game (Maybe on the OLED display)
    Serial.println("White wins!");
    game_state = GAME_RESET;
  } else if (game_state == GAME_OVER_BLACK_WIN) {
    // Black wins

    // OLED:
    // display_game_over()
    display_init();
    display_game_over(1, 0, display_one, display_two);
    free_displays();

    // TODO: some sort of display, and reset the game (Maybe on the OLED display)
    Serial.println("Black wins!");
    game_state = GAME_RESET;
  } else if (game_state == GAME_OVER_DRAW) {
    // Draw

    // OLED:
    // display_game_over()

    // TODO: some sort of display, and reset the game (Maybe on the OLED display)
    Serial.println("Draw!");
    Serial.println("Reason: ");
    display_init();
    if (draw_fifty_move_rule) {
      Serial.println("Fifty move rule");
      display_game_over(0, 1, display_one, display_two);
    } else if (draw_three_fold_repetition) {
      Serial.println("Three fold repetition");
      display_game_over(0, 2, display_one, display_two);
    } else if (draw_stalemate) {
      Serial.println("Stalemate");
      display_game_over(0, 3, display_one, display_two);
    } else if (draw_insufficient_material) {
      Serial.println("Insufficient material");
      display_game_over(0, 4, display_one, display_two);
    }
    free_displays();
    game_state = GAME_RESET;
  } else if (game_state == GAME_RESET) {
    delay(10000);
    // Reset the game (move pieces back to initial position, clear memory (mainly focusing on vectors))

    // First, move the pieces back to the initial position
    // TODO: motor job

    // Free memory
    delete p_board;

    // Clear vectors (find ones that aren't cleared by game_initialize)
    // TODO

    // Check the remaining variables from game_poweron or game_idle TODO

    // Reset game state
    game_state = GAME_POWER_ON;
  }
}
