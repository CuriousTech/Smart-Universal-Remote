#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <TFT_eSPI.h> // TFT_eSPI library from library manager?
#include "fonts.h"

#if (USER_SETUP_ID==303) // 240x320 2.8"
#define USE_SDCARD
#endif

#if (USER_SETUP_ID==302) // 240x240
#define ROUND_DISPLAY
#endif

// from 5-6-5 to 16 bit value (max 31, 63, 31)
#define rgb16(r,g,b) ( ((uint16_t)r << 11) | ((uint16_t)g << 5) | (uint16_t)b )

#define DISPLAY_WIDTH TFT_WIDTH
#define DISPLAY_HEIGHT TFT_HEIGHT

// todo: quick fix
#ifndef _IR_PROTOCOL_H
typedef enum {
    UNKNOWN = 0,
    PULSE_WIDTH,
    PULSE_DISTANCE,
    APPLE,
    DENON,
    JVC,
    LG,
    LG2,
    NEC,
    NEC2, /* NEC with full frame as repeat */
    ONKYO,
    PANASONIC,
    KASEIKYO,
    KASEIKYO_DENON,
    KASEIKYO_SHARP,
    KASEIKYO_JVC,
    KASEIKYO_MITSUBISHI,
    RC5,
    RC6,
    SAMSUNG,
    SAMSUNG48,
    SAMSUNG_LG,
    SHARP,
    SONY,
    /* Now the exotic protocols */
    BANG_OLUFSEN,
    BOSEWAVE,
    LEGO_PF,
    MAGIQUEST,
    WHYNTER,
    FAST
} decode_type_t;
#endif

enum Button_Function
{
  BTF_None,
  BTF_LightsDimmer,
  BTF_PCVolume,
  BTF_BT,
  BTF_Brightness,

  BTF_IR,
  BTF_BLE,
  BTF_Lights,
  BTF_WIFI_ONOFF,
  BTF_BT_ONOFF,
  BTF_Clear,
  BTF_PC_Media,
  BTF_Macro,
  BTF_RSSI,
  BTF_Time,
  BTF_DOW,
  BTF_Date,
  BTF_Volts,
  BTF_BattPerc,
  BTF_Temp,
  BTF_StatCmd, // stat commands
  BTF_Stat_Temp,
  BTF_Stat_SetTemp,
  BTF_Stat_Fan,
  BTF_Stat_OutTemp,
  BTF_GdoDoor,
  BTF_GdoCar,
  BTF_GdoCmd,
  BTF_Restart,
  BTF_TrackBtn,
  BTF_PowerOff,
};

// Button flags
#define BF_BORDER     (1 << 0) // Just a text time, not button
#define BF_REPEAT     (1 << 1) // repeatable (hold)
#define BF_STATE_2    (1 << 2) // tri-state
#define BF_FIXED_POS  (1 << 3)
#define BF_TEXT       (1<< 4)
#define BF_SLIDER_H   (1<< 5)
#define BF_SLIDER_V   (1<< 6)
#define BF_ARROW_UP   (1<< 7)
#define BF_ARROW_DOWN (1<< 8)
#define BF_ARROW_LEFT (1<< 9)
#define BF_ARROW_RIGHT (1<< 10)

// Tile extras
#define EX_NONE  (0)
#define EX_CLOCK  (1 << 1)
#define EX_NOTIF  (1 << 2)
#define EX_SCROLL (1 << 3)

#define SFL_REVERSE (1<<0)

struct Rect{
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
};

struct ArcSlider
{
  uint8_t  nFunc;       // see slider_func
  uint8_t  flags;       // see SFL_xx
  uint16_t nPos;        // angle from 12 o'clock
  uint16_t nSize;       // Size in degrees
  const char *pszText;  // optional text
  const GFXfont *pFont; // button text font (NULL = default)
  uint16_t textColor;   // text color (0 = default for now)  
  uint8_t  nValue;      // 0-100 value of screen's slider
  uint8_t pad;
};

