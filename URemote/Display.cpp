/*
Display and UI code
*/

#include <WiFi.h>
#include "display.h"
#include <TimeLib.h>
#include "eeMem.h"
#include <TFT_eSPI.h> // TFT_espi library from library manager?
#include "CST816D.h"
#include "Lights.h"

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);
extern Lights lights;
extern void sendIR(uint16_t *pCode);
extern bool sendBLE(uint16_t *pCode);

extern bool sendPCMediaCmd( uint16_t *pCode);

extern void consolePrint(String s); // for browser debug

#define I2C_SDA 4
#define I2C_SCL 5
#define TP_INT 0
#define TP_RST 1
#define TFT_BL 3
#define EXT_IO 8 // Pad by serial port (ignored unless strapping)
#define BOOT   9 // rear button (strapping, 10K pullup + internal pullup)

CST816D touch(I2C_SDA, I2C_SCL, TP_RST, TP_INT);

#include "screensavers.h" // comment this line to remove screensavers (saves 3172b PROGMEM, 1072b SRAM)
#ifdef SCREENSAVERS_H
ScreenSavers ss;
#endif

#define TITLE_HEIGHT 40
#define BORDER_SPACE 3 // H and V spacing + font size for buttons and space between buttons

void Display::init(void)
{
  pinMode(TFT_BL, OUTPUT);

  touch.begin();
  tft.init();
  tft.setSwapBytes(true);
  tft.setTextPadding(8); 

  sprite.createSprite(DISPLAY_WIDTH, DISPLAY_HEIGHT); // This uses over 115K RAM
  sprite.setSwapBytes(true);

  pinMode(TP_INT, INPUT);

  // count tiles, index rows, count columns, format the buttons
  uint8_t nRowIdx = 0;

  for(m_nTileCnt = 1; m_nTileCnt < TILES && (m_tile[m_nTileCnt].Extras || m_tile[m_nTileCnt].szTitle[0] || m_tile[m_nTileCnt].pBGImage); m_nTileCnt++) // count screens
  {
    m_nColCnt[nRowIdx]++;
    if(m_tile[m_nTileCnt].nRow != nRowIdx)
      m_nRowStart[++nRowIdx] = m_nTileCnt;
    formatButtons(m_tile[m_nTileCnt]);
  }

  drawTile(m_nCurrTile, true);
  sprite.pushSprite(0, 0); // draw screen 0 on start
}

void Display::exitScreensaver()
{
  m_backlightTimer = DISPLAY_TIMEOUT; // reset timer for any touch

  if( m_brightness == ee.brightLevel[0] ) // screensaver active
  {
    drawTile(m_nCurrTile, true); // draw over screensaver
    sprite.pushSprite(0, 0);
  }
  m_brightness = ee.brightLevel[1]; // increase brightness for any touch
}

