#include <FastLED.h> 

#define stripLen 10
int isPressed [5] {0};
int selectedCounter  = 0;
const int ROWS = 2;
const int COLUMNS = 5;
int selected_position_x = 0;
int selected_position_y = 0;
int position_x = 0;
int position_y = 0;

struct CRGB leds[stripLen];

void setup(){
  LEDS.addLeds<WS2812B, 4, GRB>(leds, stripLen);
  FastLED.setBrightness(50);
  Serial.begin(9600);
  pinMode(8, INPUT_PULLUP); //controls +y
  pinMode(9, INPUT_PULLUP); //controls -y
  pinMode(10, INPUT_PULLUP); //controls +x
  pinMode(11, INPUT_PULLUP); //controls -x
  pinMode(12, INPUT_PULLUP);
}

void fill(int i, int j, int colour_1, int colour_2, int colour_3)
{
  for(i = 0; i < j; i++)
  {
    leds[i] = CRGB(colour_1, colour_2, colour_3);
  }
}

bool isEven(int x)
{
  if(x%2 == 0)
  {
    return true;
  }
  else
  {
    return false;
  }
}

int coordinate_to_index(int x, int y)
{
  if(isEven(y))
  {
    return y * COLUMNS + x;
  }
  else
  {
    return y * COLUMNS + COLUMNS - x - 1;
  }
}

void loop()
{
  fill(0, stripLen, 0, 0, 0);
  
  //up +y
  if (digitalRead(8)) {
    isPressed[0] = 1;
  }

  if (!digitalRead(8) && isPressed[0]) {
    if(position_y < ROWS - 1)
    {
      position_y++;
    }
    isPressed[0] = 0;
  }

  //down -y 
  if (digitalRead(9)) {
    isPressed[1] = 1;
  }

  if (!digitalRead(9) && isPressed[1]) {
    
    if(position_y > 0)
    {
      position_y--;
      // leds[3] = CRGB(0,255, 0);
    }
    isPressed[1] = 0;
  }

  // right +x
  if (digitalRead(10)) {
    isPressed[2] = 1;
  }

  if (!digitalRead(10) && isPressed[2]) {
    if(position_x < COLUMNS - 1)
    {
      position_x++;
    }
    isPressed[2] = 0;
  }

  //left -x
  if (digitalRead(11)) {
    isPressed[3] = 1;
  }

  if (!digitalRead(11) && isPressed[3]) {
  
    if(position_x > 0)
    {
      position_x--;
    }
    isPressed[3] = 0;
  }


  if(selectedCounter == 1)
  { 
    if(selected_position_x == position_x && selected_position_y == position_y)
    {
      leds[coordinate_to_index(selected_position_x,selected_position_y)] = CRGB(0,255,0);
    }
    else
    {
      leds[coordinate_to_index(selected_position_x,selected_position_y)] = CRGB(255,0,0);
      leds[coordinate_to_index(position_x,position_y)] = CRGB(0,255,0);
    }

    if(digitalRead(12))
    {
      isPressed[4] = 1;
    }

    if(!digitalRead(12) && isPressed[4])
    {
      selected_position_x = position_x;
      selected_position_y = position_y;
      isPressed[4] = 0;
      selectedCounter--;
    }

  }
  else if(selectedCounter == 0)
  {
    selected_position_x = position_x;
    selected_position_y = position_y;
    leds[coordinate_to_index(position_x,position_y)] = CRGB(255, 0, 0);
  
    if(digitalRead(12))
    {
      isPressed[4] = 1;
    }

    if(!digitalRead(12) && isPressed[4])
    {
      isPressed[4] = 0;
      selectedCounter++;
    }
  }


  
  delay(100);
  FastLED.show();

  

}