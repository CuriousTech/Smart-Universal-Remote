/*
Display and UI code
*/

#include <WiFi.h>
#include "display.h"
#include <TimeLib.h>
#include "eeMem.h"
#include <TFT_eSPI.h> // TFT_eSPI library from library manager?
#include "CST816T.h"
#include "Lights.h"
#include <FunctionalInterrupt.h>
#include <math.h>

#include "QMI8658.h"

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);
TFT_eSprite sprite2 = TFT_eSprite(&tft);

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

#define I2C_SDA   6
#define I2C_SCL   7
#define IMU_INT1  4 // normally high
#define IMU_INT2  3 // normally low by UMI WOM
#define TP_RST   13
#define TP_INT    5 // normally high
#define TFT_BL    2
#define BATTV     1

CST816T touch(I2C_SDA, I2C_SCL, TP_RST, TP_INT);
QMI8658 qmi;

#include "screensavers.h" // comment this line to remove screensavers (saves 3172b PROGMEM, 1072b SRAM)
#ifdef SCREENSAVERS_H
ScreenSavers ss;
#endif

#define TITLE_HEIGHT 40
#define BORDER_SPACE 3 // H and V spacing + font size for buttons and space between buttons

void Display::init(void)
{
  pinMode(TFT_BL, OUTPUT);

  tft.init();
  tft.setSwapBytes(true);
  tft.setTextPadding(8); 

  sprite.createSprite(DISPLAY_WIDTH, DISPLAY_HEIGHT); // This uses over 115K RAM
  sprite.setSwapBytes(true);
  sprite2.createSprite(DISPLAY_WIDTH, DISPLAY_HEIGHT); // This uses over 115K RAM
  sprite2.setSwapBytes(true);

  // count tiles, index rows, count columns, format the buttons
  uint8_t nRowIdx = 0;

  for(m_nTileCnt = 0; m_nTileCnt < TILES && (m_tile[m_nTileCnt].Extras || m_tile[m_nTileCnt].pszTitle || m_tile[m_nTileCnt].pBGImage); m_nTileCnt++) // count screens
  {
    if(m_tile[m_nTileCnt].nRow != nRowIdx)
      m_nRowStart[++nRowIdx] = m_nTileCnt;
    m_nColCnt[nRowIdx]++;
    formatButtons(m_tile[m_nTileCnt]);
  }

  drawTile(m_nCurrTile, true);
  sprite.pushSprite(0, 0); // draw screen 0 on start

  pinMode(IMU_INT1, INPUT_PULLUP);
  pinMode(IMU_INT2, INPUT_PULLUP);
  qmi.init();
  touch.begin();
  attachInterrupt(IMU_INT1, std::bind(&Display::handleISR1, this), FALLING);
  attachInterrupt(IMU_INT2, std::bind(&Display::handleISR2, this), RISING);


  m_vadc = analogRead(BATTV);
  if(m_vadc > 1890) // probably full and charging
    m_bCharging = true;

  qmi.setWakeOnMotion(false); // set normal gyro/acc
  qmi.read_gyro_xyz(m_gyroCal);

  randomSeed(micros());
}

void Display::exitScreensaver()
{
  m_backlightTimer = DISPLAY_TIMEOUT; // reset timer for any touch

  if( m_brightness == ee.brightLevel[0] ) // screensaver active
  {
    drawTile(m_nCurrTile, true); // draw over screensaver
    sprite.pushSprite(0, 0);
    qmi.setWakeOnMotion(false); // enable gyro/acc/temp
    m_int2Triggered = false; // causes an interrupt
    qmi.read_gyro_xyz(m_gyroCal); // testing this
  }
  m_brightness = ee.brightLevel[1]; // increase brightness for any touch
}

