#include "Board.h"

// The Board Class
class Board {
  public:
    // 8x8 array of pieces (pointer to a piece)
    Piece* pieces[8][8];
    // Castling flags
    bool black_king_castle;
    bool black_queen_castle;
    bool white_king_castle;
    bool white_queen_castle;
    // En passant square (The location of the pawn that CAN be taken, not the location that the enemy pawn takes it from...)
    int en_passant_square_x;
    int en_passant_square_y;
    // Move counter 
    int draw_move_counter;

    // For ease of checking if a king is in check, we store the location of the kings
    // Black king location
    int black_king_x;
    int black_king_y;
    // White king location
    int white_king_x;
    int white_king_y;

    // Function that moves a piece
    // We are given the original x, original y, new x, new y, if capture happens, a "capture x" and "capture y"
    // In the end of the function, each individual piece's x and y should be updated
    // Also, the board array will reflect the new state of the board
    void move_piece(int x, int y, int new_x, int new_y, int capture_x, int capture_y) {
      /*
        Make a chess piece move happen on the actual board
        Reset en-passant square and set to new one if it's a pawn move (and reset a pawn's double move)
        If it's a king move, reset castle capability
          Also check for if this move is a castle, which moves the rook first (both update the board and the piece)
        If it's a rook move, reset castle capability
        If it has any capture, set the captured piece to EMPTY
        Update the piece and destination piece's x and y
      */
      // Increment the move counter
      draw_move_counter++;
      // TODO: remove the 3-fold repetition check - flush the vector

      // By default no en passant square
      en_passant_square_x = -1;
      en_passant_square_y = -1;
      // PAWN:
      if (pieces[y][x]->get_type() == PAWN) {
        draw_move_counter = 0; // Reset the draw move counter

        // If the pawn moves two squares, it can be taken en passant
        if (abs(new_y - y) == 2) {
          en_passant_square_x = new_x;
          en_passant_square_y = new_y;
        }
        // If the pawn moves, it can't move two squares
        pieces[y][x]->double_move = false;
      }
      // If the king moves, it can't castle
      if (pieces[y][x]->type == KING) {
        if (pieces[y][x]->color == 0) {
          white_king_castle = false;
          white_queen_castle = false;
          // Update the king's location
          white_king_x = new_x;
          white_king_y = new_y;
        } else {
          black_king_castle = false;
          black_queen_castle = false;
          // Update the king's location
          black_king_x = new_x;
          black_king_y = new_y;
        }

        if (abs(new_x - x) == 2) {
          // Castling
          // If the king moves two squares, move the rook
          // If the king moves to the right, move the rook to the left
          // Since castling doesn't involve capture but 2 pieces are involved
          // we exchange the rook with the empty space here
          if (new_x > x) {
            // Change the rook's x to 3
            pieces[new_y][7]->x = new_x - 1;
            // Change the empty space's x to 7
            pieces[new_y][new_x - 1]->x = 7;
            // Do a piece swap
            Piece *temp = pieces[new_y][7];
            pieces[new_y][7] = pieces[new_y][new_x - 1];
            pieces[new_y][new_x - 1] = temp;
          } else {
            // Change the rook's x to 5
            pieces[new_y][0]->x = new_x + 1;
            // Change the empty space's x to 0
            pieces[new_y][new_x + 1]->x = 0;
            // Do a piece swap
            Piece *temp = pieces[new_y][0];
            pieces[new_y][0] = pieces[new_y][new_x + 1];
            pieces[new_y][new_x + 1] = temp;
          }
        }
      }
      // If rook moves, king can't castle that side
      if (pieces[y][x]->type == ROOK) {
        if (pieces[y][x]->color == 0) {
          // a1 rook - white queen side
          if (x == 0) {
            white_queen_castle = false;
          } else {
            white_king_castle = false;
          }
        } else {
          // a8 rook - black queen side
          if (x == 0) {
            black_queen_castle = false;
          } else {
            black_king_castle = false;
          }
        }
      }
      // If rook is taken, king can't castle that side
      if (capture_x != -1) {
        // Check if the captured piece is a1, a8, h1, h8 (don't care if its a rook or not)
        if (capture_x == 0) {
          if (capture_y == 0) {
            white_queen_castle = false;
          } else if (capture_y == 7) {
            black_queen_castle = false;
          }
        } else if (capture_x == 7) {
          if (capture_y == 0) {
            white_king_castle = false;
          } else if (capture_y == 7) {
            black_king_castle = false;
          }
        }
      }

      // Update piece's x and y (move captured piece first, then the new_x, new_y, then the original x, y)
      // Captured (set piece type to EMPTY) (Graveyard function is implemented outside the chess_game code, it's part of the real physical board)
      //  And we can initialize a new empty piece for the new location
      if (capture_x != -1) {
        pieces[capture_y][capture_x]->type = EMPTY;
        draw_move_counter = 0; // Reset the draw move counter
        // TODO: flush the 3-fold repetition vector
      }
      // New x, new y
      // Set coordinate first, then exchange the pointers
      pieces[new_y][new_x]->x = x; // we know the new piece is empty
      pieces[new_y][new_x]->y = y;
      pieces[x][y]->x = new_x;
      pieces[x][y]->y = new_y;
      // Do a piece swap
      Piece *temp = pieces[new_y][new_x];
      pieces[new_y][new_x] = pieces[y][x];
      pieces[y][x] = temp;

      // TODO: if current board == anything stored in 3-fold repetition, increment count by 1
    }

