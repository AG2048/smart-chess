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
      // TODO: WE COULD RETURN A vector<pair<pair,pair>> The first pair is destination, second is the piece that is taken
      // TODO: can change this to a "board pointer" if we don't like passing the board as an argument
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
              if (board.pieces[i][j]->get_color() != color) {
                moves.push_back(std::make_pair(j, i));
              } else if (board.pieces[i][j]->get_type() == EMPTY) {
                moves.push_back(std::make_pair(j, i));
              }
            }
          }
          // Castling
          if (color == 0) {
            if (board.white_king_castle) {
              // Check if squares between king and rook are empty
              if (board.pieces[0][5]->get_type() == EMPTY && board.pieces[0][6]->get_type() == EMPTY) {
                moves.push_back(std::make_pair(6, 0));
              }
            }
            if (board.white_queen_castle) {
              // Check if squares between king and rook are empty
              if (board.pieces[0][1]->get_type() == EMPTY && board.pieces[0][2]->get_type() == EMPTY && board.pieces[0][3]->get_type() == EMPTY) {
                moves.push_back(std::make_pair(2, 0));
              }
            }
          } else {
            if (board.black_king_castle) {
              // Check if squares between king and rook are empty
              if (board.pieces[7][5]->get_type() == EMPTY && board.pieces[7][6]->get_type() == EMPTY) {
                moves.push_back(std::make_pair(6, 7));
              }
            }
            if (board.black_queen_castle) {
              // Check if squares between king and rook are empty
              if (board.pieces[7][1]->get_type() == EMPTY && board.pieces[7][2]->get_type() == EMPTY && board.pieces[7][3]->get_type() == EMPTY) {
                moves.push_back(std::make_pair(2, 7));
              }
            }
          }
          break;
        case QUEEN:
          // when encounter "edge of board", opponent piece, or own piece, stop
          // Add opponent piece square to moves
          for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
              if (dy == 0 && dx == 0) continue;
              int x_loop = x+dx;
              int y_loop = y+dy;
              while (1) {
                if (x_loop < 0 || y_loop < 0 || x_loop > 7 || y_loop > 7) break;  // if any coordinate is OOB, stop
                if (board.pieces[y_loop][x_loop]->get_type() == EMPTY) {
                  moves.push_back(std::make_pair(x_loop, y_loop));
                } else if (board.pieces[y_loop][x_loop]->get_color() != color) {
                  // Enemy piece encountered; we can capture it, but don't look past it
                  moves.push_back(std::make_pair(x_loop, y_loop));
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
        break;
        case BISHOP:
          // Same as queen, but only 4 directions
          for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
              if (dy == 0 && dx == 0) continue;
              if (dy == 0 || dx == 0) continue; // only looking at diagonals
              int x_loop = x+dx;
              int y_loop = y+dy;
              while (1) {
                if (x_loop < 0 || y_loop < 0 || x_loop > 7 || y_loop > 7) break;  // if any coordinate is OOB, stop
                if (board.pieces[y_loop][x_loop]->get_type() == EMPTY) {
                  moves.push_back(std::make_pair(x_loop, y_loop));
                } else if (board.pieces[y_loop][x_loop]->get_color() != color) {
                  // Enemy piece encountered; we can capture it, but don't look past it
                  moves.push_back(std::make_pair(x_loop, y_loop));
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
        break;
        case KNIGHT:
          // TODO: Implement knight moves
          // 8 possible moves - don't add if edge of board / own piece
          // Knight moves logic
          int dx[] = {2, 2, -2, -2, 1, 1, -1, -1};
          int dy[] = {1, -1, 1, -1, 2, -2, 2, -2};

          // Loop through all possible knight moves
          for (int i = 0; i < 8; i++) {
              int newX = x + dx[i];
              int newY = y + dy[i];
              
              // Check if the move is within bounds of the board
              if (newX >= 0 && newX < 8 && newY >= 0 && newY < 8) {
                  // If the square is empty or contains an opponent's piece
                  if (board.pieces[newY][newX]->get_type() == EMPTY || board.pieces[newY][newX]->get_color() != color) {
                      moves.push_back(std::make_pair(newX, newY));
                  }
              }
          }
        break;
        case ROOK:
          // 4 possible directions - don't add if edge of board / own piece
          for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
              if (dy == 0 && dx == 0) continue;
              if (dy != 0 && dx != 0) continue; //if dy and dx are both nonzero, it's a diagonal
              // loop in a direction to see valid moves
              int x_loop = x+dx;
              int y_loop = y+dy;
              while (1) {
                if (x_loop < 0 || y_loop < 0 || x_loop > 7 || y_loop > 7) break;  // if any coordinate is OOB, stop
                if (board.pieces[y_loop][x_loop]->get_type() == EMPTY) {
                  moves.push_back(std::make_pair(x_loop, y_loop));
                } else if (board.pieces[y_loop][x_loop]->get_color() != color) {
                  // Enemy piece encountered; we can capture it, but don't look past it
                  moves.push_back(std::make_pair(x_loop, y_loop));
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

    // Constructor
    Piece(PieceType new_type, bool new_color, int new_x, int new_y) {
      type = new_type;
      color = new_color;
      x = new_x;
      y = new_y;
      double_move = true;
    }
};

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
      // By default no en passant square
      en_passant_square_x = -1;
      en_passant_square_y = -1;
      // PAWN:
      if (pieces[y][x]->get_type() == PAWN) {
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
      // Captured (set piece type to EMPTY)
      if (capture_x != -1) pieces[capture_y][capture_x]->type = EMPTY;
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
    }

    // Constructor
    Board() {
      // TODO
    }

void setup() {
  // put your setup code here, to run once:

}

void loop() {
  // put your main code here, to run repeatedly:

}
