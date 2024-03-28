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

#ifndef CST816T_H
#define CST816T_H

#include <Arduino.h>

#define CST816T_ADDRESS 0x15

#define REG_GESTURE_ID 0x01
#define REG_FINGER_NUM 0x02
#define REG_XPOS_H 0x03
#define REG_XPOS_L 0x04
#define REG_YPOS_H 0x05
#define REG_YPOS_L 0x06
#define REG_CHIP_ID 0xA7
#define REG_PROJ_ID 0xA8
#define REG_FW_VERSION 0xA9
#define REG_FACTORY_ID 0xAA
#define REG_SLEEP_MODE 0xE5
#define REG_IRQ_CTL 0xFA
#define REG_LONG_PRESS_TICK 0xEB
#define REG_MOTION_MASK 0xEC
#define REG_DIS_AUTOSLEEP 0xFE

#define MOTION_MASK_CONTINUOUS_LEFT_RIGHT 0b100
#define MOTION_MASK_CONTINUOUS_UP_DOWN 0b010
#define MOTION_MASK_DOUBLE_CLICK 0b001

#define IRQ_EN_TOUCH 0x40
#define IRQ_EN_CHANGE 0x20
#define IRQ_EN_MOTION 0x10
#define IRQ_EN_LONGPRESS 0x01

enum GESTURE {
  NONE = 0x00,
  SWIPE_UP = 0x01,
  SWIPE_DOWN = 0x02,
  SWIPE_LEFT = 0x03,
  SWIPE_RIGHT = 0x04,
  SINGLE_CLICK = 0x05,
  DOUBLE_CLICK = 0x0B,
  LONG_PRESS = 0x0C
};

class CST816T
{
public:
  CST816T(int sda, int scl, int rst, int irq);
  void begin(int interrupt = RISING);
  void sleep();
  bool available();

  uint16_t gestureID; // Gesture ID
  uint8_t event; // Event (0 = Down, 1 = Up, 2 = Contact)
  uint8_t x;
  uint8_t y;
  uint8_t version;
  uint8_t versionInfo[3];
  uint8_t down;

private:
  int _sda;
  int _scl;
  int _rst;
  int _irq;
  bool _event_available;

  void IRAM_ATTR handleISR();
  void read_touch();

  uint8_t i2c_read(uint8_t reg_addr, uint8_t * reg_data, uint8_t len);
  uint8_t i2c_write(uint8_t reg_addr, uint8_t reg_data);
};

#endif
