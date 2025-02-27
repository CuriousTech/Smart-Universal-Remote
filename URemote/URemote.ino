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
//  ESP32: (2.0.14) ESP32S3 Dev Module, QIO, CPU Freq: 80MHz for lowest power (<60mA),
//  Flash: 16MB
//  Partition: 16MB (3MB APP/9.9 FATFS)
// For 1.28"
//  PSRAM: QSPI PSRAM
//  In TFT_eSPI/User_Setup_Select.h use #include <User_Setups/Setup302_Waveshare_ESP32S3_GC9A01.h>
// For 1.69"
//  PSRAM: QPI PSRAM
//    In TFT_eSPI/User_Setup_Select.h use #include <User_Setups/Setup203_ST7789.h> (custom, included in repo)
// For 2.8"
//  PSRAM: QPI PSRAM
//  In TFT_eSPI/User_Setup_Select.h use #include <User_Setups/Setup303_Waveshare_ESP32S3_ST7789.h> (custom, included in repo)

#define OTA_ENABLE  //uncomment to enable Arduino IDE Over The Air update code (uses ~4K heap)
#define SERVER_ENABLE // uncomment to enable server and WebSocket
//#define BLE_ENABLE // uncomment for BLE keyboard (uses 510KB (16%) PROGMEN and >100K heap)
#define CLIENT_ENABLE // uncomment for PC client WebSocket
#define IR_ENABLE // uncomment for IR

#include <WiFi.h>
#include <TimeLib.h> // https://github.com/PaulStoffregen/Time
#include <UdpTime.h> // https://github.com/CuriousTech/ESP07_WiFiGarageDoor/tree/master/libraries/UdpTime

#include "Media.h"
#include "eeMem.h"
#include <ESPmDNS.h>
#ifdef OTA_ENABLE
#include <ArduinoOTA.h>
#endif

#ifdef SERVER_ENABLE
#include <ESPAsyncWebServer.h> // https://github.com/mathieucarbou/ESPAsyncWebServer
#include <JsonParse.h> // https://github.com/CuriousTech/ESP-HVAC/tree/master/Libraries/JsonParse
#include "jsonString.h"
#include "pages.h"

Media media;

AsyncWebServer server( 80 );
AsyncWebSocket ws("/ws"); // access at ws://[esp ip]/ws
int WsClientID;
void jsonCallback(int16_t iName, int iValue, char *psValue);
JsonParse jsonParse(jsonCallback);
#endif
int WsPcClientID;

#ifdef BLE_ENABLE
#include <BleKeyboard.h> // https://github.com/T-vK/ESP32-BLE-Keyboard
BleKeyboard bleKeyboard;
#endif

#ifdef CLIENT_ENABLE
#include <WebSocketsClient.h>
WebSocketsClient wsClient;
bool bWscConnected;
void WscSend(String s);
void startListener(void);
#endif

// GPIO 15-18 reserved for keypad

#ifdef IR_ENABLE
#if (USER_SETUP_ID==302) // 240x240
 #define IR_RECEIVE_PIN   15
 #define IR_SEND_PIN      16
#elif (USER_SETUP_ID==303) // 240x320 2.8"
 #define IR_RECEIVE_PIN   43 // UART0
 #define IR_SEND_PIN      44
#else
 #define IR_RECEIVE_PIN   44 // UART0  or 17,18
 #define IR_SEND_PIN      43
#endif

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
#endif

#include "display.h"
#include "Lights.h" // Uses ~3KB
Lights lights;
const char *hostName = "URemoteB"; // Device and OTA name

bool bConfigDone = false; // EspTouch done or creds set
bool bStarted = false; // first start
bool bRestarted = false; // consecutive restarts

uint32_t connectTimer;
bool bRXActive;

Display display;
eeMem ee;
UdpTime udpTime;

void consolePrint(String s)
{
#ifdef SERVER_ENABLE
  jsonString js("print");
  js.Var("text", s);
  ws.textAll(js.Close());
#endif
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

  String s = js.Close();
  ws.textAll(s);
  ets_printf(s.c_str());
}
#endif

void sendIR(uint16_t *pCode)
{
#ifdef IR_ENABLE
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
#endif
}

//-----------------

void stopWiFi()
{
  if(WiFi.status() != WL_CONNECTED)
    return;

//  ets_printf("stopWifi\n");
  ee.update();
#ifdef OTA_ENABLE
  ArduinoOTA.end();
  delay(100); // fix ArduinoOTA crash
#endif

  display.m_nWsConnected = 0;
#ifdef SERVER_ENABLE
  WsClientID = 0;
  WsPcClientID = 0;
#ifdef CLIENT_ENABLE
  wsClient.disconnect();
#endif
#endif
  
  WiFi.setSleep(true);
  bRestarted = false;
}

