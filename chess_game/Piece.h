// Piece.h
#ifndef PIECE_H
#define PIECE_H
// #include "ArduinoSTL.h"
#include "Board.h"
#include "PieceType.h"
#include <stdint.h>
#include <vector>
#include <utility>
#include <string.h>
#include <Arduino.h>

class Board;

// The Piece Class
class Piece {
  public:
    // Knight move
    static constexpr int8_t knight_dx[8] = {2, 2, -2, -2, 1, 1, -1, -1};
    static constexpr int8_t knight_dy[8] = {1, -1, 1, -1, 2, -2, 2, -2};

    // Piece type:
    PieceType type;
    // Color of the piece:
    bool color; // 0 for white, 1 for black
    // X and Y coordinates of the piece: (-1, -1) for piece off grid
    int8_t x;
    int8_t y;
    // Pawn: Double move flag - 1 if this pawn can move two squares
    bool double_move;

    PieceType get_type();

    bool get_color();

    int8_t get_x();

    int8_t get_y();

    bool get_double_move();

    // Function that returns x,y coordinates of all possible moves

    std::vector<std::pair<int8_t, int8_t>> get_possible_moves(Board* board) const;

    // Constructor

    Piece(PieceType new_type, bool new_color, int8_t new_x, int8_t new_y);

};

#endif