void Display::service(void)
{
  static Button *pCurrBtn; // for button repeats and release
  static uint32_t lastms; // for button repeats

  if( m_int1Triggered) // may be used later
  {
    m_int1Triggered = false;
    ets_printf("INT1\n");
    m_backlightTimer = DISPLAY_TIMEOUT; // reset timer for any touch
//    endSleep();
  }

  if( m_int2Triggered) // set for IMU WOM
  {
    ets_printf("INT2\n");
    m_int2Triggered = false;
    m_backlightTimer = DISPLAY_TIMEOUT;
    if( m_brightness < ee.brightLevel[1] )
      exitScreensaver();
  }

  if( m_tpintTriggered) // may be used later
  {
    m_tpintTriggered = false;
    ets_printf("TP_INT\n");
  }

  if(m_bSleeping)
  {
    if( snooze(30000) )
    {
      endSleep();
      m_backlightTimer = DISPLAY_TIMEOUT;
      if( m_brightness < ee.brightLevel[1] )
        exitScreensaver();
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

#ifdef SCREENSAVERS_H
  if(m_brightness == ee.brightLevel[0]) // full dim activates screensaver
    ss.run();
  else if(m_tile[m_nCurrTile].Extras & EX_TEST)
    testTile();
#endif

  static bool bScrolling; // scrolling active
  static bool bSliderHit;
  static int16_t touchXstart, touchYstart; // first touch pos for scolling and non-moving detect
  static uint32_t touchStartMS; // timing for detecting swipe vs press

  if(touch.available() )
  {
    if(touch.event == 1) // release
    {
      if(!bSliderHit)
      {
        if((int16_t)touch.y - (int16_t)touchYstart > 14)
          touch.gestureID = SWIPE_DOWN;
        else if((int16_t)touch.y - (int16_t)touchYstart < -14)
          touch.gestureID = SWIPE_UP;
        else if((int16_t)touch.x - (int16_t)touchXstart > 14)
          touch.gestureID = SWIPE_RIGHT;
        else if((int16_t)touch.x - (int16_t)touchXstart < -14)
          touch.gestureID = SWIPE_LEFT;
      }

      if(pCurrBtn)
      {
        drawButton(m_tile[m_nCurrTile], pCurrBtn, false); // draw released button
        sprite.pushSprite(0, 0);
        pCurrBtn = NULL;
      }
      else if(touch.gestureID)
      {
        swipeTile();
      }
    }
    else if(touch.event == 0) // initial touch
    {
      touchXstart = touch.x;
      touchYstart = touch.y;
      touch.gestureID = 0;
      bSliderHit = false;
      bScrolling = false;
      touchStartMS = millis();

      m_backlightTimer = DISPLAY_TIMEOUT; // reset timer for any touch
  
      if( m_brightness < ee.brightLevel[1] )
      {
        exitScreensaver();
        endSleep();
      }
    }
    else // 2=active
    {
      if( m_brightness < ee.brightLevel[1] ) // ignore input until brightness is full
        return;

      Tile& pTile = m_tile[m_nCurrTile];

      // Handle slider
      uint8_t sliderVal;
      if(pTile.nSliderType && sliderHit(sliderVal) )
      {
        drawSlider(sliderVal);
        sprite.pushSprite(0, 0);
        if(pTile.nSliderValue != sliderVal)
        {
          sliderCmd(pTile.nSliderType, pTile.nSliderValue, sliderVal);
          pTile.nSliderValue = sliderVal;
        }
        bSliderHit = true;
      }
      else if(bScrolling) // scrolling detected
      {
        if(touch.y - touchYstart)
          scrollPage(m_nCurrTile, touch.y - touchYstart);
        touchYstart = touch.y;
      }
      else // normal press or hold
      {
        if(millis() - touchStartMS < 100 && touch.gestureID == 0) // delay a bit
        {

        }
        else if(pCurrBtn == NULL) // check button hit
        {
          Button *pBtn = &pTile.button[0];
          for(uint8_t i = 0; pBtn[i].x; i++) // check for press in button bounds without slide
          {
            int16_t y = pBtn[i].y;

            if( !(pBtn->flags & BF_FIXED_POS) ) // adjust for scroll offset
              y -= pTile.nScrollIndex;
  
            if ( (touch.x >= pBtn[i].x && touch.x <= pBtn[i].x + pBtn[i].w && touch.y >= y && touch.y <= y + pBtn[i].h)
               && (touchXstart >= pBtn[i].x && touchXstart <= pBtn[i].x + pBtn[i].w && touchYstart >= y && touchYstart <= y + pBtn[i].h) ) // make sure not swiping
            {
              pCurrBtn = &pBtn[i];
              drawButton(pTile, pCurrBtn, true); // draw pressed state
              sprite.pushSprite(0, 0);
              buttonCmd(pCurrBtn, false);
              lastms = millis() - 300; // slow first repeat (300+300ms)
              break;
            }
          }
        }
        else if(millis() - lastms > 300) // repeat speed
        {
          if(pCurrBtn && (pCurrBtn->flags & BF_REPEAT) )
            buttonCmd(pCurrBtn, true);
          lastms = millis();
        }

        if((pTile.Extras & EX_SCROLL) )
        {
          if( touchYstart > 40 && touchYstart < DISPLAY_HEIGHT - 40 && touch.y != touchYstart && (touch.y - touchYstart < 10 || touchYstart - touch.y < 10)) // slow/short flick
          {
            if( touchXstart > 40 && touchXstart < DISPLAY_WIDTH - 40) // start needs to be in the middle somewhere
              bScrolling = true;
          }
        }
      }
    }
  }

  if(ee.bWiFiEnabled && (year() < 2024 || WiFi.status() != WL_CONNECTED)) // Do a connecting effect
  {
    static int16_t ang;
    uint16_t ang2;
    static uint8_t rgb[3] = {32, 128, 128};

    ang2 = ang;
    for(uint8_t i = 0; i < 12; i++)
    {
      tft.drawArc(DISPLAY_WIDTH/2, DISPLAY_HEIGHT/2, 120, 117, ang2, ang2 + 5, (WiFi.status() == WL_CONNECTED) ? tft.color565(rgb[0], rgb[1], rgb[2]):TFT_RED, TFT_BLACK, false);
      ang2 += 30;
      ang2 %= 360;
    }
    ++ang %= 360;
    for(uint8_t i = 0; i < 3; i++)
    {
      rgb[i] += random(-3, 6);
      rgb[i] = (uint8_t)constrain(rgb[i], 2, 255);
    }
  }
}

void Display::startSleep()
{
  if(m_bSleeping || m_sleepTimer)
    return;

  m_bSleeping = true;
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

// A tile for playing around
void Display::testTile()
{
  static uint8_t dly = 8;
  if(--dly)
    return;
  dly = 8;

  static float gyroPos[3] = {120, 120, 180};
  float gyro[3];

  qmi.read_gyro_xyz(gyro);

  if(gyro[0] == 0)
  {
    qmi.enableSensors(7);
    qmi.read_gyro_xyz(m_gyroCal);
  }

  sprite.drawArc(DISPLAY_WIDTH/2, DISPLAY_HEIGHT/2, 111, 108, (int)gyroPos[2], ((int)gyroPos[2]+4) % 360, TFT_BLACK, TFT_BLACK, false);
  sprite.fillCircle(gyroPos[0], gyroPos[1], 8, TFT_BLACK );

  gyroPos[0] += ((gyro[0]-m_gyroCal[0]) / 100);
  gyroPos[1] += ((gyro[1]-m_gyroCal[1]) / 100);
  gyroPos[2] -= ((gyro[2]-m_gyroCal[2]) / 280);

  gyroPos[0] = constrain(gyroPos[0], 0, 240);
  gyroPos[1] = constrain(gyroPos[1], 0, 240);
  if(gyroPos[2] < 0)  gyroPos[2] += 360;
  if(gyroPos[2] >= 360)  gyroPos[2] -= 360;

  uint16_t r = gyroPos[2];
  uint16_t y = gyroPos[1];
  uint16_t x = gyroPos[0];

  sprite.fillCircle(x, y, 8, TFT_BLUE );
  sprite.drawArc(DISPLAY_WIDTH/2, DISPLAY_HEIGHT/2, 111, 108, (int)gyroPos[2], ((int)gyroPos[2]+4) % 360, TFT_RED, TFT_BLACK, false);
  sprite.pushSprite(0, 0);
}

// swipe effects
void Display::swipeTile()
{
  static uint8_t nCurrRow;
  uint8_t nRowOffset;

  switch(touch.gestureID)
  {
    case SWIPE_RIGHT:
      if(m_nCurrTile <= 0) // don't screoll if button pressed or screen 0
        break;
  
      if(m_tile[m_nCurrTile].nRow != m_tile[m_nCurrTile-1].nRow) // don't scroll left if leftmost
        break;
  
      sprite.pushToSprite(&sprite2, 0, 0); // copy to temp sprite

      m_nCurrTile--;
      drawTile(m_nCurrTile, true);
      for(int16_t i = -DISPLAY_WIDTH; i <= 0; i += 12)
      {
         // Push a windowed area of the sprite to the TFT at tx, ty
        //  bool     pushSprite(int32_t tx, int32_t ty, int32_t sx, int32_t sy, int32_t sw, int32_t sh);
        
        sprite2.pushSprite(i + DISPLAY_WIDTH, 0);
        sprite.pushSprite(i, 0);
      }
      break;
    case SWIPE_LEFT:
      if(m_nCurrTile >= m_nTileCnt - 1) // don't scroll if button pressed or last screen
        break;
  
      if(m_tile[m_nCurrTile].nRow != m_tile[m_nCurrTile+1].nRow) // don't scroll right if rightmost
        break;
  
      sprite.pushToSprite(&sprite2, 0, 0); // copy to temp sprite

      m_nCurrTile++;
      drawTile(m_nCurrTile, true);
      for(int16_t i = DISPLAY_WIDTH; i >= 0; i -= 12)
      {
        sprite2.pushSprite(i - DISPLAY_WIDTH, 0);
        sprite.pushSprite(i, 0);
      }
      break;
    case SWIPE_DOWN:
      if(m_nCurrTile == 0 || nCurrRow == 0) // don't slide up from top
        break;
  
      nRowOffset = m_nCurrTile - m_nRowStart[nCurrRow];
      nCurrRow--;
      m_nCurrTile = m_nRowStart[nCurrRow] + min((int)nRowOffset, m_nColCnt[nCurrRow]-1 ); // get close to the adjacent screen

      sprite.pushToSprite(&sprite2, 0, 0); // copy to temp sprite

      drawTile(m_nCurrTile, true);
      for(int16_t i = -DISPLAY_HEIGHT; i <= 0; i += 12)
      {
        sprite2.pushSprite(0, i + DISPLAY_HEIGHT);
        sprite.pushSprite(0, i);
      }
      break;
    case SWIPE_UP:
      nRowOffset = m_nCurrTile - m_nRowStart[nCurrRow];
      nCurrRow++;
      { // fix
        uint8_t nCS = m_nRowStart[nCurrRow] + min((int)nRowOffset, m_nColCnt[nCurrRow]-1 );
        if(nCS < m_nTileCnt) m_nCurrTile = nCS;
        else{ nCurrRow--; break;}// back out
      }
      sprite.pushToSprite(&sprite2, 0, 0); // copy to temp sprite
      drawTile(m_nCurrTile, true);
      for(int16_t i = DISPLAY_HEIGHT; i >= 0; i -= 12)
      {
        sprite2.pushSprite(0, 0, 0, DISPLAY_HEIGHT - i, DISPLAY_WIDTH, i); // TFT_eSPI crops so this is no faster
        sprite.pushSprite(0, i, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT - i);
      }
      break;
  }
}

void IRAM_ATTR Display::handleISR1(void)
{
  m_int1Triggered = true;
}
void IRAM_ATTR Display::handleISR2(void)
{
  m_int2Triggered = true;
}
void IRAM_ATTR Display::handleTPISR(void)
{
  m_tpintTriggered = true;
}

// called each second
void Display::oneSec()
{
  if(m_bSleeping || m_nDispFreeze) // don't overwrite the popup
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
      m_int2Triggered = false; // causes an interrupt
      m_sleepTimer = ee.sleepTime;
    }
  }

  if(m_brightness > ee.brightLevel[0] && m_nDispFreeze == 0) // not screensaver
  {
    drawTile(m_nCurrTile, false);
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
      // Find SL_Lights screen
      uint8_t nLi = 0;
      while(m_tile[nLi].nSliderType != SL_Lights && (m_tile[nLi].pszTitle || m_tile[nLi].Extras) )
        nLi++;

      Tile& pTile = m_tile[nLi];

      if(pTile.nSliderType == SL_Lights)
      {
        for(uint8_t i = 0; i < BUTTON_CNT; i++)
        {
          if(ee.lights[i].szName[0] == 0)
            break;
          pTile.button[i].pszText = ee.lights[i].szName;
          pTile.button[i].ID = i + 1;
          pTile.button[i].row = i;
          pTile.button[i].w = 110;
          pTile.button[i].h = 28;
          pTile.button[i].nFunction = BTF_Lights;
        }
      }
      formatButtons(pTile);
    }
  }

  uint16_t v = analogRead(BATTV);

  if(m_vadc < v - 50) // charging usually jumps about 300+
    m_bCharging = true;
  else if(m_vadc > v + 50)
    m_bCharging = false;
  m_vadc = v;
}