const char *szWiFiStat[] = {
    "IDLE", "NO_SSID_AVAIL", "SCAN_COMPLETED", "CONNECTED", "CONNECT_FAILED", "CONNECTION_LOST", "DISCONNECTED"
};

bool secondsWiFi() // called once per second
{
  bool bConn = false;

  if(display.m_bSleeping)
    return false;

//  ets_printf("wifi status = %s\n", szWiFiStat[WiFi.status()]); // Wifi debugging

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
        lights.init();
        bConn = true;
#ifdef CLIENT_ENABLE
        startListener();
#endif
      }
      if(!bRestarted) // wakeup work if needed
      {
        bRestarted = true;
// todo: reconnect to host
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

bool sendPCMediaCmd( uint16_t *pCode)
{
#ifdef CLIENT_ENABLE

  display.RingIndicator(2);
  if(pCode[0] == 1000)
  {
    jsonString js("volume");
    js.Var("value", pCode[1]);
    WscSend(js.Close());
  }
  else if(pCode[0] == 1001)
  {
    jsonString js("pos");
    js.Var("value", pCode[1]);
    WscSend(js.Close());
  }
  else
  {
    jsonString js("media");
    js.Var("HOTKEY", pCode[0]);
    WscSend(js.Close());
  }
  return true;
#else
  return false;
#endif
}

// Currently using PC to relay
bool sendStatCmd( uint16_t *pCode)
{
#ifdef CLIENT_ENABLE
  display.RingIndicator(2);
  jsonString js("STAT");
  js.Var("value", pCode[0]);
  WscSend(js.Close());
  return true;
#else
  return false;
#endif
}

// Currently using PC to relay
bool sendGdoCmd( uint16_t *pCode)
{
#ifdef CLIENT_ENABLE
  display.RingIndicator(2);
  jsonString js("GDOCMD");
  js.Var("value", pCode[0]);
  WscSend(js.Close());
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
  "PC_MED_POS",
  "LED",
  "STATTEMP",
  "STATSETTEMP", // 10
  "STATFAN",
  "OUTTEMP",
  "ST", // sleeptime
  "SS", // screensaver time
  "restart",
  "GDODOOR",
  "GDOCAR",
  "delf",
  NULL
};

// this is for the web page, and needs to be seperated from the PC client
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
#if defined(IR_ENABLE) && !defined(DISABLE_CODE_FOR_RECEIVER)
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
      display.setSliderValue(BTF_PCVolume, iValue);
      break;
    case 7: // PC_MED_POS
      display.setSliderValue(BTF_PC_Media, iValue);
      break;
    case 8: // LED
//      digitalWrite(IR_SEND_PIN, iValue ? HIGH:LOW);
      break;
    case 9: // STATTEMP
      display.m_statTemp = iValue;
      break;
    case 10: // STATSETTEMP
      display.m_statSetTemp = iValue;
      break;
    case 11: // STATFAN
      display.m_bStatFan = iValue;
      break;
    case 12: // OUTTEMP
      display.m_outTemp = iValue;
      break;
    case 13: // ST
      ee.sleepTime = iValue;
      break;
    case 14: // SS
      ee.ssTime = iValue;
      break;
    case 15:
      ESP.restart();
      break;
    case 16:
      display.m_bGdoDoor = iValue;
      break;
    case 17:
      display.m_bGdoCar = iValue;
      break;
    case 18: // delf
      media.deleteIFile(psValue);
      ws.textAll(setupJson()); // update disk free
      break;
  }
}

void onUploadInternal(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
  if(!index)
  {
    String sFile = "/";
    sFile += filename;
    request->_tempFile = INTERNAL_FS.open(sFile, "w");
  }
  if(len)
   request->_tempFile.write((byte*)data, len);
  if(final)
  {
    request->_tempFile.close();
  }
}

#endif

void startWiFi()
{
  // ets_printf("startWiFi\r\n");
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
//    ets_printf("Start WiFi %s %s\n", ee.szSSID, ee.szSSIDPassword);
  }
  else
  {
    WiFi.beginSmartConfig();
    display.notify("Use EspTouch");
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

#ifdef SERVER_ENABLE
  static bool bInit = false;
  if(!bInit)
  {
    bInit = true;
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
    server.on("/del-btn.png", HTTP_GET, [](AsyncWebServerRequest *request){
      AsyncWebServerResponse *response = request->beginResponse_P(200, "image/png", delbtn_png, sizeof(delbtn_png));
      request->send(response);
    });
    server.on ( "/upload_internal", HTTP_POST, [](AsyncWebServerRequest * request)
    {
      request->send( 200);
      ws.textAll(setupJson()); // update free space
    }, onUploadInternal);
    server.begin();
    jsonParse.setList(jsonListCmd);
  }
  server.serveStatic("/fs", INTERNAL_FS, "/");
#endif
}

