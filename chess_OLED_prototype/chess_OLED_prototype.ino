// Refer to this example https://github.com/adafruit/Adafruit_SSD1306/blob/master/examples/ssd1306_128x64_i2c/ssd1306_128x64_i2c.ino
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Timer.h"

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library. 
// On an arduino MEGA 2560: 20(SDA), 21(SCL)

#define OLED_RESET     -1 // Reset pin (-1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C // Address is 0x3D for 128x64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Important game state items (picked from chess_game.ino)

enum GameState {
  GAME_POWER_ON,    // Power on the board, motors calibrate
  GAME_IDLE,        // No game is being played yet, waiting for a game to start
  GAME_INITIALIZE,  // Game started, initialize the board
  GAME_BEGIN_TURN,  // Begin a turn - generate moves, remove illegal moves,
                    // check for checkmate/draw
  GAME_WAIT_FOR_SELECT,  // Joystick moving, before pressing select button on
                         // valid piece
  GAME_WAIT_FOR_MOVE,  // Joystick moving, after pressing select button on valid
                       // destination
  GAME_MOVE_MOTOR,     // Motors moving pieces
  GAME_END_MOVE,       // Update chess board, see if a pawn can promote
  GAME_WAIT_FOR_SELECT_PAWN_PROMOTION,  // Joystick moving, before pressing
                                        // select button on promotion piece
  GAME_PAWN_PROMOTION_MOTOR,  // Motors moving pieces for pawn promotion
  GAME_END_TURN,              // End a turn - switch player
  GAME_OVER_WHITE_WIN,        // White wins
  GAME_OVER_BLACK_WIN,        // Black wins
  GAME_OVER_DRAW,             // Draw
  GAME_RESET                  // Reset the game
};

GameState game_state;
bool player_turn; // 0 for white, 1 for black

/* OLED wrapper functions
 * display_idle_screen(unsigned long time_called, interacted)


*/

static uint32_t last_interacted_time;
static bool idle_one, idle_two;
#define IDLE_ONE_DELAY_MS 1000*20 //20 seconds in ms
#define IDLE_TWO_DELAY_MS 1000*40 //40 seconds in ms
#define NUMFLAKES 10 // number of snowflakes to use in idle animation
#define LOGO_WIDTH 16     // bitmap dimensions
#define LOGO_HEIGHT 16

#define XPOS   0 // Indexes into the 'icons' array for display_idle_screen fn
#define YPOS   1
#define DELTAY 2

static const unsigned char PROGMEM logo_bmp[] = // A bitmap that we can change to draw whatever image we want.
{ 0b00000000, 0b11000000,
  0b00000001, 0b11000000,
  0b00000001, 0b11000000,
  0b00000011, 0b11100000,
  0b11110011, 0b11100000,
  0b11111110, 0b11111000,
  0b01111110, 0b11111111,
  0b00110011, 0b10011111,
  0b00011111, 0b11111100,
  0b00001101, 0b01110000,
  0b00011011, 0b10100000,
  0b00111111, 0b11100000,
  0b00111111, 0b11110000,
  0b01111100, 0b11110000,
  0b01110000, 0b01110000,
  0b00000000, 0b00110000 };

/* display_idle_screen arguments
  Timer timer, passed in so we can poll and see the current time and hence how much time has passed

  bool interacted, passed high if a user has interacted with the screen, so if necessary we know to 
  exit idle states

  bool select_player, high if that is the screen to be displayed outside of idle states

  bool select_computer, high if that is the screen to be displayed outside of idle states

  uint8_t comp_diff, the difficulty value the user is selecting for Stockfish
*/

