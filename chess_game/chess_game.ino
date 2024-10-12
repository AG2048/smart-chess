// Include
#include <vector>

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
    // Function pointer to move function that returns a list of possible moves
    std::vector<Square> (*move_function)(int, int);

    // Function that moves the piece to a new square
    // Assuming all moves are legal
    // Update king_castle, queen_castle, en_passant, double_move flags
    void move(int new_x, int new_y, Board board) {
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
      // By default, can't en passant, can move double, can castle
      en_passant = false;
      double_move = true;
      queen_castle = true;
      king_castle = true;
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
