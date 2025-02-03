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

bool player_turn; // 0 for white, 1 for black

/* OLED wrapper functions
 * display_idle_screen(unsigned long time_called, interacted)


*/

static uint32_t last_interacted_time, last_idle_change;
// Flags to ensure we only print corresponding screens ONCE, not every loop
static bool idle_one, idle_two, displayed_player, displayed_computer;
bool button;
Timer OLED_timer;
#define IDLE_ONE_DELAY_MS 1000*5 //20 seconds in ms
#define IDLE_TWO_DELAY_MS 1000*10 //40 seconds in ms
#define IDLE_DELAY_MS 1000*5 // 5 seconds in ms
#define NUMFLAKES 10 // number of snowflakes to use in idle animation
#define LOGO_WIDTH 16     // bitmap dimensions
#define LOGO_HEIGHT 16

static int8_t icons[NUMFLAKES][3]; // Used for idle animation
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

/*
  TODO

  make functions to wrap the code to display the different screens

  draw the screens if not interacted, but not idle

  for select screen, we can't reprint it every loop; this causes nothing to be displayed
  need to modify such that we only print it the first time. we can add additional logic in the wrapper functions

  don't need 2 vars for sel_player and sel_comp
  
  init function for the OLED timer, resetting flags
*/

// No screen_num argument -- assuming both screens will display the same thing while idle
void display_idle_screen(Timer timer, bool interacted, bool screen_select, uint8_t comp_diff) {
  /* display_idle_screen arguments
  Timer timer, passed in so we can poll and see the current time and hence how much time has passed

  bool interacted, passed high if a user has interacted with the screen, so if necessary we know to 
  exit idle states

  bool screen_select, high if displaying select_player screen, low if displaying select_computer

  uint8_t comp_diff, the difficulty value the user is selecting for Stockfish
  */
  uint32_t current_time = timer.read();
  
  if (interacted) { // If the user has interacted with the display

    // Setting idle screens back to default, so we can enter them again later
    idle_one = 1;
    idle_two = 0; 
    display.stopscroll();

    // Draw the screen we're currently on
    // TO DO: find images for both screens (sel_player, sel_comp) and convert them to bitmaps 
    if (screen_select) {

      if (!displayed_player) {
        display_select_player();
        displayed_player = 1;
        Serial.println("Interacted, displayed player");
      }
      displayed_computer = 0;

    } else { // If screen_select is low

      if (!displayed_computer) {
        display_select_computer(comp_diff);
        displayed_computer = 1;
        Serial.println("Interacted, displayed computer");

      }
      displayed_player = 0;
    }

    last_interacted_time = current_time;

  } else { // If the user hasn't interacted with the display

    if ( current_time - last_interacted_time > IDLE_DELAY_MS ) {// Showing an idle animation

      // Reset display_screen flags, so that we can display them again once we exit idle screens
      displayed_computer = 0;
      displayed_player = 0;

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
/*
if current - last interacted > IDLE delay && current - lastIdleChange > 5s:
  // we are in idle, and it's time to change
  lastIdleChange = current; // next change would be 5 s from now -- if no interaction
  if idleone: set idle1 to false, idle 2 true, do snowflake whatever...
  else: set idle1 to true, idle 2 to false do whatever. 
  (make sure to have a "default", that idle 1 comes first)
*/
 
    } else { // We're not idle, but user hasn't interacted
      if (screen_select) {

        if (!displayed_player) {
          display_select_player();
          displayed_player = 1;
        }
        displayed_computer = 0;

      } else {

        if (!displayed_computer) {
          display_select_computer(comp_diff);
          displayed_computer = 1;
        }
        displayed_player = 0;

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

void display_select_player() {
  display.clearDisplay();
  display.setTextSize(1);
  // Draw bitmap
  display.setCursor(0, 0);
  display.println(F(      "PLAYER"));
  display.println(F("HUMAN  Comp>"));
  display.print(F("MODE"));
  display.display();
}

void display_select_computer(uint8_t comp_diff) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("STOCKFISH"));
  display.print(F("<Human  LV:"));
  // Draw bitmap
  display.print(comp_diff, DEC);
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
}

void setup() {
  Serial.begin(9600);
  last_interacted_time = 0;
  last_idle_change = 0;
  idle_one = 1; // Display idle screen one first by default
  idle_two = 0;
  displayed_computer = 0;
  displayed_player = 0;
  
  bool selecting_player = 0, selecting_computer = 0; // To be passed into display_idle_screen, we'll change these variables to simulate
                                                      // joystick input or whatever to switch screens

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  Serial.println("Allocation succeeded, proceeding");
  pinMode(22, INPUT_PULLUP);
  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(500); // Pause for 2 seconds

  // Code to test the functionality of display_idle_screen
  OLED_timer.start();
  Serial.println("Setup done. Entering loop");
  display.setTextColor(SSD1306_WHITE);
}

void loop() {
  button = digitalRead(22); // Digital pin 22
  display_idle_screen(OLED_timer, !button, 0, 5); // test values for screens & comp_diff
}
