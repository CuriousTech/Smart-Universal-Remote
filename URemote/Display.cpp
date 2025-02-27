/*
Display and UI code
*/

#include <WiFi.h>
#include <TimeLib.h>
#include "eeMem.h"
#include "CST816T.h"
#include "Lights.h"
#include <FunctionalInterrupt.h>
#include "display.h"
#include <math.h>
#include "QMI8658.h"

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);

//Lights class
extern Lights lights;

// URemote functions
extern int WsPcClientID;
extern void stopWiFi();
extern void startWiFi();
extern void sendIR(uint16_t *pCode);
extern bool sendBLE(uint16_t *pCode);
extern bool sendPCMediaCmd( uint16_t *pCode);
extern bool sendStatCmd( uint16_t *pCode);
extern bool sendGdoCmd( uint16_t *pCode);
extern void consolePrint(String s); // for browser debug

#if (USER_SETUP_ID==302) // 240x240

 #define I2C_SDA   6
 #define I2C_SCL   7
 #define IMU_INT   3 // INT2
 #define TP_RST   13
 #define TP_INT    GPIO_NUM_5 // normally high
 #define TFT_BL    2
 #define BATTV     1

 #define DISPLAY_HEIGHT 240
 CST816T touch(I2C_SDA, I2C_SCL, TP_RST, TP_INT);
#endif

#if (USER_SETUP_ID==303) // 240x320 2.8"
 #define I2C_SDA  11
 #define I2C_SCL  10
 #define TP_SDA    1
 #define TP_SCL    3
 #define IMU_INT  12 // INT2
 #define TP_RST    2
 #define TP_INT    GPIO_NUM_4 // normally high
 #define TFT_BL    5
 #define BATTV     8
 #define PWR_CTRL  7
 #define PWR_BTN   6
 
 #define DISPLAY_HEIGHT 320
 #include "CST328.h"
 CST328 touch(TP_SDA, TP_SCL, TP_RST, TP_INT);
#endif

#if (USER_SETUP_ID==203) // 240x280
 #define I2C_SDA  11
 #define I2C_SCL  10
 #define IMU_INT  38 // INT1
 #define TP_RST   13
 #define TP_INT   GPIO_NUM_14 // normally high
 #define TFT_BL   15
 #define BATTV     1

 #define DISPLAY_HEIGHT 280
 CST816T touch(I2C_SDA, I2C_SCL, TP_RST, TP_INT);
#endif

const int16_t bgColor = TFT_BLACK;

QMI8658 qmi;

#include "screensavers.h" // comment this line to remove screensavers (saves 3172b PROGMEM, 1072b SRAM)
#ifdef SCREENSAVERS_H
ScreenSavers ss;
#endif

#define ROTATE_DISPLAY // Upside down, puts USB on top
#define TITLE_HEIGHT 40
#define BORDER_SPACE 3 // H and V spacing + font size for buttons and space between buttons

void Display::init(void)
{
  pinMode(TFT_BL, OUTPUT);

  ee.bBtEnabled = false; // BUG

#ifdef PWR_CTRL // Power stay-on thing
  pinMode(PWR_CTRL, OUTPUT);
  digitalWrite(PWR_CTRL, HIGH);
#endif

  tft.init();
#ifdef ROTATE_DISPLAY
  tft.setRotation(2);
#endif
  tft.setTextPadding(8); 

#if (USER_SETUP_ID==302) // 240x240
  tft.setSwapBytes(true);
  sprite.setSwapBytes(true);
#endif

  sprite.createSprite(DISPLAY_WIDTH, DISPLAY_HEIGHT); // This uses over 115K RAM for 240x240
  
  drawTile(m_nCurrTile, true, 0, 0);
  sprite.pushSprite(0, 0); // draw screen 0 on start

  // count tiles, index rows, count columns, format the buttons
  uint8_t nRowIdx = 0;

  for(m_nTileCnt = 0; m_nTileCnt < TILES && (m_tile[m_nTileCnt].Extras || m_tile[m_nTileCnt].pszTitle || m_tile[m_nTileCnt].pBGImage); m_nTileCnt++) // count screens
  {
    if(m_tile[m_nTileCnt].nRow != nRowIdx)
      m_nRowStart[++nRowIdx] = m_nTileCnt;
    m_nColCnt[nRowIdx]++;
    formatButtons(m_tile[m_nTileCnt]);
  }

  pinMode(IMU_INT, INPUT);
  qmi.init(I2C_SDA, I2C_SCL);
  touch.begin();
  attachInterrupt(IMU_INT, std::bind(&Display::handleISR, this), RISING);

  m_vadc = analogRead(BATTV);
  if(ee.vadcMax < m_vadc)
    ee.vadcMax = m_vadc; // calibration

  qmi.setWakeOnMotion(true); // set WOM (sets acc values)
  m_intTriggered = false;
#ifdef SCREENSAVERS_H
  randomSeed(micros());
  ss.init(DISPLAY_WIDTH, DISPLAY_HEIGHT);
#endif
}

void Display::exitScreensaver()
{
  m_backlightTimer = ee.ssTime; // reset timer for any touch

  if( m_brightness == ee.brightLevel[0] ) // screensaver active
  {
    drawTile(m_nCurrTile, true, 0, 0); // draw over screensaver
    sprite.pushSprite(0, 0);
    qmi.setWakeOnMotion(false); // enable gyro/acc/temp
    m_intTriggered = false; // causes an interrupt
  }
  m_brightness = ee.brightLevel[1]; // increase brightness for any touch
  ets_printf("exitSS\n");
}

