#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include "arrows.h"

// from 5-6-5 to 16 bit value (max 31, 63, 31)
#define rgb16(r,g,b) ( ((uint16_t)r << 11) | ((uint16_t)g << 5) | (uint16_t)b )

#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 240
#define DISPLAY_TIMEOUT  90  // 90 seconds

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
  BTF_IR,
  BTF_BLE,
  BTF_Lights,
  BTF_WIFI_ONOFF,
  BTF_BT_ONOFF,
  BTF_Stat,
  BTF_Clear,
  BTF_LearnIR,
  BTF_PC_Media,
  BTF_Macro,
};

// Button flags
#define BF_INPUT   (1 << 0)
#define BF_REPEAT  (1 << 1)
#define BF_STATE_2 (1 << 2)
#define BF_FIXED_POS (1 << 3) // not yet implemented
#define BF_RSSI    (1 << 4)
#define BF_TIME    (1 << 5)
#define BF_DOW     (1 << 6)
#define BF_DATE    (1 << 7)

// Tile extras
#define EX_CLOCK  (1 << 1)
#define EX_NOTIF  (1 << 2)
#define EX_SCROLL (1 << 3)

enum slider_type
{
  SL_None,
  SL_Lights,
  SL_PC,
  SL_BT,
};

struct Button
{
  uint8_t ID;             // Don't set to 0
  uint8_t row;            // Used with w to calculate x positions
  uint16_t flags;          // see BF_ flags
  const char *pszText;    // Button text
  const unsigned short *pIcon[2]; // Normal, pressed icons
  uint8_t w;              // calculated if 0
  uint8_t h;              // ""
  uint8_t nFunction;     // see enum Button_Function
  uint16_t code[4];       // codes for IR, BT HID, etc
  uint8_t x;
  int16_t y; // y can go over 240
};

