#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <TFT_eSPI.h> // TFT_eSPI library from library manager?
#include "arrows.h"

// from 5-6-5 to 16 bit value (max 31, 63, 31)
#define rgb16(r,g,b) ( ((uint16_t)r << 11) | ((uint16_t)g << 5) | (uint16_t)b )

#define DISPLAY_WIDTH 240

#if (USER_SETUP_ID==302) // 240x240
#define DISPLAY_HEIGHT 240
#define VADC_MAX 1550
#define ROUND_DISPLAY
#else
#define DISPLAY_HEIGHT 280
#define VADC_MAX 1642 // 1623 = full
#endif

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
};

// Button flags
#define BF_BORDER  (1 << 0) // Just a text time, not button
#define BF_REPEAT  (1 << 1) // repeatable (hold)
#define BF_STATE_2 (1 << 2) // tri-state
#define BF_FIXED_POS (1 << 3)
#define BF_TEXT    (1<< 4)
#define BF_SLIDER_H (1<< 5)
#define BF_SLIDER_V (1<< 6)

// Tile extras
#define EX_NONE  (0)
#define EX_CLOCK  (1 << 1)
#define EX_NOTIF  (1 << 2)
#define EX_SCROLL (1 << 3)

#define SFL_REVERSE (1<<0)

struct ArcSlider
{
  uint8_t  nFunc;         // see slider_func
  uint8_t  flags;         // see SFL_xx
  uint16_t nPos;          // angle from 12 o'clock
  uint16_t nSize;         // Size in degrees
  uint8_t  nValue;        // 0-100 value of screen's slider
};

struct Button
{
  uint8_t ID;             // Don't set to 0
  uint8_t row;            // Used with w to calculate x positions
  uint16_t flags;          // see BF_ flags
  uint8_t nFunction;     // see enum Button_Function
  const char *pszText;    // Button text
  const unsigned short *pIcon[2]; // Normal, pressed icons
  uint16_t w;              // calculated if 0
  uint16_t h;              // ""
  uint16_t data[4];       // codes for IR, BT HID, etc (1=slider value, 2=prev slider value)
  int16_t x;
  int16_t y; // y can go over 240 (scrollable pages)
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
  void drawTile(int8_t nTile, bool bFirst, int16_t x, int16_t y);
  bool scrollPage(uint8_t nTile, int16_t nDelta);
  void formatButtons(Tile& pTile);
  void drawButton(Tile& pTile, Button *pBtn, bool bPressed, int16_t x, int16_t y);
  void buttonCmd(Button *pBtn, bool bRepeat);
  void sliderCmd(uint8_t nFunc, uint8_t nNewVal);
  void dimmer(void);
  void drawArcSlider(ArcSlider& slider, uint8_t newValue, int16_t x, int16_t y);
  void btnRSSI(Button *pBtn, int16_t x, int16_t y);
  void drawNotifs(Tile& pTile, int16_t x, int16_t y);
  void drawClock(int16_t x, int16_t y);
  bool arcSliderHit(ArcSlider& slider, uint8_t& value);

  void cspoint(int16_t &x2, int16_t &y2, int16_t x, int16_t y, float angle, float size);