bool Display::snooze(uint32_t ms)
{
#define S_TO_uS 1000000
#define mS_TO_uS 1000

  static uint32_t oldMs;

  if(ms != oldMs)
  { 
    oldMs = ms;
    esp_sleep_enable_ext0_wakeup( GPIO_NUM_5, 0); // TP_INT
    esp_sleep_enable_ext1_wakeup( 1 << IMU_INT2, ESP_EXT1_WAKEUP_ANY_HIGH);
    if(ms > 10000) ms = 10000;
    esp_sleep_enable_timer_wakeup(ms * mS_TO_uS);
    delay(100);
  }
  esp_light_sleep_start();

  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  return (wakeup_reason != ESP_SLEEP_WAKEUP_TIMER);
}

// scrolling oversized tiles and notification list
void Display::scrollPage(uint8_t nTile, int16_t nDelta)
{
  Tile& pTile = m_tile[nTile];

  if( !(pTile.Extras & EX_SCROLL) )
    return;

  uint8_t nViewSize = DISPLAY_HEIGHT;
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
    return;

  pTile.nScrollIndex = constrain(pTile.nScrollIndex - nDelta, -TITLE_HEIGHT, pTile.nPageHeight - nViewSize);

  drawTile(nTile, false);
  sprite.pushSprite(0,0);
}