void Display::service(void)
{
  if( m_intTriggered) // set for IMU WOM
  {
    m_intTriggered = false;
    m_backlightTimer = ee.ssTime;
    if( m_brightness < ee.brightLevel[1] )
      exitScreensaver();
  }

  if(m_bSleeping)
  {
    if( snooze(30000) )
    {
      endSleep();
      m_backlightTimer = ee.ssTime;
    }
    else
      return;
  }

  dimmer();

  if(m_nDispFreeze)
  {
    if( millis() < m_nDispFreeze) // freeze display for notification
      return;
    m_nDispFreeze = 0;
  }

  battRead();

#ifdef SCREENSAVERS_H
  if(m_brightness == ee.brightLevel[0]) // full dim activates screensaver
    ss.run();
#endif

  static bool bScrolling; // scrolling active
  static bool bSliderHit;
  static int16_t touchXstart, touchYstart; // first touch pos for scolling and non-moving detect
  static uint32_t touchStartMS; // timing for detecting swipe vs press

  if(touch.available() )
  {
#ifdef ROTATE_DISPLAY
    touch.x = DISPLAY_WIDTH - touch.x;
    touch.y = DISPLAY_HEIGHT - touch.y;
#endif
    if(touch.event == 0) // initial touch
    {
      touchXstart = touch.x;
      touchYstart = touch.y;
      touch.gestureID = 0;
      bSliderHit = false;
      bScrolling = false;
      touchStartMS = millis();

      m_backlightTimer = ee.ssTime; // reset timer for any touch
  
      if( m_brightness < ee.brightLevel[1] )
      {
        exitScreensaver();
        endSleep();
      }
    }
    else if(touch.event == 1) // release
    {
      if(m_pCurrBtn)
      {
        drawButton(m_tile[m_nCurrTile], m_pCurrBtn, false, 0, 0, false); // draw released button
        sprite.pushSprite(0, 0);

        if(m_pCurrBtn && (m_pCurrBtn->flags & (BF_SLIDER_H|BF_SLIDER_V)) )
        {
          buttonCmd(m_pCurrBtn, true);
        }

        m_pCurrBtn = NULL;
      }
      else if(touch.gestureID)
      {
        snapTile(); // finish the swipe (only forward for now)
        m_bSwipeReady = false;
        touch.gestureID = 0; // unlock the idle display updates
      }
      else if(bScrolling == false) // check for a quick button hit if nothing else appeared to happen
      {
        checkButtonHit(touchXstart, touchYstart);
      }
      touch.gestureID = 0;
    }
    else // 2=active
    {
      if( m_brightness < ee.brightLevel[1] ) // ignore input until brightness is full
        return;

      Tile& pTile = m_tile[m_nCurrTile];

#if defined(ROUND_DISPLAY)
      // Handle arc slider
      for(uint8_t j = 0; j <SLIDER_CNT; j++)
      {
        uint8_t sliderVal;
        if(pTile.slider[j].nFunc && arcSliderHit(pTile.slider[j], sliderVal) )
        {
          drawArcSlider(pTile.slider[j], sliderVal, 0, 0);
          sprite.pushSprite(0, 0);
          if(pTile.slider[j].nValue != sliderVal)
          {
            sliderCmd(pTile.slider[j].nFunc, sliderVal);
            pTile.slider[j].nValue = sliderVal;
          }
          bSliderHit = true;
        }
      }
#endif
      if(bSliderHit);
      else if(touch.gestureID) // swiping mode
      {
        swipeTile(touch.x - touchXstart, touch.y - touchYstart);
        touchXstart = touch.x;
        touchYstart = touch.y;        
      }
      else if(bScrolling) // scrolling detected
      {
        if(touch.y - touchYstart)
          bScrolling = scrollPage(m_nCurrTile, touch.y - touchYstart);
        touchYstart = touch.y;
      }
      else // normal press or hold
      {
        if( pTile.Extras & EX_SCROLL ) // give this a bit more priority
        {
          if( touchYstart > 40 && touchYstart < DISPLAY_HEIGHT - 40 && touch.y != touchYstart && (touch.y - touchYstart < 10 || touchYstart - touch.y < 10)) // slow/short flick
          {
            if( touchXstart > 40 && touchXstart < DISPLAY_WIDTH - 40) // start needs to be in the middle somewhere
            {
              bScrolling = scrollPage(m_nCurrTile, touch.y - touchYstart); // test it
              touchYstart = touch.y;
            }
          }
        }

        if(millis() - touchStartMS < 150 && touch.gestureID == 0 && bScrolling == false) // delay a bit
        {
          swipeCheck((int16_t)touch.x - (int16_t)touchXstart, (int16_t)touch.y - (int16_t)touchYstart);
          if(touch.gestureID)
            startSwipe();
        }
        else if(m_pCurrBtn == NULL && touch.gestureID == 0 && bScrolling == false) // check button hit
        {
          checkButtonHit(touchXstart, touchYstart);
        }
        else if(m_pCurrBtn && (m_pCurrBtn->flags & (BF_SLIDER_H|BF_SLIDER_V)) )
        {
          if(m_pCurrBtn->flags & BF_SLIDER_H)
          {
            uint16_t off = constrain(touch.x - m_pCurrBtn->r.x, 0, m_pCurrBtn->r.w);

            m_pCurrBtn->data[1] = off * 100 / m_pCurrBtn->r.w;
            drawButton(pTile, m_pCurrBtn, true, 0, 0, false);
            sprite.pushSprite(0, 0);
          }
          else if(m_pCurrBtn->flags & BF_SLIDER_V)
          {
            uint16_t off = constrain(touch.y - m_pCurrBtn->r.y, 0, m_pCurrBtn->r.h);

            m_pCurrBtn->data[1] = 100 - (off * 100 / m_pCurrBtn->r.h);

            drawButton(pTile, m_pCurrBtn, true, 0, 0, false);
            sprite.pushSprite(0, 0);
          }
        }
        else if(millis() - m_lastms > 300) // repeat speed
        {
          if(m_pCurrBtn && (m_pCurrBtn->flags & (BF_REPEAT|BF_SLIDER_H|BF_SLIDER_V)) )
          {
            buttonCmd(m_pCurrBtn, true);
            if(m_pCurrBtn->flags & (BF_SLIDER_H|BF_SLIDER_V)) 
              bSliderHit = true;
          }
          m_lastms = millis();
        }
      }
    }
  }

  if(ee.bWiFiEnabled && (year() < 2024 || WiFi.status() != WL_CONNECTED)) // Do a connecting effect
  {
#if defined(ROUND_DISPLAY) // 240x240
    static int16_t ang;
    uint16_t ang2;
    static uint8_t rgb[3] = {32, 128, 128};

    ang2 = ang;
    for(uint8_t i = 0; i < 12; i++)
    {
      tft.drawArc(DISPLAY_WIDTH/2, DISPLAY_HEIGHT/2, 120, 117, ang2, ang2 + 5, (WiFi.status() == WL_CONNECTED) ? tft.color565(rgb[0], rgb[1], rgb[2]):TFT_RED, bgColor, false);
      ang2 += 30;
      ang2 %= 360;
    }
    ++ang %= 360;
    for(uint8_t i = 0; i < 3; i++)
    {
      rgb[i] += random(-3, 6);
      rgb[i] = (uint8_t)constrain(rgb[i], 2, 255);
    }

#else // 240x280, 240x320
    static uint8_t rgb[3] = {0, 0, 0};
    static uint8_t cnt;
    if(++cnt > 2)
    {
      cnt = 0;
      if(WiFi.status() != WL_CONNECTED)
        rgb[0] ++;
      else
        rgb[2] ++;
      tft.drawSmoothRoundRect(0, 0, 43, 44, DISPLAY_WIDTH-1, DISPLAY_HEIGHT-1, tft.color565(rgb[0], rgb[1], rgb[2]), bgColor);
    }

#endif
  }
}

void Display::swipeCheck(int16_t dX, int16_t dY)
{
  if(dX == 0 && dY == 0)
    return;

  if(dX == 0)
      touch.gestureID = (dY > 0) ? SWIPE_DOWN : SWIPE_UP;
  else if(dY == 0)
      touch.gestureID = (dX > 0) ? SWIPE_RIGHT : SWIPE_LEFT;
  else if(dX > 0 && dY > 0)
      touch.gestureID = (dX > dY) ? SWIPE_DOWN : SWIPE_RIGHT;
  else if(dX > 0 && dY < 0)
      touch.gestureID = (dX > dY) ? SWIPE_RIGHT : SWIPE_DOWN;
  else if(dX < 0 && dY < 0)
      touch.gestureID = (dX > dY) ? SWIPE_UP : SWIPE_LEFT;
  else  // -dX +dY
      touch.gestureID = (-dX > dY) ? SWIPE_LEFT : SWIPE_DOWN;
}