struct Button
{
  uint16_t row;        // Used with w to calculate x positions
  uint16_t flags;      // see BF_ flags
  uint16_t nFunction;  // see enum Button_Function
  const char *pszText; // Button text
  const GFXfont *pFont; // button text font (NULL = default)
  uint16_t textColor;   // text color (0 = default for now)
  const unsigned short *pIcon[2]; // Normal, pressed icons
  Rect r;             // Leave x,y 0 if not fixed, w,h calculated if 0, or set icon size here
  uint16_t data[4];   // codes for IR, BT HID, etc (data[1]=slider value)
};

#define SLIDER_CNT 2 // increase for more arc sliders per tile

struct Tile
{
  const char *pszTitle;    // Top text
  uint8_t nRow;
  uint8_t Extras;           // See EX_ flags
  ArcSlider slider[SLIDER_CNT];
  int16_t nScrollIndex;     // page scrolling top offset
  uint16_t nPageHeight;     // height for page scrolling
  const unsigned short *pBGImage; // background image for screen
#define BUTTON_CNT 20
  Button button[BUTTON_CNT];        // all the buttons
};

class Display
{
public:
  Display()
  {
  }
  void init(void);
  void oneSec(void);
  void service(void);
  void exitScreensaver(void);
  void notify(char *pszNote);
  void setSliderValue(uint8_t st, int8_t iValue);
  void RingIndicator(uint8_t n);

private:
  void swipeCheck(int16_t dX, int16_t dY);
  void checkButtonHit(int16_t touchXstart, int16_t touchYstart);
  void startSwipe(void);
  void swipeTile(int16_t dX, int16_t dY);
  void drawSwipeTiles(void);
  void snapTile(void);
  void swipeBump(void);
  void drawTile(int8_t nTile, bool bFirst, int16_t x, int16_t y);
  bool scrollPage(uint8_t nTile, int16_t nDelta);
  void formatButtons(Tile& pTile);
  void drawButton(Tile& pTile, Button *pBtn, bool bPressed, int16_t x, int16_t y, bool bInit);
  void buttonCmd(Button *pBtn, bool bRepeat);
  void sliderCmd(uint8_t nFunc, uint8_t nNewVal);
  void dimmer(void);
  void drawArcSlider(ArcSlider& slider, uint8_t newValue, int16_t x, int16_t y);
  void btnRSSI(Button *pBtn, int16_t x, int16_t y);
  void drawNotifs(Tile& pTile, int16_t x, int16_t y);
  void drawClock(int16_t x, int16_t y);
  bool arcSliderHit(ArcSlider& slider, uint8_t& value);
  void drawText(String s, int16_t x, int16_t y, int16_t angle, uint16_t cFg, uint16_t cBg, const GFXfont *pFont );
  void drawArcText(String s, int16_t x, int16_t y, const GFXfont *pFont, uint16_t color, float angle, int8_t distance);
  void cspoint(int16_t &x2, int16_t &y2, int16_t x, int16_t y, float angle, float size);

  void startSleep(void);
  void endSleep(void);
  bool snooze(uint32_t ms);
  void battRead(void);

