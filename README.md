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
## LED Format:
Example: Colour + Pattern

Cursor: Cyan + Cross

Selected Piece: Green + Solid

Previous Start: Yellow + Solid

Previous End: YelloW + Solid 

Valid Moves: Orange + Solid

Valid Captures: Red + X Pattern

Pawn Promotion: Purple??

## Modules Used:
`Arduino_AVRSTL` 1.2.5 and `ArduinoSTL` 1.3.3 by Mike Matera

`Adafruit BusIO` 1.17.0, `Adafruit GFX Library` 1.11.11, `Adafruit SSD1306` 2.5.13 by Adafruit

`FastLED` 3.9.3 by Daniel Garcia

`Timer` 1.2.1 by Stefan Staub

## Bug Fixes:
In ArduinoSTL new_handler.cpp, comment out line 22: `const std::nothrow_t ...`

## Bug Fix with FASTLED:
In the ArduinoSTL `new` file, modify the following to `namespace std`
```
namespace std{
	class _UCXXEXPORT bad_alloc : public exception {};

    // Check if std::nothrow_t is already defined
    #ifndef __STD_NOTHROW_T_DEFINED
    #define __STD_NOTHROW_T_DEFINED
    #if defined(__AVR__) || defined(ARDUINO)
        // Avoid redefining nothrow_t if included from the Arduino core
        struct nothrow_t; // Declare without defining
        extern const nothrow_t nothrow;
    #else
        // Define nothrow_t if not already defined
        struct _UCXXEXPORT nothrow_t {};
        extern const nothrow_t nothrow;
    #endif
    #endif

	typedef void (*new_handler)();
	_UCXXEXPORT new_handler set_new_handler(new_handler new_p) _UCXX_USE_NOEXCEPT;
}
```
This allows the code to compile

## Raspberry Pi Setup
Raspberry Pi 5, all GPIO and I2C and any other interfaces are set open. 

git cloned stockfish from official stockfish into `~/` directory. 

created a python venv at `~/venv` and installed the stockfish pip library. 

## Arduino Mega running out of memory
The inclusion of 2 displays is really costly in terms of memory. So some fixes regarding that:
1. Before we run begin_turn, we actually just delete the display object, which also frees the memory associated with its buffer. And whenever we want the display again, we will call init_display() which will create the display object again.
2. We **HOPE** to re-write the remove_illegal_moves function to be more memory efficient, currently it copies the entire board object which can be expensive.
3. We reduced some memory usage of 3-fold repetition vector, by not storing ANY pawn states, since in our code, we restart any counter / 3-fold checks every time a pawn makes a move (since that state can never be repeated).
4. Reduced the space required for possible_moves vector. Before it stores a `vector<pair<pair<int, int>, pair<int, int>>>` which is 16 bytes per move. Now it stores a `vector<pair<int, int>>` which is 8 bytes per move. Done by replacing any pair(x,y) into y*8 + x. (We can extract x and y by y = move/8, x = move%8).
5. We kinda removed one animation for the OLED display, which is the "stars falling" animation. We can re-add it later, but just throw the control code in the main loop, and not in the OLED code... (since this is only for IDLE animation)

Memory regarding LED: LED seems to only need 1 pixel at a time, so we can use some algorithm to reduce memory usage. 

## Bug with OLED display:
When adding OLED display code, we changed the selected_x, selected_y,... to be initialized to -1. Instead of previously determined 0. This kinda caused a small error in the display code, where a selected_x of -1 causes it to access wrong memory... and cause the print statement to output wrong value..