void Display::checkButtonHit(int16_t touchXstart, int16_t touchYstart)
{
  Tile& pTile = m_tile[m_nCurrTile];
  Button *pBtn = &pTile.button[0];

  for(uint16_t i = 0; pBtn[i].row != 0xFF; i++) // check for press in button bounds, rejects swiping
  {
    int16_t y = pBtn[i].r.y;

    if( !(pBtn[i].flags & BF_FIXED_POS) ) // adjust for scroll offset
      y -= pTile.nScrollIndex;

    if ( (touch.x >= pBtn[i].r.x-1 && touch.x <= pBtn[i].r.x + pBtn[i].r.w+1 && touch.y >= y-1 && touch.y <= y + pBtn[i].r.h+1)
       && (touchXstart >= pBtn[i].r.x-1 && touchXstart <= pBtn[i].r.x + pBtn[i].r.w+1 && touchYstart >= y-1 && touchYstart <= y + pBtn[i].r.h+1) ) // make sure not swiping
    {
      m_pCurrBtn = &pBtn[i];

      if(m_pCurrBtn->flags & BF_SLIDER_H)
      {
        uint16_t off = touch.x - m_pCurrBtn->r.x;
        m_pCurrBtn->data[1] = off * 100 / m_pCurrBtn->r.w;
      }
      else if(m_pCurrBtn->flags & BF_SLIDER_V)
      {
        uint16_t off = touch.y - m_pCurrBtn->r.y;
        m_pCurrBtn->data[1] = 100 - (off * 100 / m_pCurrBtn->r.h);
      }
      drawButton(pTile, m_pCurrBtn, true, 0, 0, false); // draw pressed state
      sprite.pushSprite(0, 0);
      buttonCmd(m_pCurrBtn, false);
      m_lastms = millis() - 300; // slow first repeat (300+300ms)
      break;
    }
  }
}

void Display::startSleep()
{
  if(m_bSleeping || m_sleepTimer)
    return;

  m_bSleeping = true;
  m_brightness = ee.brightLevel[0] + 1; // no screensaver
  analogWrite(TFT_BL, m_bright = 0);
  if(ee.bWiFiEnabled)
    stopWiFi();
  if(ee.bBtEnabled)
    btStop();
}

void Display::endSleep()
{
  if(!m_bSleeping)
    return;

  m_sleepTimer = 0;
  m_bSleeping = false;

  if(ee.bWiFiEnabled)
    startWiFi();
  if(ee.bBtEnabled)
    btStart();
}

void Display::startSwipe()
{
  uint8_t nRowOffset;

  m_bSwipeReady = false;
  m_swipePos = 0;

  switch(touch.gestureID)
  {
    case SWIPE_RIGHT:
      if( (m_nCurrTile <= 0) // don't screoll if button pressed or screen 0
        || (m_tile[m_nCurrTile].nRow != m_tile[m_nCurrTile-1].nRow) ) // don't scroll left if leftmost
      {
        swipeBump();
        break;
      }

      m_nLastRow = m_nCurrRow;
      m_nNextTile = m_nCurrTile - 1;
      m_bSwipeReady = true;
      break;
    case SWIPE_LEFT:
      if( (m_nCurrTile >= m_nTileCnt - 1) // don't scroll if last screen
        || (m_tile[m_nCurrTile].nRow != m_tile[m_nCurrTile+1].nRow) ) // don't scroll right if rightmost
      {
        swipeBump();
        break;
      }
  
      m_nNextTile = m_nCurrTile + 1; // save for snap
      m_nLastRow = m_nCurrRow;
      m_bSwipeReady = true;
      break;
    case SWIPE_DOWN:
      if(m_tile[m_nCurrTile].nRow == 0) // don't slide up from top
      {
        swipeBump();
        break;
      }
  
      nRowOffset = m_nCurrTile - m_nRowStart[m_nCurrRow];
      m_nLastRow = m_nCurrRow;
      m_nCurrRow--;
      m_nNextTile = m_nRowStart[m_nCurrRow] + min((int)nRowOffset, m_nColCnt[m_nCurrRow]-1 ); // get close to the adjacent screen

      m_bSwipeReady = true;
      break;
    case SWIPE_UP:
      if(m_nCurrRow == m_tile[m_nTileCnt-1].nRow)
      {
        swipeBump();
        break;
      }

      nRowOffset = m_nCurrTile - m_nRowStart[m_nCurrRow];
      m_nLastRow = m_nCurrRow;
      m_nNextTile = m_nRowStart[m_nCurrRow+1] + min((int)nRowOffset, m_nColCnt[m_nCurrRow+1]-1 );
      if(m_nNextTile < m_nTileCnt)
      {
        m_bSwipeReady = true;
        m_nCurrRow++;
      }
      break;
  }
}

// swipe effect
void Display::swipeTile(int16_t dX, int16_t dY)
{
  if(m_bSwipeReady == false)
    return;

  switch(touch.gestureID)
  {
    case SWIPE_RIGHT: // >>>
      m_swipePos = constrain(m_swipePos + dX, 0, DISPLAY_WIDTH);
      break;
    case SWIPE_LEFT: // <<<
      m_swipePos = constrain(m_swipePos - dX, 0, DISPLAY_WIDTH);
      break;
    case SWIPE_DOWN:
      m_swipePos = constrain(m_swipePos + dY, 0, DISPLAY_HEIGHT);
      break;
    case SWIPE_UP:
      m_swipePos = constrain(m_swipePos - dY, 0, DISPLAY_HEIGHT);
      break;
  }
  drawSwipeTiles();
}

// do some kind of visual effect
void Display::swipeBump()
{
  switch(touch.gestureID)
  {
    case SWIPE_RIGHT: // >>>
      tft.fillRect(0, 0, 5, DISPLAY_HEIGHT, TFT_BLUE);
      sprite.pushSprite(5, 0);
      break;
    case SWIPE_LEFT: // <<<
      tft.fillRect(DISPLAY_WIDTH-5, 0, 5, DISPLAY_HEIGHT, TFT_BLUE);
      sprite.pushSprite(-5, 0);
      break;
    case SWIPE_DOWN:
      tft.fillRect(0, 0, DISPLAY_WIDTH, 5, TFT_BLUE);
      sprite.pushSprite(0, 5);
      break;
    case SWIPE_UP:
      tft.fillRect(0, DISPLAY_HEIGHT-5, DISPLAY_WIDTH, 5, TFT_BLUE);
      sprite.pushSprite(0, -5);
      break;
  }
  delay(50);
  sprite.pushSprite(0, 0);
}

// finish unfinished swipe
void Display::snapTile()
{
  if(m_bSwipeReady == false)
    return;

  uint8_t nSpeed = 50;
  uint16_t space = DISPLAY_HEIGHT;

  if( touch.gestureID == SWIPE_LEFT || touch.gestureID == SWIPE_RIGHT)
    space = DISPLAY_WIDTH;

  if(m_swipePos < space/2)
  {
    m_nCurrRow = m_nLastRow;

    while(m_swipePos > 0)
    {
      if(m_swipePos > nSpeed)
        m_swipePos -= nSpeed;
      else
        m_swipePos = 0;
      drawSwipeTiles();
      nSpeed -= 5;
    }
    return;
  }

  while( m_swipePos < space)
  {
    if(m_swipePos < space - nSpeed)
      m_swipePos += nSpeed;
    else
      m_swipePos = space;
    drawSwipeTiles();
    nSpeed -= 5;
  }

  m_nCurrTile = m_nNextTile;
}

