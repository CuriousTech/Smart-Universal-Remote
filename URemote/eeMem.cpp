#include "eeMem.h"
#include <FS.h>
#include <FFat.h>

void eeMem::init()
{
  uint8_t data[EESIZE];
  uint16_t *pwTemp = (uint16_t *)data;

  m_bFsOpen = FFat.begin(true);

  if(!m_bFsOpen)
  {
    ets_printf("FATFS failed\r\n");
    return;
  }

  File F = FFat.open("/eemem.bin", "r");
  if(F)
  {
    F.read(data, EESIZE);
    F.close();
  }
  if(pwTemp[0] != EESIZE)
    return; // revert to defaults if struct size changes

  uint16_t sum = pwTemp[1];
  pwTemp[1] = 0;
  pwTemp[1] = Fletcher16(data, EESIZE );
  if(pwTemp[1] != sum)
    return; // revert to defaults if sum fails
  memcpy(this + offsetof(eeMem, size), data, EESIZE );
}

bool eeMem::update() // write the settings if changed
{
  if(!check() || m_bFsOpen == false) // make sure sum is correct, and if it can save
    return false;

  nWriteCnt++;
  check(); // recalc sum

  File F = FFat.open("/eemem.bin", "w");
  if(F)
  {
    F.write((byte*) this + offsetof(eeMem, size), EESIZE);
    F.close();
    return true;
  }
  return false;
}

bool eeMem::check()
{
  uint16_t old_sum = ee.sum;
  ee.sum = 0;
  ee.sum = Fletcher16((uint8_t*)this + offsetof(eeMem, size), EESIZE);
  return (old_sum == ee.sum) ? false:true;
}

uint16_t eeMem::getSum()
{
  return Fletcher16((uint8_t*)this + offsetof(eeMem, size), EESIZE);
}

uint16_t eeMem::Fletcher16( uint8_t* data, int count)
{
   uint16_t sum1 = 0;
   uint16_t sum2 = 0;

   for( int index = 0; index < count; ++index )
   {
      sum1 = (sum1 + data[index]) % 255;
      sum2 = (sum2 + sum1) % 255;
   }

   return (sum2 << 8) | sum1;
}
