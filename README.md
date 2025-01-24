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