void Display::drawSwipeTiles()
{
  switch(touch.gestureID)
  {
    case SWIPE_RIGHT: // >>>
      drawTile(m_nCurrTile, true, m_swipePos, 0);
      drawTile(m_nNextTile, true, m_swipePos - DISPLAY_WIDTH, 0);
      break;
    case SWIPE_LEFT: // <<<
      drawTile(m_nCurrTile, true, -m_swipePos, 0);
      drawTile(m_nNextTile, true, DISPLAY_WIDTH - m_swipePos, 0);
      break;
    case SWIPE_DOWN:
      drawTile(m_nCurrTile, true, 0, m_swipePos);
      drawTile(m_nNextTile, true, 0, m_swipePos - DISPLAY_HEIGHT);
      break;
    case SWIPE_UP:
      drawTile(m_nCurrTile, true, 0, -m_swipePos);
      drawTile(m_nNextTile, true, 0, DISPLAY_HEIGHT - m_swipePos);
      break;
  }
  sprite.pushSprite(0, 0);  
}

void IRAM_ATTR Display::handleISR(void)
{
  m_intTriggered = true;
}

// called each second
void Display::oneSec()
{
  if(m_bSleeping || m_nDispFreeze || touch.gestureID) // don't overwrite the popup or while swiping
    return;

  if( m_backlightTimer ) // the dimmer thing
  {
    if(--m_backlightTimer == 0)
    {
      m_brightness = ee.brightLevel[0]; // dim level
#ifdef SCREENSAVERS_H
      ss.select( random(0, SS_Count) );
#endif
      qmi.setWakeOnMotion(true); // enable movement to wake
      m_intTriggered = false; // causes an interrupt
      m_sleepTimer = ee.sleepTime;
    }
  }

  if(m_brightness > ee.brightLevel[0]) // not screensaver
  {
    drawTile(m_nCurrTile, false, 0, 0);
    sprite.pushSprite(0,0);
  }

  if(m_sleepTimer && ((WsPcClientID) ? (m_nWsConnected <= 1) : (m_nWsConnected == 0) ) ) // PC connection doesn't count
  {
    if(--m_sleepTimer == 0)
    {
      startSleep();
      return;
    }
  }
  
  static bool bQDone = false;
  // A query or command is done
  if(lights.checkStatus() == LI_Done)
  {
    if(bQDone == false) // first response is list from a lightswitch
    {
      bQDone = true;
      // Find Lights tile
      uint8_t nLi = 0;
      while(m_tile[nLi].pszTitle || m_tile[nLi].Extras )
      {
        if( !strcmp(m_tile[nLi].pszTitle, "Lights") )
          break;
        nLi++;
      }

      Tile& pTile = m_tile[nLi];

      if(pTile.Extras)
      {
        uint8_t i;

        for(i = 0; i < BUTTON_CNT - 2 && ee.lights[i].szName[0]; i++)
        {
          pTile.button[i+1].pszText = ee.lights[i].szName;
          pTile.button[i+1].row = i;
          pTile.button[i+1].r.w = 140;
          pTile.button[i+1].r.h = 28;
          pTile.button[i+1].nFunction = BTF_Lights;
        }

        pTile.button[i].row = 0xFF; // end row
      }
      formatButtons(pTile);
    }
  }
}

void Display::battRead()
{
  static uint32_t ms;

  uint32_t ms2 = millis();
  if(ms2 - ms < 200) // run every 200ms
    return;

  ms = ms2;

  static uint16_t v[8];
  static uint8_t nV = 0;
  v[nV] = analogRead(BATTV);

  if(ee.vadcMax < v[nV])
    ee.vadcMax = v[nV]; // calibration

  if(m_vadc < v[nV] - 29) // charging usually jumps about 300+/-, 30 on new one
    m_bCharging = true;
  else if(m_vadc > v[nV] + 29)
    m_bCharging = false;

  if(++nV >= 8) nV= 0;
  m_vadc = 0;
  for(uint8_t vi = 0; vi < 8; vi++)  // do a small average
    m_vadc += v[vi];
  m_vadc >>= 3;
}

bool Display::snooze(uint32_t ms)
{
#define S_TO_uS 1000000
#define mS_TO_uS 1000

  static uint32_t oldMs;

  if(ms != oldMs)
  { 
    oldMs = ms;
    esp_sleep_enable_ext0_wakeup( TP_INT, 0);
    esp_sleep_enable_ext1_wakeup( 1 << IMU_INT, ESP_EXT1_WAKEUP_ANY_HIGH);
    esp_sleep_enable_timer_wakeup(ms * mS_TO_uS);
    delay(100);
  }

  if(m_vadc < 1283) // 3.1V
  {
    esp_sleep_enable_timer_wakeup(60 * 60 * S_TO_uS);  // 1 hour
    delay(100);
    esp_deep_sleep_start();
  }
  esp_light_sleep_start();

  delay(1000);
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  return (wakeup_reason != ESP_SLEEP_WAKEUP_TIMER);
}

// scrolling oversized tiles and notification list
bool Display::scrollPage(uint8_t nTile, int16_t nDelta)
{
  Tile& pTile = m_tile[nTile];
  uint16_t nViewSize = DISPLAY_HEIGHT;

  if(pTile.pszTitle && pTile.pszTitle[0])
    nViewSize -= TITLE_HEIGHT;

  if(pTile.Extras & EX_NOTIF )
  {
    uint8_t nCnt;
    for(nCnt = 0; nCnt < NOTE_CNT && m_pszNotifs[nCnt]; nCnt++);

    if(nCnt) nCnt--; // fudge
    pTile.nPageHeight = nCnt * sprite.fontHeight();
    nViewSize -= 28; // clear button
  }

  if(pTile.nPageHeight <= nViewSize) // not large enough to scroll
    return false;

  pTile.nScrollIndex = constrain(pTile.nScrollIndex - nDelta, -TITLE_HEIGHT, pTile.nPageHeight - nViewSize);

  drawTile(nTile, false, 0, 0);
  sprite.pushSprite(0,0);
  return true; // scrollable
}

// calculate positions of non-fixed buttons
void Display::formatButtons(Tile& pTile)
{
  sprite.setTextDatum(MC_DATUM); // center text on button
  sprite.setFreeFont(&FreeSans9pt7b);

  uint8_t btnCnt;
  for( btnCnt = 0; btnCnt < BUTTON_CNT && pTile.button[btnCnt].row != 0xFF; btnCnt++) // count buttons
    if( pTile.button[btnCnt].r.w == 0 && pTile.button[btnCnt].pszText && pTile.button[btnCnt].pszText[0] ) // if no width
      pTile.button[btnCnt].r.w = sprite.textWidth(pTile.button[btnCnt].pszText) + (BORDER_SPACE*2);   // calc width based on text width

  if(btnCnt == 0)
    return;

  uint8_t btnIdx = 0;
  uint8_t nRows = pTile.button[btnCnt - 1].row + 1; // use last button for row total
  uint8_t nRow = 0;
  uint16_t nCenter;

  // Calc X positions
  while( nRow < nRows )
  {
    uint8_t rBtns = 0;
    uint16_t rWidth = 0;

    for(uint8_t i = btnIdx; i < btnCnt && pTile.button[i].row == nRow; i++, rBtns++)  // count buttons for current row
    {
      if( pTile.button[i].r.x == 0)// && (!pTile.button[ i ].flags & BF_FIXED_POS))
      {
        rWidth += pTile.button[i].r.w + BORDER_SPACE; // total up width of row
      // todo: find row height here, max up
      }
    }

    nCenter = DISPLAY_WIDTH/2;

    uint16_t cx = nCenter - (rWidth >> 1);

    for(uint8_t i = btnIdx; i < btnIdx+rBtns; i++)
    {
      if(pTile.button[i].r.x == 0)
      {
        pTile.button[i].r.x = cx;
        cx += pTile.button[i].r.w + BORDER_SPACE;
      }
    }

    btnIdx += rBtns;
    nRow++;
  }

  // Y positions
  uint8_t nBtnH = pTile.button[0].r.h;
  if(pTile.button[0].flags & BF_FIXED_POS) // fix: kludge for light slider
    nBtnH = pTile.button[1].r.h;

  if(nRows) nRows--; // todo: something is amiss here
  pTile.nPageHeight = nRows * (nBtnH + BORDER_SPACE) - BORDER_SPACE;

  uint16_t nViewSize = DISPLAY_HEIGHT;
  if(pTile.pszTitle && pTile.pszTitle[0])                           // shrink view by title height
    nViewSize -= TITLE_HEIGHT;

  pTile.nScrollIndex = 0;

  // ScrollIndex is added to button Y on draw and detect
  if(!(pTile.Extras & EX_NOTIF) && pTile.nPageHeight <= nViewSize) // Center smaller page
    pTile.nScrollIndex = -(nViewSize - pTile.nPageHeight) >> 1;
  else if(pTile.pszTitle && pTile.pszTitle[0])                       // Just offset by title height if too large
    pTile.nScrollIndex -= (TITLE_HEIGHT + BORDER_SPACE);             // down past title

  for(btnIdx = 0; btnIdx < btnCnt; btnIdx++ )
    if( !(pTile.button[ btnIdx ].flags & BF_FIXED_POS) )
    {
      pTile.button[ btnIdx ].r.y = pTile.button[ btnIdx ].row * (nBtnH + BORDER_SPACE);
    }
}

