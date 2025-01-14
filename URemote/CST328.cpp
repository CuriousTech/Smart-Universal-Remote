#include "CST328.h"
#include <FunctionalInterrupt.h>

CST328::CST328(int sda, int scl, int rst, int irq)
{
  _sda = sda;
  _scl = scl;
  _rst = rst;
  _irq = irq;
}

bool CST328::I2C_Read(uint8_t Driver_addr, uint16_t Reg_addr, uint8_t *Reg_data, uint32_t Length)
{
  Wire1.beginTransmission(Driver_addr);
  Wire1.write((uint8_t)(Reg_addr >> 8)); 
  Wire1.write((uint8_t)Reg_addr);        
  if ( Wire1.endTransmission(true))
    return false;
  Wire1.requestFrom(Driver_addr, Length);
  for (int i = 0; i < Length; i++)
    *Reg_data++ = Wire1.read();

  return true;
}
bool CST328::I2C_Write(uint8_t Driver_addr, uint16_t Reg_addr, const uint8_t *Reg_data, uint32_t Length)
{
  Wire1.beginTransmission(Driver_addr);
  Wire1.write((uint8_t)(Reg_addr >> 8)); 
  Wire1.write((uint8_t)Reg_addr);        

  for (int i = 0; i < Length; i++)
    Wire1.write(*Reg_data++);

  if ( Wire1.endTransmission(true))
    return false;
  return true;
}

void IRAM_ATTR CST328::handleISR()
{
  Touch_interrupts = true;
}

bool CST328::begin(int interrupt)
{
  Wire1.begin(_sda, _scl, I2C_MASTER_FREQ_HZ);
  pinMode(_irq, INPUT);
  pinMode(_rst, OUTPUT);

  reset();
  uint16_t Verification = Read_cfg();
  if(!((Verification==0xCACA)?true:false))
   ets_printf("Touch initialization failed!\r\n");

  attachInterrupt(_irq, std::bind(&CST328::handleISR, this), interrupt);

  return ((Verification==0xCACA)?true:false);
}

// Reset controller
uint8_t CST328::reset(void)
{
    digitalWrite(_rst, HIGH );
    delay(50);
    digitalWrite(_rst, LOW);
    delay(5);
    digitalWrite(_rst, HIGH );
    delay(50);
    return true;
}

uint16_t CST328::Read_cfg(void)
{
  uint8_t buf[24];

  I2C_Write(CST328_ADDR, HYN_REG_MUT_DEBUG_INFO_MODE, buf, 0);
  I2C_Read(CST328_ADDR, HYN_REG_MUT_DEBUG_INFO_BOOT_TIME,buf, 4);
//  ets_printf("TouchPad_ID:0x%02x,0x%02x,0x%02x,0x%02x\r\n", buf[0], buf[1], buf[2], buf[3]);
  I2C_Read(CST328_ADDR, HYN_REG_MUT_DEBUG_INFO_BOOT_TIME, buf, 4);
//  ets_printf("TouchPad_X_MAX:%d    TouchPad_Y_MAX:%d \r\n", buf[1]*256+buf[0],buf[3]*256+buf[2]);

  I2C_Read(CST328_ADDR, HYN_REG_MUT_DEBUG_INFO_TP_NTX, buf, 24);
//  ets_printf("D1F4:0x%02x,0x%02x,0x%02x,0x%02x\r\n", buf[0], buf[1], buf[2], buf[3]);
//  ets_printf("D1F8:0x%02x,0x%02x,0x%02x,0x%02x\r\n", buf[4], buf[5], buf[6], buf[7]);
//  ets_printf("D1FC:0x%02x,0x%02x,0x%02x,0x%02x\r\n", buf[8], buf[9], buf[10], buf[11]);
//  ets_printf("D200:0x%02x,0x%02x,0x%02x,0x%02x\r\n", buf[12], buf[13], buf[14], buf[15]);
//  ets_printf("D204:0x%02x,0x%02x,0x%02x,0x%02x\r\n", buf[16], buf[17], buf[18], buf[19]);
//  ets_printf("D208:0x%02x,0x%02x,0x%02x,0x%02x\r\n", buf[20], buf[21], buf[22], buf[23]);
//  ets_printf("CACA Read:0x%04x\r\n", (((uint16_t)buf[11] << 8) | buf[10]));

  I2C_Write(CST328_ADDR, HYN_REG_MUT_NORMAL_MODE, buf, 0);
  return (((uint16_t)buf[11] << 8) | buf[10]);
}

bool CST328::available(void)
{
  if(Touch_interrupts == false)
    return false;
  Touch_interrupts = false;

  uint8_t buf[41];
  uint8_t touch_cnt = 0;
  uint8_t clear = 0;

  I2C_Read(CST328_ADDR, ESP_LCD_TOUCH_CST328_READ_Number_REG, buf, 1);
  touch_cnt = buf[0] & 0x0F;

  if(touch_cnt == 0){
    I2C_Write(CST328_ADDR, ESP_LCD_TOUCH_CST328_READ_Number_REG, &clear, 1);  // No touch data (never happens)
  } else {
    // Read all points
    I2C_Read(CST328_ADDR, ESP_LCD_TOUCH_CST328_READ_XY_REG, &buf[1], 27);
    // Clear all
    I2C_Write(CST328_ADDR, ESP_LCD_TOUCH_CST328_READ_Number_REG, &clear, 1);

    if((buf[1]&0xF) == 0)
      event = 1; // release
    else if(event == 1)
      event = 0; // touch start
    else
      event = 2; // touch continue

    // Number of touched points
    if(touch_cnt > CST328_LCD_TOUCH_MAX_POINTS)
        touch_cnt = CST328_LCD_TOUCH_MAX_POINTS;
    points = (uint8_t)touch_cnt;
    // Fill all coordinates
    uint8_t num = 0;
    for(uint8_t i = 0; i < touch_cnt; i++)
    {
      if(i) num = 2;
      coords[i].x = (uint16_t)(((uint16_t)buf[(i * 5) + 2 + num] << 4) + ((buf[(i * 5) + 4 + num] & 0xF0)>> 4));
      coords[i].y = (uint16_t)(((uint16_t)buf[(i * 5) + 3 + num] << 4) + ( buf[(i * 5) + 4 + num] & 0x0F));
      coords[i].strength = ((uint16_t)buf[(i * 5) + 5 + num]);
    }
    x = coords[0].x;
    y = coords[0].y;
    strength = coords[0].strength;
  }
  return true;
}