    bool under_check(bool color) {
      // Check if the king is under check
      // color: 0 for white, 1 for black
      // Loop through all pieces
      for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
          // If the piece is an enemy piece
          if (pieces[i][j]->get_color() != color && pieces[i][j]->get_type() != EMPTY) {
            // Get all possible moves of the piece
            std::vector<std::pair<std::pair<int, int>, std::pair<int, int>>> moves = pieces[i][j]->get_possible_moves(*this);
            // Check if any of the moves are on the king
            for (int k = 0; k < moves.size(); k++) {
              // The second pair is the capture square, which we care about
              if (moves[k].second.first == (color == 0 ? white_king_x : black_king_x) && moves[k].second.second == (color == 0 ? white_king_y : black_king_y)) {
                return true;
              }
            }
          }
        }
      }
      return false;
    }
    
    Board copy_board() {
      // Copy the board
      Board new_board; // TODO: check if this needs to have a space allocated in memory
      // Copy the pieces
      for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
          new_board.pieces[i][j] = new Piece(
                    pieces[i][j]->type,
                    pieces[i][j]->color,
                    pieces[i][j]->x,
                    pieces[i][j]->y
                );
        }
      }
      // Copy the castling flags
      new_board.black_king_castle = black_king_castle;
      new_board.black_queen_castle = black_queen_castle;
      new_board.white_king_castle = white_king_castle;
      new_board.white_queen_castle = white_queen_castle;
      // Copy the en passant square
      new_board.en_passant_square_x = en_passant_square_x;
      new_board.en_passant_square_y = en_passant_square_y;
      // Copy the move counter
      new_board.draw_move_counter = draw_move_counter;
      // Copy the king locations
      new_board.black_king_x = black_king_x;
      new_board.black_king_y = black_king_y;
      new_board.white_king_x = white_king_x;
      new_board.white_king_y = white_king_y;

      // TODO: also copy the 3-fold repetition vector
      return new_board;
    }

    // when checking if a move is illegal due to checks, make sure to consider the path of king's castling
    void remove_illegal_moves_for_a_piece(int x, int y, std::vector<std::pair<std::pair<int, int>, std::pair<int, int>>> &moves) {
      bool piece_color = pieces[y][x]->get_color();

      // If the piece is a king, check if the king is under check after the move
      if (pieces[y][x]->get_type() == KING) {
        // If king is under check, remove castling moves
        if (under_check(piece_color)){
          // Find move that changes king's position by 2
          for (int i = 0; i < moves.size(); i++) {
            if (abs(moves[i].first.first - x) == 2) {
              moves.erase(moves.begin() + i);
              i--;
            }
          }
        }
        // Remove "castling thru check"
        // Find move that changes king's position by 1
        for (int i = 0; i < moves.size(); i++) {
          // TODO: for efficiency, can check the "castle flag first"
          if (abs(moves[i].first.first - x) == 1 && moves[i].first.second == y) {
            // Now, copy new board, and check if king is under check after ONE move to left / right. Remove if it is, and remove subsequent castling moves
            Board new_board = copy_board();
            new_board.move_piece(x, y, moves[i].first.first, moves[i].first.second, moves[i].second.first, moves[i].second.second);
            if (new_board.under_check(piece_color)) {
              // Now, remove castling moves
              for (int j = 0; j < moves.size(); j++) {
                if ((moves[j].first.first - x) == 2 * (moves[i].first.first - x)) {
                  moves.erase(moves.begin() + j);
                  j--;
                  if (j < i) i--;
                }
              }
              moves.erase(moves.begin() + i);
              i--;
            }
          }
        }
      }

      // Loop through all possible moves
      for (int i = 0; i < moves.size(); i++) {
        // Copy the board
        Board new_board = copy_board();

        // Move the piece
        new_board.move_piece(x, y, moves[i].first.first, moves[i].first.second, moves[i].second.first, moves[i].second.second);
        // Check if the king is under check
        if (new_board.under_check(piece_color)) {
          // If the king is under check, remove the move
          moves.erase(moves.begin() + i);
          i--;
        }
      }
    }

    // Give a pawn coordinate, check if it can promote
    std::vector<std::pair<int, int>> can_pawn_promote(int x, int y) {
      // If return empty vector, it can't promote
      // If return vector with one element, it can promote to that piece
      // Make a vector of possible promotions
      std::vector<std::pair<int, int>> promotions;
      // Check if a pawn can promote
      if (pieces[y][x]->get_type() == PAWN) {
        if (pieces[y][x]->get_color() == 0 && y == 0) {
          promotions.push_back(std::make_pair(x, y));
        } else if (pieces[y][x]->get_color() == 1 && y == 7) {
          promotions.push_back(std::make_pair(x, y));
        }
      }
      return promotions;
    }

    void promote_pawn(int x, int y, PieceType new_type) {
      // Promote a pawn
      pieces[y][x]->type = new_type;
    }

    // Constructor
    Board() {
      // Initialize a initial board
      // Make all pieces:
      // White pieces
      pieces[0][0] = new Piece(ROOK, 0, 0, 0);
      pieces[0][1] = new Piece(KNIGHT, 0, 1, 0);
      pieces[0][2] = new Piece(BISHOP, 0, 2, 0);
      pieces[0][3] = new Piece(QUEEN, 0, 3, 0);
      pieces[0][4] = new Piece(KING, 0, 4, 0);
      pieces[0][5] = new Piece(BISHOP, 0, 5, 0);
      pieces[0][6] = new Piece(KNIGHT, 0, 6, 0);
      pieces[0][7] = new Piece(ROOK, 0, 7, 0);
      for (int i = 0; i < 8; i++) {
        pieces[1][i] = new Piece(PAWN, 0, i, 1);
      }
      // Empty pieces
      for (int i = 2; i < 6; i++) {
        for (int j = 0; j < 8; j++) {
          pieces[i][j] = new Piece(EMPTY, 0, j, i);
        }
      }
      // Black pieces
      pieces[7][0] = new Piece(ROOK, 1, 0, 7);
      pieces[7][1] = new Piece(KNIGHT, 1, 1, 7);
      pieces[7][2] = new Piece(BISHOP, 1, 2, 7);
      pieces[7][3] = new Piece(QUEEN, 1, 3, 7);
      pieces[7][4] = new Piece(KING, 1, 4, 7);
      pieces[7][5] = new Piece(BISHOP, 1, 5, 7);
      pieces[7][6] = new Piece(KNIGHT, 1, 6, 7);
      pieces[7][7] = new Piece(ROOK, 1, 7, 7);
      for (int i = 0; i < 8; i++) {
        pieces[6][i] = new Piece(PAWN, 1, i, 6);
      }
      // Initialize castling flags
      black_king_castle = true;
      black_queen_castle = true;
      white_king_castle = true;
      white_queen_castle = true;

      // Initialize en passant square
      en_passant_square_x = -1;
      en_passant_square_y = -1;

      // Initialize move counter
      draw_move_counter = 0;

      // Initialize king locations
      white_king_x = 4;
      white_king_y = 0;
      black_king_x = 4;
      black_king_y = 7;
      // TODO: add 3-fold repetition, and add initial board to the list, with count 1
    }
}