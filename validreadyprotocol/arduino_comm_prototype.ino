

/*
In setup
- First define port direction
- Set all port output to low
initiliazed=false...
- while "not initialized":
    - Read
    - see if read is 01010101010.... or 1010101010...
    If so, initialized=true else repeat.
    If initiliazed: overwrite = true; // to tell that I've received your
message!

// For testing purposes:
- Send: 100000000...
Send 110000000.... (basically setting both players to computer)

Send 000000000 (tell computer to start white move)
start_loop:
Read:
print it out... (convert to a move first...)
Write: basically write the read again with 00 in front (no promotion...
assuming) Repeat the read/write part again and again...

*/

// Pin definitions
const int clk = 13;     // Clock signal (output)
const int RValid = 12;  // Valid signal (input)
const int RReady = 11;  // Ready signal (output)
const int RData = 10;   // Data signal (input)
const int WValid = 12;  // Valid signal (output)
const int WReady = 11;  // Ready signal (input)
const int WData = 10;   // Data signal (output)
const int OVERWRITE = 0;
const int clock_half_period = 10;

int received_data = 0;

void setup() {
  pinMode(clk, OUTPUT);
  pinMode(RValid, INPUT);
  pinMode(RReady, OUTPUT);
  pinMode(RData, INPUT);
  pinMode(WValid, OUTPUT);
  pinMode(WReady, INPUT);
  pinMode(WData, OUTPUT);
  pinMode(OVERWRITE, OUTPUT);

  digitalWrite(clk, LOW);
  digitalWrite(RReady, LOW);
  Serial.begin(9600);
  digitalWrite(WValid, LOW);
  digitalWrite(WData, LOW);
  digitalWrite(OVERWRITE, LOW);
  bool initialized = false;

  while (!initialized) {
    received_data = read();
    if (received_data != 0b1010101010101010 ||
        received_data != 0b01010101010101)
      initialized = true;
    digitalWrite(OVERWRITE, HIGH);
  }
  write(0,   // writing_all_zeros
        1,   // is programming
        0,   // programming colour
        0,   // is human
        20,  // difficulty
        0,   // from square
        0,   // to square
        0,   // is promotion
        0);  // promotion square
  write(0,   // writing_all_zeros
        1,   // is programming
        1,   // programming colour
        0,   // is human
        20,  // difficulty
        0,   // from square
        0,   // to square
        0,   // is promotion
        0);  // promotion square

  // send an all zeros to tell white to start moving
  write(1,   // writing_all_zeros
        1,   // is programming
        0,   // programming colour
        0,   // is human
        20,  // difficulty
        0,   // from square
        0,   // to square
        0,   // is promotion
        0);  // promotion square
}

void loop() {
  received_data = read();  // this is white's first move.
  // process the received_data
  /*
  get a "from square", "to square", "promotion"
  */
  // print the move
  // send the same move back to pi.
  write(0,                 // writing_all_zeros
        0,                 // is programming
        0,                 // programming colour
        0,                 // is human
        20,                // difficulty
        from_square,       // from square
        to_square,         // to square
        0,                 // is promotion
        promotion_piece);  // promotion square
}

int read() {
  // State variables
  int receiving = 0;
  int i = 0;

  // Begin with not ready
  digitalWrite(clk, LOW);
  digitalWrite(RReady, LOW);

  // Rising clock - do one cycle for fun
  delay(clock_half_period);
  digitalWrite(clk, HIGH);
  delay(clock_half_period);
  digitalWrite(clk, LOW);

  // Set ready
  digitalWrite(RReady, HIGH);  // Indicate ready to receive
  delay(clock_half_period);
  int receivedData[14];
  int current_data;

  // Loop:
  while (1) {
    // Read data and valid status before edge
    current_data = digitalRead(RData);
    if (!receiving) receiving = digitalRead(RValid);

    // Rising edge and falling edge
    digitalWrite(clk, HIGH);
    delay(clock_half_period);
    digitalWrite(clk, LOW);

    // load in data
    if (receiving) {
      digitalWrite(RReady, LOW);
      receivedData[i++] = current_data;
    }
    if (i >= 14) {
      break;
    }
  }

  Serial.println("Data read from Raspberry Pi.");
  int num_received = 0;
  for (int j = 0; j < 14; j++) {
    Serial.print(receivedData[j]);
    Serial.print(" ");
    num_received += receivedData[j] << j;
  }
  Serial.print("\t");
  Serial.println(num_received);
  int result = 0;
  for (int i = 0; i < 14; i++) {
    result = result + (2 * receivedData[i]);
  }
  return result;
}

int pi_return_to_from_square(int value) {
  // return the from square value
  return value >> 8;
}
int pi_return_to_to_square(int value) {
  // return the to square value
  return (value >> 2) && 0b111111;
}
int pi_return_to_promotion(int value) {
  // return the promotion value
  return value && 0b11;
}

int write(bool writing_all_zeros, int is_programming, int programming_colour,
          bool is_human, int programming_difficulty, int from_square,
          int to_square, bool is_promotion,
          int promotion_piece) {  // could change argument to: "Is programming"
                                  // "programming_colour"
                                  // "programming_difficulty" "from square" "to
                                  // square" "if promotion" "promotion piece"
  // 010000000000000 // a promotion is happening and we are promoting to queen.
  // 000000000... // No promotion hapening
  // 0100000000000011 // a promotion is happening, and we promoting to a knight.
  int transmitting = 0;
  int i = 0;
  int temp_difficulty = programming_difficulty;
  int temp_from = from_square;
  int temp_to = to_square;
  int datas[16] = {0};  // Example data, all zeros
  if (!writing_all_zeros) {
    if (is_programming) {
      datas[0] = 1;
      datas[1] = programming_colour;
      for (i = 2; i < 16; i++) {
        datas[i] = temp_difficulty & 1 | is_human;
        temp_difficulty = temp_difficulty >> 1;
      }
    } else {
      datas[0] = 0;
      datas[1] = is_promotion;
      for (i = 2; i < 8; i++) {
        datas[i] = temp_from & 1;
        temp_from = temp_from >> 1;
      }
      for (i = 8; i < 14; i++) {
        datas[i] = temp_to & 1;
        temp_to = temp_to >> 1;
      }
      datas[14] = promotion_piece & 1;
      datas[15] = promotion_piece >> 1;
    }
  }

  digitalWrite(clk, LOW);
  digitalWrite(WValid, LOW);

  delay(clock_half_period);
  digitalWrite(clk, HIGH);
  delay(clock_half_period);
  digitalWrite(clk, LOW);

  digitalWrite(WValid, HIGH);  // Indicate data is valid
  digitalWrite(WData, datas[i]);
  delay(clock_half_period);

  while (true) {
    if (!transmitting) {
      transmitting = digitalRead(WReady);
    }  // Set valid back to zero
    Serial.print("WREADY: ");
    Serial.println(digitalRead(WReady));
    digitalWrite(clk, HIGH);
    delay(clock_half_period);
    digitalWrite(clk, LOW);
    if (transmitting) {
      digitalWrite(WValid, LOW);
      i++;
      if (i >= 14) {
        break;
      }
      digitalWrite(WData, datas[i]);
    }
    delay(clock_half_period);
  }
  Serial.println("Data sent to Raspberry Pi.");
  for (int j = 0; j < 14; j++) {
    Serial.print(datas[j]);
    Serial.print(" ");
  }
  Serial.println("");
  return 0;
}
