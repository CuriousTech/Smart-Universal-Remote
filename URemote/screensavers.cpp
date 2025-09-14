#include "screensavers.h"
#include <time.h>
#include "display.h"

extern TFT_eSPI tft;
extern TFT_eSprite sprite;
extern tm gLTime;

extern void consolePrint(String s); // for browser debug

const char _dayStr[7][7] PROGMEM = {"Sun","Mon","Tues","Wednes","Thurs","Fri","Satur"};
const char _monthShortStr[12][4] PROGMEM = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
const char _dayShortStr[7][4] PROGMEM = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
const char _monthStr[12][10] PROGMEM = {"January","February","March","April","May","June","July","August","September","October","November","December"};

void ScreenSavers::init(uint16_t w, uint16_t h)
{
  m_nDisplayWidth = w;
  m_nDisplayHeight = h;
}

const char *ScreenSavers::monthStr(uint8_t m)
{
  return _monthStr[m];
}
const char *ScreenSavers::monthShortStr(uint8_t m)
{
  return _monthShortStr[m];
}
const char *ScreenSavers::dayShortStr(uint8_t m)
{
  return _dayShortStr[m];
}

String ScreenSavers::localTimeString()
{
  String sTime = String( hourFormat12(gLTime.tm_hour) );
  if(hourFormat12(gLTime.tm_hour) < 10)
    sTime = " " + sTime;
  sTime += ":";
  if(gLTime.tm_min < 10) sTime += "0";
  sTime += gLTime.tm_min;
  sTime += ":";
  if(gLTime.tm_sec < 10) sTime += "0";
  sTime += gLTime.tm_sec;
  sTime += " ";
  sTime += (gLTime.tm_hour >= 12) ? "PM":"AM";
  return sTime;
}

uint8_t ScreenSavers::hourFormat12(uint8_t h)
{
  if(h > 12) return h - 12;
  if(h == 0) h = 12;
  return h;
}

void ScreenSavers::select(int n)
{
  m_saver = n;
  randomSeed(micros());

  switch(m_saver)
  {
    case SS_Lines:
      Lines(true);
      break;
    case SS_Bubbles:
      Bubbles(true);
      break;
    case SS_Starfield:
      Starfield(true);
      break;
  }
}

void ScreenSavers::run()
{
  switch(m_saver)
  {
    case SS_Lines:
      Lines(false);
      break;
    case SS_Bubbles:
      Bubbles(false);
      break;
    case SS_Starfield:
      Starfield(false);
      break;
  }

  // battery meter
#if defined(ROUND_DISPLAY)
  int16_t v = (display.m_bCharging) ? (display.m_vadc - 1283) * 360 /  (1802 - 1283) : (display.m_vadc - 1283) * 360 /  (1550 - 1283);
  int16_t ang = constrain( v, 0, 359);

  uint16_t color = (display.m_bCharging) ? TFT_BLUE:TFT_GREEN;
  if(display.m_battPercent <= 25) color = TFT_RED; // 25% left
  else if(display.m_battPercent <= 50) color = TFT_YELLOW; // 50% left

  tft.drawArc(m_nDisplayWidth >> 1, m_nDisplayHeight >> 1, 120, 117, 0, ang, color, TFT_BLACK, false);
  if(ang < 359)
    tft.drawArc(m_nDisplayWidth >> 1, m_nDisplayHeight >> 1, 120, 117, ang, 359, TFT_BLACK, TFT_BLACK, false); // remove the rest

#else // 240x280
  const uint16_t w = DISPLAY_WIDTH - 40;

  int16_t pos = display.m_battPercent * w / 100;
  pos = constrain( pos, 0, w);

  uint16_t color = (display.m_bCharging) ? TFT_BLUE:TFT_GREEN;
  if(pos <= w>>2) color = TFT_RED; // 25% left
  else if(pos <= w>>1) color = TFT_YELLOW; // 50% left

  if(pos < w)
    tft.drawWideLine(20 + pos, 15, DISPLAY_WIDTH - 20, 15, 4, TFT_DARKGREY, TFT_BLACK);
  tft.drawWideLine(20, 15, 20 + pos, 15, 4, color, TFT_BLACK);
#endif
}

