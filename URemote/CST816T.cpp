/*
   MIT License

  Copyright (c) 2021 Felix Biego

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include "Arduino.h"
#include <Wire.h>
#include <FunctionalInterrupt.h>

#include "CST816T.h"

CST816T::CST816T(int sda, int scl, int rst, int irq)
{
  _sda = sda;
  _scl = scl;
  _rst = rst;
  _irq = irq;
}

void CST816T::read_touch()
{
  uint8_t data_raw[8];
  i2c_read(REG_GESTURE_ID, data_raw, 6);

//  gestureID = data_raw[0]; // reading only 0
  event = data_raw[2] >> 6;
  down = data_raw[1];
  x = ((data_raw[2]&0xF) << 8) | data_raw[3];
  y = ((data_raw[4]&0xF) << 8) | data_raw[5];
}

void IRAM_ATTR CST816T::handleISR(void)
{
  _event_available = true;
}

void CST816T::begin(int interrupt)
{
//  Wire.setPins(_sda, _scl); // set in IMU
//  Wire.setClock(400000);
//  Wire.begin();

  pinMode(_irq, INPUT);
  pinMode(_rst, OUTPUT);

  digitalWrite(_rst, HIGH );
  delay(50);
  digitalWrite(_rst, LOW);
  delay(5);
  digitalWrite(_rst, HIGH );
  delay(50);

  i2c_read(REG_CHIP_ID, versionInfo, 3); // (0xB5 = CST816T)
  // ets_printf("CST816 Chip ID = 0x%x\n", versionInfo[0]);

  attachInterrupt(_irq, std::bind(&CST816T::handleISR, this), interrupt);

//  uint8_t dis_auto_sleep = 0xFF;
//  i2c_write(REG_DIS_AUTOSLEEP, &dis_auto_sleep);
}

bool CST816T::available()
{
  if(_event_available)
  {
    read_touch();
    _event_available = false;
    return true;
  }
  return false;
}

void CST816T::sleep()
{
  digitalWrite(_rst, LOW);
  delay(5);
  digitalWrite(_rst, HIGH );
  delay(50);
//  i2c_write(REG_SLEEP_MODE, 0x03);
}

uint8_t CST816T::i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint8_t len)
{
  Wire.beginTransmission(CST816T_ADDRESS);
  Wire.write(reg_addr);
  if ( Wire.endTransmission(true)) return false;
  Wire.requestFrom((uint8_t)CST816T_ADDRESS, (size_t)len, true);
  for (uint8_t i = 0; i < len; i++)
    *reg_data++ = Wire.read();
  return true;
}

uint8_t CST816T::i2c_write(uint8_t reg_addr, const uint8_t reg_data)
{
  Wire.beginTransmission(CST816T_ADDRESS);
  Wire.write(reg_addr);
  Wire.write(reg_data);
  if ( Wire.endTransmission(true)) return false;
  return true;
}