void Display::drawTile(int8_t nTile, bool bFirst, int16_t x, int16_t y)
{
  Tile& pTile = m_tile[nTile];

  if(pTile.pBGImage)
    sprite.pushImage(x, y, DISPLAY_WIDTH, DISPLAY_HEIGHT, pTile.pBGImage);
  else if(x == 0 && y == 0)
    sprite.fillSprite(bgColor);
  else
    sprite.fillRect(x, y, DISPLAY_WIDTH, DISPLAY_HEIGHT, bgColor);

  if(pTile.Extras & EX_NOTIF)
    drawNotifs(pTile, x, y);

  sprite.setTextDatum(MC_DATUM);
  sprite.setFreeFont(&FreeSans9pt7b);

  uint8_t btnCnt;
  for( btnCnt = 0; btnCnt < BUTTON_CNT && pTile.button[btnCnt].row != 0xFF; btnCnt++); // count buttons

  if( btnCnt )
  {
    for(uint8_t btnIdx = 0; btnIdx < btnCnt; btnIdx++ )
      drawButton( pTile, &pTile.button[ btnIdx ], false, x, y, bFirst);
  }

  if(pTile.Extras & EX_CLOCK) // draw clock over other objects
    drawClock(x, y);

  if(pTile.pszTitle && pTile.pszTitle[0])
  {
    sprite.fillRect(x, y, DISPLAY_WIDTH, TITLE_HEIGHT, bgColor); // cover any scrolled buttons/text
    sprite.setTextColor(rgb16(16,63,0), bgColor );
    sprite.drawString(pTile.pszTitle, x + DISPLAY_WIDTH/2, y + 27);
  }

  for(uint8_t j = 0; j < SLIDER_CNT; j++)
    if( pTile.slider[j].nFunc )
    {
      switch( pTile.slider[j].nFunc )
      {
        case BTF_Brightness:
          pTile.slider[j].nValue = ee.brightLevel[1] * 100 / 255;
          break;
      }
      drawArcSlider( pTile.slider[j], pTile.slider[j].nValue, x, y );
    }
}

