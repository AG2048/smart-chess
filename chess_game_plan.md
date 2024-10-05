# Chess Game Plan

## Data Structure
`piece` denote piece type and colour. For pawn record if en-passant-able, pawn 2-move-able, kings record castle capability on both sides. Also an "empty type"

We could possibly add a "function" pointer to the piece structure such that it takes in a board object with its current location, and figure out where it can possibly move to

`board` is a 1D or 2D array of "pieces". (square can be denoted by its x and y coordinate for 2D, OR by i*w+j for 1D) 

also store move numbers since last pawn move or capture

RNBKQBNR\PPPPPPPPP\8\8\8\8\pppppppp\rnbkqbnr KQkq 0 0

## Core Functions

This game will need the following functions implemented. The high-level goal of this game engine is to achieve the following functionalities:

`is_game_over()` checks if the game is over. Return 1 if game is over, and 0 if game is still going for any given board state. 

`game_over_type()` used when game is over, use 1, 0, -1 for win and draws. Behaviour undefined if game isn't over. 

`draw_type()` determines if game is stalemate, insufficient materials, or 50 move rule. Behaviour undefined if game isn't over.

`possible_moves()` given a square, return a list of squares the piece can move to. Empty set for square without pieces or cannot move. (Make sure to account for pawn's special movement AND invalid moves when in check or when pinned)

`all_possible_moves()` list of all possible moves that can be done by a side

`is_check()` given a board, determine if any side is in check (-1, 0, 1) - can be used to check if a move is legal (if the move exposes check) - can also be used to determine checkmate (all moves remains in check)

`check_source()` if a board is in check, indicate source of check (there can be multiple sources for double-check)

`move_piece()` move a piece to a position (don't need to check for valid moves) (captures if destination has a piece already) (NOTE CASTLE IS SPECIAL MOVE) (NOTE EN-PASSANT IS SPECIAL MOVE) 

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

# Interface with Physical Game

There should be ways for the game engine to interface with the actual game.

## LEDs

The plan is to have LED denote:

1. past piece move (the previous "from" and "to" square)
  - Have a variable recording 2 coordinates. Can default to -1, -1 if first move.
2. Cursor Selection (have different colours if cursor is on "movable" piece)
3. Possible Move Indication (When a piece is selected, show all possible squares it can move to)
4. Possible Capture Indication (if the possible move is a capture, use different colour)
5. Checks - highlight check source
6. Pawn promotion lights up pawns and enables cursor to select on 4 symbols of "promotable" pieces
7. Draw type / checkmate indication




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
