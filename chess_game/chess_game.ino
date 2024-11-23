// Include
#include "ArduinoSTL.h"
#include "Board.h"
#include "Piece.h"
#include "PieceType.h"
#include "MemoryFree.h"

enum GameState {
  GAME_POWER_ON,  // Power on the board, motors calibrate
  GAME_IDLE,  // No game is being played yet, waiting for a game to start
  GAME_INITIALIZE,  // Game started, initialize the board
  GAME_BEGIN_TURN,  // Begin a turn - generate moves, remove illegal moves, check for checkmate/draw
  GAME_WAIT_FOR_SELECT,  // Joystick moving, before pressing select button on valid piece
  GAME_WAIT_FOR_MOVE,  // Joystick moving, after pressing select button on valid destination
  GAME_MOVE_MOTOR,  // Motors moving pieces
  GAME_END_MOVE,  // Update chess board, see if a pawn can promote
  GAME_WAIT_FOR_SELECT_PAWN_PROMOTION,  // Joystick moving, before pressing select button on promotion piece
  GAME_PAWN_PROMOTION_MOTOR,  // Motors moving pieces for pawn promotion
  GAME_END_TURN,  // End a turn - switch player
  GAME_OVER_WHITE_WIN,  // White wins
  GAME_OVER_BLACK_WIN,  // Black wins
  GAME_OVER_DRAW,  // Draw
  GAME_RESET  // Reset the game
};

// Game state
GameState game_state;

// Display in serial monitor
const char CHESS_PIECE_CHAR[] = {' ', 'K', 'Q', 'B', 'N', 'R', 'P', 'k', 'q', 'b', 'n', 'r', 'p'};

// 0 for white to move, 1 for black to move
bool player_turn;

// 0 for player is not under check, 1 for player is under check
bool current_player_under_check;

// Sources of check, if any (x, y coordinates of the piece that is checking the king)
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
int8_t promotion_type; // 0 for queen, 1 for rook, 2 for bishop, 3 for knight

// TODO: initialize a graveyard for pieces. 
// TODO: have a vector list of all promoted pawns that are using "temp" promotion pieces

// Initialize Memory
Board *p_board;
std::vector<std::pair<std::pair<int8_t, int8_t>, std::pair<int8_t, int8_t>>> all_moves[8][8];

// JOYSTICK CONTROL
const int JOYSTICK_POS_X_PIN[] = {2, 2};
const int JOYSTICK_POS_Y_PIN[] = {3, 3};
const int JOYSTICK_NEG_X_PIN[] = {4, 4};
const int JOYSTICK_NEG_Y_PIN[] = {5, 5};
const int JOYSTICK_BUTTON_PIN[] = {6, 6};
// User Joystick Location
int8_t joystick_x[2];
int8_t joystick_y[2];
bool confirm_button_pressed[2];
// Previous joystick input -- for edge detection
bool prev_confirm_button_pressed[2];
bool prev_joystick_neutral[2];
// Promotion Joystick Selection
int8_t promotion_joystick_selection; // only happening on one player's turn, so no need for 2