void Display::drawButton(Tile& pTile, Button *pBtn, bool bPressed, int16_t x, int16_t y, bool bInit)
{
  String s = pBtn->pszText;

  switch(pBtn->nFunction)
  {
    case BTF_RSSI:
      btnRSSI(pBtn, x, y);
      return;
    case BTF_Time:
      if(year() < 2024) // don't display invalid time
        return;
      s = String( hourFormat12() );
      if(hourFormat12() < 10)
        s = " " + s;
      s += ":";
      if(minute() < 10) s += "0";
      s += minute();
      s += ":";
      if(second() < 10) s += "0";
      s += second();
      s += " ";
      s += isPM() ? "PM":"AM";
      s += " ";
      break;
    case BTF_Date:
      if(year() < 2024) // don't display invalid time
        return;

      s = monthShortStr(month());
      s += " ";
      s += String(day());
      break;
    case BTF_DOW:
      if(year() < 2024) // don't display invalid time
        return;
      s = dayShortStr(weekday());
      break;
    case BTF_Volts:
      {
#if (USER_SETUP_ID == 302 || USER_SETUP_ID == 303)
        float fV = 3.3 / 4096 * 3 * m_vadc;
#else
        float fV = 3.3 / 4096 * 3.22 * m_vadc; // fudged for 1.69"
#endif
        s =String(fV, 2)+"V";
      }
      break;
    case BTF_BattPerc:

      // 1550 = 3.74 = full with normal draw  (0.5V drop on 1200mAh!)
      // 1283 = 3.1V
      {
        uint8_t perc = constrain( (m_vadc - 1283) * 100 / (1550 - 1283), 0, 100);
        s = String(perc)+'%';
      }
      break;
    case BTF_Temp:
      s = String(qmi.readTemp(), 1)+"F";
      break;
    case BTF_Stat_Fan:
      if(m_bStatFan)
        pBtn->flags |= BF_STATE_2;
      else
        pBtn->flags &= ~BF_STATE_2;
      break;
    case BTF_Stat_Temp:
      s = String((float)m_statTemp/10, 1);
      break;
    case BTF_Stat_SetTemp:
      s  = String((float)m_statSetTemp/10, 1);
      break;
    case BTF_Stat_OutTemp:
      s = String((float)m_outTemp/10, 1);
      break;
    case BTF_GdoDoor:
      s = m_bGdoDoor ? "GD Open":"GD Closed";
      break;
    case BTF_GdoCar:
      s = m_bGdoCar ? "Car In":"Car Out";
      break;
    case BTF_GdoCmd:
      s = m_bGdoDoor ? "Close":"Open";
      break;
    case BTF_Lights:
      if(lights.getSwitch(pBtn->row+1)) // light is on
        pBtn->flags |= BF_STATE_2;
      else
        pBtn->flags &= ~BF_STATE_2;
      break;
    case BTF_WIFI_ONOFF:
      if(ee.bWiFiEnabled) // on
        pBtn->flags |= BF_STATE_2;
      else
        pBtn->flags &= ~BF_STATE_2;      
      break;
    case BTF_BT_ONOFF:
      if(ee.bBtEnabled) // on
        pBtn->flags |= BF_STATE_2;
      else
        pBtn->flags &= ~BF_STATE_2;      
      break;
   case BTF_Brightness:
      if(bInit) // only set when drawing tile, not user interaction
        pBtn->data[1] = m_brightness * 100 / 255;
      s = "Brightness ";
      s += pBtn->data[1];
      break;
    case BTF_PCVolume:
      s = "Volume ";
      s += pBtn->data[1];
      break;
  }

  int16_t yOffset = pBtn->r.y;

  if( !(pBtn->flags & BF_FIXED_POS) ) // don't scroll fixed pos buttons
    yOffset -= pTile.nScrollIndex;

  if(yOffset < (pTile.pszTitle[0] ? 10:0) || yOffset >= DISPLAY_HEIGHT) // cut off anything out of bounds (top is covered by title band)
    return;

  yOffset += y; // swipe offset

  const int radius = 5;
  uint8_t nState = (pBtn->flags & BF_STATE_2) ? 1:0;
  uint16_t colorBg = (nState) ? TFT_NAVY : TFT_DARKGREY;

  if(bPressed)
  {
    nState = 1;
    colorBg = TFT_DARKCYAN;
  }

  if(pBtn->flags & (BF_ARROW_UP|BF_ARROW_DOWN|BF_ARROW_LEFT|BF_ARROW_RIGHT))
  {
    sprite.fillRoundRect(x + pBtn->r.x, yOffset, pBtn->r.w, pBtn->r.h, radius, colorBg);
    sprite.drawRoundRect(x + pBtn->r.x, yOffset, pBtn->r.w, pBtn->r.h, radius, TFT_CYAN);

    const uint8_t pad = 5;
    int16_t x2 = x + pBtn->r.x + (pBtn->r.w>>1);

    if(pBtn->flags & (BF_ARROW_UP|BF_ARROW_DOWN))
      sprite.drawWideLine(x2, yOffset + pad, x2, yOffset + pBtn->r.h - pad, 4, TFT_CYAN, colorBg);
    else
      sprite.drawWideLine(x + pBtn->r.x + pad, yOffset + (pBtn->r.h>>1), x + pBtn->r.x + pBtn->r.w - pad, yOffset + (pBtn->r.h>>1), 4, TFT_CYAN, colorBg);

    if(pBtn->flags & (BF_ARROW_UP|BF_ARROW_LEFT))
      sprite.drawWideLine(x2, yOffset + pad, x + pBtn->r.x + pad, yOffset + (pBtn->r.h>>1), 4, TFT_CYAN, colorBg);

    if(pBtn->flags & (BF_ARROW_UP|BF_ARROW_RIGHT))
      sprite.drawWideLine(x2, yOffset + pad, x + pBtn->r.x + pBtn->r.w - pad, yOffset + (pBtn->r.h>>1), 4, TFT_CYAN, colorBg);

    if(pBtn->flags & (BF_ARROW_DOWN|BF_ARROW_LEFT))
      sprite.drawWideLine(x + pBtn->r.x + pad, yOffset + (pBtn->r.h>>1), x2, yOffset + pBtn->r.h - pad, 4, TFT_CYAN, colorBg);

    if(pBtn->flags & (BF_ARROW_DOWN|BF_ARROW_RIGHT))
      sprite.drawWideLine(x + pBtn->r.x + pBtn->r.w - pad, yOffset + (pBtn->r.h>>1), x2, yOffset + pBtn->r.h - pad, 4, TFT_CYAN, colorBg);
  }
  else if(pBtn->pIcon[nState]) // draw an image if given
    sprite.pushImage(x + pBtn->r.x, yOffset, pBtn->r.w, pBtn->r.h, pBtn->pIcon[nState]);
  else if(pBtn->flags & BF_BORDER) // bordered text item
    sprite.drawRoundRect(x + pBtn->r.x, yOffset, pBtn->r.w, pBtn->r.h, radius, colorBg);
  else if( (pBtn->flags & (BF_TEXT|BF_SLIDER_H|BF_SLIDER_V)) == 0 ) // if no image, or no image for state 2, and not just text, fill with a color
    sprite.fillRoundRect(x + pBtn->r.x, yOffset, pBtn->r.w, pBtn->r.h, radius, colorBg);

  const uint8_t dotSz = 10;
  if(pBtn->flags & BF_SLIDER_H)
    sprite.drawWideLine(x + pBtn->r.x, yOffset + (pBtn->r.h>>1), x + pBtn->r.x + pBtn->r.w, yOffset + (pBtn->r.h>>1), dotSz<<1, bgColor, bgColor);
  else if(pBtn->flags & BF_SLIDER_V)
    sprite.drawWideLine(x + pBtn->r.x + (pBtn->r.w>>1), yOffset, x + pBtn->r.x + (pBtn->r.w>>1), yOffset + pBtn->r.h, dotSz<<1, bgColor, bgColor); // erase old spot

  if(s.length())
  {
    if(pBtn->pFont)
      sprite.setFreeFont(pBtn->pFont);
    else
      sprite.setFreeFont(&FreeSans9pt7b); // default font

    if(pBtn->flags & BF_TEXT) // text only
      sprite.setTextColor(pBtn->textColor ? pBtn->textColor : TFT_WHITE,  bgColor);
    else
      sprite.setTextColor(TFT_CYAN, colorBg);

    if(pBtn->flags & BF_SLIDER_V) // vertical slider text
      drawText(s, x + pBtn->r.x + 26, yOffset + (pBtn->r.h >> 1), 270, pBtn->textColor ? pBtn->textColor : TFT_WHITE,  bgColor, pBtn->pFont);
    else // normal text
      sprite.drawString( s, x + pBtn->r.x + (pBtn->r.w >> 1), yOffset + (pBtn->r.h >> 1) - 2 );
  }

  if(pBtn->flags & BF_SLIDER_H)
  {
    sprite.drawWideLine(x + pBtn->r.x, yOffset + (pBtn->r.h>>1), x + pBtn->r.x + pBtn->r.w, yOffset + (pBtn->r.h>>1), 5, TFT_BLUE, bgColor);
    sprite.drawSpot(x + pBtn->r.x + dotSz + pBtn->data[1] * (pBtn->r.w - dotSz*2) / 100, yOffset + 15, dotSz, TFT_YELLOW);
  }
  else if(pBtn->flags & BF_SLIDER_V)
  {
    sprite.drawWideLine(x + pBtn->r.x + (pBtn->r.w>>1), yOffset, x + pBtn->r.x + (pBtn->r.w>>1), yOffset + pBtn->r.h, 5, TFT_BLUE, bgColor);
    sprite.drawSpot(x + pBtn->r.x + (pBtn->r.w>>1), yOffset + dotSz + (100 - pBtn->data[1]) * (pBtn->r.h - dotSz*2) / 100, dotSz, TFT_YELLOW);
  }
}

// called when button is activated
void Display::buttonCmd(Button *pBtn, bool bRepeat)
{
  uint16_t code[4];

  switch(pBtn->nFunction)
  {
    case BTF_LightsDimmer: // light dimmer (-1 = last active)
      lights.setSwitch( -1, true, pBtn->data[1]);
      break;
    case BTF_PCVolume: // PC volume
      code[0] = 1000;
      code[1] = pBtn->data[1];
      sendPCMediaCmd(code);
      break;
    case BTF_Brightness: // screen brightness
      m_brightness = ee.brightLevel[1] = pBtn->data[1] * 255 / 100; // 100% = 255
      break;

    case BTF_Lights:
      if(ee.bWiFiEnabled)
        if(!lights.setSwitch( pBtn->row+1, !lights.getSwitch(pBtn->row+1), 0 ) )
          notify("Light command failed");
      break;

    case BTF_IR:
      sendIR(pBtn->data);
      break;

    case BTF_BT_ONOFF:
      ee.bBtEnabled = !ee.bBtEnabled;
      if(ee.bBtEnabled)
        btStart();
      else
        btStop();

      pBtn->pszText = (ee.bBtEnabled) ? "Bluetooth On" : "Bluetooth Off";
      notify((char *)pBtn->pszText);

      if(ee.bBtEnabled)
        pBtn->flags |= BF_STATE_2;
      else
        pBtn->flags &= ~BF_STATE_2;
      break;

    case BTF_WIFI_ONOFF:
      ee.bWiFiEnabled = !ee.bWiFiEnabled;
      if(ee.bWiFiEnabled)
        startWiFi();
      else
        stopWiFi();

      pBtn->pszText = (ee.bWiFiEnabled) ? "WiFi On" : "WiFi Off";
      notify((char *)pBtn->pszText);
      if(ee.bWiFiEnabled)
        pBtn->flags |= BF_STATE_2;
      else
        pBtn->flags &= ~BF_STATE_2;
      break;

    case BTF_Restart:
      ESP.restart();
      break;

    case BTF_BLE:
      if( !sendBLE(pBtn->data) )
        notify("BLE command failed");
      break;

    case BTF_PC_Media:
      if( !sendPCMediaCmd(pBtn->data) )
        notify("PC command failed");
      break;

    case BTF_StatCmd:
    case BTF_Stat_Fan:
      if( !sendStatCmd(pBtn->data) )
        notify("Stat command failed");
      break;

    case BTF_GdoCmd:
      if( !sendGdoCmd(pBtn->data) )
        notify("GDO command failed");
      break;

    case BTF_Clear:
      memset(m_pszNotifs, 0, sizeof(m_pszNotifs) );
      break;
  }
}

