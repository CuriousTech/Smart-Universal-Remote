#ifndef SCREENSAVERS_H
#define SCREENSAVERS_H

#include <Arduino.h>
#include <TFT_eSPI.h>

#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 240

enum Saver
{
  SS_Lines,
  SS_Bubbles,
  SS_Count, // Last one
};

struct Line
{
  int16_t x1;
  int16_t y1;
  int16_t x2;
  int16_t y2;
};

struct Bubble
{
  int16_t x;
  int16_t y;
  int8_t dx;
  int8_t dy;
  int8_t is;
  int16_t size;
  int16_t color;
};


class ScreenSavers
{
#define LINES 50
#define BUBBLES 10
#define BUFFER_SIZE 1024

static_assert(sizeof(Bubble) * BUBBLES < BUFFER_SIZE, "m_buffer size too small");
static_assert(sizeof(Line) * LINES < BUFFER_SIZE, "m_buffer size too small");

public:
  ScreenSavers(){};
  void select(int n);
  void run(void);

  uint8_t m_saver;
  uint8_t m_buffer[BUFFER_SIZE];

private:
  void Lines(bool bInit);
  void Bubbles(bool bInit);
  void initBubble(Bubble& bubble);
};

#endif // SCREENSAVERS_H
