// Include
#include <ArduinoSTL.h>
#include "Board.h"
#include "Piece.h"
/*
What did we assume board has?

8x8 array that is called "pieces"
pieces[i][j] is a piece object

board.black_king_castle
board.black_queen_castle
board.white_king_castle
board.white_queen_castle

board.en_passant_square_x
board.en_passant_square_y

board.move_counter
*/

// Enum for piece types
enum PieceType {
  EMPTY,
  KING,
  QUEEN,
  BISHOP,
  KNIGHT,
  ROOK,
  PAWN
};



Board* p_board;
std::vector<std::pair<std::pair<int, int>, std::pair<int, int>>> all_moves[8][8];
void setup() {
  // put your setup code here, to run once:
  // Initialize board class
  p_board = new Board;
  
}

void loop() {
  // put your main code here, to run repeatedly:

  // Generate possible moves for all pieces in the board class
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      all_moves[row][col] = p_board->get_possible_moves(p_board);
    }
  }

  // Remove illegal moves

  // Detect checkmate / draw

    // If detected, enter game over

  // Coordinate with LEDs --> indicate possible moves, selected piece, 

  // Coordinate with motors --> tell them what pieces to move

  // Pieces have been moved, update the Board and Piece classes

  // Check pawn promotion

    // If pawn on end ranks, select promotion piece

    // Coordinating with LEDs, motors again

    // Update Board & Piece classes again

}