void move_user_joystick_x_y(bool color) {
  // Get input from the joystick
  // color: 0 for white, 1 for black
  // Update the joystick coordinates and confirm button pressed flag
  // Result: <color>_joystick_<x,y> ++ or -- (mod 8), <color>_confirm_button_pressed = true if confirm button is pressed from neutral state

  // Read the joystick values -- low is pressed, high is not pressed
  int x_val = digitalRead(JOYSTICK_POS_X_PIN[color]);
  int y_val = digitalRead(JOYSTICK_POS_Y_PIN[color]);
  int neg_x_val = digitalRead(JOYSTICK_NEG_X_PIN[color]);
  int neg_y_val = digitalRead(JOYSTICK_NEG_Y_PIN[color]);
  int button_val = digitalRead(JOYSTICK_BUTTON_PIN[color]);

  // Update the joystick values if joystick WAS neutral and now has a value. Update neutral flag.
  // Note active low for button (pressed is low 0)
  if (prev_joystick_neutral[color]){
    // If previous joystick WAS neutral
    bool change_happened = false; // For displaying, only serial printif change happened.
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
    confirm_button_pressed[color] = false; // Note: this also automatically reset the confirm_button_pressed flag
    // If your state machine requires multiple cycles to "load in" the confirm button press, you need to remove this line from this function
  }
  // Update the previous confirm button pressed flag (true if button was pressed, but button is active low)
  prev_confirm_button_pressed[color] = !button_val; // active low
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

  // Update the joystick values if joystick WAS neutral and now has a value. Update neutral flag.
  // Note active low for button (pressed is low 0)
  if (prev_joystick_neutral[color]){
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

  // Update the previous confirm button pressed flag (true if button was pressed, but button is active low)
  prev_confirm_button_pressed[color] = !button_val; // active low
}

void serial_display_board_and_selection(){
  Serial.println("---------------------------------------------------");
  for (int row = 7; row >=0; row--){
    for (int col = 0; col < 8; col++)
    { 
      bool showed = false;
      for (int i = 0; i < all_moves[selected_y][selected_x].size(); i++) {
        if (all_moves[selected_y][selected_x][i].second.first == col && all_moves[selected_y][selected_x][i].second.second == row) {
          Serial.print("X");
          Serial.print("\t");
          showed = true;
          break;
        }
        if (all_moves[selected_y][selected_x][i].first.first == col && all_moves[selected_y][selected_x][i].first.second == row) {
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
        // Print CHESS_PIECE_CHAR[p_board->pieces[row][col]->get_type() + 6*p_board->pieces[row][col]->get_color()];
        Serial.print(CHESS_PIECE_CHAR[p_board->pieces[row][col]->get_type() + 6*p_board->pieces[row][col]->get_color()]);
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

void setup()
{
  Serial.begin(9600);

  // Initial game state
  game_state = GAME_POWER_ON;
}

void loop()
{
  // Serial.print("Current State: ");
  // Serial.println(game_state);
  // Serial.print("Free memory: ");
  // Serial.println(freeMemory());
  delay(50);

  // State machine, during each loop, we are in a certain state
  if (game_state == GAME_POWER_ON) {
      // Power on the board, motors calibrate, initialize pins

      // Pins initialization
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
      // TODO
      game_state = GAME_IDLE;
  } else if (game_state == GAME_IDLE) {
      // No game is being played yet, waiting for a game to start
      // Wait for a button press to start the game
      // If button pressed, initialize the game
      // If button not pressed, stay in this state
      // TODO: currently we just skip this state, assume game is always started
      game_state = GAME_INITIALIZE;
  } else if (game_state == GAME_INITIALIZE) { 
      // Game started, initialize the board
      // Initialize the board
      p_board = new Board();
      player_turn = 0; // White to move first
      current_player_under_check = 0;
      sources_of_check.clear();
      selected_x = 0;
      selected_y = 0;
      destination_x = 0;
      destination_y = 0;
      capture_x = 0;
      capture_y = 0;
      
      // Joystick location
      joystick_x[0] = 4;
      joystick_y[0] = 0;
      joystick_x[1] = 4;
      joystick_y[1] = 7;
      confirm_button_pressed[0] = false;
      confirm_button_pressed[1] = false;
      prev_confirm_button_pressed[0] = true; // Assume the button is pressed before the game starts, so force user to release the button
      prev_confirm_button_pressed[1] = true;
      prev_joystick_neutral[0] = true;
      prev_joystick_neutral[1] = true;
      
      // Promotion joystick selection
      promotion_joystick_selection = 0;

      // TODO: initialize graveyard
      // TODO: initialize promoted pawns (with temp pieces) vector
      // TODO: motor calibration can be done here
      game_state = GAME_BEGIN_TURN;
    } else if (game_state == GAME_BEGIN_TURN) {
      // Begin a turn - generate moves, remove illegal moves, check for checkmate/draw

      // TODO: this is temporary, just to see the board state
      serial_display_board_and_selection();

      // Check if 50 move rule is reached
      if (p_board->draw_move_counter >= 50) {
        game_state = GAME_OVER_DRAW;
        return;
      }

      // Check if 3-fold repetition is reached
      // TODO: implement this
      if (p_board->is_three_fold_repetition()) {
        game_state = GAME_OVER_DRAW;
        return;
      }

      // Generate all possible moves for the player, and remove illegal moves
      bool no_moves = true;
      for (int8_t i = 0; i < 8; i++) {
        for (int8_t j = 0; j < 8; j++) {
          // Non-player's piece should have empty moves
          all_moves[i][j].clear();
          if (p_board->pieces[i][j]->get_type() == EMPTY || p_board->pieces[i][j]->get_color() != player_turn) {
            continue;
          }
          all_moves[i][j] = p_board->pieces[i][j]->get_possible_moves(p_board);
          p_board->remove_illegal_moves_for_a_piece(j, i, all_moves[i][j]);  // NOTE it's j, i!!!!!
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
      // If it's a valid piece, indicate the piece is selected, and move to GAME_WAIT_FOR_MOVE
      
      // LED display
      // HERE, highlight the "prevoius move" that just happened by highlighting pieces... (orange)
      // Also display the cursor location - which is joystick_x[player_turn], joystick_y[player_turn] (green - overwrites orange)

      // Get button input, update joystick location
      move_user_joystick_x_y(player_turn);

      // Don't move until the confirm button is pressed
      if (confirm_button_pressed[player_turn]) {
        confirm_button_pressed[player_turn] = false;
      } else {
        return;
      }

      selected_x = joystick_x[player_turn];
      selected_y = joystick_y[player_turn];

      // Check for valid input
      if (selected_x < 0 || selected_x > 7 || selected_y < 0 || selected_y > 7) {
        // Invalid input
        return;
      }
      if (p_board->pieces[selected_y][selected_x]->get_type() == EMPTY || p_board->pieces[selected_y][selected_x]->get_color() != player_turn) {
        // Not player's piece
        return;
      }

      // TEMP DISPLAY CODE:
      serial_display_board_and_selection();

      // PROCEED TO NEXT STATE
      game_state = GAME_WAIT_FOR_MOVE;
    } else if (game_state == GAME_WAIT_FOR_MOVE) {
      // Joystick moving, after pressing select button on valid destination
      // Wait for joystick input
      // If there is input, check if it's a valid destination, if not, ignore
      // If the input is the selected_x, selected_y, move back to GAME_WAIT_FOR_SELECT
      // If it's another of your own piece, replace selected_x, selected_y with the new piece, stay in this state
      // If it's a valid destination, move the piece, and move to GAME_MOVE_MOTOR

      // LED display
      // HERE, highlight the "prevoius move" that just happened by highlighting pieces...
      // Also display the cursor location - which is joystick_x[player_turn], joystick_y[player_turn] (green - overwrites orange)
      // Also highlight the "selected piece" with a different color (selected_x, selected_y) (blue) overwrites green

      // Get button input, update joystick location
      move_user_joystick_x_y(player_turn);

      // Don't move until the confirm button is pressed
      if (confirm_button_pressed[player_turn]) {
        confirm_button_pressed[player_turn] = false;
      } else {
        return;
      }

      destination_x = joystick_x[player_turn];
      destination_y = joystick_y[player_turn];
      
      // Check for valid input
      if (destination_x < 0 || destination_x > 7 || destination_y < 0 || destination_y > 7) {
        // Invalid input
        return;
      }
      if (destination_x == selected_x && destination_y == selected_y) {
        // Move back to select
        game_state = GAME_WAIT_FOR_SELECT;
        return;
      }
      if (p_board->pieces[destination_y][destination_x]->get_type() != EMPTY && p_board->pieces[destination_y][destination_x]->get_color() == player_turn) {
        // Replace selected piece, and repeat this state
        selected_x = destination_x;
        selected_y = destination_y;
        return;
      }
      // Check if the move is valid
      bool valid_move = false;
      for (int8_t i = 0; i < all_moves[selected_y][selected_x].size(); i++) {
        if (all_moves[selected_y][selected_x][i].first.first == destination_x && all_moves[selected_y][selected_x][i].first.second == destination_y) {
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
      // Motors moving pieces given the selected piece and destination, and capture square - capture square, if any, should be moved to the graveyard

      // LED: just display the new "move" that just happened - which is seleced_x, selected_y, destination_x, destination_y, (capture_x, capture_y -- don't need to display this, but ok if you want to)

      // Important: If captured square contains a piece a current pawn is promoted to, and is using temp piece, replace the temp piece with the promoted piece
      // Then move temp piece to the graveyard
      // Update the graveyard memory and the promoted pawns using temp pieces vector.

      // Important: If this move is a castle, move the rook first (do a check if p_board->pieces[selected_y][selected_x]->type == KING && abs(destination_x - selected_x) == 2)
      // Assume this is valid move, since it's checked in the previous state

      // TODO: implement motor moving pieces, now just skip this state
      game_state = GAME_END_MOVE;
  } else if (game_state ==  GAME_END_MOVE) {
      // Update chess board, see if a pawn can promote
      p_board->move_piece(selected_x, selected_y, destination_x, destination_y, capture_x, capture_y);
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
      // If it's a valid promotion piece, indicate the piece is selected, and move to GAME_PAWN_PROMOTION_MOTOR
      
      // LED display
      // HERE (Display the promotion_joystick_selection, which has value 0,1,2,3) (separate LED maybe at the edge of board? - the "promotion" indicator squares are basically same as other squares, but with a "thicker" top custom shape that does not allow a "queen-shaped light" to go thru, so when this light is on... queen-shaped black line or queen-shaped light goes thru)
      // Also display the "move" that just happened by highlight pieces... (orange colour)

      // Get button input, update joystick location (Special for pawn promotion, use promotion_joystick_selection)
      move_user_joystick_promotion(player_turn);

      // Don't move until the confirm button is pressed
      if (confirm_button_pressed[player_turn]) {
        confirm_button_pressed[player_turn] = false;
      } else {
        return;
      }

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
      // Motors moving pieces for pawn promotion

      // LED: keep the "promotion light" on, but with a different color (maybe purple?)

      // Check if there's a valid piece in the graveyard to be used for promotion

      // If there is one, use that piece for promotion, and update graveyard memory

      // If there isn't one, use a temp piece but record the promoted pawn in the promoted pawns using temp pieces vector

      // TODO: now we skip this state
      game_state = GAME_END_TURN;
  } else if (game_state == GAME_END_TURN) {
      // End a turn - switch player
      player_turn = !player_turn;

      // Turn off promotion LED light if that was on. (if you have a separate LED for promotion indicator)

      game_state = GAME_BEGIN_TURN;
  } else if (game_state == GAME_OVER_WHITE_WIN) {
      // White wins

      // TODO: some sort of display, and reset the game
      Serial.println("White wins!");
      game_state = GAME_RESET;  
  } else if (game_state == GAME_OVER_BLACK_WIN) {
      // Black wins

      // TODO: some sort of display, and reset the game
      Serial.println("Black wins!");
      game_state = GAME_RESET;
  } else if (game_state == GAME_OVER_DRAW) {
      // Draw

      // TODO: some sort of display, and reset the game
      Serial.println("Draw!");
      game_state = GAME_RESET;
  } else if (game_state == GAME_RESET) {
      // Reset the game

      // First, move the pieces back to the initial position
      // TODO: motor job

      // Free memory
      delete p_board;
      // Reset game state
      game_state = GAME_POWER_ON;
  }
}
