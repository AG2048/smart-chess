// Include
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <stdint.h>
#include <vector>
#include <utility>
#include <string.h>
#include <Arduino.h>
#include "FastLED.h"
// #include "ArduinoSTL.h"
#include "Board.h"
// #include "MemoryFree.h"
#include "Piece.h"
#include "PieceType.h"
#include "Timer.h"

// SOME DEBUG DEFINES...
#define USING_STOCKFISH 0 // 1 for using stockfish, 0 for not using stockfish
#define USING_OLED 0 // 1 for using OLED, 0 for not using OLED

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
int8_t player_is_computer[2];  // 0 for human, 1 for computer

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
bool promotion_happened = false;  // If true, a pawn has been promoted
bool is_first_move = true;  // If true, this is the first move of the game

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
      return std::make_pair(-3, graveyard[graveyard_index]); // small bug that was ignored before, used 11 instead of -3. 
    }
  }
}

// Function to return a vector of motor moves that would
// result in the board being reset to starting position.

// The pair consists of Board indices, calculated using (y * 14 + 3) + x (this 14+3 is to handle negative x coordinate from -3 all the way to 10)
std::vector<std::pair<int8_t, int8_t>> reset_board(Board *p_board){ // instead of _ * 14 + 3 + col, --> _ * 14 + col

  /*
  This function will return a vector of motor moves that will result in the board being reset to the starting position.
  1. The function moves all temp pieces on the board back to their starting squares.
  2. Function moves all pieces currently on board to their starting squares.
  3. Function moves all pieces in the graveyard to their starting squares.

  RETURN VALUE: vector of integers. From and to squares in the form of (y * 14 + 3) + x
  */

  // temporary -- check to be sure
  int8_t temp_x = -1, temp_y = 0;
  bool loop_detected;
  std::vector<int8_t> current_chain; // Chain to hold the current chain of indices we're moving (acts like a stack)
  std::vector<std::pair<int8_t, int8_t>> reset_moves; // The vector to be returned

  // An array to keep track of where each piece wants to move to (for 2nd step)
  int8_t destination_arr[14*8] = {-1}; // Initialize every element to -1
  bool square_is_already_destination[14*8] = {false}; // Initialize every element to false, this array is to keep track if a square has been designated a destination already (so 2 pieces of same kind don't move to same square)

  // ### Moving all temp pieces off of the board ###
  // Looping through all of the pieces in the board to find temp pieces
  for (int8_t i = 0; i < 8; i++) {
    for (int8_t j = 0; j < 8; j++) {
      // i is x, j is y
      if (p_board->pieces[i][j]->type == EMPTY || p_board->pieces[i][j]->type == PAWN) {
        // Ignore pawn or empty squares
        continue;
      } else {
        // check if this piece is currently using a temp piece
        for (int8_t temp_piece_index = 0; temp_piece_index < promoted_pawns_using_temp_pieces.size(); temp_piece_index++) { 
          if (promoted_pawns_using_temp_pieces[temp_piece_index].first == j && promoted_pawns_using_temp_pieces[temp_piece_index].second == i) {
            // This piece is using a temp piece
            // Find first empty square in graveyard, send it back (graveyard (6 == temp piece))
            std::pair<int8_t, int8_t> graveyard_coordinate = get_graveyard_empty_coordinate(6, p_board->pieces[i][j]->get_color());
            // Add the move to the reset_moves vector
            reset_moves.push_back(std::make_pair((i * 14 + 3) + j, (graveyard_coordinate.second * 14 + 3) + graveyard_coordinate.first));
            // Update the graveyard memory
            graveyard[10 + p_board->pieces[i][j]->get_color()]++;
            // Update the board object, make it think this square is empty
            p_board->pieces[i][j]->type = EMPTY;
            break;
          }
        }
      }
    }
  }
  
  // TODO: loop thru the board, and find ALL R,B,N,pawns that are at ONE of their valid destination squares. 
  // The idea of this code is to prevent the case where a piece is at its destination square, and then we move it to another square.
 for (int8_t j = 0; j < 8; j++) {
    for (int8_t i = 0; i < 8; i++) {
      // We are looping in column major order (for purpose of allowing each piece to get to the "closer" square)
      if (p_board->pieces[i][j]->type == EMPTY || p_board->pieces[i][j]->type == KING || p_board->pieces[i][j]->type == QUEEN) {
        // Ignore empty squares
        continue;
      } else {
        // Find out where this piece originally belongs to
        if (p_board->pieces[i][j]->type == ROOK){
          // For rooks, we have to check if the "leftmore" square is already occupied
          // White rook:
          if (p_board->pieces[i][j]->color == 0) {
            if ((i == 0) && (j == 0 || j ==7)) {
              destination_arr[i * 14 + 3 + j] = -1;
              square_is_already_destination[i * 14 + 3 + j] = true;
            }
          // Black rook:
          } else {
            if ((i == 7) && (j == 0 || j ==7)) {
              destination_arr[i * 14 + 3 + j] = -1;
              square_is_already_destination[i * 14 + 3 + j] = true;
            }
          }
        } else if (p_board->pieces[i][j]->type == BISHOP){
          // For bishops, we have to check if the "leftmore" square is already occupied
          // White bishop:
          if (p_board->pieces[i][j]->color == 0) {
          if ((i == 0) && (j == 2 || j == 5)) {
              destination_arr[i * 14 + 3 + j] = -1;
              square_is_already_destination[i * 14 + 3 + j] = true;
            }
          // Black bishop:
          } else {
          if ((i == 7) && (j == 2 || j == 5)) {
              destination_arr[i * 14 + 3 + j] = -1;
              square_is_already_destination[i * 14 + 3 + j] = true;
            }
          }
        } else if (p_board->pieces[i][j]->type == KNIGHT){
          // For knights, we have to check if the "leftmore" square is already occupied
          // White knight:
          if (p_board->pieces[i][j]->color == 0) {
          if ((i == 0) && (j == 1 || j == 6)) {
              destination_arr[i * 14 + 3 + j] = -1;
              square_is_already_destination[i * 14 + 3 + j] = true;
            }
          // Black knight:
          } else {
          if ((i == 7) && (j == 1 || j == 6)) {
              destination_arr[i * 14 + 3 + j] = -1;
              square_is_already_destination[i * 14 + 3 + j] = true;
            }
          }
        } else if (p_board->pieces[i][j]->type == PAWN){
          // For pawn, we have a small for loop to check if the "leftmore" square is already occupied
          // White pawn:
          if (p_board->pieces[i][j]->color == 0) {
          if ((i == 1)) {
              destination_arr[i * 14 + 3 + j] = -1;
              square_is_already_destination[i * 14 + 3 + j] = true;
            }
          // Black pawn:
          } else {
          if (i == 6) {
              destination_arr[i * 14 + 3 + j] = -1;
              square_is_already_destination[i * 14 + 3 + j] = true;
            }
          }
        }
      }
    }
  }

  // ### Find out where each piece that's currently on the board wants to move to ###
  // If the piece is currently on their destination, destination_arr = -1, square_is_already_destination = true
  for (int8_t j = 0; j < 8; j++) {
    for (int8_t i = 0; i < 8; i++) {
      // We are looping in column major order (for purpose of allowing each piece to get to the "closer" square)
      if (p_board->pieces[i][j]->type == EMPTY) {
        // Ignore empty squares
        continue;
      } else {
        // Find out where this piece originally belongs to
        if (p_board->pieces[i][j]->type == KING){
          // White king:
          if (p_board->pieces[i][j]->color == 0) {
          destination_arr[(i * 14 + 3) + j] = (0 * 14 + 3) + 4;
          square_is_already_destination[(0 * 14 + 3) + 4] = true;
          // Black king:
          } else {
          destination_arr[(i * 14 + 3) + j] = (7 * 14 + 3) + 4;
          square_is_already_destination[(7 * 14 + 3) + 4] = true;
          }
        } else if (p_board->pieces[i][j]->type == QUEEN){
          // White queen:
          if (p_board->pieces[i][j]->color == 0) {
          destination_arr[(i * 14 + 3) + j] = (0 * 14 + 3) + 3;
          square_is_already_destination[(0 * 14 + 3) + 3] = true;
          // Black queen:
          } else {
          destination_arr[(i * 14 + 3) + j] = (7 * 14 + 3) + 3;
          square_is_already_destination[(7 * 14 + 3) + 3] = true;
          }
        } else if (p_board->pieces[i][j]->type == ROOK){
          // For rooks, we have to check if the "leftmore" square is already occupied
          // White rook:
          if (p_board->pieces[i][j]->color == 0) {
          if (square_is_already_destination[(0 * 14 + 3) + 0] == false) {
            destination_arr[(i * 14 + 3) + j] = (0 * 14 + 3) + 0;
            square_is_already_destination[(0 * 14 + 3) + 0] = true;
          } else {
            destination_arr[(i * 14 + 3) + j] = (0 * 14 + 3) + 7;
            square_is_already_destination[(0 * 14 + 3) + 7] = true;
          }
          // Black rook:
          } else {
          if (square_is_already_destination[(7 * 14 + 3) + 0] == false) {
            destination_arr[(i * 14 + 3) + j] = (7 * 14 + 3) + 0;
            square_is_already_destination[(7 * 14 + 3) + 0] = true;
          } else {
            destination_arr[(i * 14 + 3) + j] = (7 * 14 + 3) + 7;
            square_is_already_destination[(7 * 14 + 3) + 7] = true;
          }
          }
        } else if (p_board->pieces[i][j]->type == BISHOP){
          // For bishops, we have to check if the "leftmore" square is already occupied
          // White bishop:
          if (p_board->pieces[i][j]->color == 0) {
          if (square_is_already_destination[(0 * 14 + 3) + 2] == false) {
            destination_arr[(i * 14 + 3) + j] = (0 * 14 + 3) + 2;
            square_is_already_destination[(0 * 14 + 3) + 2] = true;
          } else {
            destination_arr[(i * 14 + 3) + j] = (0 * 14 + 3) + 5;
            square_is_already_destination[(0 * 14 + 3) + 5] = true;
          }
          // Black bishop:
          } else {
          if (square_is_already_destination[(7 * 14 + 3) + 2] == false) {
            destination_arr[(i * 14 + 3) + j] = (7 * 14 + 3) + 2;
            square_is_already_destination[(7 * 14 + 3) + 2] = true;
          } else {
            destination_arr[(i * 14 + 3) + j] = (7 * 14 + 3) + 5;
            square_is_already_destination[(7 * 14 + 3) + 5] = true;
          }
          }
        } else if (p_board->pieces[i][j]->type == KNIGHT){
          // For knights, we have to check if the "leftmore" square is already occupied
          // White knight:
          if (p_board->pieces[i][j]->color == 0) {
          if (square_is_already_destination[(0 * 14 + 3) + 1] == false) {
            destination_arr[(i * 14 + 3) + j] = (0 * 14 + 3) + 1;
            square_is_already_destination[(0 * 14 + 3) + 1] = true;
          } else {
            destination_arr[(i * 14 + 3) + j] = (0 * 14 + 3) + 6;
            square_is_already_destination[(0 * 14 + 3) + 6] = true;
          }
          // Black knight:
          } else {
          if (square_is_already_destination[(7 * 14 + 3) + 1] == false) {
            destination_arr[(i * 14 + 3) + j] = (7 * 14 + 3) + 1;
            square_is_already_destination[(7 * 14 + 3) + 1] = true;
          } else {
            destination_arr[(i * 14 + 3) + j] = (7 * 14 + 3) + 6;
            square_is_already_destination[(7 * 14 + 3) + 6] = true;
          }
          }
        } else if (p_board->pieces[i][j]->type == PAWN){
          // For pawn, we have a small for loop to check if the "leftmore" square is already occupied
          // White pawn:
          if (p_board->pieces[i][j]->color == 0) {
          for (int8_t k = 0; k < 8; k++) {
            if (square_is_already_destination[(1 * 14 + 3) + k] == false) {
            destination_arr[(i * 14 + 3) + j] = (1 * 14 + 3) + k;
            square_is_already_destination[(1 * 14 + 3) + k] = true;
            break;
            }
          }
          // Black pawn:
          } else {
          for (int8_t k = 0; k < 8; k++) {
            if (square_is_already_destination[(6 * 14 + 3) + k] == false) {
            destination_arr[(i * 14 + 3) + j] = (6 * 14 + 3) + k;
            square_is_already_destination[(6 * 14 + 3) + k] = true;
            break;
            }
          }
          }
        }
        // Check if the piece is already on its destination. If so, set destination_arr to -1
        if ((i * 14 + 3) + j == destination_arr[(i * 14 + 3) + j]) {
          destination_arr[(i * 14 + 3) + j] = -1;
        }
      }
    }
  }

  // ### Handling destination square chains and loops ###
  // We loop across the board, find a square that wants to move to another square. Loop thru the chain until it reaches a -1 or a loop
  // If -1, add chain to reset_moves in reverse order
  // If loop, add a move to temp, then add the chain to reset_moves in reverse order

  for (int8_t i = 0; i < 8; i++)
  {
    for (int8_t j = 0; j < 8; j++)
    {
      int8_t curr_idx = (i * 14 + 3) + j;
      // If we've come across an index that is -1, continue
      // This accounts for free squares as well as pieces moved in previous chains
      if (destination_arr[curr_idx] < 0)
        continue;

      // First reset the loop chain and the loop_detected flag
      current_chain.clear();
      loop_detected = 0;

      // Remember the first index in the chain
      int8_t starting_x = j;
      int8_t starting_y = i;

      // Handle the chain of indices
      while (1)
      {
        current_chain.push_back(curr_idx); // push_back adds an element to the end of the vector
        curr_idx = destination_arr[curr_idx]; // Go to the index that curr_idx wants to move to

        if (destination_arr[curr_idx] == -1)
        { // chain ends in a -1, means no loop.
          break; // Exit while loop
        } // else, chain continues and we go to the next index

        // Check if curr_idx wants to move to the first index in the chain
        if (curr_idx == (starting_y * 14 + 3) + starting_x)
        {
          loop_detected = 1; // Loop detected
          break;            // Exit while loop
        }
      }

      // If loop detected, move first square to temp. Add the chain in reverse order, then add temp to first square destination.
      if (loop_detected)
      {
        // The temp square is at: 8,0 or -1,8. Move it to closer square
        if (starting_x < starting_y){
          temp_x = -1;
          temp_y = 8;
        } else {
          temp_x = 8;
          temp_y = 0;
        }
        reset_moves.push_back(std::make_pair(current_chain[0], (temp_y * 14 + 3) + temp_x)); // Move the first square to temp
        // Remove first square from chain
        current_chain.erase(current_chain.begin());
      }

      // While the chain is not empty, add the moves in reverse order. And set the destination_arr to -1
      while (!current_chain.empty())
      {
        reset_moves.push_back(std::make_pair(current_chain.back(), destination_arr[current_chain.back()]));
        // Now, update the destination_arr to -1
        destination_arr[current_chain.back()] = -1;
        current_chain.pop_back();
      }

      // If loop detected, add the temp square to the first square destination
      if (loop_detected)
      {
        // cuz the one moved to temp location is always the starting square
        reset_moves.push_back(std::make_pair((temp_y * 14 + 3) + temp_x, destination_arr[starting_y * 14 + 3 + starting_x]));
        destination_arr[starting_y * 14 + 3 + starting_x] = -1;
      }
      // Flush the stack
      current_chain.clear();
    }
  }

  // ### Moving pieces in graveyard to their starting squares
  // For each graveyard types, as long as the graveyard[index] is not 0, move the piece to its starting square. Ignore index 10 and 11 as they are temp pieces
  for (int8_t i = 0; i < 10; i++)
  {
    while (graveyard[i] > 0)
    {
      // Update the graveyard memory
      graveyard[i]--; // we decrease first cuz we want the piece square, not the empty square. 
      // Find the first empty square in graveyard (the function takes 1 for queen, 2 for rook, 3 for bishop, 4 for knight, 5 for pawn)
      std::pair<int8_t, int8_t> graveyard_coordinate = get_graveyard_empty_coordinate((i%5)+1, i < 5);
      // Find the first available square on the board
      if (i==0) {
        // White queen
        reset_moves.push_back(std::make_pair((graveyard_coordinate.second * 14 + 3) + graveyard_coordinate.first, (0 * 14 + 3) + 3));
      } else if (i==1) {
        // White Rook
        if (!square_is_already_destination[(0 * 14 + 3) + 7]) {
          reset_moves.push_back(std::make_pair((graveyard_coordinate.second * 14 + 3) + graveyard_coordinate.first, (0 * 14 + 3) + 7));
          square_is_already_destination[(0 * 14 + 3) + 7] = true;
        } else {
          reset_moves.push_back(std::make_pair((graveyard_coordinate.second * 14 + 3) + graveyard_coordinate.first, (0 * 14 + 3) + 0));
          square_is_already_destination[(0 * 14 + 3) + 0] = true;
        }
      } else if (i==2) {
        // White Bishop
        if (!square_is_already_destination[(0 * 14 + 3) + 5]) {
          reset_moves.push_back(std::make_pair((graveyard_coordinate.second * 14 + 3) + graveyard_coordinate.first, (0 * 14 + 3) + 5));
          square_is_already_destination[(0 * 14 + 3) + 5] = true;
        } else {
          reset_moves.push_back(std::make_pair((graveyard_coordinate.second * 14 + 3) + graveyard_coordinate.first, (0 * 14 + 3) + 2));
          square_is_already_destination[(0 * 14 + 3) + 2] = true;
        }
      } else if (i==3) {
        // White Knight
        if (!square_is_already_destination[(0 * 14 + 3) + 6]) {
          reset_moves.push_back(std::make_pair((graveyard_coordinate.second * 14 + 3) + graveyard_coordinate.first, (0 * 14 + 3) + 6));
          square_is_already_destination[(0 * 14 + 3) + 6] = true;
        } else {
          reset_moves.push_back(std::make_pair((graveyard_coordinate.second * 14 + 3) + graveyard_coordinate.first, (0 * 14 + 3) + 1));
          square_is_already_destination[(0 * 14 + 3) + 1] = true;
        }
      } else if (i==4) {
        // White Pawn
        for (int8_t k = 7; k >= 0; k--) {
          if (!square_is_already_destination[(1 * 14 + 3) + k]) {
            reset_moves.push_back(std::make_pair((graveyard_coordinate.second * 14 + 3) + graveyard_coordinate.first, (1 * 14 + 3) + k));
            square_is_already_destination[(1 * 14 + 3) + k] = true;
            break;
          }
        }
      } else if (i==5) {
        // Black Queen
        reset_moves.push_back(std::make_pair((graveyard_coordinate.second * 14 + 3) + graveyard_coordinate.first, (7 * 14 + 3) + 3));
      } else if (i==6) {
        // Black Rook
        if (!square_is_already_destination[(7 * 14 + 3) + 0]) {
          reset_moves.push_back(std::make_pair((graveyard_coordinate.second * 14 + 3) + graveyard_coordinate.first, (7 * 14 + 3) + 0));
          square_is_already_destination[(7 * 14 + 3) + 0] = true;
        } else {
          reset_moves.push_back(std::make_pair((graveyard_coordinate.second * 14 + 3) + graveyard_coordinate.first, (7 * 14 + 3) + 7));
          square_is_already_destination[(7 * 14 + 3) + 7] = true;
        }
      } else if (i==7) {
        // Black Bishop
        if (!square_is_already_destination[(7 * 14 + 3) + 2]) {
          reset_moves.push_back(std::make_pair((graveyard_coordinate.second * 14 + 3) + graveyard_coordinate.first, (7 * 14 + 3) + 2));
          square_is_already_destination[(7 * 14 + 3) + 2] = true;
        } else {
          reset_moves.push_back(std::make_pair((graveyard_coordinate.second * 14 + 3) + graveyard_coordinate.first, (7 * 14 + 3) + 5));
          square_is_already_destination[(7 * 14 + 3) + 5] = true;
        }
      } else if (i==8) {
        // Black Knight
        if (!square_is_already_destination[(7 * 14 + 3) + 1]) {
          reset_moves.push_back(std::make_pair((graveyard_coordinate.second * 14 + 3) + graveyard_coordinate.first, (7 * 14 + 3) + 1));
          square_is_already_destination[(7 * 14 + 3) + 1] = true;
        } else {
          reset_moves.push_back(std::make_pair((graveyard_coordinate.second * 14 + 3) + graveyard_coordinate.first, (7 * 14 + 3) + 6));
          square_is_already_destination[(7 * 14 + 3) + 6] = true;
        }
      } else if (i==9) {
        // Black Pawn
        for (int8_t k = 0; k < 8; k++) {
          if (!square_is_already_destination[(6 * 14 + 3) + k]) {
            reset_moves.push_back(std::make_pair((graveyard_coordinate.second * 14 + 3) + graveyard_coordinate.first, (6 * 14 + 3) + k));
            square_is_already_destination[(6 * 14 + 3) + k] = true;
            break;
          }
        }
      }
    }
  }
  return reset_moves;
}

