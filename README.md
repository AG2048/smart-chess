# smart-chess
Smart Chess

## File Structure
Eventually they will all be in 1 controller
```
chess_game/
  Game Engine - remembers game state, runs game.
  LED Indication Blocks - based on game state, tell which LEDs should be lit up in each scenario
controls/
  Reads user inputs - process user inputs.
  Main - inits game engine, loop between: read user state, LED light up, game engine run if user moved, move pieces...
stockfish/
  Does stockfish stuff. 
```

## Bug Fixes:
In ArduinoSTL new_handler.cpp, comment out line 22: `const std::nothrow_t ...`