void Display::service(void)
{
  static Button *pCurrBtn; // for button repeats and release
  static uint32_t lastms; // for button repeats
  static bool bGestured;  // gesture detected

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
#endif
  uint8_t gesture;
  uint8_t nRowOffset;
  static bool bScrolling; // scrolling active
  uint16_t touchX, touchY;
  static int16_t touchXstart, touchYstart; // first touch pos for scolling and non-moving detect
  static uint32_t touchStartMS; // timing for 
  static uint8_t nCurrRow;
 
  if ( !touch.getTouch(&touchX, &touchY, &gesture) ) // no touch
  {
    if(pCurrBtn)
    {
      drawButton(m_tile[m_nCurrTile], pCurrBtn, false); // draw released button
      sprite.pushSprite(0, 0);
    }
    pCurrBtn = NULL;
    bGestured = false;
    touchXstart = -1;
    touchYstart = -1;
    bScrolling = false;
  }
  else
  {
    m_backlightTimer = DISPLAY_TIMEOUT; // reset timer for any touch
    Tile& pTile = m_tile[m_nCurrTile];

    if( m_brightness < ee.brightLevel[1] ) // ignore input until brightness is full
    {
      exitScreensaver();
      return;
    }

    // Handle slider
    if(pTile.nSliderType && touchX > DISPLAY_WIDTH - 70 )
    {
      uint8_t val = (uint8_t)(float)(DISPLAY_HEIGHT - 40 - touchY) * 0.7;
      if(val > 100) val = 100;
      drawSlider(val);
      sprite.pushSprite(0, 0);
      if(pTile.nSliderValue != val)
      {
        sliderCmd(pTile.nSliderType, pTile.nSliderValue, val);
        pTile.nSliderValue = val;
      }
    }
    else if(bScrolling) // scrolling detected
    {
      if(touchY - touchYstart)
        scrollPage(m_nCurrTile, touchY - touchYstart);
      touchYstart = touchY;
    }
    else if(gesture == None || gesture == LongPress) // normal press or hold
    {
      if(touchYstart == -1) // first touch
      {
        touchXstart = touchX;
        touchYstart = touchY;
        touchStartMS = millis();
      }
      else if(pCurrBtn == NULL)
      {
        if(millis() - touchStartMS < 100)
        {
          // first 90ms do nothing to detect any slide
        }
        else
        {
          if(touchY - touchYstart || touchX - touchXstart) // movement, not a direct touch
            return;
          Button *pBtn = &pTile.button[0];
          for(uint8_t i = 0; pBtn[i].x; i++) // check for press in button bounds
          {
            int16_t y = pBtn[i].y;

            if( !(pBtn->flags & BF_FIXED_POS) ) // adjust for scroll offset
              y -= pTile.nScrollIndex;
  
            if (touchX >= pBtn[i].x && touchX <= pBtn[i].x + pBtn[i].w && touchY >= y && touchY <= y + pBtn[i].h)
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
      }
      else if(millis() - lastms > 300) // repeat speed
      {
        if(pCurrBtn->flags & BF_REPEAT)
          buttonCmd(pCurrBtn, true);
        lastms = millis();
      }
    }
    else if(bGestured == false && gesture) // slide gestures
    {
      switch(gesture)
      {
        case SlideRight:
          if(pCurrBtn || m_nCurrTile <= 0) // don't screoll if button pressed or screen 0
            break;

          if(m_tile[m_nCurrTile].nRow != m_tile[m_nCurrTile-1].nRow) // don't scroll left if leftmost
            break;

          m_nCurrTile--;
          drawTile(m_nCurrTile, true);
          for(int16_t i = -DISPLAY_WIDTH; i <= 0; i += 8)
            sprite.pushSprite(i, 0);
          break;
        case SlideLeft:
          if(pCurrBtn || m_nCurrTile >= m_nTileCnt - 1) // don't scroll if button pressed or last screen
            break;

          if(m_tile[m_nCurrTile].nRow != m_tile[m_nCurrTile+1].nRow) // don't scroll right if rightmost
            break;

          m_nCurrTile++;
          drawTile(m_nCurrTile, true);
          for(int16_t i = DISPLAY_WIDTH; i >= 0; i -= 8)
            sprite.pushSprite(i, 0);
          break;
        case SlideDown:
          if((pTile.Extras & EX_SCROLL) && touchY - touchYstart < 20) // slow/short flick
          {
            bScrolling = true;
            break;
          }

          if(pCurrBtn || m_nCurrTile == 0 || nCurrRow == 0) // don't slide up from top
            break;

          nRowOffset = m_nCurrTile - m_nRowStart[nCurrRow];
          nCurrRow--;
          m_nCurrTile = m_nRowStart[nCurrRow] + min((int)nRowOffset, m_nColCnt[nCurrRow]-1 ); // get close to the adjacent screen

          drawTile(m_nCurrTile, true);
          for(int16_t i = -DISPLAY_HEIGHT; i <= 0; i += 8)
            sprite.pushSprite(0, i);
          break;
        case SlideUp:
          if((pTile.Extras & EX_SCROLL) && touchYstart - touchY < 20) // slow/short flick
          {
            bScrolling = true;
            break;
          }

          if(pCurrBtn) // don't slide if button pressed
            break;

          nRowOffset = m_nCurrTile - m_nRowStart[nCurrRow];
          nCurrRow++;
          { // fix
            uint8_t nCS = m_nRowStart[nCurrRow] + min((int)nRowOffset, m_nColCnt[nCurrRow]-1 );
            if(nCS < m_nTileCnt) m_nCurrTile = nCS;
            else{ nCurrRow--; break;}// back out
          }
          drawTile(m_nCurrTile, true);
          for(int16_t i = DISPLAY_HEIGHT; i >= 0; i -= 8)
            sprite.pushSprite(0, i);
          break;
        default:
          break;
      }
      if(gesture <= SlideRight)
        bGestured = true;
    }

  }

  if(year() < 2024 || WiFi.status() != WL_CONNECTED) // Do a connecting effect
  {
    static int16_t ang[2] = {3, 20};
    tft.drawSmoothArc(DISPLAY_WIDTH/2, DISPLAY_HEIGHT/2, 119, 116, ang[0], ang[1], TFT_BLUE, TFT_BLACK, false);
    ang[0] += 16;
    ang[1] += 32;
    ang[0] %= 360;
    ang[1] %= 360;
    tft.drawSmoothArc(DISPLAY_WIDTH/2, DISPLAY_HEIGHT/2, 119, 116, ang[0], ang[1], (WiFi.status() == WL_CONNECTED) ? TFT_GREEN:TFT_RED, TFT_BLACK, false);
  }
}

// called each second
void Display::oneSec()
{
  if(m_nDispFreeze)
    return;

  if( m_backlightTimer ) // the dimmer thing
  {
    if(--m_backlightTimer == 0)
    {
      m_brightness = ee.brightLevel[0]; // dim level
      m_blSurgeTimer = 0;
#ifdef SCREENSAVERS_H
      ss.select( random(0, SS_Count) );
#endif
    }
  }

  if(m_brightness > ee.brightLevel[0] && m_nDispFreeze == 0) // not screensaver
  {
    drawTile(m_nCurrTile, false);
    sprite.pushSprite(0,0);
  }

  // Reset the IP5306 shutdown timer when dim / consuming <45mA
  if(m_bright == ee.brightLevel[0])
  {
    if(++m_blSurgeTimer > 30)
    {
      m_bright = ee.brightLevel[1]; // increase brightness for a surge
      analogWrite(TFT_BL, 200);
      m_blSurgeTimer = 0;
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
      while(m_tile[nLi].nSliderType != SL_Lights && (m_tile[nLi].szTitle[0] || m_tile[nLi].Extras) )
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
}

void Display::scrollPage(uint8_t nTile, int16_t nDelta)
{
  Tile& pTile = m_tile[nTile];

  if( !(pTile.Extras & EX_SCROLL) )
    return;

  uint8_t nViewSize = DISPLAY_HEIGHT;
  if(pTile.szTitle[0])
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
  if(pTile.szTitle[0])                                              // shrink view by title height
    nViewSize -= TITLE_HEIGHT;

  pTile.nScrollIndex = 0;

  // ScrollIndex is added to button Y on draw and detect
  if(!(pTile.Extras & EX_NOTIF) && pTile.nPageHeight <= nViewSize) // Center smaller page
    pTile.nScrollIndex = -(nViewSize - pTile.nPageHeight) / 2;
  else if(pTile.szTitle[0])                                          // Just offset by title height if too large
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

  if(pTile.szTitle[0])
  {
    sprite.fillRect(0, 0, DISPLAY_WIDTH, 40, TFT_BLACK); // cover any scrolled buttons/text
    sprite.setTextColor(rgb16(16,63,0), TFT_BLACK );
    sprite.drawString(pTile.szTitle, DISPLAY_WIDTH/2, 27);
  }

  if( pTile.nSliderType )
    drawSlider( pTile.nSliderValue );
}

void Display::drawButton(Tile& pTile, Button *pBtn, bool bPressed)
{
  if(pBtn->flags & BF_RSSI)
  {
    updateRSSI(pBtn);
    return;
  }
  if(pBtn->flags & BF_TIME)
  {
    drawTime(pBtn);
    return;
  }
  if(pBtn->flags & BF_DATE)
  {
    drawDate(pBtn);
    return;
  }
  if(pBtn->flags & BF_DOW)
  {
    drawDOW(pBtn);
    return;
  }


  switch(pBtn->nFunction)
  {
    case BTF_Lights:
      if(lights.getSwitch(pBtn->ID)) // light is on
        pBtn->flags |= BF_STATE_2;
      else
        pBtn->flags &= ~BF_STATE_2;
      break;
  }

  uint8_t nState = (pBtn->flags & BF_STATE_2) ? 1:0;
  uint16_t colorBg = (nState) ? TFT_NAVY : TFT_DARKGREY;

  if(bPressed)
  {
    nState = 1; // todo: icon tri-state
    colorBg = TFT_DARKCYAN;
  }

  int16_t yOffset = pBtn->y;
  
  if( !(pBtn->flags & BF_FIXED_POS) )
    yOffset -= pTile.nScrollIndex;

  if(yOffset < 10 || yOffset >= DISPLAY_HEIGHT) // cut off anything out of bounds (top is covered by title blank)
    return;

  if(pBtn->pIcon[nState])
    sprite.pushImage(pBtn->x, yOffset, pBtn->w, pBtn->h, pBtn->pIcon[nState]);
  else // if no image or no image for state 2, fill with a color
    sprite.fillRoundRect(pBtn->x, yOffset, pBtn->w, pBtn->h, 5, colorBg);

  sprite.setTextColor(TFT_CYAN, colorBg);
  if(pBtn->pszText && pBtn->pszText[0])
    sprite.drawString( pBtn->pszText, pBtn->x + (pBtn->w / 2), yOffset + (pBtn->h / 2) - 2 );
}

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

    case BTF_LearnIR:
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
/*    if(ee.bWiFiEnabled)
        restartWiFi();
      else
        stopWiFi();
*/
      pBtn->pszText = (ee.bWiFiEnabled) ? "WiFi On" : "WiFi Off";
      notify((char *)pBtn->pszText);
      if(ee.bWiFiEnabled)
        pBtn->flags |= BF_STATE_2;
      else
        pBtn->flags &= ~BF_STATE_2;
      break;

    case BTF_BLE:
      if( !sendBLE(pBtn->code) )
        notify("BLE command failed");
      break;

    case BTF_PC_Media:
      if( !sendPCMediaCmd(pBtn->code) )
        notify("PC command failed");
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
    case SL_Lights:
      lights.setSwitch( -1, true, nNewVal);
      break;
    case SL_PC:
      code[0] = 1000;
      code[1] = nNewVal;
      sendPCMediaCmd(code);
      break;
  }
}

void Display::notify(char *pszNote)
{
  if( m_brightness < ee.brightLevel[1] ) // make it bright if not
    exitScreensaver();

  tft.setFreeFont(&FreeSans9pt7b);

  uint8_t w = tft.textWidth(pszNote) + (BORDER_SPACE*2);
  if(w > 230) w = 230;
  uint8_t x = (DISPLAY_WIDTH/2) - (w >> 1); // center on screen
  const uint8_t y = 30; // kind of high up

  tft.fillRoundRect(x, y, w, tft.fontHeight() + 5, 5, TFT_ORANGE);
  tft.setTextColor(TFT_BLACK, TFT_ORANGE);
  tft.drawString( pszNote, x + BORDER_SPACE, y + BORDER_SPACE );

  for(int8_t i = NOTE_CNT - 2; i >= 0; i--) // move old notifications down
    m_pszNotifs[i+1] = m_pszNotifs[i];
  m_pszNotifs[0] = pszNote;
  m_nDispFreeze = millis() + 900; // pop the toast up for 900ms
}

void Display::drawNotifs(Tile& pTile)
{
  sprite.setTextDatum(TL_DATUM);
  sprite.setTextColor(TFT_WHITE, TFT_BLACK );
  sprite.setFreeFont(&FreeSans9pt7b);

  const uint8_t x = 40; // left padding
  int16_t y = -pTile.nScrollIndex; // offset by title

  for(uint8_t i = 0; i < NOTE_CNT && m_pszNotifs[i]; i++)
  {
    if(y > 30) // somewhat covered by title
      sprite.drawString(m_pszNotifs[i], x, y);
    y += sprite.fontHeight();
    if(y > DISPLAY_HEIGHT + 4)
      break;
  }
}

// time
void Display::drawTime(Button *pBtn)
{
  if(year() < 2024) // don't display invalid time
    return;

  String sTime = String( hourFormat12() );
  if(hourFormat12() < 10)
    sTime = " " + sTime;
  sTime += ":";
  if(minute() < 10) sTime += "0";
  sTime += minute();
  sTime += ":";
  if(second() < 10) sTime += "0";
  sTime += second();
  sTime += " ";
  sTime += isPM() ? "PM":"AM";
  sTime += " ";

  sprite.setTextColor(TFT_WHITE, TFT_BLACK );
  sprite.setFreeFont(&FreeSans9pt7b);
  sprite.drawString(sTime, pBtn->x, pBtn->y);
}

void Display::drawDate(Button *pBtn)
{
  if(year() < 2024) // don't display invalid time
    return;

  sprite.setTextColor(TFT_WHITE, TFT_BLACK );
  sprite.setFreeFont(&FreeSans9pt7b);

  String sTime = monthShortStr(month());
  sTime += " ";
  sTime += String(day());
  sprite.drawString(sTime, pBtn->x, pBtn->y);
}

void Display::drawDOW(Button *pBtn)
{
  if(year() < 2024) // don't display invalid time
    return;

  sprite.setTextColor(TFT_WHITE, TFT_BLACK );
  sprite.setFreeFont(&FreeSans9pt7b);

  sprite.drawString(dayShortStr(weekday()), pBtn->x, pBtn->y);
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
  static float oldAng = 0;
  uint8_t dist = 107;
  float ax = (DISPLAY_WIDTH/2) + dist * sin(oldAng);
  float ay = (DISPLAY_HEIGHT/2) + dist * cos(oldAng);  
  sprite.drawSpot(ax, ay, 11, TFT_BLACK); // remove old spot

  //                  ( x,   y,  r,   ir,  startAngle, endAngle, fg_color, bg_color, roundEnds)
  sprite.drawSmoothArc(DISPLAY_WIDTH/2, DISPLAY_HEIGHT/2, 111, 106, 180 + 28, 360- 28, TFT_DARKCYAN, TFT_BLACK, true);

  uint8_t angle = 30 + (val * (DISPLAY_WIDTH/2) / 100);
  float ang =  M_PI * angle / 180;
  ax = (DISPLAY_WIDTH/2) + dist * sin(ang);
  ay = (DISPLAY_HEIGHT/2) + dist * cos(ang);

  sprite.drawSpot(ax, ay, 10, TFT_YELLOW);
  oldAng = ang;
}

void Display::updateRSSI(Button *pBtn)
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

  sprite.drawSmoothCircle(x, y, x-2, TFT_WHITE, TFT_BLACK);

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

  sprite.drawWedgeLine(x, y, xH, yH, 8, 2, TFT_DARKGREY, TFT_BLACK); // hour hand
  sprite.drawWedgeLine(x, y, xM, yM, 5, 2, TFT_LIGHTGREY, TFT_BLACK); // minute hand
  sprite.fillCircle(x, y, 8, TFT_LIGHTGREY ); // center cap
  sprite.drawWideLine(xS2, yS2, xS, yS, 2.5, TFT_RED, TFT_BLACK ); // second hand
}

void Display::cspoint(uint16_t &x2, uint16_t &y2, uint16_t x, uint16_t y, float angle, float size)
{
  float ang =  M_PI * (180-angle) / 180;
  x2 = x + size * sin(ang);
  y2 = y + size * cos(ang);
}

void Display::setPcVolume(int8_t iValue)
{
  uint8_t i;
  for(i = 0; i < m_nTileCnt; i++)
    if(m_tile[i].nSliderType == SL_PC)
      break;
  m_tile[i].nSliderValue = iValue;
}

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
