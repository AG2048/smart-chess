// Refer to this example https://github.com/adafruit/Adafruit_SSD1306/blob/master/examples/ssd1306_128x64_i2c/ssd1306_128x64_i2c.ino
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Timer.h"
#include "ArduinoSTL.h"

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library. 
// On an arduino MEGA 2560: 20(SDA), 21(SCL)

#define OLED_RESET     -1 // Reset pin (-1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C // Address is 0x3D for 128x64
#define SCREEN_ADDRESS_TWO 0x3D  // Apparently the next address is 0x3D not 0x7A on the board.
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_SSD1306 displayTwo(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

enum PieceType {
  EMPTY,
  KING,
  QUEEN,
  BISHOP,
  KNIGHT,
  ROOK,
  PAWN
};

bool player_turn; // 0 for white, 1 for black

/* OLED Game State functions --> @ indicates functionality has been verified
 @ void display_init()
 @ void display_idle_screen(Timer timer, bool interacted, bool screen_select, uint8_t comp_diff)
 @ void display_initialization() 
 @ void display_turn_select(int8_t selector, int8_t joystick_x, int8_t joystick_y, int8_t selected_x, int8_t selected_y, 
   int8_t destination_x, int8_t destination_y)
 @ void display_promotion(int8_t player_promoting, int8_t piece)
 @ void display_game_over(int8_t winner)

*/

// TODO: do all of these even need to be static? they are global variables that don't live in just the scope of the OLED functions
static uint32_t last_interacted_time, last_idle_change;
static int8_t old_joystick_x, old_joystick_y, old_promotion_piece;

// Flags to ensure we only print corresponding screens ONCE, not every loop
static bool idle_one, idle_two, displayed_player, displayed_computer, displayed_initializing, displayed_motor_moving, displayed_game_over;
Timer OLED_timer;
#define IDLE_ONE_DELAY_MS 1000*5 //20 seconds in ms
#define IDLE_TWO_DELAY_MS 1000*10 //40 seconds in ms
#define IDLE_DELAY_MS 1000*5 // 5 seconds in ms
#define NUM_FLAKES 10 // number of snowflakes to use in idle animation
#define LOGO_WIDTH 16     // bitmap dimensions
#define LOGO_HEIGHT 16

static int8_t icons[NUM_FLAKES][3]; // Used for idle animation
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

// This function initializes the timer, the displays, as well as all the different game state flags
void display_init() {

  // Initializing flags
  last_interacted_time = 0;
  last_idle_change = 0;
  idle_one = 1; // Display idle screen one first by default
  idle_two = 0;
  displayed_computer = 0;
  displayed_player = 0;
  displayed_initializing = 0;
  displayed_motor_moving = 0;
  old_joystick_x = -1;
  old_joystick_y = -1;
  old_promotion_piece = -1;
  displayed_game_over = 0;

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 ONE allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  if(!displayTwo.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS_TWO)) {
    Serial.println(F("SSD1306 TWO allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  Serial.println("Allocation succeeded, proceeding");

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(500);
  display.setTextColor(SSD1306_WHITE);
  OLED_timer.start();
}

/*
  TODO
  function for the OLED timer, resetting flags
  *** Adding display two support ***
*/
// This function handles the chess game's GAME_IDLE state.
// No screen_num argument -- assuming both screens will display the same thing while idle

// still keep timer but ONLY for idle animation switching. Have a "is-idle" flag that the main will tell if we are in idle or not
// have a player_0_state, player_1_state, player_0_difficulty, player_1_difficulty (note that these numbers will range from 0 to 19 for diff. Display need to +1)
// screen_index maybe... then only need one set of state/difficulty
void display_idle_screen(Timer timer, bool is_idle, bool screen_select, uint8_t comp_diff, Adafruit_SSD1306 display) {
  /* display_idle_screen arguments
  Timer timer, passed in so we can poll and see the current time and hence how much time has passed

  is_idle, true if we are currently displaying an idle screen

  screen_select to know which screen we are on; 1 for choosing player opponent, 0 for changing computer opponent difficulty

  comp_diff to know the current difficulty to be displayed when setting up computer opponents (0 to 19)

  display to display to a specific display
  */

  uint32_t current_time = timer.read();
  if (is_idle) {

    displayed_computer = 0;
    displayed_player = 0; // resetting flags

    if ( current_time - last_idle_change > IDLE_DELAY_MS ) { // Swapping idle screens
      idle_one = !idle_one;
      idle_two = !idle_two;
      last_idle_change = current_time;
      display.stopscroll();
    }

    if (idle_one) { // Scrolling text

      if (current_time == last_idle_change) { // If we are displaying this idle screen for the first time
        display_idle_scroll();
      }

    } else if (idle_two) { // Idle animation

      display_idle_animation(current_time);

    }    

  } else { // Displaying setup screens

    if (screen_select) {

      if (!displayed_player) {
        display_select_player(display);
        displayed_player = 1;
      }
      displayed_computer = 0;

    } else {

      if (!displayed_computer) {
        display_select_computer(display, comp_diff);
        displayed_computer = 1;
      }
      displayed_player = 0;

    }

  }

}

void display_initialization() {
  // Display initialization message to both screens
  if (!displayed_initializing) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(F("Initializing..."));
    display.display();
    
    displayTwo.clearDisplay();
    displayTwo.setTextSize(1);
    displayTwo.setCursor(0, 0);
    displayTwo.print(F("Initializing..."));
    displayTwo.display();

    displayed_initializing = 1;
  }
  
}

/* display_turn_select encompasses the following states

  GAME_WAIT_FOR_SELECT
  GAME_WAIT_FOR_MOVE
  GAME_MOVE_MOTOR

*/
void display_turn_select(int8_t selector, int8_t joystick_x, int8_t joystick_y, int8_t selected_x, int8_t selected_y, 
int8_t destination_x, int8_t destination_y) {
  /* display_turn_select args

    selector ; indicates which player is selecting a move right now (1 for P1, 2 for P2)

    joystick_x, joystick_y ; coordinate currently selected via joystick. will be constantly updated

    selected_x, selected_y ; the first move coordinate that player selects
      both assumed to be -1 if not set

    destination_x, destination_y ; the second move coordinate that player selects
      both assumed to be -1 if not set
  */
  char msg[100];
  // Convert joystick_x and joystick_y into correct ASCII values
    // joystick_x + 'a'
    // joystick_y + '1'

  if (old_joystick_x != joystick_x || old_joystick_y != joystick_y) {

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    displayTwo.clearDisplay();
    displayTwo.setTextSize(1);
    displayTwo.setCursor(0, 0);

    if (selected_x == -1 && selected_y == -1) { // if the user is still selecting their first move
      sprintf(msg, "Moving %c%c...", (joystick_x + 'a'), (joystick_y + '1'));

      if (selector == 1) {

        display.print(msg);
        msg[0] = 'm';
        displayTwo.println(F("Your opponent is "));
        displayTwo.print(msg);

      } else {

        displayTwo.print(msg);
        msg[0] = 'm';
        display.println(F("Your opponent is "));
        display.print(msg);
      }

  } else if (destination_x == -1 && destination_y == -1) { // if the user is selecting their second move
    sprintf(msg, "Moving %c%c to %c%c...", (selected_x + 'a'), (selected_y + '1'), (joystick_x + 'a'), (joystick_y + '1'));

    // msg: Moving from selected coords to joystick coords (all converted into board coords)

    if (selector == 1) {
 
      display.print(msg);
      msg[0] = 'm';
      displayTwo.println(F("Your opponent is "));
      displayTwo.print(msg);     

    } else {
      displayTwo.print(msg);
      msg[0] = 'm';
      display.println(F("Your opponent is "));
      display.print(msg);
    }

  }

  display.display();
  displayTwo.display();
  old_joystick_x = joystick_x;
  old_joystick_y = joystick_y;

  } else { // Joystick hasn't changed

  }

  if (!displayed_motor_moving && selected_x != -1 && destination_x != -1) {
    // msg: Motor moving!
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(F("Motor is moving!"));
    display.display();

    displayTwo.clearDisplay();
    displayTwo.setTextSize(1);
    displayTwo.setCursor(0, 0);
    displayTwo.print(F("Motor is moving!"));
    displayTwo.display();
    displayed_motor_moving = 1;
  }

}

void display_promotion(int8_t player_promoting, int8_t piece) {
  /* display_promotion arguments

    player_promoting to know which player is promoting; that way, we know which player to display "opponent is promoting..." message
    
    piece to know which piece the promoting player is currently hovering, so we can print it on their display.

  */
  if (old_promotion_piece != piece) {

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    displayTwo.clearDisplay();
    displayTwo.setTextSize(1);
    displayTwo.setCursor(0, 0);

    if (player_promoting == 1) {

      // Player 1 is promoting   
      display.println(F("You are currently promoting to a..."));

      switch(piece) {
        case QUEEN:
          display.print(F("QUEEN"));
          break;
        case KNIGHT:
          display.print(F("KNIGHT"));
          break;
        case ROOK:
          display.print(F("ROOK"));
          break;
        case BISHOP:
          display.print(F("BISHOP"));
          break;
      }
      // Player 2 is waiting
      displayTwo.print(F("Your opponent is promoting..."));
      display.display();
      displayTwo.display();
      
    } else if (player_promoting == 2) {

      // Player 2 is promoting   
      displayTwo.println(F("You are currently promoting to a..."));

      switch(piece) {
        case QUEEN:
          displayTwo.print(F("QUEEN"));
          break;
        case KNIGHT:
          displayTwo.print(F("KNIGHT"));
          break;
        case ROOK:
          displayTwo.print(F("ROOK"));
          break;
        case BISHOP:
          displayTwo.print(F("BISHOP"));
          break;
      }
      // Player 1 is waiting
      display.print(F("Your opponent is promoting..."));
      display.display();
      displayTwo.display();

    }
    old_promotion_piece = piece;
  }


}

void display_game_over(int8_t winner, int8_t draw) {
  /* display_game_over arguments

    winner to know which player won: 
      1 for player 1
      2 for player 2
      0 for draw

    draw to know what kind of draw it is
      0 for no draw
      1 for 50 move draw
      2 for 3 fold repetition    
      3 for draw via insufficient material
    
    depending on winner value, the corresponding helper functions will be called to display information to each display

  */

  if (!displayed_game_over) {

    if (winner == 0) {
    // Displays stalemate to both displays
    display_stalemate(draw, display);
    display_stalemate(draw, displayTwo); 

    } else if (winner == 1) {
      display_winner(1, display);
      display_loser(2, displayTwo);
    } else if (winner == 2) {
      display_winner(2, displayTwo);
      display_loser(1, display);
    }  
  
    displayed_game_over = 1;
  }

}
// display_idle_screen helper functions
void display_select_player(Adafruit_SSD1306 display) {
  display.clearDisplay();
  display.setTextSize(1);
  // Draw bitmap
  display.setCursor(0, 0);
  display.println(F(      "PLAYER"));
  display.println(F("HUMAN  Comp>"));
  display.print(F("MODE"));
  display.display();
}

void display_select_computer(Adafruit_SSD1306 display, uint8_t comp_diff) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("STOCKFISH"));
  display.print(F("<Human  LV:"));
  // Draw bitmap
  display.print(comp_diff+1, DEC);
  display.display();
}