// calculate positions of non-fixed buttons
void Display::formatButtons(Tile& pTile)
{
  sprite.setTextDatum(MC_DATUM); // center text on button
  sprite.setFreeFont(&FreeSans9pt7b);

  uint8_t btnCnt;
  for( btnCnt = 0; btnCnt < BUTTON_CNT && pTile.button[btnCnt].ID; btnCnt++) // count buttons
    if( pTile.button[btnCnt].w == 0 && pTile.button[btnCnt].pszText && pTile.button[btnCnt].pszText[0] ) // if no width
      pTile.button[btnCnt].w = sprite.textWidth(pTile.button[btnCnt].pszText) + (BORDER_SPACE*2);   // calc width based on text width

  if(btnCnt == 0)
    return;

  uint8_t btnIdx = 0;
  uint8_t nRows = pTile.button[btnCnt - 1].row + 1; // use last button for row total
  uint8_t nRow = 0;
  uint8_t nCenter;

  // Calc X positions
  while( nRow < nRows )
  {
    uint8_t rBtns = 0;
    uint8_t rWidth = 0;

    for(uint8_t i = btnIdx; i < btnCnt && pTile.button[i].row == nRow; i++, rBtns++)  // count buttons for current row
    {
      if( pTile.button[i].x == 0 )
      {
        rWidth += pTile.button[i].w + BORDER_SPACE; // total up width of row
      // todo: find row height here, max up
      }
    }

    nCenter = DISPLAY_WIDTH/2;
    if( pTile.nSliderType ) // shift left if slider active
      nCenter -= 20;

    uint8_t cx = nCenter - (rWidth >> 1);

    for(uint8_t i = btnIdx; i < btnIdx+rBtns; i++)
    {
      if(pTile.button[i].x == 0)
      {
        pTile.button[i].x = cx;
        cx += pTile.button[i].w + BORDER_SPACE;
      }
    }

    btnIdx += rBtns;
    nRow++;
  }

  // Y positions
  uint8_t nBtnH = pTile.button[0].h;

  if(nRows) nRows--; // todo: something is amiss here
  pTile.nPageHeight = nRows * (nBtnH + BORDER_SPACE) - BORDER_SPACE;

  uint8_t nViewSize = DISPLAY_HEIGHT;
  if(pTile.pszTitle && pTile.pszTitle[0])                           // shrink view by title height
    nViewSize -= TITLE_HEIGHT;

  pTile.nScrollIndex = 0;

  // ScrollIndex is added to button Y on draw and detect
  if(!(pTile.Extras & EX_NOTIF) && pTile.nPageHeight <= nViewSize) // Center smaller page
    pTile.nScrollIndex = -(nViewSize - pTile.nPageHeight) / 2;
  else if(pTile.pszTitle && pTile.pszTitle[0])                       // Just offset by title height if too large
    pTile.nScrollIndex -= (TITLE_HEIGHT + BORDER_SPACE);             // down past title

  for(btnIdx = 0; btnIdx < btnCnt; btnIdx++ )
    if( !(pTile.button[ btnIdx ].flags & BF_FIXED_POS) )
      pTile.button[ btnIdx ].y = pTile.button[ btnIdx ].row * (nBtnH + BORDER_SPACE);
}

