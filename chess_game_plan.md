# Chess Game Plan
This game will need the following functions implemented. The high-level goal of this game engine is to achieve the following functionalities:

`is_game_over()` checks if the game is over. Return 1 if game is over, and 0 if game is still going for any given board state. 
`game_over_type()` used when game is over, use 1, 0, -1 for win and draws. Behaviour undefined if game isn't over. 
`draw_type()` determines if game is stalemate, insufficient materials, or 50 move rule. Behaviour undefined if game isn't over.
`possible_moves()` given a square, return a list of squares the piece can move to. Empty set for square without pieces or cannot move.
`all_possible_moves()` list of all possible moves that can be done by a side
`is_check()` given a board, determine if any side is in check (-1, 0, 1) - can be used to check if a move is legal (if the move exposes check) - can also be used to determine checkmate (all moves remains in check)
`check_source()` if a board is in check, indicate source of check (there can be multiple sources for double-check)
`move_piece()` move a piece to a position (don't need to check for valid moves) (captures if destination has a piece already)
`can_promote()` check if the board allows a pawn to promote
`set_square()` set a square to a specific piece (used for pawn promotion).

# Game Procedure

```
Initialize board.
While not is_game_over():
  if is_check():
    check_source()...
  while move not confirmed:
    get user input to select piece
    possible_moves
    get confirmation of destination / continue if move cancelled
  move_piece()
  can_promote():
    get user input on what to promote to
    set_square()
game_over_type():
  draw_type() if draw - display draw type
  check_source for checkmate.
```
