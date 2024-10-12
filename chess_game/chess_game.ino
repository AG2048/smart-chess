// Include
#include <vector>

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

// The Piece Class
class Piece {
  private:
    // Piece type:
    PieceType type;
    // Color of the piece:
    bool color; // 0 for white, 1 for black
    // X and Y coordinates of the piece: (-1, -1) for piece off grid
    int x;
    int y;
    // Pawn: Double move flag - 1 if this pawn can move two squares
    bool double_move;


  public:
    PieceType get_type() {
      return type;
    }

    bool get_color() {
      return color;
    }

    int get_x() {
      return x;
    }

    int get_y() {
      return y;
    }

    bool get_double_move() {
      return double_move;
    }
    // Function that returns x,y coordinates of all possible moves
    // This move function does not check for any potential checks that might occur by this move.
    // The checks should be done by the board class - the board checks the board state after possible moves and evaluate if the move is legal
    std::vector<std::pair<int, int>> get_possible_moves(Board board) {
      std::vector<std::pair<int, int>> moves;
      // switch piece type
      switch (type) {
        case EMPTY:
          break;
        case KING:
          for(int i = y-1; i <= y+1; i++){
            for(int j = x-1; j <= x+1; j++){
              if (i == y && j == x) continue;
              // If empty piece, or opponent piece, you can move there
              if (board.pieces[i][j].get_color() != color) {
                moves.push_back(std::make_pair(j, i));
              } else if (board.pieces[i][j].get_type() == EMPTY) {
                moves.push_back(std::make_pair(j, i));
              }
            }
          }
          // Castling
          if (color == 0) {
            if (board.white_king_castle) {
              // Check if squares between king and rook are empty
              if (board.pieces[5][0].get_type() == EMPTY && board.pieces[6][0].get_type() == EMPTY) {
                moves.push_back(std::make_pair(6, 0));
              }
            }
            if (board.white_queen_castle) {
              // Check if squares between king and rook are empty
              if (board.pieces[1][0].get_type() == EMPTY && board.pieces[2][0].get_type() == EMPTY && board.pieces[3][0].get_type() == EMPTY) {
                moves.push_back(std::make_pair(2, 0));
              }
            }
          } else {
            if (board.black_king_castle) {
              // Check if squares between king and rook are empty
              if (board.pieces[5][7].get_type() == EMPTY && board.pieces[6][7].get_type() == EMPTY) {
                moves.push_back(std::make_pair(6, 7));
              }
            }
            if (board.black_queen_castle) {
              // Check if squares between king and rook are empty
              if (board.pieces[1][7].get_type() == EMPTY && board.pieces[2][7].get_type() == EMPTY && board.pieces[3][7].get_type() == EMPTY) {
                moves.push_back(std::make_pair(2, 7));
              }
            }
          }
          break;
        case QUEEN:
          // TODO: Implement queen moves
          // for loop in "distance" for each of the 8 directions
          // when encounter "edge of board", opponent piece, or own piece, stop
          // Add opponent piece square to moves
        break;
        case BISHOP:
          // TODO: Implement bishop moves
          // Same as queen, but only 4 directions
        break;
        case KNIGHT:
          // TODO: Implement knight moves
          // 8 possible moves - don't add if edge of board / own piece
        break;
        case ROOK:
          // TODO: Implement rook moves
          // 4 possible directions - don't add if edge of board / own piece
        break;
        case PAWN:
          // TODO: Implement pawn moves
          // Check if it's blocked immediately in front
          // Check if it can move 2 pieces (piece.get_double_move())
          // Check if that 2nd square is empty
          // Check if it can take opponent piece (diagonal)
          // Check if it can take en passant (basically see if its left / right neighbor is en-passantable)
        break;
      }
      return moves;
    }

    // Function that moves the piece to a new square
    // Assuming all moves are legal
    // Update king_castle, queen_castle, en_passant, double_move flags
    void move(int new_x, int new_y, Board board) {
      // TODO: migrate this function to board class (because we are updating the pieces themselves and the board grid, might be better to do it there)
      if (type == PAWN) {
        // If the pawn moves two squares, it can be taken en passant
        if (abs(new_y - y) == 2) {
          board.en_passant_square_x = new_x;
          board.en_passant_square_y = new_y;
          // TODO: Board stores the only pawn that is en-passantable, and update the board object and this piece object
          // After each move, board checks that piece that just moved and see if that's en-passantable and stores it
        }
        // If the pawn moves, it can't move two squares
        double_move = false;

        // TODO: if we take en passant, remove the piece that was taken
      }
      // If the king moves, it can't castle
      if (type == KING) {
        if (color == 0) {
          board.white_king_castle = false;
          board.white_queen_castle = false;
        } else {
          board.black_king_castle = false;
          board.black_queen_castle = false;
        }

        // TODO: Check if the king is moving two squares, then it's castling
        // If it's castling, move the rook (call board[0][0].move(3, 0, board)) basically
      }
      // If rook moves, king can't castle that side
      if (type == ROOK) {
        if (color == 0) {
          // a1 rook - white queen side
          if (x == 0) {
            board.white_queen_castle = false;
          } else {
            board.white_king_castle = false;
          }
        } else {
          // a8 rook - black queen side
          if (x == 0) {
            board.black_queen_castle = false;
          } else {
            board.black_king_castle = false;
          }
        }
      }
      x = new_x;
      y = new_y;
    }

    // Constructor
    Piece(PieceType new_type, bool new_color, int new_x, int new_y) {
      type = new_type;
      color = new_color;
      x = new_x;
      y = new_y;
      double_move = true;
    }
};


typedef struct Square {
  int x;
  int y;
}

typedef struct Board {
  bool white_king_castle;
  bool white_queen_castle;
  bool black_king_castle;
  bool black_queen_castle;
  int half_move_counter;
  int full_move_counter;
  Squre en_passant_square; // (-1, -1) for no square
  Piece board_pieces[8][8];
} Board



void setup() {
  // put your setup code here, to run once:

}

void loop() {
  // put your main code here, to run repeatedly:

}