// ############################################################
// #                          STOCKFISH                       #
// ############################################################

// Pin definitions
const int clk = 0;    
const int WValid = 15;  
const int WReady = 2; 
const int WData = 4;   
const int RValid = 16; 
const int RReady = 17;  
const int RData = 5;   
const int OVERWRITE = 18;
const int clock_half_period = 10; // how long a clock half period is in ms

int stockfish_received_data = 0; // an int storing whatever stockfish sends us

int stockfish_read() {
  //
  // State variables
  int receiving = 0;
  int i = 0;

  // Begin with not ready
  digitalWrite(clk, LOW);
  digitalWrite(RReady, LOW);

  // Rising clock - do one cycle for fun
  delay(clock_half_period);
  digitalWrite(clk, HIGH);
  delay(clock_half_period);
  digitalWrite(clk, LOW);

  // Set ready
  digitalWrite(RReady, HIGH);  // Indicate ready to receive
  delay(clock_half_period);
  int receivedData[14];
  int current_data;

  // Loop:
  while (1) {
    // Read data and valid status before edge
    current_data = digitalRead(RData);
    if (!receiving) receiving = digitalRead(RValid);

    // Rising edge and falling edge
    digitalWrite(clk, HIGH);
    delay(clock_half_period);
    digitalWrite(clk, LOW);

    // load in data
    if (receiving) {
      digitalWrite(RReady, LOW);
      receivedData[i++] = current_data;
    }
    if (i >= 14) {
      break;
    }
  }

  Serial.println("Data read from Raspberry Pi.");
  int num_received = 0;
  for (int j = 0; j < 14; j++) {
    Serial.print(receivedData[j]);
    Serial.print(" ");
    num_received += receivedData[j] << j;
  }
  Serial.print("\t");
  Serial.println(num_received);
  num_received = 0;
  for (int j = 0; j < 14; j++) {
      num_received |= (receivedData[j] << (14-j-1));
  }
  digitalWrite(RReady, LOW);
  digitalWrite(WValid, LOW);
  return num_received; 
}