void display_idle_scroll() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println(F(" > Spark <"));
  display.print(F("SMARTCHESS"));
  display.display();
  // display.startscrollleft(0, 1); // (row 1, row 2). Scrolls just the first row of text.
  display.startscrollright(2, 3); // SSD1306 can't handle two concurrent scroll directions.
}

void display_idle_animation(uint32_t current_time) {

  /* Once we make OLED_timer a global thing in our library, we won't need to pass in current_time anymore */

  if (current_time == last_idle_change) { // We are just entering idle animation. Initialize!

    for (int8_t f = 0; f < NUM_FLAKES; f++) {
      icons[f][XPOS]   = random(1 - LOGO_WIDTH, display.width());
      icons[f][YPOS]   = -LOGO_HEIGHT;
      icons[f][DELTAY] = random(1, 6);
    }

  } else { // We've already initialized this idle screen. Draw next frame!
    display.clearDisplay();

    // Draw each snowflake:
    for (int8_t f = 0; f < NUM_FLAKES; f++) {
      display.drawBitmap(icons[f][XPOS], icons[f][YPOS], logo_bmp, LOGO_WIDTH, LOGO_HEIGHT, SSD1306_WHITE);
    }

    display.display(); // Show the display buffer on the screen
    // delay(200);        // Pause for 1/10 second. Should we?

    // Then update coordinates of each flake...
    for (int8_t f = 0; f < NUM_FLAKES; f++) {
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
}

//display_game_over helper functions

void display_stalemate(int8_t draw, Adafruit_SSD1306 display) {

  /*
    0 for no draw
    1 for 50 move draw
    2 for 3 fold repetition    
    3 for draw via insufficient material 

  */

  // Display to player 1
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);

  if (draw == 1) {

    display.println(F("Draw by 50 move rule!"));

  } else if (draw == 2) {

    display.println(F("Draw by 3 fold repetition!"));

  } else if (draw == 3) {

    display.println(F("Draw by insufficient material!"));

  } else {
    Serial.println("Bad draw argument");
    return;
  }

  display.display();
}

void display_winner(int8_t winner, Adafruit_SSD1306 display) {

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);

  display.print(F("P"));
  display.print(winner);
  display.println(F(" wins!"));

  display.display();

}