void ScreenSavers::Lines(bool bInit)
{
  Line *line = (Line *)m_buffer;
  static Line delta;
  uint16_t color;
  static int16_t rgb[3] = {40,40,40};
  static int8_t rgbd[3] = {0};
  static uint8_t idx;

  if(bInit)
  {
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
  line[next].x1 = constrain(line[idx].x1 + delta.x1, 0, m_nDisplayWidth-1); // keep it on the screen
  line[next].x2 = constrain(line[idx].x2 + delta.x2, 0, m_nDisplayWidth-1);
  line[next].y1 = constrain(line[idx].y1 + delta.y1, 0, m_nDisplayHeight-1);
  line[next].y2 = constrain(line[idx].y2 + delta.y2, 0, m_nDisplayHeight-1);

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
  if(line[next].x1 == m_nDisplayWidth-1 && delta.x1 > 0) delta.x1 = -delta.x1;
  if(line[next].x2 == m_nDisplayWidth-1 && delta.x2 > 0) delta.x2 = -delta.x2;
  if(line[next].y1 == m_nDisplayHeight-1 && delta.y1 > 0) delta.y1 = -delta.y1;
  if(line[next].y2 == m_nDisplayHeight-1 && delta.y2 > 0) delta.y2 = -delta.y2;
  idx = next;
}

void ScreenSavers::Bubbles(bool bInit)
{
  Bubble *bubble = (Bubble *)m_buffer;
  static uint8_t skipper = 4;

  if(bInit)
  {
    tft.fillScreen(TFT_BLACK);
    for(uint8_t i = 0; i < BUBBLES; i++)
    {
      resetBubble(bubble[i]);
    }
  }

  if(--skipper) // slow it down to look good
    return;
  skipper = 7;

  // Erase last bubble
  for(uint8_t i = 0; i < BUBBLES; i++)
  {
    tft.drawCircle(bubble[i].x, bubble[i].y, bubble[i].bsize, TFT_BLACK );
    if(bubble[i].bsize > random(24, 14))
    {
      resetBubble(bubble[i]);
    }
  }

  // Draw bubbles
  for(uint8_t i = 0; i < BUBBLES; i++)
  {
    bubble[i].bsize += bubble[i].is;
    bubble[i].x += bubble[i].dx;
    bubble[i].y += bubble[i].dy;
    bubble[i].x = constrain(bubble[i].x, 1, m_nDisplayWidth-1);
    bubble[i].y = constrain(bubble[i].y, 1, m_nDisplayHeight-1);
    tft.drawCircle(bubble[i].x, bubble[i].y, bubble[i].bsize, bubble[i].color );
  }
}

void ScreenSavers::resetBubble(Bubble& bubble)
{
  const uint16_t palette[] = {TFT_MAROON,TFT_PURPLE,TFT_OLIVE,TFT_LIGHTGREY,
        TFT_DARKGREY,TFT_BLUE,TFT_GREEN,TFT_CYAN,TFT_RED,TFT_MAGENTA,
        TFT_YELLOW,TFT_WHITE,TFT_ORANGE,TFT_GREENYELLOW,TFT_PINK,
        TFT_GOLD,TFT_SILVER,TFT_SKYBLUE,TFT_VIOLET};

  bubble.x = (int16_t)random(2, m_nDisplayWidth - 2);
  bubble.y = (int16_t)random(2, m_nDisplayHeight - 2);
  bubble.dx = (int8_t)random(-3, 6);
  bubble.dy = (int8_t)random(-3, 6);
  bubble.bsize = 1;
  bubble.is = random(1, 4);
  bubble.color = palette[ random(0, sizeof(palette) / sizeof(uint16_t) ) ];
}

void ScreenSavers::Starfield(bool bInit)
{
  Star *star = (Star *)m_buffer;

  if(bInit)
  {
    tft.fillScreen(TFT_BLACK);
    for(uint8_t i = 0; i < STARS; i++)
      resetStar(star[i]);
  }

  for(uint8_t i = 0; i < STARS; i++)
  {
    tft.drawPixel((uint16_t)star[i].x, (uint16_t)star[i].y, TFT_BLACK );
    star[i].x += star[i].dx;
    star[i].y += star[i].dy;
    if(star[i].z < 255) star[i].z++;

    if(star[i].x < 0 || star[i].x >= m_nDisplayWidth || star[i].y < 0 || star[i].y >= m_nDisplayHeight )
    {
      resetStar(star[i]);
    }
    tft.drawPixel((uint16_t)star[i].x, (uint16_t)star[i].y, tft.color565(star[i].z, star[i].z, star[i].z) );
  }
}

void ScreenSavers::resetStar(Star& star)
{
  star.x = m_nDisplayWidth >> 1;
  star.y = m_nDisplayHeight >> 1;
  star.dx = (float)random(-45, 90) / 100;
  star.dy = (float)random(-45, 90) / 100;
  star.z = 0;
}