void Display::drawTile(int8_t nTile, bool bFirst)
{
  Tile& pTile = m_tile[nTile];

  if(pTile.pBGImage)
    sprite.pushImage(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, pTile.pBGImage);
  else
    sprite.fillSprite(TFT_BLACK);

  if(pTile.Extras & EX_NOTIF)
    drawNotifs(pTile);

  sprite.setTextDatum(MC_DATUM);
  sprite.setFreeFont(&FreeSans9pt7b);

  uint8_t btnCnt;
  for( btnCnt = 0; btnCnt < BUTTON_CNT && pTile.button[btnCnt].ID; btnCnt++); // count buttons

  if( btnCnt )
  {
    for(uint8_t btnIdx = 0; btnIdx < btnCnt; btnIdx++ )
      drawButton( pTile, &pTile.button[ btnIdx ], false );
  }

  if(pTile.Extras & EX_CLOCK) // draw clock over other objects
    drawClock();

  if(pTile.pszTitle && pTile.pszTitle[0])
  {
    sprite.fillRect(0, 0, DISPLAY_WIDTH, 40, TFT_BLACK); // cover any scrolled buttons/text
    sprite.setTextColor(rgb16(16,63,0), TFT_BLACK );
    sprite.drawString(pTile.pszTitle, DISPLAY_WIDTH/2, 27);
  }

  if( pTile.nSliderType )
    drawSlider( pTile.nSliderValue );
}

