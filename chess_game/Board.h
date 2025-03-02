// Board.h file

#ifndef BOARD_H
#define BOARD_H
#include "Piece.h"
#include "ArduinoSTL.h"
#include "PieceType.h"

class Piece;
// The Board Class

class Board {
  public:
    // 8x8 array of pieces (point8_ter to a piece)
    Piece* pieces[8][8];
    // Castling flags
    bool black_king_castle;
    bool black_queen_castle;
    bool white_king_castle;
    bool white_queen_castle;
    // En passant square (The location of the pawn that CAN be taken, not the location that the enemy pawn takes it from...)
    int8_t en_passant_square_x;
    int8_t en_passant_square_y;
    // Move counter 
    int8_t draw_move_counter;
    // Three-fold repetition (vector<vector<y*8 + x, piece_type>, number_of_occrurences>)
    std::vector<std::pair<std::vector<std::pair<int8_t, int8_t>>, int8_t>> three_fold_repetition_vector;

    // For ease of checking if a king is in check, we store the location of the kings
    // Black king location
    int8_t black_king_x;
    int8_t black_king_y;
    // White king location
    int8_t white_king_x;
    int8_t white_king_y;

    // Function that moves a piece
    // We are given the original x, original y, new x, new y, if capture happens, a "capture x" and "capture y"
    // In the end of the function, each individual piece's x and y should be updated
    // Also, the board array will reflect the new state of the board
    // This function also updates the three_fold_repetition_vector -- if move will reset the 50-move counter, it will "destroy" the three_fold_repetition_vector (clear memory)
    void move_piece(int8_t x, int8_t y, int8_t new_x, int8_t new_y, int8_t capture_x, int8_t capture_y);

    bool under_check(bool color);

    std::vector<std::pair<int8_t, int8_t>> sources_of_check(bool color);
    
    Board copy_board();

    void remove_illegal_moves_for_a_piece(int8_t x, int8_t y, std::vector<std::pair<int8_t, int8_t>> &moves);

    bool can_pawn_promote(int8_t x, int8_t y);

    void promote_pawn(int8_t x, int8_t y, PieceType new_type);

    // Three-fold repetition updator, to be called at the end of move_piece()
    // Copies current board state into one instance of the "sub-vector" of three_fold_repetition_vector std::vector<std::pair<int8_t, int8_t>>
    // It checks over the three_fold_repetition_vector to see if the current board state is already in the vector, if so, its count++
    // If not, it adds the current board state to the vector with count 1
    void update_three_fold_repetition_vector();

    // Checks the three_fold_repetition_vector to see if the current board state is already in the vector with count 3, if so, returns true
    bool is_three_fold_repetition();
    
    // Checks if the game is a draw due to insufficient material
    bool is_insufficient_material();

    // Constructor
    Board();

    ~Board();

};

#endif
