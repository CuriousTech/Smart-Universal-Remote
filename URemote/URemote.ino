/**The MIT License (MIT)

Copyright (c) 2024 by Greg Cunningham, CuriousTech

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

// Build with Arduino IDE 1.8.57.0
//  ESP32: (2.0.13) ESP32S3 Dev Module, QIO, CPU Freq: 80MHz for lowest power (<60mA), Flash: 16MB, PSRAM: QSPI PSRAM, Partition: 16MB or Rainmaker.

#define OTA_ENABLE  //uncomment to enable Arduino IDE Over The Air update code (uses ~4K heap)
#define SERVER_ENABLE // uncomment to enable server and WebSocket
//#define BLE_ENABLE // uncomment for BLE keyboard (uses 510KB (16%) PROGMEN and >100K heap)

#include <WiFi.h>
#include <TimeLib.h> // http://www.pjrc.com/teensy/td_libs_Time.html
#include <UdpTime.h> // https://github.com/CuriousTech/ESP07_WiFiGarageDoor/tree/master/libraries/UdpTime
#include "eeMem.h"
#include <ESPmDNS.h>
#ifdef OTA_ENABLE
#include <ArduinoOTA.h>
#endif

#ifdef SERVER_ENABLE
#include <ESPAsyncWebServer.h> // https://github.com/me-no-dev/ESPAsyncWebServer
#include <JsonParse.h> // https://github.com/CuriousTech/ESP8266-HVAC/tree/master/Libraries/JsonParse
#include "jsonString.h"
#include "pages.h"

AsyncWebServer server( 80 );
AsyncWebSocket ws("/ws"); // access at ws://[esp ip]/ws
int WsClientID;
int WsPcClientID;
void jsonCallback(int16_t iName, int iValue, char *psValue);
JsonParse jsonParse(jsonCallback);
#endif

#ifdef BLE_ENABLE
#include <BleKeyboard.h> // https://github.com/T-vK/ESP32-BLE-Keyboard
BleKeyboard bleKeyboard;
#endif

// GPIO 15-18 reserved for keypad

#define IR_RECEIVE_PIN   21
#define IR_SEND_PIN      33
//#define DISABLE_CODE_FOR_RECEIVER // Disables restarting receiver after each send. Saves 450 bytes program memory and 269 bytes RAM if receiving functions are not used.
#define EXCLUDE_EXOTIC_PROTOCOLS  // Saves around 240 bytes program memory if IrSender.write is used
//#define SEND_PWM_BY_TIMER         // Disable carrier PWM generation in software and use (restricted) hardware PWM.
//#define USE_NO_SEND_PWM           // Use no carrier PWM, just simulate an active low receiver signal. Overrides SEND_PWM_BY_TIMER definition
#define NO_LED_FEEDBACK_CODE      // Saves 566 bytes program memory

#if !defined(RAW_BUFFER_LENGTH)
#  if RAMEND <= 0x4FF || RAMSIZE < 0x4FF
#define RAW_BUFFER_LENGTH  180  // 750 (600 if we have only 2k RAM) is the value for air condition remotes. Default is 112 if DECODE_MAGIQUEST is enabled, otherwise 100.
#  elif RAMEND <= 0x8FF || RAMSIZE < 0x8FF
#define RAW_BUFFER_LENGTH  600  // 750 (600 if we have only 2k RAM) is the value for air condition remotes. Default is 112 if DECODE_MAGIQUEST is enabled, otherwise 100.
#  else
#define RAW_BUFFER_LENGTH  750  // 750 (600 if we have only 2k RAM) is the value for air condition remotes. Default is 112 if DECODE_MAGIQUEST is enabled, otherwise 100.
#  endif
#endif
#define MARK_EXCESS_MICROS    20    // Adapt it to your IR receiver module. 20 is recommended for the cheap VS1838 modules.

#include <IRremote.hpp> // IRremote from library manager

#include "display.h"
#include "Lights.h" // Uses ~3KB
Lights lights;
const char *hostName = "URemote"; // Device and OTA name

bool bConfigDone = false; // EspTouch done or creds set
bool bStarted = false;
uint32_t connectTimer;
bool bRXActive;

Display display;
eeMem ee;
UdpTime uTime;

void consolePrint(String s)
{
  jsonString js("print");
  js.Var("text", s);
  ws.textAll(js.Close());  
}

#if defined(SERVER_ENABLE) && !defined(DISABLE_CODE_FOR_RECEIVER)
void decodePrint(uint8_t proto, uint16_t addr, uint16_t cmd, uint32_t raw, uint8_t bits, uint8_t flags)
{
  jsonString js("decode");
  js.Var("proto", proto);
  js.Var("addr", addr);
  js.Var("code", cmd);
  js.Var("raw", raw);
  js.Var("bits", bits);
//  js.Var("flags", flags);
  ws.textAll(js.Close());  
}
#endif

void sendIR(uint16_t *pCode)
{
  display.RingIndicator(0);
  IRData IRSendData;
  IRSendData.protocol = (decode_type_t)pCode[0];
  IRSendData.address = pCode[1]; // Address
  IRSendData.command = pCode[2]; // Command
  IRSendData.flags = IRDATA_FLAGS_EMPTY;

  IrSender.write(&IRSendData, pCode[3]); // 3=repeats
//    delay(DELAY_AFTER_SEND);

#ifdef SERVER_ENABLE
  jsonString js("send");
  js.Var("proto", pCode[0]);
  js.Var("addr", pCode[1]);
  js.Var("code", pCode[2]);
  js.Var("rep", pCode[3]);
  ws.textAll(js.Close());  
#endif
}

//-----------------

void stopWiFi()
{
  if(WiFi.status() != WL_CONNECTED)
    return;

  ee.update();
#ifdef OTA_ENABLE
  ArduinoOTA.end();
  delay(100); // fix ArduinoOTA crash?
#endif

  display.m_nWsConnected = 0;
  WsClientID = 0;
  WsPcClientID = 0;
  
  WiFi.setSleep(true);
}

void startWiFi()
{
  if(WiFi.status() == WL_CONNECTED)
    return;

  WiFi.setSleep(false);

  WiFi.hostname(hostName);
  WiFi.mode(WIFI_STA);

  if ( ee.szSSID[0] )
  {
    WiFi.begin(ee.szSSID, ee.szSSIDPassword);
    WiFi.setHostname(hostName);
    bConfigDone = true;
  }
  else
  {
    WiFi.beginSmartConfig();
  }
  connectTimer = now();

#ifdef OTA_ENABLE
  ArduinoOTA.setHostname(hostName);
  ArduinoOTA.begin();
  ArduinoOTA.onStart([]() {
    display.notify("OTA Update");
    ee.update();
#ifdef SERVER_ENABLE
    jsonString js("print");
    js.Var("text", "OTA update started");
    ws.textAll(js.Close());
#endif
    delay(100);
  });
#endif
}

void serviceWiFi()
{
#ifdef OTA_ENABLE
// Handle OTA server.
  ArduinoOTA.handle();
#endif
}

bool secondsWiFi() // called once per second
{
  bool bConn = false;

//  ets_printf("heap %d\n", ESP.getFreeHeap());

  if(!bConfigDone)
  {
    if( WiFi.smartConfigDone())
    {
      bConfigDone = true;
      connectTimer = now();
    }
  }
  if(bConfigDone)
  {
    if(WiFi.status() == WL_CONNECTED)
    {
      if(!bStarted)
      {
        MDNS.begin( hostName );
        bStarted = true;
        MDNS.addService("iot", "tcp", 80);
        WiFi.SSID().toCharArray(ee.szSSID, sizeof(ee.szSSID)); // Get the SSID from SmartConfig or last used
        WiFi.psk().toCharArray(ee.szSSIDPassword, sizeof(ee.szSSIDPassword) );
        bConn = true;
        lights.init();
      }
    }
    else if(now() - connectTimer > 10) // failed to connect for some reason
    {
      connectTimer = now();
      WiFi.mode(WIFI_AP_STA);
      WiFi.beginSmartConfig();
      bConfigDone = false;
      bStarted = false;
    }
  }

  if(WiFi.status() != WL_CONNECTED)
    return bConn;

  return bConn;
}

bool sendBLE(uint16_t *pCode)
{
#ifdef BLE_ENABLE
  return bleKeyboard.write(pCode[0]);
#else
  return false;
#endif
}

/* HOTKEY
  case 0: // play/pause
  case 1: // stop
  case 2: // next
  case 3: // back
  case 4: // Mute
  case 5: // Volume up
  case 6: // Volume down
*/

