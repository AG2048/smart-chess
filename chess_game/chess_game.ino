// Include
#include "FastLED.h"
#include "ArduinoSTL.h"
#include "Board.h"
// #include "MemoryFree.h"
#include "Piece.h"
#include "PieceType.h"

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
std::vector<std::pair<std::pair<int8_t, int8_t>, std::pair<int8_t, int8_t>>>
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

  int graveyard_index = 0;
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
const int PUL_PIN[] = {9, 9};
const int DIR_PIN[] = {8, 8};
const int stepsPerSquare = 1536; // measured value from testing, 3.95cm per rotation
const int motorStepDelay = 50;
int motor_coordinates[2] = {0, 0};  // x, y coordinates of the motor in millimeters
// ALSO NEED SOME PINS FOR CALLIBRATION SWITCHES

// TODO: motor function need a "safe move" parameter, to tell if the motor need to move the piece along an edge, or just straight to the destination
void move_piece_by_motor(int from_x, int from_y, int to_x, int to_y, int stepDelay, bool fastMove) {
  // Move the motor from one square to another
  // from_x, from_y: x, y coordinates of the square to move from
  // to_x, to_y: x, y coordinates of the square to move to
  // stepDelay: time in microseconds to wait between each step (half a period)
  // fastMove: if true, move piece in a straight line to destination, else, move along edges
  // Move the motor from (from_x, from_y) to (to_x, to_y)

  // TODO
  // TODO: don't write the motor moving code here, just write the logic to move the motor and call motor_move_to_coordinate function
  digitalWrite(DIR_PIN[0], to_x - from_x > 0);
  digitalWrite(DIR_PIN[1], to_y - from_y > 0);

  for (int i = 0; i < (to_x - from_x) * stepsPerSquare; i++) {
    digitalWrite(PUL_PIN[0], HIGH);
    delayMicroseconds(stepDelay);
    digitalWrite(PUL_PIN[0], LOW);
    delayMicroseconds(stepDelay);
  }

  for (int i = 0; i < (to_y - from_y) * stepsPerSquare; i++) {
    digitalWrite(PUL_PIN[1], HIGH);
    delayMicroseconds(stepDelay);
    digitalWrite(PUL_PIN[1], LOW);
    delayMicroseconds(stepDelay);
  }

  Serial.println("Moving should be happening here, but not implemented yet. ");
  Serial.print("Moving from (");
  Serial.print(from_x);
  Serial.print(", ");
  Serial.print(from_y);
  Serial.print(") to (");
  Serial.print(to_x);
  Serial.print(", ");
  Serial.print(to_y);
  Serial.println(")");
}

void move_motor_to_coordinate(int x, int y) {
  // Move the motor to a specific coordinate
  // x, y: x, y coordinates of the square to move to (IN MILLIMETERS)
  // Move the motor to (x, y)
  // This function should be used to move the motor to a specific coordinate without any checks, it should be used AFTER calibrating the motor
  // The "starting" coordinate is assumed to be motor_coordinates variable (x, y)

  // TODO

  Serial.println("Moving should be happening here, but not implemented yet. ");
  Serial.print("Moving to (");
  Serial.print(x);
  Serial.print(", ");
  Serial.print(y);
  Serial.println(")");
}

void move_motor_to_origin(bool fastMove) {
  // resets the motor to origin (0, 0) and re-calibrate the motor
  // fastMove: if true, it will move fast until the last 50mm until "supposed" origin and move slowly until it hits the limit switch
  //           if false, it will move slowly from the beginning until it hits the limit switch
  // The function should reference motor_coordinates variable to estimate where it is

}

// ############################################################
// #                     JOYSTICK CONTROL                     #
// ############################################################

// JOYSTICK CONTROL (each corresponds to the pin number) (first index is for white's joystick, second index is for black's joystick)
// Joystick is active LOW, so connect the other pin to GND
const int JOYSTICK_POS_X_PIN[] = {2, 2};
const int JOYSTICK_POS_Y_PIN[] = {3, 3};
const int JOYSTICK_NEG_X_PIN[] = {4, 4};
const int JOYSTICK_NEG_Y_PIN[] = {5, 5};
const int JOYSTICK_BUTTON_PIN[] = {6, 6};
// User Joystick Location - keeping track of white x, black x, white y, black y
int8_t joystick_x[2];
int8_t joystick_y[2];
bool confirm_button_pressed[2];
// Previous joystick input -- for edge detection (true if the button / joystick was pressed in the previous detection cycle)
bool prev_confirm_button_pressed[2];
bool prev_joystick_neutral[2];
// Promotion Joystick Selection - index of promotion piece selected (0,1,2,3)
int8_t promotion_joystick_selection;  // only happening on one player's turn, so no need for 2