void display_loser(int8_t loser, Adafruit_SSD1306 display) {

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);

  display.print(F("P"));
  display.print(loser);
  display.print(F(" loses!"));

  display.display();
}
int8_t x_test = 0;
int8_t y_test = 0;

void setup() {
  Serial.begin(9600);

  display_init();
  
  // Some variables to be passed into display_idle_screen()
  bool selecting_player = 0, selecting_computer = 0; 

  pinMode(22, INPUT_PULLUP);
  pinMode(24, INPUT_PULLUP);
  Serial.println("Setup done. Entering loop");
}
bool x_change, y_change;
void loop() {
  x_change = digitalRead(22); // Digital pin 22
  y_change = digitalRead(24);
  if (!x_change) {
    x_test++;
    if (x_test > 7) x_test = 0;
    Serial.println("Incremented x");
    delay(1000);
  }
  if (!y_change) {
    y_test++;
    if (y_test > 7) y_test = 0;
    Serial.println("Incremented y");
    delay(1000);
  }

  display_idle_screen(OLED_timer, 0, 0, 15, display); // test values for screens & comp_diff
  //display_initialization();
  //void display_turn_select(int8_t selector, int8_t joystick_x, int8_t joystick_y, int8_t selected_x, int8_t selected_y, 
  //int8_t destination_x, int8_t destination_y)
  //display_turn_select(2, x_test, y_test, -1, -1, -1, -1);
  //display_promotion(2, QUEEN + x_test);
  //display_game_over(0, 3);
}