bool sendPCMediaCmd( uint16_t *pCode)
{
#ifdef SERVER_ENABLE
  if(WsPcClientID == 0)
    return false;

  display.RingIndicator(2);
  if(pCode[0] == 1000)
  {
    jsonString js("volume");
    js.Var("value", pCode[1]);
    ws.text(WsPcClientID, js.Close());
  }
  else
  {
    jsonString js("media");
    js.Var("HOTKEY", pCode[0]);
    ws.text(WsPcClientID, js.Close());
  }
  return true;
#else
  return false;
#endif
}

// Currently using PC to relay
bool sendStatCmd( uint16_t *pCode)
{
#ifdef SERVER_ENABLE
  if(WsPcClientID == 0)
    return false;

  display.RingIndicator(2);
  jsonString js("STAT");
  js.Var("value", pCode[0]);
  ws.text(WsPcClientID, js.Close());
  return true;
#else
  return false;
#endif
}

// Currently using PC to relay
bool sendGdoCmd( uint16_t *pCode)
{
#ifdef SERVER_ENABLE
  if(WsPcClientID == 0)
    return false;

  display.RingIndicator(2);
  jsonString js("GDOCMD");
  js.Var("value", pCode[0]);
  ws.text(WsPcClientID, js.Close());
  return true;
#else
  return false;
#endif
}