void serviceWiFi()
{
#ifdef OTA_ENABLE
// Handle OTA server.
  ArduinoOTA.handle();
#endif

#ifdef CLIENT_ENABLE
  wsClient.loop();
#endif
}

#ifdef SERVER_ENABLE

//this does basically nothing for now (but used for keepalive)
String dataJson()
{
  jsonString js("state");
  js.Var("t", (long)now() - ( (ee.tz + udpTime.getDST() ) * 3600) );
  return js.Close();
}

uint8_t ssCnt = 26;

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
  uint32_t freeK = (INTERNAL_FS.totalBytes() - INTERNAL_FS.usedBytes()) / 1024;
  js.Var("freek",  freeK);
  return js.Close();
}

void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len)
{ //Handle WebSocket event

  switch (type)
  {
    case WS_EVT_CONNECT:      //client connected
      client->text(setupJson());
      client->text(dataJson());
      client->text( media.internalFileListJson(INTERNAL_FS, "/") );
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
  delay(1000);
  ets_printf("\nStarting\n"); // print over USB
  ets_printf("Free: %ld\n", heap_caps_get_free_size(MALLOC_CAP_8BIT) );

  ee.init();
  display.init();
  media.init();

  if(ee.bWiFiEnabled)
    startWiFi();

#ifdef IR_ENABLE
  pinMode(IR_SEND_PIN, OUTPUT);
  digitalWrite(IR_SEND_PIN, LOW);
  IrSender.begin(); // Start with IR_SEND_PIN as send pin
  IrSender.enableIROut(SAMSUNG_KHZ); // Call it with 38 kHz just to initialize the values

#endif

  if(ee.bBtEnabled == false)
    btStop();
#ifdef BLE_ENABLE
  if(ee.bBtEnabled)
    bleKeyboard.begin();
#endif
}

void loop()
{
  static uint8_t hour_save, min_save = 255, sec_save;
  static int8_t lastSec;
  static int8_t lastHour;

  display.service();  // check for touch, etc.

  if(WiFi.status() == WL_CONNECTED)
    if(udpTime.check(ee.tz))
    {
    }

  serviceWiFi(); // handles OTA

#if defined(IR_ENABLE) && !defined(DISABLE_CODE_FOR_RECEIVER)
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

              ets_printf("Received noise or an unknown (or not yet enabled) protocol. Stopped\r\n");

              IrReceiver.stop();
              bRXActive = false;
            }
            else
            {
              display.RingIndicator(1);
#ifdef SERVER_ENABLE
              decodePrint( IrReceiver.decodedIRData.protocol, IrReceiver.decodedIRData.address, IrReceiver.decodedIRData.command,
                IrReceiver.decodedIRData.decodedRawData, IrReceiver.decodedIRData.numberOfBits, IrReceiver.decodedIRData.flags);
#endif
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
      udpTime.start();

    display.oneSec();

    if(min_save != minute()) // only do stuff once per minute
    {
      min_save = minute();

      if(hour_save != hour()) // update our IP and time daily (at 2AM for DST)
      {
        hour_save = hour();
        if(hour_save == 2 && WiFi.status() == WL_CONNECTED)
          udpTime.start(); // update time daily at DST change

        ee.update();
      }
    }
//    String s = "ADC ";
//    s += display.m_vadc;
//    ws.textAll( s);
#ifdef SERVER_ENABLE
    if (--ssCnt == 0) // keepalive
      sendState();
#endif
  }

  delay(1);
}


// remote WebSocket for sending all commands to PC and recieving states
// Thermostate and GDO go through this as well for speed

#ifdef CLIENT_ENABLE

void WscSend(String s) 
{
  if(bWscConnected)
    wsClient.sendTXT(s);
}

static void clientEventHandler(WStype_t type, uint8_t * payload, size_t length)
{
  switch(type)
  {
    case WStype_CONNECTED:
      bWscConnected = true;
//      display.notify("PC connected"); // helps see things connecting
      break;
    case WStype_DISCONNECTED:
      bWscConnected = false;
      break;
    case WStype_TEXT:
      jsonParse.process((char*)payload);
      break;
    case WStype_BIN:
      break;
    case WStype_ERROR:      
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
      break;
  }
}

void startListener()
{
  IPAddress ip(ee.hostIp);
  static char szHost[32];
  wsClient.begin(ip.toString().c_str(), ee.hostPort, "/ws");

  wsClient.onEvent(clientEventHandler);
  // try ever 5000 again if connection has failed
  wsClient.setReconnectInterval(5000);
}
#endif
