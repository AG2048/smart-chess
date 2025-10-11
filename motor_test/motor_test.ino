#include <Wire.h>

#define SUBORDINATE_ADDR 8

void setup() {
  Serial.begin(9600);
  Wire.begin();
}

void loop() {
    delay(3000);
    motor_i2c(0, 0, 0, 0, 0);
    delay(100000);
}

void motor_i2c(int8_t x0, int8_t y0, int8_t x1, int8_t y1, uint8_t motor_mode) { // y: [0, 7], x: [-3, 10], motor_mode -> [0:n/a, 1:taxicab, 2:calibrate]
  Wire.beginTransmission(SUBORDINATE_ADDR);
  Wire.write((y0 << 4) | (x0 + 3)); // +3 to shift x into positive range
  Wire.write((y1 << 4) | (x1 + 3));
  Wire.write(motor_mode);
  Wire.endTransmission();

  while (1) {
    delay(100);
    Wire.requestFrom(SUBORDINATE_ADDR, 1);
    if (Wire.available() != 0) {
      uint8_t status = Wire.read();
      if (status == 0x96) break;
    }
  }
}