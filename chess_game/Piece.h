// Piece.h
#ifndef PIECE_H
#define PIECE_H
#include <ArduinoSTL.h>
#include "Board.h"


// The Piece Class
class Piece {
  public:
    // Piece type:
    PieceType type;
    // Color of the piece:
    bool color; // 0 for white, 1 for black
    // X and Y coordinates of the piece: (-1, -1) for piece off grid
    int x;
    int y;
    // Pawn: Double move flag - 1 if this pawn can move two squares
    bool double_move;

    PieceType get_type();

    bool get_color();

    int get_x();

    int get_y();

    bool get_double_move();

    // Function that returns x,y coordinates of all possible moves

    std::vector<std::pair<std::pair<int, int>, std::pair<int, int>>> get_possible_moves(Board* board);

    // Constructor

    Piece(PieceType new_type, bool new_color, int new_x, int new_y);

};

#endif