void Display::drawButton(Tile& pTile, Button *pBtn, bool bPressed)
{
  uint8_t nState = (pBtn->flags & BF_STATE_2) ? 1:0;
  uint16_t colorBg = (nState) ? TFT_NAVY : TFT_DARKGREY;

  if(bPressed)
  {
    nState = 1; // todo: icon tri-state
    colorBg = TFT_DARKCYAN;
  }

  String s = pBtn->pszText;

  switch(pBtn->nFunction)
  {
    case BTF_RSSI:
      btnRSSI(pBtn);
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
        float fV = 3.3 / (1<<12) * 3 * m_vadc;
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
      s = m_bGdoDoor ? "Open":"Close";
      break;
    case BTF_Lights:
      if(lights.getSwitch(pBtn->ID)) // light is on
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
  }

  int16_t yOffset = pBtn->y;

  if( !(pBtn->flags & BF_FIXED_POS) ) // don't scroll fixed pos buttons
    yOffset -= pTile.nScrollIndex;

  if(yOffset < 10 || yOffset >= DISPLAY_HEIGHT) // cut off anything out of bounds (top is covered by title blank)
    return;

  if(pBtn->pIcon[nState]) // draw an image if given
    sprite.pushImage(pBtn->x, yOffset, pBtn->w, pBtn->h, pBtn->pIcon[nState]);
  else if(pBtn->flags & BF_BORDER) // bordered text item
    sprite.drawRoundRect(pBtn->x, yOffset, pBtn->w, pBtn->h, 5, colorBg);
  else if(!(pBtn->flags & BF_TEXT) ) // if no image, or no image for state 2, and not just text, fill with a color
    sprite.fillRoundRect(pBtn->x, yOffset, pBtn->w, pBtn->h, 5, colorBg);

  if(s.length())
  {
    if(pBtn->flags & BF_TEXT) // text only
      sprite.setTextColor(TFT_WHITE, TFT_BLACK);
    else
      sprite.setTextColor(TFT_CYAN, colorBg);
    sprite.drawString( s, pBtn->x + (pBtn->w / 2), yOffset + (pBtn->h / 2) - 2 );
  }  
}