int pi_return_to_from_square(int value) {
  // return the from square value
  return (value >> 8) & 0b111111;;
}
int pi_return_to_to_square(int value) {
  // return the to square value
  return (value >> 2) & 0b111111;
}
int pi_return_to_promotion(int value) {
  // return the promotion value
  return value & 0b11;
}

int stockfish_write(bool writing_all_zeros, int is_programming, int programming_colour,
          bool is_human, int programming_difficulty, int from_square,
          int to_square, bool is_promotion,
          int promotion_piece) {  // could change argument to: "Is programming"
                                  // "programming_colour"
                                  // "programming_difficulty" "from square" "to
                                  // square" "if promotion" "promotion piece"
  // 010000000000000 // a promotion is happening and we are promoting to queen.
  // 000000000... // No promotion hapening
  // 0100000000000011 // a promotion is happening, and we promoting to a knight.
  uint8_t transmitting = 0;
  int i = 0;
  uint8_t temp_difficulty = programming_difficulty;
  uint8_t temp_from = from_square;
  uint8_t temp_to = to_square;
  int datas[16] = {0};  // Example data, all zeros
  if (!writing_all_zeros) {
    if (is_programming) {
      datas[0] = 1; 
      datas[1] = programming_colour;
      for (i = 2; i < 16; i++) {
        datas[i] = temp_difficulty & 1 | is_human;
        temp_difficulty = temp_difficulty >> 1;
      }
    } else {
      datas[0] = 0;
      datas[1] = is_promotion;
      for (i = 7; i >= 2; i--) {
        datas[i] = temp_from & 1;
        temp_from = temp_from >> 1;
      }
      for (i = 13; i >= 8; i--) {
        datas[i] = temp_to & 1;
        temp_to = temp_to >> 1;
      }
      datas[14] = promotion_piece & 1;
      datas[15] = promotion_piece >> 1;
    }
  }

  digitalWrite(clk, LOW);
  digitalWrite(WValid, LOW);
  digitalWrite(WData, datas[0]);

  delay(clock_half_period);
  digitalWrite(clk, HIGH);
  delay(clock_half_period);
  digitalWrite(clk, LOW);

  digitalWrite(WValid, HIGH);  // Indicate data is valid
  delay(clock_half_period);

  i = 0;

  while (true) {
    if (!transmitting) {
      transmitting = digitalRead(WReady);
    }  // Set valid back to zero
    // Serial.print("WREADY: ");
    // Serial.println(digitalRead(WReady));
    digitalWrite(clk, HIGH);
    delay(clock_half_period);
    digitalWrite(clk, LOW);
    if (transmitting) {
      // Serial.print("WRITING BIT: ");
      // Serial.print(i);
      // Serial.print(" ");
      // Serial.println(datas[i]);

      digitalWrite(WValid, LOW);
      i++;
      if (i >= 16) {
        break;
      }
      digitalWrite(WData, datas[i]);
    }
    delay(clock_half_period);
  }
  // Serial.println("Data sent to Raspberry Pi.");
  // for (int j = 0; j < 16; j++) {
  //   Serial.print(datas[j]);
  //   Serial.print(" ");
  // }
  digitalWrite(RReady, LOW);
  digitalWrite(WValid, LOW);
  // Serial.println("");
  // delay(5000);
  return 0;
}