#ifdef SERVER_ENABLE

const char *jsonListCmd[] = {
  "PROTO", // IR send commands
  "ADDR",
  "CODE",
  "REP",
  "RX",
  "PC_REMOTE", // PC commands client
  "PC_VOLUME",
  "LED",
  "STATTEMP",
  "STATSETTEMP",
  "STATFAN", // 10
  "OUTTEMP",
  "ST", // sleeptime
  "SS", // screensaver time
  "restart",
  "GDODOOR",
  "GDOCAR",
  NULL
};

void jsonCallback(int16_t iName, int iValue, char *psValue)
{
  static uint16_t code[4];

  switch (iName)
  {
    case 0: // PROTO
      code[0] = iValue;
      break;
    case 1: // ADDR
      code[1] = iValue;
      break;
    case 2: // CODE
      code[2] = iValue;
      break;
    case 3: // REP
      code[3] = iValue;
      sendIR(code);
      break;
    case 4: // RX
#ifndef DISABLE_CODE_FOR_RECEIVER
      IrReceiver.begin(IR_RECEIVE_PIN);
      consolePrint("Decode started");
      bRXActive = true;
#endif
      break;
    case 5: // PC_REMOTE
      if(WsPcClientID == 0)
        WsPcClientID = WsClientID;
      break;
    case 6: // PC_VOLUME
      display.setSliderValue(SL_PC, iValue);
      break;
    case 7: // LED
      digitalWrite(IR_SEND_PIN, iValue ? HIGH:LOW);
      break;
    case 8: // STATTEMP
      display.m_statTemp = iValue;
      break;
    case 9: // STATSETTEMP
      display.m_statSetTemp = iValue;
      break;
    case 10: // STATFAN
      display.m_statFan = iValue;
      break;
    case 11: // OUTTEMP
      display.m_outTemp = iValue;
      break;
    case 12: // ST
      ee.sleepTime = iValue;
      break;
    case 13: // SS
      ee.ssTime = iValue;
      break;
    case 14:
      ESP.restart();
      break;
    case 15:
      display.m_bGdoDoor = iValue;
      break;
    case 16:
      display.m_bGdoCar = iValue;
      break;
  }
}

String dataJson()
{
  jsonString js("state");
  js.Var("t", (long)now() - ( (ee.tz + uTime.getDST() ) * 3600) );
  return js.Close();
}

uint8_t ssCnt = 58;

void sendState()
{
  if (display.m_nWsConnected)
    ws.textAll( dataJson() );
  ssCnt = 58;
}

