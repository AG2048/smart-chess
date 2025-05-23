#include "Piece.h"
#include "Board.h"
#include <stdint.h>
#include <vector>
#include <utility>
#include <string.h>
#include <Arduino.h>

// The Piece Class

// Define the static constexpr arrays
constexpr int8_t Piece::knight_dx[];
constexpr int8_t Piece::knight_dy[];

PieceType Piece::get_type() {
  return type;
}

bool Piece::get_color() {
  return color;
}

int8_t Piece::get_x() {
  return x;
}

int8_t Piece::get_y() {
  return y;
}

bool Piece::get_double_move() {
  return double_move;
}
// Function that returns x,y coordinates of all possible moves
// This move function does not check for any potential checks that might occur by this move.
// The checks should be done by the board class - the board checks the board state after possible moves and evaluate if the move is legal
std::vector<std::pair<int8_t, int8_t>> Piece::get_possible_moves(Board* board) const {
  // TODO: WE COULD RETURN A vector<pair<pair,pair>> The first pair is destination, second is the piece that is taken
  // TODO: can change this to a "board point8_ter" if we don't like passing the board as an argument
  std::vector<std::pair<int8_t, int8_t>> moves;
  // switch piece type
  if (type==EMPTY) {
  } else if (type == KING) {
    for(int8_t i = y-1; i <= y+1; i++){
      for(int8_t j = x-1; j <= x+1; j++){
        if (i == y && j == x) continue;
        // If square outside board, continue
        if (i < 0 || i > 7 || j < 0 || j > 7) continue;

        // If empty piece, or opponent piece, you can move there
        if (board->pieces[i][j]->get_type() == EMPTY) {
          moves.push_back(std::make_pair(i*8+j, -1)); // make a pair of destination, while capture square is (-1, -1) (no capture)
        } else if (board->pieces[i][j]->get_color() != color) {
          moves.push_back(std::make_pair(i*8+j, i*8+j)); // make a pair of destination and capture square, which is the same
        } 
      }
    }
    // Castling
    if (color == 0) {
      if (board->white_king_castle) {
        // Check if squares between king and rook are empty
        if (board->pieces[0][5]->get_type() == EMPTY && board->pieces[0][6]->get_type() == EMPTY) {
          moves.push_back(std::make_pair(6, -1));
        }
      }
      if (board->white_queen_castle) {
        // Check if squares between king and rook are empty
        if (board->pieces[0][1]->get_type() == EMPTY && board->pieces[0][2]->get_type() == EMPTY && board->pieces[0][3]->get_type() == EMPTY) {
          moves.push_back(std::make_pair(2, -1));
        }
      }
    } else {
      if (board->black_king_castle) {
        // Check if squares between king and rook are empty
        if (board->pieces[7][5]->get_type() == EMPTY && board->pieces[7][6]->get_type() == EMPTY) {
          moves.push_back(std::make_pair(62, -1));
        }
      }
      if (board->black_queen_castle) {
        // Check if squares between king and rook are empty
        if (board->pieces[7][1]->get_type() == EMPTY && board->pieces[7][2]->get_type() == EMPTY && board->pieces[7][3]->get_type() == EMPTY) {
          moves.push_back(std::make_pair(58, -1));
        }
      }
    }
  } else if (type == QUEEN) {
    // when encounter "edge of board", opponent piece, or own piece, stop
    // Add opponent piece square to moves
    for (int8_t dx = -1; dx <= 1; dx++) {
      for (int8_t dy = -1; dy <= 1; dy++) {
        if (dy == 0 && dx == 0) continue;
        int8_t x_loop = x+dx;
        int8_t y_loop = y+dy;
        while (1) {
          if (x_loop < 0 || y_loop < 0 || x_loop > 7 || y_loop > 7) break;  // if any coordinate is OOB, stop
          if (board->pieces[y_loop][x_loop]->get_type() == EMPTY) {
            // Empty square, no capture
            moves.push_back(std::make_pair(y_loop * 8 + x_loop, -1));
          } else if (board->pieces[y_loop][x_loop]->get_color() != color) {
            // Enemy piece encountered; we can capture it, but don't look past it
            moves.push_back(std::make_pair(y_loop * 8 + x_loop, y_loop * 8 + x_loop));
            break;
          } else {  // != EMPTY, == color
            // Found a friendly piece; don't look past it
            break;
          }
          // Look at the next coordinate in the current direction
          x_loop += dx;
          y_loop += dy;
        }
      }
    }
  } else if (type == BISHOP) {
    // Same as queen, but only 4 directions
    for (int8_t dx = -1; dx <= 1; dx++) {
      for (int8_t dy = -1; dy <= 1; dy++) {
        if (dy == 0 && dx == 0) continue;
        if (dy == 0 || dx == 0) continue; // only looking at diagonals
        int8_t x_loop = x+dx;
        int8_t y_loop = y+dy;
        while (1) {
          if (x_loop < 0 || y_loop < 0 || x_loop > 7 || y_loop > 7) break;  // if any coordinate is OOB, stop
          if (board->pieces[y_loop][x_loop]->get_type() == EMPTY) {
            moves.push_back(std::make_pair(y_loop * 8 + x_loop, -1));
          } else if (board->pieces[y_loop][x_loop]->get_color() != color) {
            // Enemy piece encountered; we can capture it, but don't look past it
            moves.push_back(std::make_pair(y_loop * 8 + x_loop, y_loop * 8 + x_loop));
            break;
          } else {  // != EMPTY, == color
            // Found a friendly piece; don't look past it
            break;
          }
          // Look at the next coordinate in the current direction
          x_loop += dx;
          y_loop += dy;
        }
      }
    }
  } else if (type == KNIGHT) {
    // 8 possible moves - don't add if edge of board / own piece
    
    // Loop through all possible knight moves
    for (int8_t knight_move_index = 0; knight_move_index < 8; knight_move_index++) {
        int8_t newX = x + knight_dx[knight_move_index];
        int8_t newY = y + knight_dy[knight_move_index];
        
        // Check if the move is within bounds of the board
        if (newX >= 0 && newX < 8 && newY >= 0 && newY < 8) {
            // If the square is empty or contains an opponent's piece
            if (board->pieces[newY][newX]->get_type() == EMPTY) {
                moves.push_back(std::make_pair(newY * 8 + newX, -1));
            } else if (board->pieces[newY][newX]->get_color() != color) {
                moves.push_back(std::make_pair(newY * 8 + newX, newY * 8 + newX));
            }
        }
    }
  } else if (type == ROOK) {
    // 4 possible directions - don't add if edge of board / own piece
    for (int8_t dx = -1; dx <= 1; dx++) {
      for (int8_t dy = -1; dy <= 1; dy++) {
        if (dy == 0 && dx == 0) continue;
        if (dy != 0 && dx != 0) continue; //if dy and dx are both nonzero, it's a diagonal
        // loop in a direction to see valid moves
        int8_t x_loop = x+dx;
        int8_t y_loop = y+dy;
        while (1) {
          if (x_loop < 0 || y_loop < 0 || x_loop > 7 || y_loop > 7) break;  // if any coordinate is OOB, stop
          if (board->pieces[y_loop][x_loop]->get_type() == EMPTY) {
            moves.push_back(std::make_pair(y_loop * 8 + x_loop, -1));
          } else if (board->pieces[y_loop][x_loop]->get_color() != color) {
            // Enemy piece encountered; we can capture it, but don't look past it
            moves.push_back(std::make_pair(y_loop * 8 + x_loop, y_loop * 8 + x_loop));
            break;
          } else {  // != EMPTY, == color
            // Found a friendly piece; don't look past it
            break;
          }
          // Look at the next coordinate in the current direction
          x_loop += dx;
          y_loop += dy;
        }
      }
    }
  } else if (type == PAWN) {
    // TODO: Implement pawn moves

    if (color == 0) { // White pawn. diagonally upwards
      if (y+1 < 8 && x+1 < 8 && board->pieces[y+1][x+1]->get_type() != EMPTY && board->pieces[y+1][x+1]->get_color() != color) { // If the piece diagonally to the right is not empty
        // When pushing a possible move, first pair is where the capturing
        // piece goes, second pair is square of piece we captured
        // In this case, it's the same
        moves.push_back(std::make_pair((y+1)*8 + x+1, (y+1)*8 + x+1));
      }
      if (y+1 < 8 && x-1 >= 0 && board->pieces[y+1][x-1]->get_type() != EMPTY && board->pieces[y+1][x-1]->get_color() != color) { // If the piece diagonally to the right is opposite color
        moves.push_back(std::make_pair((y+1)*8 + x-1, (y+1)*8 + x-1));
      }
        // Check if square immediately in front is empty (single move forward)
      if (y+1 < 8 && board->pieces[y+1][x]->get_type() == EMPTY) {
          moves.push_back(std::make_pair((y+1)*8 + x, -1));
          
          // Check if pawn can move 2 squares (first move only) and second square is also empty
          if (double_move && y+2 < 8 && board->pieces[y+2][x]->get_type() == EMPTY) { // Initial position for white pawn
              moves.push_back(std::make_pair((y+2)*8 + x, -1));
          }
      }

      // Capture moves (diagonally to the left and right)
      if (x + 1 < 8 && y + 1 < 8 && board->pieces[y][x+1]->get_color() != color && board->en_passant_square_x == x+1 && board->en_passant_square_y == y) { // En passant to the right
          moves.push_back(std::make_pair((y+1)*8 + x+1, (y)*8 + x+1));
      }
      if (x - 1 >= 0 && y + 1 < 8 && board->pieces[y][x-1]->get_color() != color && board->en_passant_square_x == x-1 && board->en_passant_square_y == y) { // En passant to the left
          moves.push_back(std::make_pair((y+1)*8 + x-1, (y)*8 + x-1));
      }
    } else { // Black pawn. diagonally downwards
      if (y-1 >= 0 && x+1 < 8 && board->pieces[y-1][x+1]->get_type() != EMPTY && board->pieces[y-1][x+1]->get_color() != color) { // If the piece diagonally to the right is not empty
        moves.push_back(std::make_pair((y-1)*8 + x+1, (y-1)*8 + x+1));
      }
      if (y-1 >= 0 && x-1 >= 0 && board->pieces[y-1][x-1]->get_type() != EMPTY && board->pieces[y-1][x-1]->get_color() != color) { // If the piece diagonally to the right is opposite color
        moves.push_back(std::make_pair((y-1)*8 + x-1, (y-1)*8 + x-1));
      }
      // Check if square immediately in front is empty (single move forward)
      if (y-1 >= 0 && board->pieces[y-1][x]->get_type() == EMPTY) {
          moves.push_back(std::make_pair((y-1)*8 + x, -1));

          // Check if pawn can move 2 squares (first move only) and second square is also empty
          if (double_move && board->pieces[y-2][x]->get_type() == EMPTY) { // Initial position for black pawn
              moves.push_back(std::make_pair((y-2)*8 + x, -1));
          }
      }

      // Capture moves (diagonally to the left and right)
      if (x + 1 < 8 && y - 1 >= 0 && board->pieces[y][x+1]->get_color() != color && board->en_passant_square_x == x+1 && board->en_passant_square_y == y) { // En passant to the right
          moves.push_back(std::make_pair((y-1)*8 + x+1, (y)*8 + x+1));
      }
      if (x - 1 >= 0 && y - 1 >= 0 && board->pieces[y][x-1]->get_color() != color && board->en_passant_square_x == x-1 && board->en_passant_square_y == y) { // En passant to the left
          moves.push_back(std::make_pair((y-1)*8 + x-1, (y)*8 + x-1));
      }
    }
  }
  return moves;
}

// Constructor
Piece::Piece(PieceType new_type, bool new_color, int8_t new_x, int8_t new_y) {
  type = new_type;
  color = new_color;
  x = new_x;
  y = new_y;
  double_move = true;
}