// ############################################################
// #                       MOTOR CONTROL                      #
// ############################################################

// MOTOR CONTROL VARIABLES
const int8_t PUL_PIN[] = {27, 12}; // x, y
const int8_t DIR_PIN[] = {14, 13}; // x, y
const int8_t LIMIT_PIN[] = {19, 3, 1, 23}; // x-, x+, y-, y+
const int STEPS_PER_MM = 80; // measured value from testing, 3.95cm per rotation (1600 steps)
const int MM_PER_SQUARE = 66; // width of chessboard squares in mm
const int FAST_STEP_DELAY = 50; // half the period of square wave pulse for stepper motor
const int SLOW_STEP_DELAY = 100; // half the period of square wave pulse for stepper motor
const long ORIGIN_RESET_TIMEOUT = 50000; // how many steps motor will attempt to recenter to origin before giving up

int motor_error = 0; // 0: normal operation, 1: misaligned origin, 2: cannot reach origin (stuck)
int motor_coordinates[2] = {0, 0};  // x, y coordinates of the motor in millimeters

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
  long dx = x-motor_coordinates[0];
  long dy = y-motor_coordinates[1];

  // Setting direction
  digitalWrite(DIR_PIN[0], dx > 0);
  digitalWrite(DIR_PIN[1], dy > 0);

  dx = abs(dx);
  dy = abs(dy);

  // Check if movement should be axis-aligned or diagonal
  if (axisAligned || dx == 0 || dy == 0) {
    // Move x axis
    for (long i = 0; i < dx*STEPS_PER_MM; i++) {
      //if (digitalRead(LIMIT_PIN[0]) || digitalRead(LIMIT_PIN[1]) || digitalRead(LIMIT_PIN[2]) || digitalRead(LIMIT_PIN[3])) return 1;
      stepper_square_wave(0, stepDelay);
    }
    // Move y axis
    for (long i = 0; i < dy*STEPS_PER_MM; i++) {
      //if (digitalRead(LIMIT_PIN[0]) || digitalRead(LIMIT_PIN[1]) || digitalRead(LIMIT_PIN[2]) || digitalRead(LIMIT_PIN[3])) return 1;
      stepper_square_wave(1, stepDelay);
    }
  } else if (dx == dy) {
    // Move diagonal axis
    for (long i = 0; i < dx*STEPS_PER_MM; i++) {
      //if (digitalRead(LIMIT_PIN[0]) || digitalRead(LIMIT_PIN[1]) || digitalRead(LIMIT_PIN[2]) || digitalRead(LIMIT_PIN[3])) return 1;
      stepper_square_wave(2, stepDelay);
    }
  } else if (dx > dy){
    for (long i = 0; i < dx*STEPS_PER_MM; i++) {
      if (i < dy*STEPS_PER_MM) {
        //if (digitalRead(LIMIT_PIN[0]) || digitalRead(LIMIT_PIN[1]) || digitalRead(LIMIT_PIN[2]) || digitalRead(LIMIT_PIN[3])) return 1;
        stepper_square_wave(2, stepDelay);
      } else {
        //if (digitalRead(LIMIT_PIN[0]) || digitalRead(LIMIT_PIN[1]) || digitalRead(LIMIT_PIN[2]) || digitalRead(LIMIT_PIN[3])) return 1;
        stepper_square_wave(0, stepDelay);
      }
    }
  } else {
    for (long i = 0; i < dy*STEPS_PER_MM; i++) {
      if (i < dx*STEPS_PER_MM) {
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

  Serial.print("Moving to (");
  Serial.print(x);
  Serial.print(", ");
  Serial.print(y);
  Serial.println(") in mm");

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
    from_x = from_x * MM_PER_SQUARE - 10;
  } else if (from_x > 7) {
    from_x = from_x * MM_PER_SQUARE + 10;
  } else {
    from_x = from_x * MM_PER_SQUARE;
  }

  if (to_x < 0) {
    to_x = to_x * MM_PER_SQUARE - 10;
  } else if (to_x > 7) {
    to_x = to_x * MM_PER_SQUARE + 10;
  } else {
    to_x = to_x * MM_PER_SQUARE;
  }

  from_y = from_y * MM_PER_SQUARE;
  to_y = to_y * MM_PER_SQUARE;

  int ret;
  int corner_x, corner_y;

  // Move motor to starting location
  if (ret = move_motor_to_coordinate(from_x, from_y, false, FAST_STEP_DELAY)) return ret;

  delay(1000); // pick up piece
  Serial.println("Pick up piece");

  if (gridAligned) {
    // Move motor onto edges instead of centers, then move to destination
    corner_x = from_x < to_x ? from_x + (MM_PER_SQUARE)/2 : from_x - (MM_PER_SQUARE)/2;
    corner_y = from_y < to_y ? from_y + (MM_PER_SQUARE)/2 : from_y - (MM_PER_SQUARE)/2;
    if (ret = move_motor_to_coordinate(corner_x, corner_y, false, FAST_STEP_DELAY)) return ret;

    corner_x = from_x < to_x ? to_x - (MM_PER_SQUARE)/2 : to_x + (MM_PER_SQUARE)/2;
    corner_y = from_y < to_y ? to_y - (MM_PER_SQUARE)/2 : to_y + (MM_PER_SQUARE)/2;
    if (ret = move_motor_to_coordinate(corner_x, corner_y, true, FAST_STEP_DELAY)) return ret;
  }

  if (ret = move_motor_to_coordinate(to_x, to_y, false, FAST_STEP_DELAY)) return ret;

  delay(1000); // release piece
  Serial.println("Release piece");

  return 0;
}

int move_motor_to_origin(int offset) {
  // resets the motor to origin (0, 0) and re-calibrate the motor
  // fastMove: if true, it will move fast until the last 50mm until "supposed" origin and move slowly until it hits the limit switch
  //           if false, it will move slowly from the beginning until it hits the limit switch
  // The function should reference motor_coordinates variable to estimate where it is
  int dx = -motor_coordinates[0];
  int dy = -motor_coordinates[1];
  
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

// ############################################################
// #                     JOYSTICK CONTROL                     #
// ############################################################

// JOYSTICK CONTROL (each corresponds to the pin number) (first index is for white's joystick, second index is for black's joystick)
// Joystick is active LOW, so connect the other pin to GND
const int8_t JOYSTICK_POS_X_PIN[] = {25, 25};
const int8_t JOYSTICK_POS_Y_PIN[] = {35, 35};
const int8_t JOYSTICK_NEG_X_PIN[] = {32, 32};
const int8_t JOYSTICK_NEG_Y_PIN[] = {33, 33};
const int8_t JOYSTICK_BUTTON_PIN[] = {26, 26};
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

  // Also display the OLED if a joystick movement is detected

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

  // Also show the OLED screen for joystick promotion selection

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

  // Handle displaying the IDLE screen. Only run if a change happened (joystick moved or button pressed)

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
      in_idle_screen = false; 
      // Issue: Do note that this code will have issues if both buttons are tied together. Since this case, player_0's button will trigger an "exit idle screen", but player_1's button (which is the same thing), triggers button press...
      // This should be fine since in reality they are not the same button, and will have at least some sort of delay between them
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

// Decleration for number of rows, the number of leds in a single 1x8 row, 
// and he number of nuber of leds in a row of a single 4x4 chess square
// LED colours, led_display struct and strip length are defined.
const uint8_t COLUMNS = 32;
const uint8_t LEDSPERROW = 128;
const uint8_t LEDSPERSQUAREROW = 4;
const uint8_t CYAN = 0;
const uint8_t GREEN = 1;
const uint8_t YELLOW = 2;
const uint8_t W_WHITE = 3;
const uint8_t RED = 4;
const uint8_t PURPLE = 5;
const uint8_t SOLID = 0;
const uint8_t CURSOR = 1;
const uint8_t CAPTURE = 2;
const uint16_t STRIP_LEN = 256;
const uint8_t PROMOTION_STRIP_LEN = 4;
// The pin define doesn't do anything, change the values manually later
// #define LED_BOARD_PIN_0 = 12
// #define LED_BOARD_PIN_1 = 13
// #define LED_BOARD_PIN_2 = 14
// #define LED_BOARD_PIN_3 = 15
// #define LED_BOARD_PIN_4 = 16
const uint8_t LED_PROMOTION_PIN = 17; 
const int LED_BRIGHTNESS = 50;
// Array of CRGB objects corresponding to LED colors / brightness (0 indexed)
struct CRGB led_promotion[PROMOTION_STRIP_LEN];
struct CRGB led_display[5][STRIP_LEN]; 
// Due to hardware limitations, the 4th block of 256 leds (8 rows) has to be addressed separately as a 5 row and 3 row block
// Thus led_display[3] and led_display[4] are both for the 4th block of 256 LEDs


// If we wish to add cosmetic things, add another array of "previous states", or "pre-set patterns" or other stuff
// Also we may need a separate LED strip timer variable to keep track "how long ago was the last LED update"
// A function to update LED strip in each situation

// Takes in a chess square coordinate and a local LED coordinate. Returns index and data line of the selected LED
void coordinate_to_index(int x, int y, int u, int v, int &index, int &data_line){
  if (v % 2 == 0){
    index = u + (COLUMNS * v) + (LEDSPERSQUAREROW * x) + (LEDSPERROW * y);
  } else {
    index = -u + (COLUMNS * (v + 1)) - (LEDSPERSQUAREROW * x) + (LEDSPERROW * y) - 1;
  }

  if (index < 928) { // 928 is the first index of the 2nd type of LED
    data_line = index / 256;
    index -= data_line * 256;
  } else {
    data_line = 4;
    index -= 928;
  }
}

// Sets specific LED to a specific colour
void setLED(int x, int y, int u, int v, struct CRGB colour) {
  int index, data_line;

  coordinate_to_index(x, y, u, v, index, data_line);
  led_display[data_line][index] = colour;
}

// Assumes a fill_solid is called before this function
// This function will NOT reset LEDs of squares that are not being used
void set_LED_Pattern(int x, int y, int patternType, struct CRGB colour){
  int *index, *data_line;

  // Pattern Types
  // 0-Solid, 1-Cursor, 2-Capture

  if(patternType == SOLID){
    for(int i = 0; i < LEDSPERSQUAREROW; i++){
      for(int j = 0; j < LEDSPERSQUAREROW; j++){
        setLED(x, y, i, j, colour);
      }
    }
  }else if(patternType == CURSOR){
    for(int i = 1; i < LEDSPERSQUAREROW - 1; i++){
       for(int j = 1; j < LEDSPERSQUAREROW - 1; j++){
        setLED(x, y, i, j, colour);
      }
    }
  }else if(patternType == CAPTURE){
    for(int i = 0; i < LEDSPERSQUAREROW; i++){
      setLED(x, y, i, i, colour);
      setLED(x, y, i, LEDSPERSQUAREROW - 1 - i, colour);
    }
  }
}

// Assumes a fill_solid is called before this function
// Sets a 4x4 square of LEDs to a specific colour
void setSquareLED(int x, int y, int colourNumber, int patternType){
  struct CRGB colour;

  // Colour Coding
  // 0-Cyan, 1-Green, 2-Yellow, 3-Orange, 4-Red, 5-Purple

  for(int i = 0; i < 4; i++){

    for(int j = 0; j < 4; j++){

      if(colourNumber == CYAN){
        colour = CRGB(0, 255, 255);
      }else if(colourNumber == GREEN){
        colour = CRGB(0, 255, 15);
      }else if(colourNumber == YELLOW){
        colour = CRGB(255, 247, 18);
      }else if(colourNumber == W_WHITE){
        colour = CRGB(255, 153, 0);
      }else if(colourNumber == RED){
        colour = CRGB(255, 0, 0);
      }else if(colourNumber == PURPLE){
        colour = CRGB(209, 22, 219);
      }

      set_LED_Pattern(x, y, patternType, colour);
    }

  }
  
}

// Sets all the LEDs of the main board to off
// Does not clear the promotion LEDs
void clearLEDs() {
  int led_len;

  for (int i = 0; i < 5; i++) {
    if (i < 3) {
      led_len = STRIP_LEN;
    } else if (i == 3) {
      led_len = 5 * (STRIP_LEN / 8);
    } else if (i == 4) {
      led_len = 3 * (STRIP_LEN / 8);
    } 

    fill_solid(led_display[i], led_len, CRGB(0, 0, 0));
  }
}

// Idle animation for LEDs
void idleAnimationLEDs() {
  clearLEDs(); // for now just clears the LEDs
}

// ############################################################
// #                       OLED DISPLAY                       #
// ############################################################
// Keep track of when IDLE is supposed to switch
uint32_t last_interacted_time, last_idle_change;

// So we don't display idle or waiting screen multiple times...
bool idle_one, idle_two;

void free_displays() {
  if (!USING_OLED) {
    return; // Don't do anything about displays if not using OLED
  }
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
  if (!USING_OLED) {
    display_one = nullptr;
    display_two = nullptr;
    return; // Don't initialize displays if not using OLED
  }

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
  // This code is run by IDLE joystick (if a change is detected such as a button press or joystick movement)
  // The IDLE animation is run first time the game enters IDLE state. (Currently this part of code is removed due to testing purposes)
  // This code is also run when the game FIRST starts up. (since joystick won't be pressed...)

  /* display_idle_screen arguments
  Timer timer, passed in so we can poll and see the current time and hence how much time has passed

  is_idle, true if we are currently displaying an idle screen

  screen_select to know which screen we are on; 1 for choosing player opponent, 0 for changing computer opponent difficulty

  comp_diff to know the current difficulty to be displayed when setting up computer opponents (0 to 19)

  display to display to a specific display
  */

  if (!USING_OLED) {
    return; // Don't do anything about displays if not using OLED
  }

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
  if (!USING_OLED) {
    return; // Don't do anything about displays if not using OLED
  }

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
  if (!USING_OLED) {
    return; // Don't do anything about displays if not using OLED
  }
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
  if (!USING_OLED) {
    return; // Don't do anything about displays if not using OLED
  }
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
  if (!USING_OLED) {
    return; // Don't do anything about displays if not using OLED
  }
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
  if (!USING_OLED) {
    return; // Don't do anything about displays if not using OLED
  }
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
  if (!USING_OLED) {
    return; // Don't do anything about displays if not using OLED
  }
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
  if (!USING_OLED) {
    return; // Don't do anything about displays if not using OLED
  }
  display->clearDisplay();
  display->setTextSize(2);
  display->setCursor(0, 0);
  display->println(F(" > Spark <"));
  display->print(F("SMARTCHESS"));
  display->display();
  // display->startscrollleft(0, 1); // (row 1, row 2). Scrolls just the first row of text.
  display->startscrollright(2, 3); // SSD1306 can't handle two concurrent scroll directions.

  // Serial.print("Free memory: ");
  // Serial.println(freeMemory());
}

void display_draw(int8_t draw, Adafruit_SSD1306 *display) {
  if (!USING_OLED) {
    return; // Don't do anything about displays if not using OLED
  }

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
  if (!USING_OLED) {
    return; // Don't do anything about displays if not using OLED
  }

  display->clearDisplay();
  display->setTextSize(1);
  display->setCursor(0, 0);

  if (winner == 0) display->print("White");
  else display->print("Black");
  display->println(F(" wins!"));

  display->display();

}

void display_loser(int8_t loser, Adafruit_SSD1306 *display) {
  if (!USING_OLED) {
    return; // Don't do anything about displays if not using OLED
  }

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

// ############################################################
// #                           MAIN                           #
// ############################################################

void setup() {
  Serial.begin(9600);
  // delay(1000);  // Wait for serial monitor to open
  Serial.println("Starting up...");
  // Serial.println("Free memory: ");
  // Serial.println(freeMemory());
  // return;
  // Serial.println(freeMemory());

  // LED pin initialize:
  // LEDS.addLeds<WS2812B, 12, GRB>(led_display[0], STRIP_LEN);
  // LEDS.addLeds<WS2812B, 13, GRB>(led_display[1], STRIP_LEN);
  // LEDS.addLeds<WS2812B, 14, GRB>(led_display[2], STRIP_LEN);
  // LEDS.addLeds<WS2812B, 15, GRB>(led_display[3], 5 * (STRIP_LEN / 8));
  // LEDS.addLeds<WS2812B, 16, GRB>(led_display[4], 3 * (STRIP_LEN / 8));
  // LEDS.addLeds<WS2812B, LED_PROMOTION_PIN, GRB>(led_display[5], PROMOTION_STRIP_LEN);
  // FastLED.setBrightness(LED_BRIGHTNESS);

  // Initial game state
  game_state = GAME_POWER_ON;
}

void loop() {
  // Serial.println("Starting up...");
  // return;
  Serial.print("Current State: ");
  Serial.println(game_state);
  // Serial.print("Free memory: ");
  // Serial.println(freeMemory());

  // FastLED.show();  // Display board via LEDs

  // delay(500);  // Delay for 50ms - just a standard delay (although not necessary)

  // State machine, during each loop, we are in a certain state, each state handles the transition to the next state
  // Chained if block below does not have an else case
  if (game_state == GAME_POWER_ON) {
    // Power on the board, motors calibrate, initialize pins, set LED to blank / off
    Serial.print("Game Power ON");

    game_timer.start();  // Start the game timer
    Serial.print("Game Power ON2");
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
    Serial.print("Game Power ON3");
    // MOTOR pins
    pinMode(PUL_PIN[0], OUTPUT);
    pinMode(DIR_PIN[0], OUTPUT);
    pinMode(PUL_PIN[1], OUTPUT);
    pinMode(DIR_PIN[1], OUTPUT);
    Serial.print("Game Power ON4");
    // Set LED to blank initially
    clearLEDs();
    Serial.print("Game Power ON5");

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
    Serial.print("Game Power ON6");

    // Initialize the OLED display
    display_init();
    display_idle_screen(game_timer, in_idle_screen, idle_joystick_x[1], idle_joystick_y[1], display_two, 1);
    display_idle_screen(game_timer, in_idle_screen, idle_joystick_x[0], idle_joystick_y[0], display_one, 0);

    Serial.print("Game Power ON7");
    // STOCKFISHTODO: Read from stockfish data pins until we get 10101010101010.... or 01010101010101... Then set OVERWRITE pin to true.
    pinMode(clk, OUTPUT);
    pinMode(RValid, INPUT);
    pinMode(RReady, OUTPUT);
    pinMode(RData, INPUT);
    pinMode(WValid, OUTPUT);
    pinMode(WReady, INPUT);
    pinMode(WData, OUTPUT);
    pinMode(OVERWRITE, OUTPUT);

    Serial.print("Game Power ON8");

    // Set initial state of pins
    digitalWrite(clk, LOW);
    digitalWrite(RReady, LOW);
    digitalWrite(WValid, LOW);
    digitalWrite(WData, LOW);
    digitalWrite(OVERWRITE, LOW);
    Serial.print("Game Power ON9");
    bool initialized = false;

    // Wait for the stockfish to initialize
    while (!initialized && USING_STOCKFISH) { // skip if not using stockfish
      stockfish_received_data = stockfish_read();
      if (stockfish_received_data == 0b10101010101010 ||
          stockfish_received_data == 0b01010101010101) {
        initialized = true;
        digitalWrite(OVERWRITE, HIGH);
      }
    }
    Serial.println("Stockfish initialized");

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
    idleAnimationLEDs();

    // OLED display: show the current selection
    // TODO:
    // display_idle(timer=game_timer, screen=0, player_ready[0], idle_joystick_x[0], idle_joystick_y[0])
    // display_idle(timer=game_timer, screen=1, player_ready[1], idle_joystick_x[1], idle_joystick_y[1])

    // If both players are ready, start the game
    if (player_ready[0] && player_ready[1]) {
      game_state = GAME_INITIALIZE;
      is_first_move = true;  // Set to true so we can send the first move to stockfish

      // STOCKFISHTODO: Send player types and game difficulty of BOTH players to Stockfish
      // STOCKFISHTODO: If white is a computer, also write all zeros to indicate a beginning move
      if (USING_STOCKFISH){
        // Remember if players are human or computer
        // 0 = human, 1 = computer
        player_is_computer[0] = idle_joystick_x[0];
        player_is_computer[1] = idle_joystick_x[1];
        stockfish_write(0,   // writing_all_zeros
          1,   // is programming
          0,   // programming colour
          !idle_joystick_x[0],   // is human
          idle_joystick_y[0],  // difficulty
          0,   // from square (doesn't matter)
          0,   // to square (doesn't matter)
          0,   // is promotion (doesn't matter)
          0);  // promotion square (doesn't matter)

        stockfish_write(0,   // writing_all_zeros
          1,   // is programming
          1,   // programming colour
          !idle_joystick_x[1],   // is human
          idle_joystick_y[1],  // difficulty
          0,   // from square (doesn't matter)
          0,   // to square (doesn't matter)
          0,   // is promotion (doesn't matter)
          0);  // promotion square (doesn't matter)
      } else {
        // Set both to players if not using stockfish
        player_is_computer[0] = 0;
        player_is_computer[1] = 0;
      }

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
    // Print free memory
    // Serial.print("Free memory: ");
    // Serial.println(freeMemory());
    // Print the board state
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
    Serial.println("No checkmate or stalemate");

    // WE HAVE FINISHED ALL "GAME_OVER" CHECKS, tell stockfish what move just happened (this happened before the move variables are reset)
    // STOCKFISHTODO: Send the move that was made (selected_x, selected_y, destination_x, destination_y) to stockfish
    // STOCKFISHTODO: Also include promotion if there was any (see if promotion_type is -1 or not)
    if (is_first_move){
      // In the case of first move, only send something to stockfish if white is a computer
      is_first_move = false;
      if (player_is_computer[0]) {
        // If white is a computer, send all zeros to stockfish for the first move
        stockfish_write(1,   // writing_all_zeros
          0,   // is programming
          0,   // programming colour
          0,   // is human
          0,  // difficulty
          0,   // from square
          0,   // to square
          0,   // is promotion (doesn't matter)
          0);  // promotion square (doesn't matter)
      }
    } else if (USING_STOCKFISH) {
      // If not first move, send the move to stockfish
      stockfish_write(0,                 // writing_all_zeros
        0,                 // is programming
        0,                 // programming colour
        0,                 // is human
        20,                // difficulty
        selected_y * 8 + selected_x ,       // from square
        destination_y * 8 + destination_x,         // to square
        promotion_happened,                 // is promotion
        promotion_joystick_selection);  // promotion square
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
    promotion_type = -1; // This is mostly for stockfish, so we know if there is a promotion or not when sending info to stockfish
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
    // pieces... (yellow) Also display the cursor location - which is
    // joystick_x[player_turn], joystick_y[player_turn] (green - overwrites
    // orange) Also display sources of check, if any (red) (will be overwritten
    // by green if the cursor is on the same square) red on king square if under
    // check
    clearLEDs();
    
    if(number_of_turns != 0){
      setSquareLED(previous_selected_x, previous_selected_y, YELLOW, SOLID);
      setSquareLED(previous_destination_x, previous_destination_y, YELLOW, SOLID);
    }

    // STOCKFISHTODO: Let LED display first.
    // STOCKFISHTODO: If player is a computer: 
    // STOCKFISHTODO: Read from stockfish. Decode the coordinates. Set selected_x, selected_y, destination_x, destination_y, capture_x, capture_y accordingly. 
    // STOCKFISHTODO: Capture x,y have to be found within the list of possible moves...
    // STOCKFISHTODO: If any error occurs here, just make the FIRST move in the list of possible moves.
    // STOCKFISHTODO: Stockfish may also return promotion piece. If so, set promotion_joystick_selection to the correct value.
    // STOCKFISHTODO: Then we directly move to GAME_MOVE_MOTOR
    
    if (player_is_computer[player_turn]) {
      // Receive from stockfish, convert to x,y coordinates
      // Assuming stockfish doesn't return any errors. // TODO: if there happens to be error here we might need to check...
      stockfish_received_data = stockfish_read();
      int8_t stockfish_from_x = pi_return_to_from_square(stockfish_received_data) % 8;
      int8_t stockfish_from_y = pi_return_to_from_square(stockfish_received_data) / 8;
      int8_t stockfish_to_x = pi_return_to_to_square(stockfish_received_data) % 8;
      int8_t stockfish_to_y = pi_return_to_to_square(stockfish_received_data) / 8;
      int8_t stockfish_promotion = pi_return_to_promotion(stockfish_received_data);
      selected_x = stockfish_from_x;
      selected_y = stockfish_from_y;
      destination_x = stockfish_to_x;
      destination_y = stockfish_to_y;
      promotion_joystick_selection = stockfish_promotion;
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
        // Invalid move, just take the first move.
        for(int i = 0; i < 8; i++){
          bool found = false;
          for(int j = 0; j < 8; j++){
            if(all_moves[i][j].size() > 0){
              selected_x = j;
              selected_y = i;
              destination_x = all_moves[i][j][0].first % 8;
              destination_y = all_moves[i][j][0].first / 8;
              capture_x = all_moves[i][j][0].second % 8;
              capture_y = all_moves[i][j][0].second / 8;
              found = true;
              break;
            }
          }
          if(found){
            break;
          }
        }
      }
      
      // Computer move, straight to motor
      game_state = GAME_MOVE_MOTOR;
    }


    setSquareLED(joystick_x[player_turn], joystick_y[player_turn], CYAN, CURSOR);
    
    if (p_board->under_check(player_turn % 2)) {
      setSquareLED((player_turn % 2) ? p_board->black_king_x : p_board->white_king_x, (player_turn % 2) ? p_board->black_king_y : p_board->white_king_y, RED, SOLID);
    }

    // OLED display: show the current selection
    // TODO
    // display_turn_select()...
    
  


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
    Serial.println("Selected piece: ");
    Serial.print(selected_x);
    Serial.print(", ");
    Serial.println(selected_y);

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
    Serial.print("done this phase");
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
    clearLEDs();
    
    if(number_of_turns != 0){
      setSquareLED(previous_selected_x, previous_selected_y, YELLOW, SOLID);
      setSquareLED(previous_destination_x, previous_destination_y, YELLOW, SOLID);
    }
    
    for(int i = 0; i < all_moves[selected_y][selected_x].size(); i++){
      int x_move = all_moves[selected_y][selected_x][i].first % 8;
      int isCapture = all_moves[selected_y][selected_x][i].second;
      int y_move = all_moves[selected_y][selected_x][i].first / 8;

      if(isCapture != -1){
        setSquareLED(x_move, y_move, RED, CAPTURE);
      } else {
        setSquareLED(x_move, y_move, W_WHITE, SOLID);
      }

      if (p_board->under_check(player_turn % 2)) {
        setSquareLED((player_turn % 2) ? p_board->black_king_x : p_board->white_king_x, (player_turn % 2) ? p_board->black_king_y : p_board->white_king_y, RED, SOLID);
      }
    }

    setSquareLED(selected_x, selected_y, GREEN, SOLID);
    setSquareLED(joystick_x[player_turn], joystick_y[player_turn], CYAN, CURSOR);



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

    clearLEDs();

    setSquareLED(selected_x, selected_y, GREEN, SOLID);
    setSquareLED(destination_x, destination_y, RED, SOLID);



    // OLED display: show the current selection
    // TODO
    // display_turn_select()...
    // display_init();
    display_while_motor_moving(player_turn, selected_x, selected_y, destination_x, destination_y, display_one, display_two);
    // free_displays();
    Serial.println("Moving piece by motor");
    // Serial.println(freeMemory());

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
                   graveyard_coordinate.second, true);
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
            move_piece_by_motor(pawn_x, pawn_y, graveyard_coordinate.first, graveyard_coordinate.second, true);

            // Move captured piece to the pawn's location (use safe move)
            move_piece_by_motor(capture_x, capture_y, pawn_x, pawn_y, true);

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
          move_piece_by_motor(capture_x, capture_y, graveyard_coordinate.first, graveyard_coordinate.second, true);

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
    move_piece_by_motor(selected_x, selected_y, destination_x, destination_y, p_board->pieces[selected_y][selected_x]->get_type() == KNIGHT);

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
      move_piece_by_motor(rook_x, rook_y, (selected_x + destination_x) / 2, selected_y, true);
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
      promotion_happened = true;
      game_state = GAME_WAIT_FOR_SELECT_PAWN_PROMOTION;
      // STOCKFISHTODO: If this player is a computer, then the promotion_joystick_selection should already be set by stockfish
      // STOCKFISHTODO: Then we do not modify promotion_joystick_selection here (since it's already set in get_move part)
      if (!player_is_computer[player_turn]) promotion_joystick_selection = 0; // default to queen
      // display_init();
      display_promotion(player_turn, 0, display_one, display_two);
      // free_displays();
    } else {
      promotion_happened = false;
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

    for (int i = 0; i < PROMOTION_STRIP_LEN; i++) {
      if (i == promotion_joystick_selection) {
        led_promotion[i] = CRGB(255, 255, 255);
      } else {
        led_promotion[i] = CRGB(0, 0, 0);
      }
    }

    // OLED display: show the current selection
    // TODO
    // display_promotion()...

    // Get button input, update joystick location (Special for pawn promotion,
    // use promotion_joystick_selection) (there's only 4 possible values, 0, 1, 2, 3)
    // display_init();
    if (!player_is_computer[player_turn]) {
      // Only check for joystick if the player is not a computer
      move_user_joystick_promotion(player_turn);

      // Don't move until the confirm button is pressed
      // STOCKFISHTODO: If this player is a computer, then just proceed... Do not wait for confirm button
      if (confirm_button_pressed[player_turn]) {
        confirm_button_pressed[player_turn] = false;
      } else {
        return;
      }
    }
    // free_displays();

    // 0, 1, 2, 3 are the possible promotion pieces that promotion_joystick_selection can take, updated by move_user_joystick_promotion
    promotion_type = promotion_joystick_selection;

    // Check for valid input
    if (promotion_type < 0 || promotion_type > 3) {
      // Invalid input
      // STOCKFISHTODO: If stockfish gave invalid promotion here, then we should just default to queen
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
               graveyard_coordinate.second, true);
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
                 destination_x, destination_y, true);
    } else {
      // There isn't a valid piece in the graveyard, use a temp piece

      // Update the graveyard memory so the get_graveyard_empty_coordinate returns the coordinate of the first available temp piece
      graveyard[10 + p_board->pieces[destination_y][destination_x]->get_color()]--;

      // Move a temp piece to the destination (6 == temp piece)
      graveyard_coordinate = get_graveyard_empty_coordinate(
          6, p_board->pieces[destination_y][destination_x]->get_color());
      move_piece_by_motor(graveyard_coordinate.first, graveyard_coordinate.second,
                 destination_x, destination_y, true);

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

    move_motor_to_origin(10); // input is offset value, eyeball it during testing

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

    std::vector<std::pair<int8_t, int8_t>> reset_moves = reset_board(p_board);

    // The reset_moves vector contains the (from, to) coordinate pairs. Each int8_t is a coordinate.
    // Each coordinate is i*14+j. With the coordinate (0,0) being coordinate 3...
    // 3 extra cols are added on left, 3 extra cols are added on right. 
    // Regular chess board (x,y) coordinate is now converted to (y*14+3+x), so for x<0 it means graveyard on left, x>7 means graveyard on right

    for (int reset_idx = 0; reset_idx < reset_moves.size(); reset_idx++) {
      Serial.print("move ");
      Serial.print(reset_idx);
      Serial.print(": [");
      Serial.print(reset_moves[reset_idx].first % 14);
      Serial.print("][");
      Serial.print(reset_moves[reset_idx].first / 14);
      Serial.print("] to [");
      Serial.print(reset_moves[reset_idx].second % 14);
      Serial.print("][");
      Serial.print(reset_moves[reset_idx].second / 14);
      Serial.println("]");
    } // Convert from idx to coords by row = index / 14, col = index % 14 (8 from board + 3 + 3 from graveyards = 14)


      // Free unnecessary memory
      // TODO
      // 3-fold repetition vectors is no longer needed

      // First, move the pieces back to the initial position
      // TODO: motor

      // Free memory
      delete p_board;

    // Clear vectors (find ones that aren't cleared by game_initialize)
    // TODO

    // Check the remaining variables from game_poweron or game_idle TODO

    // Reset game state
    game_state = GAME_POWER_ON;
  }
}
