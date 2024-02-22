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
//  ESP32: (2.0.13) ESP32C3 Dev Module, QIO, 160MHz or 80MHz, Minimal SPIFFS (1.9MB with OTA/190KB SPIFFS) SPIFFS not used, but OTA is.

#define OTA_ENABLE  //uncomment to enable Arduino IDE Over The Air update code (uses ~4K heap)
#define SERVER_ENABLE // uncomment to enable server and WebSocket
//#define BLE_ENABLE // uncomment for BLE keyboard (uses > 100K heap)

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
uint8_t nWsConnected;
int WsClientID;
void jsonCallback(int16_t iName, int iValue, char *psValue);
JsonParse jsonParse(jsonCallback);
#endif

#include "Lights.h"

#ifdef BLE_ENABLE
#include <BleKeyboard.h> // https://github.com/T-vK/ESP32-BLE-Keyboard
BleKeyboard bleKeyboard;
#endif

#define IR_RECEIVE_PIN   20 // RXD0
#define IR_SEND_PIN      21 // TXD0
#define TONE_PIN         10 // ADC2_0
#define APPLICATION_PIN  11
#define DISABLE_CODE_FOR_RECEIVER // Disables restarting receiver after each send. Saves 450 bytes program memory and 269 bytes RAM if receiving functions are not used.
//#define EXCLUDE_EXOTIC_PROTOCOLS  // Saves around 240 bytes program memory if IrSender.write is used
//#define SEND_PWM_BY_TIMER         // Disable carrier PWM generation in software and use (restricted) hardware PWM.
//#define USE_NO_SEND_PWM           // Use no carrier PWM, just simulate an active low receiver signal. Overrides SEND_PWM_BY_TIMER definition
#define NO_LED_FEEDBACK_CODE      // Saves 566 bytes program memory
#include <IRremote.hpp> // IRremote from library manager

#include "display.h"

Lights lights;
const char *hostName = "URemote"; // Device and OTA name

bool bConfigDone = false; // EspTouch done or creds set
bool bStarted = false;
uint32_t connectTimer;

Display display;
eeMem ee;
UdpTime uTime;

uint8_t protoList[]= // conversion from webpage list
{
    SAMSUNG,
    SAMSUNG_LG,
    SAMSUNG48,
    DENON,
    JVC,
    LG,
    LG2,
    PANASONIC,
    SHARP,
    SONY,
    APPLE,
    NEC,
    NEC2, /* NEC with full frame as repeat */
    ONKYO,

    PULSE_WIDTH,
    PULSE_DISTANCE,
    KASEIKYO,
    KASEIKYO_DENON,
    KASEIKYO_SHARP,
    KASEIKYO_JVC,
    KASEIKYO_MITSUBISHI,
    RC5,
    RC6,
};

void sendIR(uint16_t *pCode)
{
  IRData IRSendData;
  IRSendData.protocol = (decode_type_t)pCode[0];
  IRSendData.address = pCode[1]; // Address
  IRSendData.command = pCode[2]; // Command
  IRSendData.flags = IRDATA_FLAGS_EMPTY;

  IrSender.write(&IRSendData, pCode[3]);
//    delay(DELAY_AFTER_SEND);

  String s = "Sent ";
  s += pCode[0];
  s += " ";
  s += pCode[1];
  s += " ";
  s += pCode[2];

  jsonString js("print");
  js.Var("text", s);
  ws.textAll(js.Close());
}

//-----------------

void stopWiFi()
{
  ee.update();
#ifdef OTA_ENABLE
  ArduinoOTA.end();
  delay(100); // fix ArduinoOTA crash?
#endif
  WiFi.disconnect();
  WiFi.enableAP(false);
  WiFi.enableSTA(false);
}

void restartWiFi()
{
#ifdef OTA_ENABLE
  ArduinoOTA.begin();
#endif
}

void startWiFi()
{
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

#ifdef BLE_ENABLE
  bleKeyboard.begin();
#endif

#ifdef OTA_ENABLE
  ArduinoOTA.setHostname(hostName);
  ArduinoOTA.begin();
  ArduinoOTA.onStart([]() {
    ee.update();
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
  return bleKeyboard.write(pCode);
#else
  return false;
#endif
}

#ifdef SERVER_ENABLE

const char *jsonListCmd[] = {
  "PROTO",
  "ADDR",
  "CODE",
  NULL
};

void jsonCallback(int16_t iName, int iValue, char *psValue)
{
  static uint16_t code[4];

  switch (iName)
  {
    case 0: // PROTO
      code[0] = protoList[iValue];
      break;
    case 1: // ADDR
      code[1] = iValue;
      break;
    case 2: // CODE
      code[2] = iValue;
      sendIR(code);
      break;
  }
}

String dataJson()
{
  jsonString js("state");
  js.Var("t", (long)now() - ( (ee.tz + uTime.getDST() ) * 3600) );
  return js.Close();
}

String setupJson()
{
  jsonString js("setup");
  js.Var("tz", ee.tz);
  return js.Close();
}

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len)
{ //Handle WebSocket event

  switch (type)
  {
    case WS_EVT_CONNECT:      //client connected
      client->text(setupJson());
      client->text(dataJson());
      nWsConnected++;
      WsClientID = client->id();
      break;
    case WS_EVT_DISCONNECT:    //client disconnected
      if (nWsConnected)
        nWsConnected--;
      WsClientID = 0;
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
  startWiFi();
  if(ee.bBtEnabled == false)
    btStop();

  IrSender.begin(); // Start with IR_SEND_PIN as send pin
  IrSender.enableIROut(SAMSUNG_KHZ); // Call it with 38 kHz just to initialize the values

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
  }
  delay(1);
}
