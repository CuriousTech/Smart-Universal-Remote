#include "screensavers.h"
#include <TimeLib.h>
#include "display.h"

extern TFT_eSPI tft;
extern TFT_eSprite sprite;

extern void consolePrint(String s); // for browser debug

void ScreenSavers::select(int n)
{
  m_saver = n;
  randomSeed(micros());
}

void ScreenSavers::run()
{
  switch(m_saver)
  {
    case SS_Lines:
      Lines();
      break;
    case SS_Bubbles:
      Bubbles();
      break;
    case SS_Starfield:
      Starfield();
      break;
  }

  if(display.m_bCharging)
    tft.drawCircle(DISPLAY_WIDTH/2, DISPLAY_HEIGHT/2, DISPLAY_WIDTH/2-1, TFT_BLUE);
  else
    delay(1);
}

void ScreenSavers::Lines()
{
  Line *line = (Line *)m_buffer;
  static Line delta;
  uint16_t color;
  static int16_t rgb[3] = {40,40,40};
  static int8_t rgbd[3] = {0};
  static uint8_t idx;

  static bool bInit = false;
  if(!bInit)
  {
    bInit = true;
    tft.fillScreen(TFT_BLACK);
    memset(line, 0, sizeof(Line) * LINES);
    memset(&delta, 1, sizeof(delta));
    idx = 0;
  }

  static int8_t dly = 0;
  if(--dly <= 0)
  {
    dly = 25; // every 25 runs
    int16_t *pd = (int16_t*)&delta;
    for(uint8_t i = 0; i < 4; i++)
      pd[i] = constrain(pd[i] + (int16_t)random(-1,2), -4, 4); // random direction delta

    for(uint8_t i = 0; i < 3; i++)
    {
      rgbd[i] += (int8_t)random(-1, 2);
      rgbd[i] = (int8_t)constrain(rgbd[i], -3, 6);
    }
  }

  uint8_t next = (idx + 1) % LINES;

  tft.drawLine(line[next].x1, line[next].y1, line[next].x2, line[next].y2, TFT_BLACK);

  // add delta to positions
  line[next].x1 = constrain(line[idx].x1 + delta.x1, 0, DISPLAY_WIDTH-1); // keep it on the screen
  line[next].x2 = constrain(line[idx].x2 + delta.x2, 0, DISPLAY_WIDTH-1);
  line[next].y1 = constrain(line[idx].y1 + delta.y1, 0, DISPLAY_HEIGHT-1);
  line[next].y2 = constrain(line[idx].y2 + delta.y2, 0, DISPLAY_HEIGHT-1);

  for(uint8_t i = 0; i < 3; i++)
  {
    rgb[i] += rgbd[i];
    rgb[i] = (int16_t)constrain(rgb[i], 1, 255);
  }

  color = tft.color565(rgb[0], rgb[1], rgb[2]);

  // Draw new line
  tft.drawLine(line[next].x1, line[next].y1, line[next].x2, line[next].y2, color);

  if(line[next].x1 == 0 && delta.x1 < 0) delta.x1 = -delta.x1; // bounce off edges
  if(line[next].x2 == 0 && delta.x2 < 0) delta.x2 = -delta.x2;
  if(line[next].y1 == 0 && delta.y1 < 0) delta.y1 = -delta.y1;
  if(line[next].y2 == 0 && delta.y2 < 0) delta.y2 = -delta.y2;
  if(line[next].x1 == DISPLAY_WIDTH-1 && delta.x1 > 0) delta.x1 = -delta.x1;
  if(line[next].x2 == DISPLAY_WIDTH-1 && delta.x2 > 0) delta.x2 = -delta.x2;
  if(line[next].y1 == DISPLAY_HEIGHT-1 && delta.y1 > 0) delta.y1 = -delta.y1;
  if(line[next].y2 == DISPLAY_HEIGHT-1 && delta.y2 > 0) delta.y2 = -delta.y2;
  idx = next;
}

void ScreenSavers::Bubbles()
{
  Bubble *bubble = (Bubble *)m_buffer;
  static uint8_t skipper = 4;

  static bool bInit = false;
  if(!bInit)
  {
    bInit = true;
    tft.fillScreen(TFT_BLACK);
    for(uint8_t i = 0; i < BUBBLES; i++)
    {
      resetBubble(bubble[i]);
    }
  }

  if(--skipper) // slow it down by x*loop delay of 1ms
    return;
  skipper = 16;

  // Erase last bubble
  for(uint8_t i = 0; i < BUBBLES; i++)
  {
    tft.drawCircle(bubble[i].x, bubble[i].y, bubble[i].size, TFT_BLACK );
    if(bubble[i].size > random(24, 14))
    {
      resetBubble(bubble[i]);
    }
  }

  // Draw bubbles
  for(uint8_t i = 0; i < BUBBLES; i++)
  {
    bubble[i].size += bubble[i].is;
    bubble[i].x += bubble[i].dx;
    bubble[i].y += bubble[i].dy;
    bubble[i].x = constrain(bubble[i].x, 1, DISPLAY_WIDTH-1);
    bubble[i].y = constrain(bubble[i].y, 1, DISPLAY_HEIGHT-1);
    tft.drawCircle(bubble[i].x, bubble[i].y, bubble[i].size, bubble[i].color );
  }
}

void ScreenSavers::resetBubble(Bubble& bubble)
{
  const uint16_t palette[] = {TFT_MAROON,TFT_PURPLE,TFT_OLIVE,TFT_LIGHTGREY,
        TFT_DARKGREY,TFT_BLUE,TFT_GREEN,TFT_CYAN,TFT_RED,TFT_MAGENTA,
        TFT_YELLOW,TFT_WHITE,TFT_ORANGE,TFT_GREENYELLOW,TFT_PINK,
        TFT_GOLD,TFT_SILVER,TFT_SKYBLUE,TFT_VIOLET};

  bubble.x = (int16_t)random(2, DISPLAY_WIDTH - 2);
  bubble.y = (int16_t)random(2, DISPLAY_WIDTH - 2);
  bubble.dx = (int8_t)random(-3, 6);
  bubble.dy = (int8_t)random(-3, 6);
  bubble.size = 1;
  bubble.is = random(1, 4);
  bubble.color = palette[ random(0, sizeof(palette) / sizeof(uint16_t) ) ];
}

void ScreenSavers::Starfield()
{
  Star *star = (Star *)m_buffer;

  static bool bInit = false;
  if(!bInit)
  {
    bInit = true;
    tft.fillScreen(TFT_BLACK);
    for(uint8_t i = 0; i < STARS; i++)
      resetStar(star[i]);
  }

  for(uint8_t i = 0; i < STARS; i++)
  {
    tft.drawPixel((uint8_t)star[i].x, (uint8_t)star[i].y, TFT_BLACK );
    star[i].x += star[i].dx;
    star[i].y += star[i].dy;
    if(star[i].z < 255) star[i].z++;

    if(star[i].x < 0 || star[i].x >= DISPLAY_WIDTH || star[i].y < 0 || star[i].y >= DISPLAY_HEIGHT )
    {
      resetStar(star[i]);
    }
    tft.drawPixel((uint8_t)star[i].x, (uint8_t)star[i].y, tft.color565(star[i].z, star[i].z, star[i].z) );
  }
}

void ScreenSavers::resetStar(Star& star)
{
  star.x = DISPLAY_WIDTH/2;
  star.y = DISPLAY_HEIGHT/2;
  star.dx = (float)random(-45, 90) / 100;
  star.dy = (float)random(-45, 90) / 100;
  star.z = 0;
}