  void startSleep(void);
  void endSleep(void);
  bool snooze(uint32_t ms);

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
        { 1, 0, BF_FIXED_POS, BTF_RSSI, "", {0},  26, 26, {0}, (DISPLAY_WIDTH/2 - 26/2), 180 + 26},
        { 2, 0, BF_FIXED_POS|BF_TEXT, BTF_Time, "12:00:00 AM", {0}, 120, 32, {0}, DISPLAY_WIDTH/2-56, 50},
        { 3, 0, BF_FIXED_POS|BF_TEXT, BTF_Date,  "Jan 01", {0}, 80, 32, {0}, DISPLAY_WIDTH/2-40, 152},
        { 4, 0, BF_FIXED_POS|BF_TEXT, BTF_DOW,   "Sun",  {0},  40, 32, {0}, 170, DISPLAY_HEIGHT/2 - 15},
        { 5, 0, BF_FIXED_POS|BF_TEXT, BTF_Volts, "4.20",  {0},  50, 32, {0}, 28, DISPLAY_HEIGHT/2 - 15},
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
      {//ID, r, F, fn,        text,   icons, w, h, {addr, code}, x, y
        { 1, 0, 0, BTF_IR,    "TV",     {0}, 48, 30, {SAMSUNG, 0x7,0xE6, 3}},
        { 2, 0, 0, BTF_IR,    "AMP",    {0}, 48, 30, {0, 0, 0x34}},
        { 3, 0, 0, BTF_IR,     "DVD",    {0}, 48, 30, {0, 0, 0x34}},
        { 1, 1, 0, BTF_Lights, "Light",  {0}, 48, 30, {0}}, // LivingRoom ID = 1
        { 2, 1, 0, BTF_Lights, "Switch", {0}, 48, 30, {0}}, // Other switch ID = 2
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
        { 1, 0, 0, BTF_IR, "1", {0}, 48, 32,  {SAMSUNG,0x7,4, 3}},
        { 2, 0, 0, BTF_IR, "2", {0}, 48, 32,  {SAMSUNG,0x7,5,3}},
        { 3, 0, 0, BTF_IR, "3", {0}, 48, 32,  {SAMSUNG,0x7,6,3}},
        {13, 0, BF_REPEAT, BTF_IR, NULL, {i_up, 0}, 32, 32, {SAMSUNG, 0x7,7, 3}},
        { 4, 1, 0, BTF_IR,"4", {0}, 48, 32, {SAMSUNG,0x7,8,3}},
        { 5, 1, 0, BTF_IR,"5", {0}, 48, 32, {SAMSUNG,0x7,9,3}},
        { 6, 1, 0, BTF_IR,"6", {0}, 48, 32, {SAMSUNG,0x7,10,3}},
        {14, 1, BF_REPEAT,BTF_IR,  NULL, {i_dn, 0}, 32, 32, {SAMSUNG, 0x7,0xE6,3}},
        { 7, 2, 0, BTF_IR,"7", {0}, 48, 32, {SAMSUNG,0x7,11,3}},
        { 8, 2, 0, BTF_IR,"8", {0}, 48, 32, {SAMSUNG,0x7,12,3}},
        { 9, 2, 0, BTF_IR,"9", {0}, 48, 32, {SAMSUNG,0x7,13,3}},
        {15, 2, BF_REPEAT, BTF_IR, NULL, {i_up, 0}, 32, 32, {SAMSUNG, 0x7,0x61,3}},
        {11, 3, 0, BTF_IR,"H", {0}, 48, 32, {SAMSUNG,0x7,0x79,3}},
        {10, 3, 0, BTF_IR,"0", {0}, 48, 32, {SAMSUNG,0x7,0x11,3}},
        {12, 3, 0, BTF_IR,"<", {0}, 48, 32, {SAMSUNG,0x7,0x13,3}},
        {16, 3, BF_REPEAT, BTF_IR, NULL, {i_dn, 0}, 32, 32, {SAMSUNG, 0x7,11,3}},
      }
    },
    //
    {
      "PC",
      1,
      EX_NONE,
#if defined(ROUND_DISPLAY) // round, use arc slider
      {{BTF_PCVolume, 0, 90, 90},{BTF_None}},
#else
      {{BTF_None},{BTF_None}},
#endif
      0,
      0,
      NULL,
      {
        {1, 0, BF_REPEAT, BTF_PC_Media, NULL,  {i_lt, 0}, 32, 32, {3,0}},
        {2, 0, 0, BTF_PC_Media,       "Play",  {0}, 60, 32, {0,0}},
        {3, 0, BF_REPEAT, BTF_PC_Media, NULL,  {i_rt, 0}, 32, 32, {2,0}},
        {4, 1, 0, BTF_PC_Media,        "STOP", {0}, 60, 32, {1,0}},
        {5, 2, BF_SLIDER_H, BTF_PC_Media, "",  {0}, 120, 32, {1001,0}},
        {6, 3, 0, BTF_PC_Media,        "Mute", {0}, 60, 32, {4,0}},
#if !defined(ROUND_DISPLAY)
        {7, 4, BF_SLIDER_V|BF_FIXED_POS, BTF_PCVolume, "",  {0}, 14, DISPLAY_HEIGHT - 80, {0}, DISPLAY_WIDTH-20, 40},
#endif
      }
    },
    //
    {
      "Lights",
      1,
      EX_SCROLL,
#if defined(ROUND_DISPLAY)
      {{BTF_Lights, SFL_REVERSE, 270, 90},{BTF_None}},
#else
      {{BTF_None},{BTF_None}},
#endif
      0,
      0,
      NULL,
      {
#if !defined(ROUND_DISPLAY)
        {1, 0, BF_SLIDER_V|BF_FIXED_POS, BTF_Lights, "",  {0}, 14, DISPLAY_HEIGHT - 80, {0}, 10, 40},
#endif
        { 2, 0, 0, BTF_Lights, "LivingRoom", {0}, 110, 28, {0}},
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
        {1, 0, BF_TEXT, 0, "Out:",  {0}, 0, 32, {0}},
        {2, 0, BF_BORDER|BF_TEXT, BTF_Stat_OutTemp, "",  {0}, 60, 32, {1,0}},
        {3, 0, BF_TEXT, 0, "",  {0}, 32, 32, {0}}, // spacer
        {4, 1, BF_TEXT, 0, " In:",  {0}, 38, 32, {1,0}},
        {5, 1, BF_BORDER|BF_TEXT, BTF_Stat_Temp, "",  {0}, 60, 32, {1,0}},
        {6, 1, BF_REPEAT, BTF_StatCmd, NULL, {i_up, 0}, 32, 32, {0}},
        {7, 2, BF_TEXT, 0, "Set:",  {0}, 0, 32, {1,0}},
        {8, 2, BF_BORDER|BF_TEXT, BTF_Stat_SetTemp, "",  {0}, 60, 32, {1,0}},
        {9, 2, BF_REPEAT, BTF_StatCmd, NULL, {i_dn, 0}, 32, 32, {1}},
        {10, 3, 0, BTF_Stat_Fan, "Fan", {0, 0}, 0, 32, {2}},
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
        {1, 0, BF_BORDER|BF_TEXT, BTF_GdoDoor, "",  {0}, 98, 32, {1,0}},
        {2, 1, BF_BORDER|BF_TEXT, BTF_GdoCar, "",  {0}, 98, 32, {1,0}},
        {3, 2, 0, BTF_GdoCmd, "Open", {0, 0}, 98, 32, {0}},
      }
    },
    // Pull-up tile (last)
    {
      "Settings",
      3, // row 3
      EX_NONE,
#if defined(ROUND_DISPLAY)
      {{BTF_Brightness, SFL_REVERSE, 270, 90},{BTF_None}},
#else
      {{BTF_None},{BTF_None}},
#endif
      0,
      0,
      NULL,
      {
        {1, 0, 0, BTF_WIFI_ONOFF, "WiFi On",      {0},  112, 28,  {0}},
        {2, 1, 0, BTF_BT_ONOFF, "Bluetooth On", {0},  112, 28, {0}},
        {2, 2, 0, BTF_Restart, "Restart", {0},  112, 28, {0}},
        {3, 2, BF_FIXED_POS, BTF_RSSI, "", {0}, 26, 26, {0}, (DISPLAY_WIDTH/2 - 26/2), 180 + 26},
#if !defined(ROUND_DISPLAY)
        {4, 3, BF_SLIDER_V|BF_FIXED_POS, BTF_Brightness, "",  {0}, 14, DISPLAY_HEIGHT - 80, {0}, 10, 40},
#endif
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
        {1, 0, BF_FIXED_POS, BTF_Clear, "Clear", {0}, 180, 24, {0}, 0, DISPLAY_HEIGHT-24}, // force y
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