// No screen_num argument -- assuming both screens will display the same thing while idle
void display_idle_screen(Timer timer, bool interacted, bool select_player, bool select_computer, uint8_t comp_diff) {
  uint32_t current_time = timer.read();

  static int8_t icons[NUMFLAKES][3]; /* Used for idle animation
  [x][0] --> XPOS
  [x][1] --> YPOS
  [x][2] --> DELTAY */  

  if (interacted) { // If the user has interacted with the display
    if (idle_one) {  // Do we need to check idle_one? 
      idle_one = 0;
      display.stopscroll();
    }
    idle_two = 0; // if user has interacted, we are certainly not in an idle screen
    last_interacted_time = current_time;
    // Draw the screen we're currently on
    // TO DO: find images for both screens (sel_player, sel_comp) and convert them to bitmaps 
    if (select_player) {
      display.clearDisplay();
      display.setTextSize(2);
      // Draw bitmap
      display.println(F(      "PLAYER"));
      display.println(F("HUMAN  Comp>"));
      display.print(F("MODE"));
      display.display();
    } else if (select_computer) {
      display.clearDisplay();
      display.setTextSize(2);
      display.println(F("STOCKFISH"));
      display.print(F("<Human  LV:"));
      // Draw bitmap
      display.print(comp_diff, DEC);
      display.display();
    }
    
  } else { // If the user hasn't interacted with the display

    if (current_time - last_interacted_time > IDLE_TWO_DELAY_MS) {// Showing screen two : idle animation
      
      if (idle_one) { // Clear idle_one flag
        idle_one = 0;
        display.stopscroll();
      }

      // Snowflake animation from example
      // Every time this function is called, draw the next frame of animation
      if (!idle_two) { // We are just entering idle animation. Initialize!
        idle_two = 1;
        for (int8_t f = 0; f < NUMFLAKES; f++) {
          icons[f][XPOS]   = random(1 - LOGO_WIDTH, display.width());
          icons[f][YPOS]   = -LOGO_HEIGHT;
          icons[f][DELTAY] = random(1, 6);
        }
      } else { // We've already initialized this idle screen. Draw next frame!
        display.clearDisplay();

        // Draw each snowflake:
        for (int8_t f = 0; f < NUMFLAKES; f++) {
          display.drawBitmap(icons[f][XPOS], icons[f][YPOS], logo_bmp, LOGO_WIDTH, LOGO_HEIGHT, SSD1306_WHITE);
        }

        display.display(); // Show the display buffer on the screen
        // delay(200);        // Pause for 1/10 second. Should we?

        // Then update coordinates of each flake...
        for (int8_t f = 0; f < NUMFLAKES; f++) {
          icons[f][YPOS] += icons[f][DELTAY];
          // If snowflake is off the bottom of the screen...
          if (icons[f][YPOS] >= display.height()) {
            // Reinitialize to a random position, just off the top
            icons[f][XPOS]   = random(1 - LOGO_WIDTH, display.width());
            icons[f][YPOS]   = -LOGO_HEIGHT;
            icons[f][DELTAY] = random(1, 6);
          }
        }

      }
      
    } else if (current_time - last_interacted_time > IDLE_ONE_DELAY_MS) { // Showing screen one : scrolling text

      if (!idle_one) { // We only want this to run once, when we first enter the second idle screen
        idle_one = 1; // We need to clear this once we exit 
        display.clearDisplay();
        display.setTextSize(2);
        display.println(F("     Spark"));
        display.print(F("SMARTCHESS"));
        display.display();
        display.startscrollleft(0x00, 10); // (row 1, row 2). 0x0F as 2nd row scrolls whole display.
        display.startscrollright(11, 0x0F);
      }

    } else { // We're not idle, but user hasn't interacted

      if (select_player) { // Displaying screens again... may not be necessary, unless we include things like blinking objects

      } else if (select_computer) {

      }

    }

  }
}

void display_select(bool screen_num) {
  /* To be called over & over whenever a screen needs to be updated (new move selected, a piece has been moved)
   * 

   */
}

void display_promotion(bool screen_num) {

}

void setup() {
  Serial.begin(9600);
  last_interacted_time = 0;
  idle_one = 0;
  idle_two = 0;
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  Serial.println("test");
  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(500); // Pause for 2 seconds

  game_state = GAME_IDLE;
  display.setTextColor(SSD1306_WHITE);

  if (game_state == GAME_IDLE) { // Idle screen. 
    // *** This might be something we only want to run once when game_state changes, aka not looping over it ***
    // This can be a function we call once game_state changes.
    Serial.println("game_idle test");
    
    // Smart chess written as scrolling text
    display.clearDisplay();
    delay(100);
    display.display();
    delay(100);

    display.setTextSize(2); // Draw 2X-scale text (fills up the whole header)
    // display.setTextColor(SSD1306_WHITE); Not sure how setting color works on the OLED we have
    display.setCursor(10, 0); // Cursor is not used for writing messages
    display.print(F("SMART CHESS           "));
    display.display();      


    //display.setCursor(50, 0); // To be tweaked probably. I'm just guessing these numbers.
    //display.print(F("Come!"));
    
    display.startscrollleft(0x00, 0x0F); // (row 1, row 2). 0x0F as 2nd row scrolls whole display.
    //display.startscrollright(10, 0x0F); // 10 is probably to be tweaked as well.
    
    delay(2000);

  //   display.clearDisplay();

  // display.setTextSize(1);      // Normal 1:1 pixel scale
  // display.setTextColor(SSD1306_WHITE); // Draw white text
  // display.setCursor(10, 50);     // Start at top-left corner (x, y)
  // display.cp437(true);         // Use full 256 char 'Code Page 437' font
  // //display.write uses cursor
  
  // // Not all the characters will fit on the display. This is normal.
  // // Library will draw what it can and the rest will be clipped.
  // for(int16_t i=0; i<256; i++) {
  //   if(i == '\n') display.write(' ');
  //   else          display.write(i);
  // }

  // display.display();
  // delay(2000);
  }

}

void loop() {
  // While we are drawing a new frame every time we loop, in reality
  // we could simply call an update_OLED() function to draw a new frame when we need to.

}