void move_user_joystick_x_y(bool color) {
  // Get input from the joystick and modify the corresponding joystick coord for the color (player)
  // color: 0 for white, 1 for black
  // Update the joystick coordinates and confirm button pressed flag
  // Result: <color>_joystick_<x,y> ++ or -- (mod 8),
  // <color>_confirm_button_pressed = true if confirm button is pressed from
  // neutral state

  // Read the joystick values -- low is pressed, high is not pressed
  int x_val = digitalRead(JOYSTICK_POS_X_PIN[color]);
  int y_val = digitalRead(JOYSTICK_POS_Y_PIN[color]);
  int neg_x_val = digitalRead(JOYSTICK_NEG_X_PIN[color]);
  int neg_y_val = digitalRead(JOYSTICK_NEG_Y_PIN[color]);
  int button_val = digitalRead(JOYSTICK_BUTTON_PIN[color]);

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
  int x_val = digitalRead(JOYSTICK_POS_X_PIN[color]);
  int y_val = digitalRead(JOYSTICK_POS_Y_PIN[color]);
  int neg_x_val = digitalRead(JOYSTICK_NEG_X_PIN[color]);
  int neg_y_val = digitalRead(JOYSTICK_NEG_Y_PIN[color]);
  int button_val = digitalRead(JOYSTICK_BUTTON_PIN[color]);

  // Update the joystick values if joystick WAS neutral and now has a value.
  // Update neutral flag. Note active low for button (pressed is low 0)
  if (prev_joystick_neutral[color]) {
    // If previous joystick WAS neutral
    if (x_val == 0) {
      promotion_joystick_selection = (promotion_joystick_selection + 1) % 4;
      prev_joystick_neutral[color] = false;
    } else if (neg_x_val == 0) {
      promotion_joystick_selection = (promotion_joystick_selection - 1) % 4;
      prev_joystick_neutral[color] = false;
    }
    // if no joystick movement, joystick_neutral stays true
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
// Array of CRGB objects corresponding to LED colors / brightness (0 indexed)
struct CRGB led_display[stripLen];
// If we wish to add cosmetic things, add another array of "previous states", or "pre-set patterns" or other stuff
// Also we may need a separate LED strip timer variable to keep track "how long ago was the last LED update"
// A function to update LED strip in each situation

// ############################################################
// #                           UTIL                           #
// ############################################################

// Display in serial monitor
const char CHESS_PIECE_CHAR[] = {' ', 'K', 'Q', 'B', 'N', 'R', 'P',
                                 'k', 'q', 'b', 'n', 'r', 'p'};

void serial_display_board_and_selection() {
  Serial.println("---------------------------------------------------");
  for (int row = 7; row >= 0; row--) {
    for (int col = 0; col < 8; col++) {
      bool showed = false;
      for (int i = 0; i < all_moves[selected_y][selected_x].size(); i++) {
        if (all_moves[selected_y][selected_x][i].second.first == col &&
            all_moves[selected_y][selected_x][i].second.second == row) {
          Serial.print("X");
          Serial.print("\t");
          showed = true;
          break;
        }
        if (all_moves[selected_y][selected_x][i].first.first == col &&
            all_moves[selected_y][selected_x][i].first.second == row) {
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
      for (int i = 0; i < 8; i++) {
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

    // TODO
    game_state = GAME_IDLE;
  } else if (game_state == GAME_IDLE) {
    // No game is being played yet, waiting for a game to start
    // Wait for game mode select (player / computer / difficulty)
    // Wait for a button press to start the game
    // If button pressed, initialize the game
    // If button not pressed, stay in this state
    
    // Idle animation (for now all LEDs off) TODO
    fill_solid(led_display, stripLen, CRGB(0, 0, 0));

    // TODO: currently we just skip this state, assume game is always started
    game_state = GAME_INITIALIZE;
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

    // Currently no piece is selected, so initialize to 0, 0
    selected_x = 0;
    selected_y = 0;
    destination_x = 0;
    destination_y = 0;
    capture_x = 0;
    capture_y = 0;
    // Graveyard count all sets to 0, except for temp pieces which is 8
    for (int i = 0; i < 10; i++) {
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

    // Move to the next state
    game_state = GAME_WAIT_FOR_SELECT;
  } else if (game_state == GAME_WAIT_FOR_SELECT) {
    // Joystick moving, before pressing select button on valid piece
    // Wait for joystick input
    // If there is input, check if it's a valid piece, if not, ignore
    // If it's a valid piece, indicate the piece is selected, and move to
    // GAME_WAIT_FOR_MOVE

    // Get button input, update joystick location, update confirm button pressed
    move_user_joystick_x_y(player_turn);

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
    move_user_joystick_x_y(player_turn);

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
      return;
    }
    // If we selected another piece of our own, replace selected piece
    if (p_board->pieces[destination_y][destination_x]->get_type() != EMPTY &&
        p_board->pieces[destination_y][destination_x]->get_color() ==
            player_turn) {
      // Replace selected piece, and repeat this state
      selected_x = destination_x;
      selected_y = destination_y;
      return;
    }
    // Check if the move is valid (if the destination is in the list of possible moves)
    bool valid_move = false;
    for (int8_t i = 0; i < all_moves[selected_y][selected_x].size(); i++) {
      if (all_moves[selected_y][selected_x][i].first.first == destination_x &&
          all_moves[selected_y][selected_x][i].first.second == destination_y) {
        valid_move = true;
        capture_x = all_moves[selected_y][selected_x][i].second.first;
        capture_y = all_moves[selected_y][selected_x][i].second.second;
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

   
    // Check if capture:
    if (capture_x != -1) {
      // See if the captured piece is a temp piece
      bool captured_piece_is_temp = false;
      int8_t captured_temp_piece_x = -1;
      int8_t captured_temp_piece_y = -1;
      for (int i = 0; i < promoted_pawns_using_temp_pieces.size(); i++) {
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
                   graveyard_coordinate.second, motorStepDelay);
        // Update the graveyard memory
        graveyard[10 + p_board->pieces[capture_y][capture_x]->get_color()]++;
        // Remove the promoted pawn from the vector
        for (int i = 0; i < promoted_pawns_using_temp_pieces.size(); i++) {
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
        for (int i = 0; i < promoted_pawns_using_temp_pieces.size(); i++) {
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
            move_piece_by_motor(pawn_x, pawn_y, graveyard_coordinate.first, graveyard_coordinate.second, motorStepDelay);

            // Move captured piece to the pawn's location (use safe move)
            move_piece_by_motor(capture_x, capture_y, pawn_x, pawn_y, motorStepDelay);

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
          move_piece_by_motor(capture_x, capture_y, graveyard_coordinate.first, graveyard_coordinate.second, motorStepDelay);

          // If colour is black, add 5 to the index, and update the graveyard
          graveyard_index += 5 * p_board->pieces[capture_y][capture_x]->get_color();
          graveyard[graveyard_index]++;
        }
      }
    }
    // Move the piece - regardless of capture
    // If the piece that's moving is a temp piece, edit its coordinates in the
    // promoted_pawns_using_temp_pieces vector
    for (int i = 0; i < promoted_pawns_using_temp_pieces.size(); i++) {
      if (promoted_pawns_using_temp_pieces[i].first == selected_x &&
          promoted_pawns_using_temp_pieces[i].second == selected_y) {
        promoted_pawns_using_temp_pieces[i].first = destination_x;
        promoted_pawns_using_temp_pieces[i].second = destination_y;
        break;
      }
    }
    // Move the piece (fast move except knight)
    move_piece_by_motor(selected_x, selected_y, destination_x, destination_y,
               motorStepDelay);

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
      move_piece_by_motor(rook_x, rook_y, (selected_x + destination_x) / 2, selected_y,
                 motorStepDelay);
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

    // Get button input, update joystick location (Special for pawn promotion,
    // use promotion_joystick_selection) (there's only 4 possible values, 0, 1, 2, 3)
    move_user_joystick_promotion(player_turn);

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
               graveyard_coordinate.second, motorStepDelay);
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
                 destination_x, destination_y, motorStepDelay);
    } else {
      // There isn't a valid piece in the graveyard, use a temp piece

      // Update the graveyard memory so the get_graveyard_empty_coordinate returns the coordinate of the first available temp piece
      graveyard[10 + p_board->pieces[destination_y][destination_x]->get_color()]--;

      // Move a temp piece to the destination (6 == temp piece)
      graveyard_coordinate = get_graveyard_empty_coordinate(
          6, p_board->pieces[destination_y][destination_x]->get_color());
      move_piece_by_motor(graveyard_coordinate.first, graveyard_coordinate.second,
                 destination_x, destination_y, motorStepDelay);

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

    // End a turn - switch player
    player_turn = !player_turn;

    // Turn off promotion LED light if that was on. (if you have a separate LED
    // for promotion indicator)

    game_state = GAME_BEGIN_TURN;
  } else if (game_state == GAME_OVER_WHITE_WIN) {
    // White wins

    // TODO: some sort of display, and reset the game (Maybe on the OLED display)
    Serial.println("White wins!");
    game_state = GAME_RESET;
  } else if (game_state == GAME_OVER_BLACK_WIN) {
    // Black wins

    // TODO: some sort of display, and reset the game (Maybe on the OLED display)
    Serial.println("Black wins!");
    game_state = GAME_RESET;
  } else if (game_state == GAME_OVER_DRAW) {
    // Draw

    // TODO: some sort of display, and reset the game (Maybe on the OLED display)
    Serial.println("Draw!");
    Serial.println("Reason: ");
    if (draw_fifty_move_rule) {
      Serial.println("Fifty move rule");
    } else if (draw_three_fold_repetition) {
      Serial.println("Three fold repetition");
    } else if (draw_stalemate) {
      Serial.println("Stalemate");
    } else if (draw_insufficient_material) {
      Serial.println("Insufficient material");
    }
    game_state = GAME_RESET;
  } else if (game_state == GAME_RESET) {
    // Reset the game (move pieces back to initial position, clear memory (mainly focusing on vectors))

    // First, move the pieces back to the initial position
    // TODO: motor job

    // Free memory
    delete p_board;

    // Clear vectors (find ones that aren't cleared by game_initialize)
    // TODO

    // Reset game state
    game_state = GAME_POWER_ON;
  }
}