  void IRAM_ATTR handleISR();
  bool m_intTriggered;

#define TILES 10
/*
Tile layout
[0]
[1][x][x][x]
[2][x][x]
[3]
[4]
*/
  Tile m_tile[TILES] =
  {
    // tile 0 (top/pull-down tile / startup / clock)
    {
      "",
      0, // row 0
      EX_CLOCK,
      {{BTF_None},{BTF_None}},
      0,
      0,
      0,//watchFace,
      {
        { 0, BF_FIXED_POS|BF_TEXT, BTF_Time, "12:00:00 AM", NULL, 0, {0}, {DISPLAY_WIDTH/2-56, DISPLAY_HEIGHT/4, 120, 32}},
        { 0, BF_FIXED_POS|BF_TEXT, BTF_Date,  "Jan 01", NULL, 0, {0}, {DISPLAY_WIDTH/2-40, DISPLAY_HEIGHT-DISPLAY_HEIGHT/3, 80, 32} },
        { 0, BF_FIXED_POS|BF_TEXT, BTF_DOW,   "Sun", NULL, 0, {0}, {170, DISPLAY_HEIGHT/2 - 15, 40, 32} },
        { 0, BF_FIXED_POS|BF_TEXT, BTF_Volts, "4.20", NULL, 0, {0}, {28, DISPLAY_HEIGHT/2 - 15, 50, 32} },
#if defined(ROUND_DISPLAY)
        { 0, BF_FIXED_POS, BTF_RSSI, "", NULL, 0, {0}, {DISPLAY_WIDTH/2 - 26/2, DISPLAY_HEIGHT - 35, 26, 26}},
#else
        { 0, BF_FIXED_POS, BTF_RSSI, "", NULL, 0, {0}, {4, DISPLAY_HEIGHT - 35, 26, 26}}, // bottom left corner
#endif
        {0xFF}
      }
    },
    // tile 1 (left-most horizontal tile)
    {
      "Power",
      1, // row 1
      EX_NONE,
      {{BTF_None},{BTF_None}},
      0,
      0,
      NULL,
      {// r, F, fn,        text,   font, color, icons, {x, y, w, h}, {addr, code}
        { 0, 0, BTF_IR,    "TV",    NULL, 0,  {0}, {0,0, 48, 30}, {SAMSUNG, 0x7,0xE6, 3}},
        { 0, 0, BTF_IR,    "AMP",  NULL, 0,   {0}, {0,0, 48, 30}, {0, 0, 0x34}},
        { 0, 0, BTF_IR,     "DVD",   NULL, 0,  {0}, {0,0, 48, 30}, {0, 0, 0x34}},
        { 1, 0, BTF_Lights, "Light", NULL, 0,  {0}, {0,0, 48, 30}, {0}}, // LivingRoom ID = 1
        { 1, 0, BTF_Lights, "Switch", NULL, 0, {0}, {0,0, 48, 30}, {0}}, // Other switch ID = 2
        {0xFF}
      },
    },
    // tile 2
    {
      "TV",
      1,
      EX_NONE,
      {{BTF_None},{BTF_None}},
      0,
      0,
      NULL,
      {
        { 0, 0, BTF_IR, "1", NULL, 0, {0}, {0,0, 48, 32}, {SAMSUNG,0x7,4, 3}},
        { 0, 0, BTF_IR, "2", NULL, 0, {0}, {0,0, 48, 32}, {SAMSUNG,0x7,5,3}},
        { 0, 0, BTF_IR, "3", NULL, 0, {0}, {0,0, 48, 32}, {SAMSUNG,0x7,6,3}},
        { 0, BF_REPEAT|BF_ARROW_UP, BTF_IR, NULL, NULL, 0, {0}, {0,0, 32, 32}, {SAMSUNG, 0x7,7, 3}},
        { 1, 0, BTF_IR,"4", NULL, 0, {0}, {0,0, 48, 32}, {SAMSUNG,0x7,8,3}},
        { 1, 0, BTF_IR,"5", NULL, 0, {0}, {0,0, 48, 32}, {SAMSUNG,0x7,9,3}},
        { 1, 0, BTF_IR,"6", NULL, 0, {0}, {0,0, 48, 32}, {SAMSUNG,0x7,10,3}},
        { 1, BF_REPEAT|BF_ARROW_DOWN,BTF_IR,  NULL, NULL, 0, {0}, {0,0, 32, 32}, {SAMSUNG, 0x7,0xE6,3}},
        { 2, 0, BTF_IR,"7", NULL, 0, {0}, {0,0, 48, 32}, {SAMSUNG,0x7,11,3}},
        { 2, 0, BTF_IR,"8", NULL, 0, {0}, {0,0, 48, 32}, {SAMSUNG,0x7,12,3}},
        { 2, 0, BTF_IR,"9", NULL, 0, {0}, {0,0, 48, 32}, {SAMSUNG,0x7,13,3}},
        { 2, BF_REPEAT|BF_ARROW_UP, BTF_IR, NULL, NULL, 0, {0}, {0,0, 32, 32}, {SAMSUNG, 0x7,0x61,3}},
        { 3, 0, BTF_IR,"H", NULL, 0, {0}, {0,0, 48, 32}, {SAMSUNG,0x7,0x79,3}},
        { 3, 0, BTF_IR,"0", NULL, 0, {0}, {0,0, 48, 32}, {SAMSUNG,0x7,0x11,3}},
        { 3, 0, BTF_IR,"<", NULL, 0, {0}, {0,0, 48, 32}, {SAMSUNG,0x7,0x13,3}},
        { 3, BF_REPEAT|BF_ARROW_DOWN, BTF_IR, NULL, NULL, 0, {0}, {0,0, 32, 32}, {SAMSUNG, 0x7,11,3}},
        {0xFF}
      }
    },
    //
    {
      "PC",
      1,
      EX_NONE,
#if defined(ROUND_DISPLAY) // round, use arc slider
      {{BTF_PCVolume, 0, 90, 90, "Volume", &FreeSans7pt7b, TFT_CYAN},{BTF_None}},
#else
      {{BTF_None},{BTF_None}},
#endif
      0,
      0,
      NULL,
      {
        { 0, BF_REPEAT|BF_ARROW_LEFT, BTF_PC_Media, NULL,  NULL, 0, {0}, {0,0, 32, 32}, {3,0}},
        { 0, 0, BTF_PC_Media,       "Play", NULL, 0, {0}, {0,0, 60, 32}, {0,0}},
        { 0, BF_REPEAT|BF_ARROW_RIGHT, BTF_PC_Media, NULL, NULL, 0, {0}, {0,0, 32, 32}, {2,0}},
        { 1, 0, BTF_PC_Media,        "STOP", NULL, 0, {0}, {0,0, 60, 32}, {1,0}},
        { 2, BF_SLIDER_H, BTF_PC_Media, "", NULL, 0, {0}, {0,0, 120, 32}, {1001,0}},
        { 3, 0, BTF_PC_Media,        "Mute", NULL, 0, {0}, {0,0, 60, 32}, {4,0}},
#if !defined(ROUND_DISPLAY)
        { 4, BF_SLIDER_V|BF_FIXED_POS, BTF_PCVolume, "Volume", &FreeSans7pt7b, TFT_CYAN, {0}, {DISPLAY_WIDTH-30, 40, 20, DISPLAY_HEIGHT - 80} },
#endif
        {0xFF}
      }
    },
    //
    {
      "Lights",
      1,
      EX_SCROLL,
#if defined(ROUND_DISPLAY)
      {{BTF_Lights, SFL_REVERSE, 270, 90, "Brightness", &FreeSans7pt7b, TFT_CYAN},{BTF_None}},
#else
      {{BTF_None},{BTF_None}},
#endif
      0,
      0,
      NULL,
      {
#if !defined(ROUND_DISPLAY)
        { 0, BF_SLIDER_V|BF_FIXED_POS, BTF_Lights, "Brightness", &FreeSans7pt7b, TFT_CYAN, {0}, {10, 40, 20, DISPLAY_HEIGHT - 80}},
#endif
        {0xFF}
      }
    },
    //
    {
      "Thermostat",
      2, // row 2
      EX_NONE,
      {{BTF_None},{BTF_None}},
      0,
      0,
      NULL,
      {
        { 0, BF_TEXT, 0, "Out:", NULL, 0, {0}, {0,0, 0, 32}},
        { 0, BF_BORDER|BF_TEXT, BTF_Stat_OutTemp, "",  NULL, 0, {0}, {0,0, 60, 32}, {1,0}},
        { 0, BF_TEXT, 0, "", NULL, 0,  {0}, {0,0, 32, 32}}, // spacer
        { 1, BF_TEXT, 0, " In:", NULL, 0, {0}, {0,0, 38, 32}, {1,0}},
        { 1, BF_BORDER|BF_TEXT, BTF_Stat_Temp, "",  NULL, 0, {0}, {0,0, 60, 32}, {1,0}},
        { 1, BF_REPEAT|BF_ARROW_UP, BTF_StatCmd, NULL, NULL, 0, {0}, {0,0, 32, 32}, {0}},
        { 2, BF_TEXT, 0, "Set:", NULL, 0, {0}, {0,0, 0, 32}, {1,0}},
        { 2, BF_BORDER|BF_TEXT, BTF_Stat_SetTemp, "", NULL, 0,  {0}, {0,0, 60, 32}, {1,0}},
        { 2, BF_REPEAT|BF_ARROW_DOWN, BTF_StatCmd, NULL, NULL, 0, {0}, {0,0, 32, 32}, {1}},
        { 3, 0, BTF_Stat_Fan, "Fan", NULL, 0, {0, 0}, {0,0, 0, 32}, {2}},
        {0xFF}
      }
    },
    //
    {
      "Garage Door",
      2, // row 2
      EX_NONE,
      {{BTF_None},{BTF_None}},
      0,
      0,
      NULL,
      {
        { 0, BF_BORDER|BF_TEXT, BTF_GdoDoor, "", NULL, 0,  {0}, {0,0, 98, 32}},
        { 1, BF_BORDER|BF_TEXT, BTF_GdoCar, "", NULL, 0,  {0}, {0,0, 98, 32}},
        { 2, 0, BTF_GdoCmd, "Open", NULL, 0, {0, 0}, {0,0, 98, 32}, {0}},
        {0xFF}
      }
    },
    // Pull-up tile (last)
    {
      "Settings",
      3, // row 3
      EX_NONE,
#if defined(ROUND_DISPLAY)
      {{BTF_Brightness, SFL_REVERSE, 270, 90, "Brightness", &FreeSans7pt7b, TFT_CYAN},{BTF_None}},
#else
      {{BTF_None},{BTF_None}},
#endif
      0,
      0,
      NULL,
      {
        { 0, 0, BTF_WIFI_ONOFF, "WiFi On",   NULL, 0,  {0},  {0,0, 112, 28}},
        { 1, 0, BTF_BT_ONOFF, "Bluetooth On", NULL, 0, {0},  {0,0, 112, 28}},
        { 2, 0, BTF_Restart, "Restart", NULL, 0, {0},  {0,0, 112, 28}},
#if defined(ROUND_DISPLAY)
        { 3, BF_FIXED_POS, BTF_RSSI, "", NULL, 0, {0}, {DISPLAY_WIDTH/2 - 26/2, DISPLAY_HEIGHT - 35, 26, 26}},
#else
        { 3, 0, BTF_PowerOff, "Power Off", NULL, 0, {0},  {0,0, 112, 28}},
        { 4, BF_SLIDER_V|BF_FIXED_POS, BTF_Brightness, "Brightness", &FreeSans7pt7b, TFT_CYAN, {0}, {10, 40, 20, DISPLAY_HEIGHT - 80}},
        { 5, BF_FIXED_POS, BTF_RSSI, "", NULL, 0, {0}, {DISPLAY_WIDTH - 26, DISPLAY_HEIGHT - 26, 26, 26}},
#endif
        {0xFF}
      }
    },
    // Notification tile (very last)
    {
      "Notifications",
      4,
      EX_NOTIF | EX_SCROLL,
      {{BTF_None},{BTF_None}},
      0,
      0,
      NULL,
      {
        { 0, BF_FIXED_POS, BTF_Clear, "Clear", NULL, 0, {0}, {0, DISPLAY_HEIGHT-24, 180, 24} }, // force y
        {0xFF}
      }
    },
  };