// called when button is activated
void Display::buttonCmd(Button *pBtn, bool bRepeat)
{
  switch(pBtn->nFunction)
  {
    case BTF_Lights:
      if(ee.bWiFiEnabled)
        if(!lights.setSwitch( pBtn->ID, !lights.getSwitch(pBtn->ID), 0 ) )
          notify("Light command failed");
      break;

    case BTF_IR:
      sendIR(pBtn->code);
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
      if( !sendBLE(pBtn->code) )
        notify("BLE command failed");
      break;

    case BTF_PC_Media:
      if( !sendPCMediaCmd(pBtn->code) )
        notify("PC command failed");
      break;

    case BTF_StatCmd:
      if( !sendStatCmd(pBtn->code) )
        notify("Stat command failed");
      break;

    case BTF_GdoCmd:
      if( !sendGdoCmd(pBtn->code) )
        notify("GDO command failed");
      break;

    case BTF_Clear:
      memset(m_pszNotifs, 0, sizeof(m_pszNotifs) );
      break;
  }
}

void Display::sliderCmd(uint8_t nType, uint8_t nOldVal, uint8_t nNewVal)
{
  uint16_t code[4];

  switch( nType)
  {
    case SL_Lights: // light dimmer (-1 = last active)
      lights.setSwitch( -1, true, nNewVal);
      break;
    case SL_PC: // PC volume
      code[0] = 1000;
      code[1] = nNewVal;
      sendPCMediaCmd(code);
      break;
  }
}

// Pop uup a notification + add to notes list
void Display::notify(char *pszNote)
{
  if( m_brightness < ee.brightLevel[1] ) // make it bright if not
    exitScreensaver();

  tft.setFreeFont(&FreeSans9pt7b);

  uint8_t w = tft.textWidth(pszNote) + (BORDER_SPACE*2);
  if(w > 230) w = 230;
  uint8_t x = (DISPLAY_WIDTH/2) - (w >> 1); // center on screen
  const uint8_t y = 30; // kind of high up

  tft.fillRoundRect(x, y, w, tft.fontHeight() + 3, 5, TFT_ORANGE);
  tft.setTextColor(TFT_BLACK, TFT_ORANGE);
  tft.drawString( pszNote, x + BORDER_SPACE, y + BORDER_SPACE );

  for(int8_t i = NOTE_CNT - 2; i >= 0; i--) // move old notifications down
    m_pszNotifs[i+1] = m_pszNotifs[i];
  m_pszNotifs[0] = pszNote;
  m_nDispFreeze = millis() + 900; // pop the toast up for 900ms
}

