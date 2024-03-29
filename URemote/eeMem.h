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
  eeMem();
  bool check(void);
  bool update(void);
  uint16_t getSum(void);
  uint16_t Fletcher16( uint8_t* data, int count);

public:
  uint16_t size = EESIZE;            // if size changes, use defaults
  uint16_t sum = 0xAAAA;             // if sum is different from memory struct, write
  char     szSSID[24] = "";  // Enter you WiFi router SSID
  char     szSSIDPassword[24] = ""; // and password
  char     iotPassword[24] = "password"; // password for controlling dimmers and switches
  int8_t   tz = -5;                 // local timezone
  bool     bBtEnabled = true;
  bool     bWiFiEnabled = true;
  uint8_t  lightIp[4] = {192,168,31,73}; // Device to get all lights/switches/dimmers IP list
  uint8_t  brightLevel[2] = {30, 180}; // brightness {dim, highest}
#define EE_LIGHT_CNT 18
  iotLight lights[EE_LIGHT_CNT] =
  {
    {"LivingRoom", 0}, // Fill in priority
    {"Basement", 0},
    {},
  };
  uint8_t  reserved[64];           // Note: To force an EEPROM update, just subtract 1 byte and flash again
  uint8_t  end;
}; // 512 bytes

static_assert(EESIZE <= 512, "EEPROM struct too big");

extern eeMem ee;

#endif // EEMEM_H