void Display::sliderCmd(uint8_t nFunc, uint8_t nNewVal)
{
  uint16_t code[4];

  switch( nFunc)
  {
    case BTF_LightsDimmer: // light dimmer (-1 = last active)
      lights.setSwitch( -1, true, nNewVal);
      break;
    case BTF_PCVolume: // PC volume
      code[0] = 1000;
      code[1] = nNewVal;
      sendPCMediaCmd(code);
      break;
    case BTF_Brightness: // screen brightness
      m_brightness = ee.brightLevel[1] = nNewVal * 255 / 100; // 100% = 255
      break;
  }
}

// Pop uup a notification + add to notes list
void Display::notify(char *pszNote)
{
  if(m_bSleeping == false)
  {
    if( m_brightness < ee.brightLevel[1] ) // make it bright if not
      exitScreensaver();

    tft.setFreeFont(&FreeSans9pt7b);

    uint16_t w = tft.textWidth(pszNote) + (BORDER_SPACE*2);
    if(w > 230) w = 230;
    int16_t x = (DISPLAY_WIDTH/2) - (w >> 1); // center on screen
    const int16_t y = 30; // kind of high up

    tft.fillRoundRect(x, y, w, tft.fontHeight() + 3, 5, TFT_ORANGE);
    tft.setTextColor(TFT_BLACK, TFT_ORANGE);
    tft.drawString( pszNote, x + BORDER_SPACE, y + BORDER_SPACE );
  }

  for(int8_t i = NOTE_CNT - 2; i >= 0; i--) // move old notifications down
    m_pszNotifs[i+1] = m_pszNotifs[i];
  m_pszNotifs[0] = pszNote;
  m_nDispFreeze = millis() + 900; // pop the toast up for 900ms
}

// draw the notifs list
void Display::drawNotifs(Tile& pTile, int16_t x, int16_t y)
{
  sprite.setTextDatum(TL_DATUM);
  sprite.setTextColor(TFT_WHITE, bgColor );
  sprite.setFreeFont(&FreeSans9pt7b);

  x += 40; // left padding
  y -= pTile.nScrollIndex; // offset by the scroll pos

  for(uint8_t i = 0; i < NOTE_CNT && m_pszNotifs[i]; i++)
  {
    if(y > 30) // somewhat covered by title
      sprite.drawString(m_pszNotifs[i], x, y);
    y += sprite.fontHeight();
    if(y > DISPLAY_HEIGHT + 4)
      break;
  }
}

// smooth adjust brigthness (0-255)
void Display::dimmer()
{
  static bool bDelay;

  bDelay = !bDelay;
  if(m_bright == m_brightness || bDelay)
    return;

  if(m_brightness > m_bright + 1 && ee.brightLevel[1] > 50)
    m_bright += 2;
  else if(m_brightness > m_bright)
    m_bright ++;
  else
    m_bright --;

  analogWrite(TFT_BL, m_bright);
}

void Display::drawArcSlider(ArcSlider& slider, uint8_t newValue, int16_t x, int16_t y)
{
  int16_t angle;
  float deg;
  uint16_t dist = 107;
  int16_t ax, ay;

  if(slider.nValue != newValue) // remove last spot
  {
    angle = slider.nPos;

    if(slider.flags & SFL_REVERSE)
    {
      angle -= slider.nSize >> 1;
      angle += slider.nValue * slider.nSize / 100;
    }
    else
    {
      angle += slider.nSize >> 1;
      angle -= slider.nValue * slider.nSize / 100;
    }
    if(angle < 180) angle += 360;
    if(angle > 360) angle -= 360;
    cspoint(ax, ay, DISPLAY_WIDTH/2, DISPLAY_HEIGHT/2, angle, dist);
    sprite.drawSpot(x + ax, y + ay, 11, bgColor); // remove old spot
  }

  // draw slider
  int16_t nStart = 180 + slider.nPos - (slider.nSize >> 1);
  if(nStart > 359) nStart -= 360;
  if(nStart < 0) nStart += 360;
  uint16_t nEnd = nStart + slider.nSize;

  if(nEnd > 360) // draw 2 parts if it spans past 360deg
  {
    sprite.drawSmoothArc(x+DISPLAY_WIDTH/2, x+DISPLAY_HEIGHT/2, dist+4, dist-1, nStart, 359.9, TFT_DARKCYAN, bgColor, true);
    nStart = 0;
    nEnd -= 360;
  }
  sprite.drawSmoothArc(x+DISPLAY_WIDTH/2,y+DISPLAY_HEIGHT/2, dist+4, dist-1, nStart, nEnd, TFT_DARKCYAN, bgColor, true);

  // draw new spot
  angle = slider.nPos;

  if(slider.flags & SFL_REVERSE)
  {
    angle -= slider.nSize >> 1;
    angle += newValue * slider.nSize / 100;
  }
  else
  {
    angle += slider.nSize >> 1;
    angle -= newValue * slider.nSize / 100;
  }
  if(angle < 180) angle += 360;
  if(angle > 360) angle -= 360;

  if(slider.pszText)
    drawArcText(slider.pszText, x, y, slider.pFont, slider.textColor, angle, 90);

  cspoint(ax, ay, DISPLAY_WIDTH/2, DISPLAY_HEIGHT/2, angle, dist);
  sprite.drawSpot(x+ax, y+ay, 10, TFT_YELLOW);
}

// check for edge slider hit and return 0-100 value
bool Display::arcSliderHit(ArcSlider& slider, uint8_t& value)
{
  int16_t dist = sqrt( pow((DISPLAY_WIDTH/2) - touch.x, 2) + pow((DISPLAY_HEIGHT/2) - touch.y, 2) );
  if(dist < 110) // touch on edge
    return false;

  int16_t deg = 180 - atan2(touch.x - (DISPLAY_WIDTH/2), touch.y - (DISPLAY_HEIGHT/2) ) * (180 / PI);
  deg -= (slider.nPos - (slider.nSize >> 1)); // 
  deg = deg * 100 / slider.nSize; // convert range to %
  if( !(slider.flags & SFL_REVERSE) )
    deg = 100 - deg;
  if(deg < 0 || deg > 100) // touch not over slider
    return false;
  value = deg;
  return true;
}