// draw the notifs list
void Display::drawNotifs(Tile& pTile)
{
  sprite.setTextDatum(TL_DATUM);
  sprite.setTextColor(TFT_WHITE, TFT_BLACK );
  sprite.setFreeFont(&FreeSans9pt7b);

  const uint8_t x = 40; // left padding
  int16_t y = -pTile.nScrollIndex; // offset by the scroll pos

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

void Display::drawSlider(uint8_t val)
{
  static float oldDeg = 0;
  uint8_t dist = 107;
  float ax = (DISPLAY_WIDTH/2) + dist * sin(oldDeg);
  float ay = (DISPLAY_HEIGHT/2) + dist * cos(oldDeg);
  sprite.drawSpot(ax, ay, 11, TFT_BLACK); // remove old spot

  // draw slider bar  ( x,              y,          rad, inner rad, startAngle, endAngle, fg_color, bg_color, roundEnds)
  sprite.drawSmoothArc(DISPLAY_WIDTH/2, DISPLAY_HEIGHT/2, 111, 106, 360-135-15, 360-45+15, TFT_DARKCYAN, TFT_BLACK, true);

  uint8_t angle = 30 + (val * 120 / 100);
  float deg =  M_PI * angle / 180;
  ax = (DISPLAY_WIDTH/2) + dist * sin(deg);
  ay = (DISPLAY_HEIGHT/2) + dist * cos(deg);

  sprite.drawSpot(ax, ay, 10, TFT_YELLOW);
  oldDeg = deg;
}

// check for edge slider hit and return 0-100 value
bool Display::sliderHit(uint8_t& value)
{
  uint8_t dist = sqrt( pow((float)DISPLAY_WIDTH/2 - touch.x, 2) + pow((float)(DISPLAY_HEIGHT/2) - touch.y, 2) );
  if(dist < 110) // touch on edge
    return false;

  int16_t deg = atan2(touch.y - (DISPLAY_HEIGHT/2), touch.x - (DISPLAY_WIDTH/2)) * (180 / PI);
  deg -= 45; // slider is 90 deg long from 45 to 135
  deg = -deg * 100 / 90; // convert 90 degs from 135 to 45 to 100%
  if(deg < 0 || deg > 100) // touch not over slider
    return false;
  value = deg;
  return true;
}

// Draw RSSI bars
void Display::btnRSSI(Button *pBtn)
{
#define RSSI_CNT 8
  static int16_t rssi[RSSI_CNT];
  static uint8_t rssiIdx = 0;

  rssi[rssiIdx] = WiFi.RSSI();
  if(++rssiIdx >= RSSI_CNT) rssiIdx = 0;

  int16_t rssiAvg = 0;
  for(int i = 0; i < RSSI_CNT; i++)
    rssiAvg += rssi[i];

  rssiAvg /= RSSI_CNT;

  const uint8_t wh = pBtn->w; // width and height
  const uint8_t sect = 127 / 5; //
  const uint8_t dist = wh  / 5; // distance between blocks
  int16_t sigStrength = 127 + rssiAvg;

  for (uint8_t i = 1; i < 6; i++)
  {
    sprite.fillRect( pBtn->x + i*dist, pBtn->y - i*dist, dist-2, i*dist, (sigStrength > i * sect) ? TFT_CYAN : TFT_BLACK );
  }
}

// Analog clock
void Display::drawClock()
{
  const float x = DISPLAY_WIDTH/2; // center
  const float y = DISPLAY_HEIGHT/2;

  if(ee.bWiFiEnabled && (year() < 2024 || WiFi.status() != WL_CONNECTED)); // Still connecting
  else sprite.drawSmoothCircle(x, y, x-1, m_bCharging ? TFT_BLUE : TFT_WHITE, TFT_BLACK); // draw outer ring

  for(uint16_t i = 0; i < 360; i += 6) // drawing the watchface instead of an image saves 9% of PROGMEM
  {
    uint16_t xH,yH,xH2,yH2;
    cspoint(xH, yH, x, y, i, 114); // outer edge
    cspoint(xH2, yH2, x, y, i, (i%30) ? 103:94); // 60 ticks/12 longer ticks
    sprite.drawWideLine(xH, yH, xH2, yH2, (i%30) ? 2:3, TFT_SILVER, TFT_BLACK); // 12 wider ticks
  }

  if(year() < 2024)
    return;

  uint16_t xH,yH, xM,yM, xS,yS, xS2,yS2;

  float a = (hour() + (minute() * 0.00833) ) * 30;
  cspoint(xH, yH, x, y, a, 64);
  cspoint(xM, yM, x, y, minute() * 6, 87);
  cspoint(xS, yS, x, y, second() * 6, 94);
  cspoint(xS2, yS2, x, y, (second()+30) * 6, 24);

  sprite.drawWedgeLine(x, y, xH, yH, 8, 2, TFT_LIGHTGREY, TFT_BLACK); // hour hand
  sprite.drawWedgeLine(x, y, xM, yM, 5, 2, TFT_LIGHTGREY, TFT_BLACK); // minute hand
  sprite.fillCircle(x, y, 8, TFT_LIGHTGREY ); // center cap
  sprite.drawWideLine(xS2, yS2, xS, yS, 2.5, TFT_RED, TFT_BLACK ); // second hand
}

// calc point for a clock hand and slider
void Display::cspoint(uint16_t &x2, uint16_t &y2, uint16_t x, uint16_t y, float angle, float size)
{
  float ang =  M_PI * (180-angle) / 180;
  x2 = x + size * sin(ang);
  y2 = y + size * cos(ang);
}

// update the PC volume slider
void Display::setPcVolumeSlider(int8_t iValue)
{
  uint8_t i;
  for(i = 0; i < m_nTileCnt; i++)
    if(m_tile[i].nSliderType == SL_PC)
      break;
  if(i < m_nTileCnt)
    m_tile[i].nSliderValue = iValue;
}

// Flash a red indicator for RX, TX, PC around the top 
void Display::RingIndicator(uint8_t n)
{
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

  tft.drawSmoothArc(DISPLAY_WIDTH/2, DISPLAY_HEIGHT/2, 119, 115, ang, ang+8, TFT_RED, TFT_BLACK, true);
}
