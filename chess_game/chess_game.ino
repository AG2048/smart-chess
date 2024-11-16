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

void setup()
{
  Serial.begin(9600);

  // Initial game state
  game_state = GAME_POWER_ON;
}

void loop()
{
  // State machine, during each loop, we are in a certain state
  switch (game_state) {
    case GAME_POWER_ON:
      // Power on the board, motors calibrate
      // TODO
      game_state = GAME_IDLE;
      break;
    case GAME_IDLE:
      // No game is being played yet, waiting for a game to start
      // Wait for a button press to start the game
      // If button pressed, initialize the game
      // If button not pressed, stay in this state
      // TODO: currently we just skip this state, assume game is always started
      game_state = GAME_INITIALIZE;
      break;
    case GAME_INITIALIZE:
      // Game started, initialize the board
      // Initialize the board
      p_board = new Board();
      player_turn = 0; // White to move first
      current_player_under_check = 0;
      sources_of_check.clear();
      selected_x = -1;
      selected_y = -1;
      destination_x = -1;
      destination_y = -1;
      capture_x = -1;
      capture_y = -1;
      // TODO: initialize graveyard
      // TODO: initialize promoted pawns (with temp pieces) vector
      // TODO: motor calibration can be done here
      game_state = GAME_BEGIN_TURN;
      break;
    case GAME_BEGIN_TURN:
      // Begin a turn - generate moves, remove illegal moves, check for checkmate/draw

      // TODO: this is temporary, just to see the board state
      for (int row = 7; row >=0; row--)
      {
        for (int col = 0; col < 8; col++)
        { 
          Serial.print(p_board->pieces[row][col]->type);
        }
        Serial.println();
      }
      
      // Check if 50 move rule is reached
      if (p_board->draw_move_counter >= 50) {
        game_state = GAME_OVER_DRAW;
        break;
      }

      // Check if 3-fold repetition is reached
      // TODO: implement this

      // Generate all possible moves for the player, and remove illegal moves
      bool no_moves = true;
      for (int8_t i = 0; i < 8; i++) {
        for (int8_t j = 0; j < 8; j++) {
          // Non-player's piece should have empty moves
          if (p_board->pieces[i][j]->get_color() != player_turn) {
            all_moves[i][j].clear();
            continue;
          }
          all_moves[i][j] = p_board->pieces[i][j]->get_possible_moves(p_board);
          p_board->remove_illegal_moves_for_a_piece(i, j, all_moves[i][j]);
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
        break;
      }

      // Move to the next state
      game_state = GAME_WAIT_FOR_SELECT;
      Serial.print("S1231231231231: ");
      break;
    case GAME_WAIT_FOR_SELECT:
    Serial.print("Serial asdaffasfas: ");
      // Joystick moving, before pressing select button on valid piece
      // Wait for joystick input
      // If there is input, check if it's a valid piece, if not, ignore
      // If it's a valid piece, indicate the piece is selected, and move to GAME_WAIT_FOR_MOVE
      
      // TODO: replace with button inputs
      Serial.print("Serial available: ");
      Serial.println(Serial.available());
      if (Serial.available() >= 2) {
        while (Serial.available() == 0) {
        // Wait for input
        Serial.println("Waiting for input");
      }
        selected_x = Serial.read();
        while (Serial.available() == 0) {
        // Wait for input
        Serial.println("Waiting for input");
      }
        selected_y = Serial.read();

        Serial.print("Selected: ");
        Serial.print(selected_x);
        Serial.print(", ");
        Serial.println(selected_y);
        

        // Convert to int8_t
        selected_x -= '0';
        selected_y -= '0';
      }

      // Check for valid input
      if (selected_x < 0 || selected_x > 7 || selected_y < 0 || selected_y > 7) {
        // Invalid input
        break;
      }
      if (p_board->pieces[selected_y][selected_x]->get_color() != player_turn) {
        // Not player's piece
        break;
      }
      game_state = GAME_WAIT_FOR_MOVE;
      break;
    case GAME_WAIT_FOR_MOVE:
      // Joystick moving, after pressing select button on valid destination
      // Wait for joystick input
      // If there is input, check if it's a valid destination, if not, ignore
      // If the input is the selected_x, selected_y, move back to GAME_WAIT_FOR_SELECT
      // If it's another of your own piece, replace selected_x, selected_y with the new piece, stay in this state
      // If it's a valid destination, move the piece, and move to GAME_MOVE_MOTOR

      // TODO: replace with button inputs
      if (Serial.available() >= 2) {
        destination_x = Serial.read();
        destination_y = Serial.read();

        // Convert to int8_t
        destination_x -= '0';
        destination_y -= '0';
      }
      
      // Check for valid input
      if (destination_x < 0 || destination_x > 7 || destination_y < 0 || destination_y > 7) {
        // Invalid input
        break;
      }
      if (destination_x == selected_x && destination_y == selected_y) {
        // Move back to select
        game_state = GAME_WAIT_FOR_SELECT;
        break;
      }
      if (p_board->pieces[destination_y][destination_x]->get_color() == player_turn) {
        // Replace selected piece, and repeat this state
        selected_x = destination_x;
        selected_y = destination_y;
        break;
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
        break;
      }

      // Move is valid, move the piece
      game_state = GAME_MOVE_MOTOR;
      break;
    case GAME_MOVE_MOTOR:
      // Motors moving pieces given the selected piece and destination, and capture square - capture square, if any, should be moved to the graveyard

      // Important: If captured square contains a piece a current pawn is promoted to, and is using temp piece, replace the temp piece with the promoted piece
      // Then move temp piece to the graveyard
      // Update the graveyard memory and the promoted pawns using temp pieces vector.

      // TODO: implement motor moving pieces, now just skip this state
      game_state = GAME_END_MOVE;
      break;
    case GAME_END_MOVE:
      // Update chess board, see if a pawn can promote
      p_board->move_piece(selected_x, selected_y, destination_x, destination_y, capture_x, capture_y);
      // Check if a pawn can promote
      if (p_board->can_pawn_promote(destination_x, destination_y)) {
        game_state = GAME_WAIT_FOR_SELECT_PAWN_PROMOTION;
      } else {
        game_state = GAME_END_TURN;
      }
      break;
    case GAME_WAIT_FOR_SELECT_PAWN_PROMOTION:
      // Joystick moving, before pressing select button on promotion piece
      // Wait for joystick input
      // If there is input, check if it's a valid promotion piece, if not, ignore
      // If it's a valid promotion piece, indicate the piece is selected, and move to GAME_PAWN_PROMOTION_MOTOR
      
      // TODO: replace with button inputs
      if (Serial.available() == 0) {
        break;
      }
      promotion_type = Serial.read();
      promotion_type -= '0';

      // Check for valid input
      if (promotion_type < 0 || promotion_type > 3) {
        // Invalid input
        break;
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
      break;
    case GAME_PAWN_PROMOTION_MOTOR:
      // Motors moving pieces for pawn promotion

      // Check if there's a valid piece in the graveyard to be used for promotion

      // If there is one, use that piece for promotion, and update graveyard memory

      // If there isn't one, use a temp piece but record the promoted pawn in the promoted pawns using temp pieces vector

      // TODO: now we skip this state
      game_state = GAME_END_TURN;
      break;
    case GAME_END_TURN:
      // End a turn - switch player
      player_turn = !player_turn;
      game_state = GAME_BEGIN_TURN;
      break;  
    case GAME_OVER_WHITE_WIN:
      // White wins

      // TODO: some sort of display, and reset the game
      Serial.println("White wins!");
      game_state = GAME_RESET;  
      break;  
    case GAME_OVER_BLACK_WIN:
      // Black wins

      // TODO: some sort of display, and reset the game
      Serial.println("Black wins!");
      game_state = GAME_RESET;
      break;  
    case GAME_OVER_DRAW:
      // Draw

      // TODO: some sort of display, and reset the game
      Serial.println("Draw!");
      game_state = GAME_RESET;
      break;
    case GAME_RESET:
      // Reset the game

      // First, move the pieces back to the initial position
      // TODO: motor job

      // Free memory
      delete p_board;
      // Reset game state
      game_state = GAME_POWER_ON;
      break;
  }
  Serial.print("Current State: ");
  Serial.println(game_state);
  Serial.print("Free memory: ");
  Serial.println(freeMemory());
  delay(1000);
}