// Draw RSSI bars
void Display::btnRSSI(Button *pBtn, int16_t x, int16_t y)
{
#define RSSI_CNT 8
  static int16_t rssi[RSSI_CNT];
  static uint8_t rssiIdx = 0;

  if(WiFi.status() != WL_CONNECTED)
    return;

  rssi[rssiIdx] = WiFi.RSSI();
  if(++rssiIdx >= RSSI_CNT) rssiIdx = 0;

  int16_t rssiAvg = 0;
  for(int i = 0; i < RSSI_CNT; i++)
    rssiAvg += rssi[i];

  rssiAvg /= RSSI_CNT;

  const uint8_t wh = pBtn->r.w; // width and height
  const uint8_t sect = 127 / 5; //
  const uint8_t dist = wh  / 5; // distance between blocks
  int16_t sigStrength = 127 + rssiAvg;

  for (uint8_t i = 1; i < 6; i++)
  {
    sprite.fillRect( x + pBtn->r.x + i*dist, y + pBtn->r.y - i*dist, dist-2, i*dist, (sigStrength > i * sect) ? TFT_CYAN : bgColor );
  }
}

// Analog clock
void Display::drawClock(int16_t x, int16_t y)
{
  const float fx = DISPLAY_WIDTH/2 + x; // center
  const float fy = DISPLAY_HEIGHT/2 + y;

  if(ee.bWiFiEnabled && (year() < 2024 || WiFi.status() != WL_CONNECTED)); // Still connecting
  else sprite.drawSmoothCircle(fx, fy, fx-1, m_bCharging ? TFT_BLUE : TFT_WHITE, bgColor); // draw outer ring

  for(int16_t i = 0; i < 360; i += 6) // drawing the watchface instead of an image saves 9% of PROGMEM
  {
    int16_t xH,yH,xH2,yH2;
    cspoint(xH, yH, fx, fy, i, 116); // outer edge
    cspoint(xH2, yH2, fx, fy, i, (i%30) ? 105:96); // 60 ticks/12 longer ticks
    sprite.drawWideLine(xH, yH, xH2, yH2, (i%30) ? 2:3, TFT_SILVER, bgColor); // 12 wider ticks
  }

  if(year() < 2024)
    return;

  int16_t xH,yH, xM,yM, xS,yS, xS2,yS2;

  float a = (hour() + (minute() * 0.00833) ) * 30;
  cspoint(xH, yH, fx, fy, a, 64);
  cspoint(xM, yM, fx, fy, minute() * 6, 87);
  cspoint(xS, yS, fx, fy, second() * 6, 96);
  cspoint(xS2, yS2, fx, fy, (second()+30) * 6, 24);

  sprite.drawWedgeLine(fx, fy, xH, yH, 8, 2, TFT_LIGHTGREY, bgColor); // hour hand
  sprite.drawWedgeLine(fx, fy, xM, yM, 5, 2, TFT_LIGHTGREY, bgColor); // minute hand
  sprite.fillCircle(fx, fy, 8, TFT_LIGHTGREY ); // center cap
  sprite.drawWideLine(xS2, yS2, xS, yS, 2.5, TFT_RED, bgColor ); // second hand
}

// calc point for a clock hand and slider
void Display::cspoint(int16_t &x2, int16_t &y2, int16_t x, int16_t y, float angle, float size)
{
  float ang =  M_PI * (180 - angle) / 180;
  x2 = x + size * sin(ang);
  y2 = y + size * cos(ang);
}

// update the PC volume slider
void Display::setSliderValue(uint8_t st, int8_t iValue)
{
  for(uint8_t i = 0; i < m_nTileCnt; i++)
  {
    for(uint8_t j = 0; j < SLIDER_CNT; j++)
    {
      if(m_tile[i].slider[j].nFunc == st)
        m_tile[i].slider[j].nValue = iValue;
    }

    Button *pBtn = &m_tile[i].button[0];
    for(uint8_t j = 0; pBtn[j].r.x && j < BUTTON_CNT; j++)
    {
      if(m_tile[i].button[j].nFunction == st)
        m_tile[i].button[j].data[1] = iValue;
    }
  }
}

void Display::drawText(String s, int16_t x, int16_t y, int16_t angle, uint16_t cFg, uint16_t cBg, const GFXfont *pFont)
{
  TFT_eSprite textSprite = TFT_eSprite(&tft);

#if (USER_SETUP_ID==302) // 240x240
  textSprite.setSwapBytes(true);
#endif
  textSprite.setTextDatum(TL_DATUM);
  textSprite.setTextColor(cFg, cBg);
  textSprite.setFreeFont(pFont ?  pFont : &FreeSans9pt7b);
  int16_t w = textSprite.textWidth(s) + 2;
  int16_t h = textSprite.fontHeight() + 2;
  textSprite.createSprite(w, h);
  textSprite.fillSprite( bgColor );

  textSprite.drawString(s, 0, 0);
  sprite.setPivot(x, y);
  textSprite.pushRotated(&sprite, angle);
//  sprite.setPivot(0, 0);
}

void Display::drawArcText(String s, int16_t x, int16_t y, const GFXfont *pFont, uint16_t color, int16_t angle, int8_t distance)
{
  TFT_eSprite textSprite = TFT_eSprite(&tft);

#if (USER_SETUP_ID==302) // 240x240
  textSprite.setSwapBytes(true);
#endif

  textSprite.setTextDatum(TL_DATUM);
  textSprite.setTextColor(color ? color : TFT_WHITE, bgColor );
  textSprite.setFreeFont(pFont ? pFont : &FreeSans9pt7b);

  int16_t w = textSprite.textWidth("A") + 1;
  int16_t h = textSprite.fontHeight() + 3;
  textSprite.createSprite(w, h);

  uint8_t cnt = s.length();
  angle -= textSprite.textWidth(s) / 2;

  const float fx = DISPLAY_WIDTH/2; // center
  const float fy = DISPLAY_HEIGHT/2;
  int16_t x1, y1;
  String chStr;

  for(uint8_t i = 0; i < cnt; i++)
  {
    chStr = s.charAt(i);

    cspoint(x1, y1, fx, fy, angle, distance);

    textSprite.fillSprite( bgColor );
    textSprite.drawString(chStr, 0, 0);
    sprite.setPivot(x+x1, y+y1);
    textSprite.pushRotated(&sprite, angle);
    angle += textSprite.textWidth( chStr );
  }
  sprite.setPivot(0, 0);
}

// Flash a red indicator for RX, TX, PC around the top 
void Display::RingIndicator(uint8_t n)
{
#ifdef ROUND_DISPLAY
  uint16_t ang = 130;

  switch(n)
  {
    case 0: // IR TX
      ang = 160;
      break;
    case 1: // IR RX
      ang = 190;
      break;
    case 2: // PC TX
      ang = 148;
      break;
  }

  tft.drawSmoothArc(DISPLAY_WIDTH/2, DISPLAY_HEIGHT/2, 119, 115, ang, ang+8, TFT_RED, bgColor, true);

#else // non-round displays

  uint16_t pos = 0;

  switch(n)
  {
    case 0: // IR TX
      pos = 10;
      break;
    case 1: // IR RX
      pos = DISPLAY_WIDTH/2 - 20;
      break;
    case 2: // PC TX
      pos = DISPLAY_WIDTH - 40;
      break;
  }

  tft.drawWideLine(pos, 5, pos + 20, 5, 4, TFT_RED, bgColor);
#endif
}
