enum Piece {
  W_KING,
  W_QUEEN,
  W_BISHOP,
  W_KNIGHT,
  W_ROOK,
  W_PAWN,
  B_KING,
  B_QUEEN,
  B_BISHOP,
  B_KNIGHT,
  B_ROOK,
  B_PAWN
}

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
