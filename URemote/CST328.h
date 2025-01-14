#ifndef CST328_H
#define CST328_H

#include "Arduino.h"
#include <Wire.h>

#define CST328_ADDR         0x1A
#define I2C_MASTER_FREQ_HZ  400000                     /*!< I2C master clock frequency */


#define CST328_LCD_TOUCH_MAX_POINTS             (5)      
/* CST328 registers */
#define ESP_LCD_TOUCH_CST328_READ_XY_REG        (0xD000)
#define ESP_LCD_TOUCH_CST328_READ_Number_REG    (0xD005)
#define ESP_LCD_TOUCH_CST328_READ_Checksum_REG  (0x80FF)
#define ESP_LCD_TOUCH_CST328_CONFIG_REG         (0x8047)

//debug info
/****************HYN_REG_MUT_DEBUG_INFO_MODE address start***********/
#define HYN_REG_MUT_DEBUG_INFO_IC_CHECKSUM      0xD208
#define HYN_REG_MUT_DEBUG_INFO_FW_VERSION       0xD204
#define HYN_REG_MUT_DEBUG_INFO_IC_TYPE			    0xD202
#define HYN_REG_MUT_DEBUG_INFO_PROJECT_ID			  0xD200 
#define HYN_REG_MUT_DEBUG_INFO_BOOT_TIME        0xD1FC
#define HYN_REG_MUT_DEBUG_INFO_RES_Y            0xD1FA
#define HYN_REG_MUT_DEBUG_INFO_RES_X            0xD1F8
#define HYN_REG_MUT_DEBUG_INFO_KEY_NUM          0xD1F7
#define HYN_REG_MUT_DEBUG_INFO_TP_NRX           0xD1F6
#define HYN_REG_MUT_DEBUG_INFO_TP_NTX           0xD1F4
//workmode
#define HYN_REG_MUT_DEBUG_INFO_MODE             0xD101
#define HYN_REG_MUT_RESET_MODE            		  0xD102
#define HYN_REG_MUT_DEBUG_RECALIBRATION_MODE    0xD104
#define HYN_REG_MUT_DEEP_SLEEP_MODE    			    0xD105
#define HYN_REG_MUT_DEBUG_POINT_MODE	    	    0xD108
#define HYN_REG_MUT_NORMAL_MODE                 0xD109

#define HYN_REG_MUT_DEBUG_RAWDATA_MODE          0xD10A
#define HYN_REG_MUT_DEBUG_DIFF_MODE             0xD10D
#define HYN_REG_MUT_DEBUG_FACTORY_MODE          0xD119
#define HYN_REG_MUT_DEBUG_FACTORY_MODE_2        0xD120

//Interrupt Modes
#define RISING    0x01
#define FALLING   0x02
#define CHANGE    0x03
#define ONLOW     0x04
#define ONHIGH    0x05
#define ONLOW_WE  0x0C
#define ONHIGH_WE 0x0D

//#define interrupt RISING

class CST328
{
public:
  CST328(int sda, int scl, int rst, int irq);
  bool begin(int interrupt = RISING);
  bool available(void);
  uint8_t reset(void);

  uint16_t x; // !< X coordinate
  uint16_t y; // !< Y coordinate
  uint16_t strength;
  int gestureID;
  int event;

private:
  int _sda;
  int _scl;
  int _rst;
  int _irq;

  uint16_t Read_cfg(void);
  bool I2C_Read(uint8_t Driver_addr, uint16_t Reg_addr, uint8_t *Reg_data, uint32_t Length);
  bool I2C_Write(uint8_t Driver_addr, uint16_t Reg_addr, const uint8_t *Reg_data, uint32_t Length);
  void IRAM_ATTR handleISR();

  bool Touch_interrupts;

  uint8_t points;    // Number of touch points
  struct {
    uint16_t x; // !< X coordinate
    uint16_t y; // !< Y coordinate
    uint16_t strength; // !< Strength
  }coords[CST328_LCD_TOUCH_MAX_POINTS];
};

#endif CST328_H