String setupJson()
{
  jsonString js("setup");
  js.Var("tz", ee.tz);
  js.Var("st", ee.sleepTime);
  js.Var("ss", ee.ssTime);
  return js.Close();
}

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len)
{ //Handle WebSocket event

  switch (type)
  {
    case WS_EVT_CONNECT:      //client connected
      client->text(setupJson());
      client->text(dataJson());
      display.m_nWsConnected++;
      WsClientID = client->id();
      break;
    case WS_EVT_DISCONNECT:    //client disconnected
      if (display.m_nWsConnected)
        display.m_nWsConnected--;
      WsClientID = 0;
      if(client->id() == WsPcClientID)
        WsPcClientID = 0;
      break;
    case WS_EVT_ERROR:    //error was received from the other end
      break;
    case WS_EVT_PONG:    //pong message was received (in response to a ping request maybe)
      break;
    case WS_EVT_DATA:  //data packet
      AwsFrameInfo * info = (AwsFrameInfo*)arg;
      if (info->final && info->index == 0 && info->len == len) {
        //the whole message is in a single frame and we got all of it's data
        if (info->opcode == WS_TEXT) {
          data[len] = 0;
          jsonParse.process((char *)data);
        }
      }
      break;
  }
}
#endif

void setup()
{
  ets_printf("Starting\n"); // print over USB
  display.init();
  if(ee.bWiFiEnabled)
    startWiFi();

  IrSender.begin(); // Start with IR_SEND_PIN as send pin
  IrSender.enableIROut(SAMSUNG_KHZ); // Call it with 38 kHz just to initialize the values

  if(ee.bBtEnabled == false)
    btStop();
#ifdef BLE_ENABLE
  if(ee.bBtEnabled)
    bleKeyboard.begin();
#endif

#ifdef SERVER_ENABLE
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on ( "/", HTTP_GET | HTTP_POST, [](AsyncWebServerRequest * request)
  {
    request->send_P( 200, "text/html", page1);
  });

  server.onNotFound([](AsyncWebServerRequest * request) {
    request->send(404);
  });
  server.on("/heap", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(200, "text/plain", String(ESP.getFreeHeap()));
  });

  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest * request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, "image/x-icon", favicon, sizeof(favicon));
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });
  server.begin();
  jsonParse.setList(jsonListCmd);
#endif
}

void loop()
{
  static uint8_t hour_save, min_save = 255, sec_save;
  static int8_t lastSec;
  static int8_t lastHour;
  
  display.service();  // check for touch, etc.

  if(uTime.check(ee.tz))
  {
  }

  serviceWiFi(); // handles OTA

#ifndef DISABLE_CODE_FOR_RECEIVER
  if(bRXActive)
  {
     if (IrReceiver.decode())
     {
        if (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_WAS_OVERFLOW)
        {
//          consolePrint("Try to increase the \"RAW_BUFFER_LENGTH\" value" );
        }
        else
        {
            if (IrReceiver.decodedIRData.protocol == UNKNOWN)
            {
              consolePrint(F("Received noise or an unknown (or not yet enabled) protocol. Stopped"));
              IrReceiver.stop();
              bRXActive = 0;
            }
            else
            {
              display.RingIndicator(1);
              decodePrint( IrReceiver.decodedIRData.protocol, IrReceiver.decodedIRData.address, IrReceiver.decodedIRData.command,
                IrReceiver.decodedIRData.decodedRawData, IrReceiver.decodedIRData.numberOfBits, IrReceiver.decodedIRData.flags);
              IrReceiver.resume();
            }
        }
     }
  }
#endif

  if(sec_save != second()) // only do stuff once per second
  {
    sec_save = second();
    if(secondsWiFi()) // once per second stuff, returns true once on connect
      uTime.start();
    display.oneSec();

    if(min_save != minute()) // only do stuff once per minute
    {
      min_save = minute();

      if(hour_save != hour()) // update our IP and time daily (at 2AM for DST)
      {
        hour_save = hour();
        if(hour_save == 2)
          uTime.start(); // update time daily at DST change

        ee.update();
      }
    }
    if (--ssCnt == 0) // keepalive
      sendState();
  }
  delay(1);
}
