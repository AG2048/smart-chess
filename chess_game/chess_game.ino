// Include
#include "ArduinoSTL.h"
#include "Board.h"
#include "Piece.h"
#include "PieceType.h"
#include "MemoryFree.h"
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

Board *p_board;
std::vector<std::pair<std::pair<int8_t, int8_t>, std::pair<int8_t, int8_t>>> all_moves[8][8];
void setup()
{
  Serial.begin(9600);
  // Serial.print("Free memory: ");
  //   Serial.println(freeMemory());
  // put your setup code here, to run once:
  // Initialize board class
  p_board = new Board;
}

void loop()
{
  // put your main code here, to run repeatedly:

  // Generate possible moves for all pieces in the board class
  for (int row = 0; row < 8; row++)
  {
    for (int col = 0; col < 8; col++)
    {
      // If colour is black, skip
      if (p_board->pieces[row][col]->get_color() == 1)
      {
        continue;
      }
      all_moves[row][col] = p_board->pieces[row][col]->get_possible_moves(p_board);
      // Serial.print(row);
      // Serial.print("\t");
      // Serial.print(col);
      // Serial.print("\t");
      // Serial.print(p_board->pieces[row][col]->type);
      // Serial.println();
      // p_board->remove_illegal_moves_for_a_piece(row, col, all_moves[row][col]);
      Serial.print("Y:");
      Serial.print(row);
      Serial.print("\tX:");
      Serial.print(col);
      Serial.print("\tTYPE:");
      Serial.print(p_board->pieces[row][col]->type);
      Serial.println();
      for (int i = 0; i < all_moves[row][col].size(); i++)
      {
        Serial.print("\t");
        Serial.print(all_moves[row][col][i].first.first);
        Serial.print("\t");
        Serial.print(all_moves[row][col][i].first.second);
        Serial.print("\t");
        Serial.print(all_moves[row][col][i].second.first);
        Serial.print("\t");
        Serial.print(all_moves[row][col][i].second.second);
      }
      Serial.println();
    }
    Serial.println();
  }

  for (int row = 7; row >=0; row--)
  {
    for (int col = 0; col < 8; col++)
    { 
      Serial.print(p_board->pieces[row][col]->type);
    }
    Serial.println();
  }

  // delay();

  p_board->move_piece(5, 1, 5, 2, -1, -1);

  for (int row = 0; row < 8; row++)
  {
    for (int col = 0; col < 8; col++)
    {
      // skip if colour is white
      if (p_board->pieces[row][col]->get_color() == 0)
      {
        continue;
      }

      all_moves[row][col] = p_board->pieces[row][col]->get_possible_moves(p_board);
      // p_board->remove_illegal_moves_for_a_piece(row, col, all_moves[row][col]);
      // p_board->remove_illegal_moves_for_a_piece(row, col, all_moves[row][col]);
      
      Serial.print(row);
      Serial.print("\t");
      Serial.print(col);
      Serial.print("\t");
      Serial.print(p_board->pieces[row][col]->type);
      Serial.println();
      for (int i = 0; i < all_moves[row][col].size(); i++)
      {
        Serial.print("\t");
        Serial.print(all_moves[row][col][i].first.first);
        Serial.print("\t");
        Serial.print(all_moves[row][col][i].first.second);
        Serial.print("\t");
        Serial.print(all_moves[row][col][i].second.first);
        Serial.print("\t");
        Serial.print(all_moves[row][col][i].second.second);
      }
      Serial.println();
    }
    Serial.println();
  }
    for (int row = 7; row >=0; row--)
  {
    for (int col = 0; col < 8; col++)
    { 
      Serial.print(p_board->pieces[row][col]->type);
    }
    Serial.println();
  }

  // delay(5000);

  p_board->move_piece(4, 6, 4, 4, -1, -1);

  for (int row = 0; row < 8; row++)
  {
    for (int col = 0; col < 8; col++)
    {
      // skip if colour is black
      if (p_board->pieces[row][col]->get_color() == 1)
      {
        continue;
      }
      all_moves[row][col] = p_board->pieces[row][col]->get_possible_moves(p_board);
      // p_board->remove_illegal_moves_for_a_piece(row, col, all_moves[row][col]);
      // p_board->remove_illegal_moves_for_a_piece(row, col, all_moves[row][col]);
      
      Serial.print(row);
      Serial.print("\t");
      Serial.print(col);
      Serial.print("\t");
      Serial.print(p_board->pieces[row][col]->type);
      Serial.println();
      for (int i = 0; i < all_moves[row][col].size(); i++)
      {
        Serial.print("\t");
        Serial.print(all_moves[row][col][i].first.first);
        Serial.print("\t");
        Serial.print(all_moves[row][col][i].first.second);
        Serial.print("\t");
        Serial.print(all_moves[row][col][i].second.first);
        Serial.print("\t");
        Serial.print(all_moves[row][col][i].second.second);
      }
      Serial.println();
    }
    Serial.println();
  }
    for (int row = 7; row >=0; row--)
  {
    for (int col = 0; col < 8; col++)
    { 
      Serial.print(p_board->pieces[row][col]->type);
    }
    Serial.println();
  }
  Serial.println("white under check?");
  Serial.println(p_board->under_check(0));

  // delay(5000);

  p_board->move_piece(6, 1, 6, 3, -1, -1);

  for (int row = 0; row < 8; row++)
  {
    for (int col = 0; col < 8; col++)
    {
      // skip if colour is white
      if (p_board->pieces[row][col]->get_color() == 0)
      {
        continue;
      }

      all_moves[row][col] = p_board->pieces[row][col]->get_possible_moves(p_board);
      // p_board->remove_illegal_moves_for_a_piece(row, col, all_moves[row][col]);
      // p_board->remove_illegal_moves_for_a_piece(row, col, all_moves[row][col]);
      Serial.print(row);
      Serial.print("\t");
      Serial.print(col);
      Serial.print("\t");
      Serial.print(p_board->pieces[row][col]->type);
      Serial.println();
      for (int i = 0; i < all_moves[row][col].size(); i++)
      {
        Serial.print("\t");
        Serial.print(all_moves[row][col][i].first.first);
        Serial.print("\t");
        Serial.print(all_moves[row][col][i].first.second);
        Serial.print("\t");
        Serial.print(all_moves[row][col][i].second.first);
        Serial.print("\t");
        Serial.print(all_moves[row][col][i].second.second);
      }
      Serial.println();
    }
    Serial.println();
  }

  for (int row = 7; row >=0; row--)
  {
    for (int col = 0; col < 8; col++)
    { 
      Serial.print(p_board->pieces[row][col]->type);
    }
    Serial.println();
  }
  Serial.println("white under check?");
  Serial.println(p_board->under_check(0));
  // delay(5000);

  p_board->move_piece(3, 7, 7, 3, -1, -1);

  for (int row = 0; row < 8; row++)
  {
    for (int col = 0; col < 8; col++)
    {
      // skip if colour is black
      if (p_board->pieces[row][col]->get_color() == 1)
      {
        continue;
      }
      all_moves[row][col] = p_board->pieces[row][col]->get_possible_moves(p_board);
      // Serial.print("Pre-remove illegal moves: ROW:");
      // Serial.print(row);
      // Serial.print("\t COL:");
      // Serial.print(col);
      // Serial.print("\t TYPE:");
      // Serial.print(p_board->pieces[row][col]->type);
      // Serial.println();
      // for (int i = 0; i < all_moves[row][col].size(); i++)
      // {
      //   Serial.print("\t");
      //   Serial.print(all_moves[row][col][i].first.first);
      //   Serial.print("\t");
      //   Serial.print(all_moves[row][col][i].first.second);
      //   Serial.print("\t");
      //   Serial.print(all_moves[row][col][i].second.first);
      //   Serial.print("\t");
      //   Serial.print(all_moves[row][col][i].second.second);
      // }
      p_board->remove_illegal_moves_for_a_piece(col, row, all_moves[row][col]);
      // p_board->remove_illegal_moves_for_a_piece(row, col, all_moves[row][col]);
      Serial.print("AFTER REMOVE illegal moves: ROW:");
      Serial.print(row);
      Serial.print("\t");
      Serial.print(col);
      Serial.print("\t");
      Serial.print(p_board->pieces[row][col]->type);
      Serial.println();
      for (int i = 0; i < all_moves[row][col].size(); i++)
      {
        Serial.print("\t");
        Serial.print(all_moves[row][col][i].first.first);
        Serial.print("\t");
        Serial.print(all_moves[row][col][i].first.second);
        Serial.print("\t");
        Serial.print(all_moves[row][col][i].second.first);
        Serial.print("\t");
        Serial.print(all_moves[row][col][i].second.second);
      }
      Serial.println();
    }
    Serial.println();
  }

  for (int row = 7; row >=0; row--)
  {
    for (int col = 0; col < 8; col++)
    { 
      Serial.print(p_board->pieces[row][col]->type);
    }
    Serial.println();
  }


  // check what happens if move
  p_board->move_piece(6, 0, 7, 2, -1, -1);


  Serial.println("white under check?");
  Serial.println(p_board->under_check(0));
  delay(500000);

  // Remove illegal moves

  // Detect checkmate / draw

  // If detected, enter game over

  // Coordinate with LEDs --> indicate possible moves, selected piece,

  // Coordinate with motors --> tell them what pieces to move

  // Pieces have been moved, update the Board and Piece classes

  // Check pawn promotion

  // If pawn on end ranks, select promotion piece

  // Coordinating with LEDs, motors again

  // Update Board & Piece classes again
}