  uint16_t m_backlightTimer = 90; // backlight/screensaver timer, seconds
  uint8_t  m_bright; // current brightness
  uint8_t  m_nTileCnt;
  int8_t   m_nCurrTile; // start screen
  Button  *m_pCurrBtn; // for button repeats and release
  uint32_t m_lastms; // for button repeats
  uint32_t m_nDispFreeze;
  uint8_t  m_nRowStart[TILES]; // index of screen at row
  uint8_t  m_nColCnt[TILES]; // column count for each row of screens
  uint16_t m_sleepTimer;
  int16_t  m_swipePos;
  bool     m_bSwipeReady;
  uint8_t  m_nNextTile;
  uint8_t  m_nCurrRow;
  uint8_t  m_nLastRow;

#define NOTE_CNT 10
  const char *m_pszNotifs[NOTE_CNT + 1];

public:
  uint8_t m_brightness = 200; // initial brightness
  bool    m_bSleeping;
  bool    m_bCharging;
  uint8_t m_nWsConnected; // websockets connected (to stay awake)

  uint16_t m_statTemp;
  uint16_t m_statSetTemp;
  bool     m_bStatFan;
  uint16_t m_outTemp;
  bool     m_bGdoDoor;
  bool     m_bGdoCar;
  uint16_t m_vadc;
};

extern Display display;

#endif // DISPLAY_H
