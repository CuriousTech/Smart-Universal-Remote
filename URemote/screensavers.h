#ifndef SCREENSAVERS_H
#define SCREENSAVERS_H

#include <Arduino.h>
#include <TFT_eSPI.h>

#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 240

enum Saver
{
  SS_Lines,
  SS_Boing,
  SS_Count, // Last one
};

struct Line
{
  int16_t x1;
  int16_t y1;
  int16_t x2;
  int16_t y2;
};

struct Ball
{
  int16_t x;
  int16_t y;
  int16_t dx;
  int16_t dy;
  int16_t color;
};


class ScreenSavers
{
#define LINES 50
#define BALLS 8
#define BUFFER_SIZE 1024

static_assert(sizeof(Ball) * BALLS < BUFFER_SIZE, "m_buffer size too small");
static_assert(sizeof(Line) * LINES < BUFFER_SIZE, "m_buffer size too small");

public:
  ScreenSavers(){};
  void select(int n);
  void run(void);

  uint8_t m_saver;
  uint8_t m_buffer[BUFFER_SIZE];

private:
  void Lines(bool bInit);
  void Boing(bool bInit);
};


#endif // SCREENSAVERS_H