struct Tile
{
  const char szTitle[16];   // Top text
  uint8_t nRow;
  uint8_t Extras;           // See EX_ flags
  uint8_t nSliderType;      // see slider_type
  uint8_t nSliderValue;     // 0-100 value of screen's slider
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
  void setPcVolume(int8_t iValue);
  void RingIndicator(uint8_t n);

private:
  void drawTile(int8_t nTile, bool bFirst);
  void scrollPage(uint8_t nTile, int16_t nDelta);
  void formatButtons(Tile& pTile);
  void drawButton(Tile& pTile, Button *pBtn, bool bPressed);
  void buttonCmd(Button *pBtn, bool bRepeat);
  void sliderCmd(uint8_t nType, uint8_t nOldVal, uint8_t nNewVal);
  void dimmer(void);
  void drawSlider(uint8_t val);
  void updateRSSI(Button *pBtn);
  void drawTime(Button *pBtn);
  void drawDate(Button *pBtn);
  void drawDOW(Button *pBtn);
  void drawNotifs(Tile& pTile);
  void drawClock(void);
  void cspoint(uint16_t &x2, uint16_t &y2, uint16_t x, uint16_t y, float angle, float size);

#define TILES 9
  Tile m_tile[TILES] =
  {
    // tile 0 (top/pull-down tile)
    {
      "",
      0,
      EX_CLOCK,
      SL_None,
      0,
      0,
      0,
      0,//watchFace,
      {
        { 1, 0, BF_RSSI|BF_FIXED_POS, "", {0},  26, 26, 0, {0}, (DISPLAY_WIDTH/2 - 26/2), 180 + 26},
        { 2, 0, BF_TIME|BF_FIXED_POS, "", {0}, 100, 10, 0, {0}, DISPLAY_WIDTH/2, 80},
        { 3, 0, BF_DATE|BF_FIXED_POS, "", {0}, 100, 10, 0, {0}, DISPLAY_WIDTH/2, 156},
        { 4, 0, BF_DOW|BF_FIXED_POS, "",  {0},  50, 10, 0, {0}, 178, 117},
      }
    },
    // tile 1 (left-most horizontal tile)
    {
      "Power",
      1,
      0,
      SL_None,
      0,
      0,
      0,
      NULL,
      {// ID, r,F, text,   icons, w, h, func, {addr, code}, x, y
        { 1, 0, 0, "TV",     {0}, 48, 30, BTF_IR, {SAMSUNG, 0x707,230, 3}},
        { 2, 0, 0, "AMP",    {0}, 48, 30, BTF_IR, {0, 0, 0x34}},
        { 3, 0, 0, "DVD",    {0}, 48, 30, BTF_IR, {0, 0, 0x34}},
        { 1, 1, 0, "Light",  {0}, 48, 30, BTF_Lights, {0}}, // LivingRoom ID = 1
        { 2, 1, 0, "Switch", {0}, 48, 30, BTF_Lights, {0}}, // Other switch ID = 2
        { 1, 2, 0, "Learn",  {0}, 48, 30, BTF_LearnIR, {0}},
      },
    },
    // tile 2
    {
      "TV",
      2,
      0,
      0,
      0,
      0,
      0,
      NULL,
      {
        { 1, 0, 0, "1", {0}, 48, 32, BTF_IR, {SAMSUNG,0x707,4, 3}},
        { 2, 0, 0, "2", {0}, 48, 32, BTF_IR, {SAMSUNG,0x707,5,3}},
        { 3, 0, 0, "3", {0}, 48, 32, BTF_IR, {SAMSUNG,0x707,6,3}},
        {13, 0, BF_REPEAT, NULL, {i_up, 0}, 32, 32, BTF_IR, {SAMSUNG, 0x707,16, 3}},
        { 4, 1, 0, "4", {0}, 48, 32, BTF_IR, {SAMSUNG,0x707,8,3}},
        { 5, 1, 0, "5", {0}, 48, 32, BTF_IR, {SAMSUNG,0x707,9,3}},
        { 6, 1, 0, "6", {0}, 48, 32, BTF_IR, {SAMSUNG,0x707,10,3}},
        {14, 1, BF_REPEAT, NULL, {i_dn, 0}, 32, 32, BTF_IR, {SAMSUNG, 0x707,18,3}},
        { 7, 2, 0, "7", {0}, 48, 32, BTF_IR, {SAMSUNG,0x707,11,3}},
        { 8, 2, 0, "8", {0}, 48, 32, BTF_IR, {SAMSUNG,0x707,12,3}},
        { 9, 2, 0, "9", {0}, 48, 32, BTF_IR, {SAMSUNG,0x707,13,3}},
        {15, 2, BF_REPEAT, NULL, {i_up, 0}, 32, 32, BTF_IR, {SAMSUNG, 0x707,0x61,3}},
        {11, 3, 0, "H", {0}, 48, 32, BTF_IR, {SAMSUNG,0x707,0x79,3}},
        {10, 3, 0, "0", {0}, 48, 32, BTF_IR, {SAMSUNG,0x707,0x11,3}},
        {12, 3, 0, "<", {0}, 48, 32, BTF_IR, {SAMSUNG,0x707,0x13,3}},
        {16, 3, BF_REPEAT, NULL, {i_dn, 0}, 32, 32, BTF_IR, {SAMSUNG, 0x707,11,3}},
      }
    },
    //
    {
      "PC",
      2,
      0,
      SL_PC,
      0,
      0,
      0,
      NULL,
      {
        {1, 0, BF_REPEAT, NULL,   {i_lt, 0}, 32, 32, BTF_PC_Media, {3,0}},
        {2, 0, 0,        "Play",  {0}, 60, 32, BTF_PC_Media, {0,0}},
        {3, 0, BF_REPEAT, NULL,   {i_rt, 0}, 32, 32, BTF_PC_Media, {2,0}},
        {4, 1, 0,         "STOP",  {0}, 60, 32, BTF_PC_Media, {1,0}},
        {5, 2, BF_REPEAT, "Vol Up",{0}, 60, 32, BTF_PC_Media, {5,0}},
        {6, 2, 0,         "Mute",  {0}, 60, 32, BTF_PC_Media, {4,0}},
        {7, 2, BF_REPEAT, "Vol Dn",{0}, 60, 32, BTF_PC_Media, {6,0}},
      }
    },
    //
    {
      "Lights",
      2,
      EX_SCROLL,
      SL_Lights,
      0,
      0,
      0,
      NULL,
      {
        { 1, 0, 0, "LivingRoom", {0}, 110, 28, BTF_Lights, {0}},
      }
    },
    //
    {
      "Thermostat",
      3,
      0,
      SL_None,
      0,
      0,
      0,
      NULL,
      {
        {1, 0, 0, NULL, {i_up, 0}, 32, 32, BTF_Stat, {0}},
        {2, 1, 0, NULL, {i_dn, 0}, 32, 32, BTF_Stat, {0}},
      }
    },
    //
    {
      "Thermostat 2",
      3,
      0,
      SL_None,
      0,
      0,
      0,
      NULL,
      {
        {1, 0, 0, NULL, {i_up, 0}, 32, 32, BTF_Stat, {0}},
        {2, 1, 0, NULL, {i_dn, 0}, 32, 32, BTF_Stat, {0}},
      }
    },
    // Pull-up tile (last)
    {
      "Settings",
      4,
      0,
      SL_None,
      0,
      0,
      0,
      NULL,
      {
        {1, 0, 0, "WiFi On",      {0},  112, 28, BTF_WIFI_ONOFF, {0}},
        {2, 1, 0, "Bluetooth On", {0},  112, 28, BTF_BT_ONOFF, {0}},
        {3, 1, BF_RSSI|BF_FIXED_POS, "", {0}, 26, 26, 0, {0}, (DISPLAY_WIDTH/2 - 26/2), 180 + 26},
      }
    },
    // Notification tile (very last)
    {
      "Notifications",
      5,
      EX_NOTIF | EX_SCROLL,
      SL_None,
      0,
      0,
      0,
      NULL,
      {
        {1, 0, BF_FIXED_POS, "Clear", {0}, 180, 24, BTF_Clear, {0}, 0, 240-24}, // force y
      }
    },
  };

  uint16_t m_backlightTimer = DISPLAY_TIMEOUT; // backlight timer, seconds
  uint8_t m_bright = 0; // current brightness
  uint8_t m_nTileCnt;
  int8_t m_nCurrTile = 0; // start screen
  uint32_t m_nDispFreeze;
  uint8_t m_blSurgeTimer; // timer to stop shutoff
  uint8_t m_nRowStart[TILES]; // index of screen at row
  uint8_t m_nColCnt[TILES]; // column count for each row of screens
#define NOTE_CNT 10
  char *m_pszNotifs[NOTE_CNT + 1];

public:
  uint8_t m_brightness = 200; // initial brightness
};

extern Display display;

#endif // DISPLAY_H
