#ifndef EEMEM_H
#define EEMEM_H

#include <Arduino.h>

#define EESIZE (offsetof(eeMem, end) - offsetof(eeMem, size) )

struct iotLight
{
  char szName[16];
  uint8_t ip[4];
};

class eeMem
{
public:
  eeMem(){};
  void init(void);
  bool check(void);
  bool update(void);
  uint16_t getSum(void);
  uint16_t Fletcher16( uint8_t* data, int count);

  uint16_t size = EESIZE;            // if size changes, use defaults
  uint16_t sum = 0xAAAA;             // if sum is different from memory struct, write
  uint16_t nWriteCnt;               // just a counter for later checking
  char     szSSID[24] = "";  // Enter you WiFi router SSID
  char     szSSIDPassword[24] = ""; // and password
  char     iotPassword[24] = "password"; // password for controlling dimmers and switches
  int8_t   tz = -5;                 // local timezone
  bool     bBtEnabled = false;
  bool     bWiFiEnabled = true;
  uint8_t  res;
  uint8_t  hostIp[4] = {192,168,31,100};
  uint8_t  hostPort = 85;
  uint8_t  brightLevel[2] = {30, 180}; // brightness {dim, highest}
  uint16_t ssTime = 90; // timer for screensaver
  uint16_t sleepTime = 5*60; // timer for sleep mode

#define EE_LIGHT_CNT 18
  iotLight lights[EE_LIGHT_CNT] =
  {
    {"LivingRoom", {192,168,31,73}}, // IP for discovery
    {},
  };
  uint16_t vadcMax = 1400;
  uint8_t  reserved[54];           // Note: To force an EEPROM update, just subtract 1 byte and flash again
  uint8_t  end;

private:
  bool m_bFsOpen;
};

extern eeMem ee;

#endif // EEMEM_H
