#include "QMI8658.h"
#include <Arduino.h>
#include <Wire.h>

#define QMI8658_UINT_MG_DPS

enum
{
	AXIS_X = 0,
	AXIS_Y = 1,
	AXIS_Z = 2,

	AXIS_TOTAL
};

typedef struct
{
	short sign[AXIS_TOTAL];
	uint16_t map[AXIS_TOTAL];
} qst_imu_layout;


bool QMI8658::init(int sda, int scl)
{
  _sda = sda;
  _scl = scl;
  
  uint8_t QMI8658_chip_id = 0x00;
  uint8_t QMI8658_revision_id = 0x00;
  uint8_t QMI8658_slave[2] = {QMI8658_SLAVE_ADDR_L, QMI8658_SLAVE_ADDR_H};
  uint8_t iCount = 0;
  int retry = 0;

  Wire.setPins(_sda, _scl);
  Wire.setClock(400000);
  Wire.begin();

  while (iCount < 2)
  {
    QMI8658_slave_addr = QMI8658_slave[iCount];
    retry = 0;

    while ((QMI8658_chip_id != 0x05) && (++retry < 10))
    {
      read_regs(QMI8658Register_WhoAmI, &QMI8658_chip_id, 1);
      ets_printf("QMI8658Register_WhoAmI = 0x%x\n", QMI8658_chip_id);
    }
    if (QMI8658_chip_id == 0x05)
    {
      break;
    }
    iCount++;
  }
  read_regs(QMI8658Register_Revision, &QMI8658_revision_id, 1);
  if (QMI8658_chip_id == 0x05)
  {
    ets_printf("QMI8658_init slave = 0x%x\n", QMI8658_slave_addr);
    ets_printf("QMI8658Register_WhoAmI = 0x%x 0x%x\n", QMI8658_chip_id, QMI8658_revision_id);
    write_reg(QMI8658Register_Ctrl1, 0x60);
    QMI8658_config.inputSelection = QMI8658_CONFIG_ACCGYR_ENABLE; // QMI8658_CONFIG_ACCGYR_ENABLE;
    QMI8658_config.accRange = QMI8658AccRange_8g;
    QMI8658_config.accOdr = QMI8658AccOdr_1000Hz;
    QMI8658_config.gyrRange = QMI8658GyrRange_512dps; // QMI8658GyrRange_2048dps   QMI8658GyrRange_1024dps
    QMI8658_config.gyrOdr = QMI8658GyrOdr_1000Hz;
    QMI8658_config.magOdr = QMI8658MagOdr_125Hz;
    QMI8658_config.magDev = MagDev_AKM09918;
    QMI8658_config.aeOdr = QMI8658AeOdr_128Hz;

    Config_apply(&QMI8658_config);
    if (1)
    {
      uint8_t read_data = 0x00;
      read_regs(QMI8658Register_Ctrl1, &read_data, 1);
      ets_printf("QMI8658Register_Ctrl1 = 0x%x\n", read_data);
      read_regs(QMI8658Register_Ctrl2, &read_data, 1);
      ets_printf("QMI8658Register_Ctrl2 = 0x%x\n", read_data);
      read_regs(QMI8658Register_Ctrl3, &read_data, 1);
      ets_printf("QMI8658Register_Ctrl3 = 0x%x\n", read_data);
      read_regs(QMI8658Register_Ctrl4, &read_data, 1);
      ets_printf("QMI8658Register_Ctrl4 = 0x%x\n", read_data);
      read_regs(QMI8658Register_Ctrl5, &read_data, 1);
      ets_printf("QMI8658Register_Ctrl5 = 0x%x\n", read_data);
      read_regs(QMI8658Register_Ctrl6, &read_data, 1);
      ets_printf("QMI8658Register_Ctrl6 = 0x%x\n", read_data);
      read_regs(QMI8658Register_Ctrl7, &read_data, 1);
      ets_printf("QMI8658Register_Ctrl7 = 0x%x\n", read_data);
    }
    return true;
  }
  else
  {
    ets_printf("QMI8658_init fail\n");
    QMI8658_chip_id = 0;
  }
  return false;
  // return QMI8658_chip_id;
}

void QMI8658::write_reg(uint8_t reg, uint8_t value)
{
  Wire.beginTransmission(QMI8658_slave_addr);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

void QMI8658::write_regs(uint8_t reg, uint8_t *value, uint8_t len)
{
	for(uint8_t i = 0; i < len; i++)
		write_reg(reg + i, value[i]);
}

void QMI8658::read_regs(uint8_t reg, uint8_t *buf, uint8_t len)
{
  Wire.beginTransmission(QMI8658_slave_addr);
  Wire.write(reg);
  Wire.endTransmission();

  Wire.requestFrom((uint8_t)QMI8658_slave_addr, (size_t)len, true);

  for(uint8_t i = 0; i < len; i++)
    buf[i] =  Wire.read();
}

uint8_t QMI8658::read_reg(uint8_t reg)
{
  Wire.beginTransmission(QMI8658_slave_addr);
  Wire.write(reg);
  Wire.endTransmission();

  Wire.requestFrom((uint8_t)QMI8658_slave_addr, (size_t)1, true);
  return Wire.read();
}


void QMI8658::config_acc(enum QMI8658_AccRange range, enum QMI8658_AccOdr odr, enum QMI8658_LpfConfig lpfEnable, enum QMI8658_StConfig stEnable)
{
	uint8_t ctl_dada;

	switch (range)
	{
  	case QMI8658AccRange_2g:
  		acc_lsb_div = (1 << 14);
  		break;
  	case QMI8658AccRange_4g:
  		acc_lsb_div = (1 << 13);
  		break;
  	case QMI8658AccRange_8g:
  		acc_lsb_div = (1 << 12);
  		break;
  	case QMI8658AccRange_16g:
  		acc_lsb_div = (1 << 11);
  		break;
  	default:
  		range = QMI8658AccRange_8g;
  		acc_lsb_div = (1 << 12);
	}
	if (stEnable == QMI8658St_Enable)
		ctl_dada = (uint8_t)range | (uint8_t)odr | 0x80;
	else
		ctl_dada = (uint8_t)range | (uint8_t)odr;

	write_reg(QMI8658Register_Ctrl2, ctl_dada);
	// set LPF & HPF
	read_regs(QMI8658Register_Ctrl5, &ctl_dada, 1);
	ctl_dada &= 0xf0;
	if (lpfEnable == QMI8658Lpf_Enable)
	{
		ctl_dada |= A_LSP_MODE_3;
		ctl_dada |= 0x01;
	}
	else
	{
		ctl_dada &= ~0x01;
	}
	ctl_dada = 0x00;
	write_reg(QMI8658Register_Ctrl5, ctl_dada);
	// set LPF & HPF
}

void QMI8658::config_gyro(enum QMI8658_GyrRange range, enum QMI8658_GyrOdr odr, enum QMI8658_LpfConfig lpfEnable, enum QMI8658_StConfig stEnable)
{
	// Set the CTRL3 register to configure dynamic range and ODR
	uint8_t ctl_dada;

	// Store the scale factor for use when processing raw data
	switch (range)
	{
  	case QMI8658GyrRange_32dps:
  		gyro_lsb_div = 1024;
  		break;
  	case QMI8658GyrRange_64dps:
  		gyro_lsb_div = 512;
  		break;
  	case QMI8658GyrRange_128dps:
  		gyro_lsb_div = 256;
  		break;
  	case QMI8658GyrRange_256dps:
  		gyro_lsb_div = 128;
  		break;
  	case QMI8658GyrRange_512dps:
  		gyro_lsb_div = 64;
  		break;
  	case QMI8658GyrRange_1024dps:
  		gyro_lsb_div = 32;
  		break;
  	case QMI8658GyrRange_2048dps:
  		gyro_lsb_div = 16;
  		break;
  	case QMI8658GyrRange_4096dps:
  		gyro_lsb_div = 8;
  		break;
  	default:
  		range = QMI8658GyrRange_512dps;
  		gyro_lsb_div = 64;
  		break;
	}

	if (stEnable == QMI8658St_Enable)
		ctl_dada = (uint8_t)range | (uint8_t)odr | 0x80;
	else
		ctl_dada = (uint8_t)range | (uint8_t)odr;
	write_reg(QMI8658Register_Ctrl3, ctl_dada);

	// Conversion from degrees/s to rad/s if necessary
	// set LPF & HPF
	read_regs(QMI8658Register_Ctrl5, &ctl_dada, 1);
	ctl_dada &= 0x0f;
	if (lpfEnable == QMI8658Lpf_Enable)
	{
		ctl_dada |= G_LSP_MODE_3;
		ctl_dada |= 0x10;
	}
	else
	{
		ctl_dada &= ~0x10;
	}
	ctl_dada = 0x00;
	write_reg(QMI8658Register_Ctrl5, ctl_dada);
	// set LPF & HPF
}

void QMI8658::config_mag(enum QMI8658_MagDev device, enum QMI8658_MagOdr odr)
{
	write_reg(QMI8658Register_Ctrl4, device | odr);
}

void QMI8658::config_ae(enum QMI8658_AeOdr odr)
{
	config_acc(QMI8658_config.accRange, QMI8658_config.accOdr, QMI8658Lpf_Enable, QMI8658St_Disable);
	config_gyro(QMI8658_config.gyrRange, QMI8658_config.gyrOdr, QMI8658Lpf_Enable, QMI8658St_Disable);
	config_mag(QMI8658_config.magDev, QMI8658_config.magOdr);
	write_reg(QMI8658Register_Ctrl6, odr);
}

uint8_t QMI8658::readStatus0(void)
{
	uint8_t status[2];

	read_regs(QMI8658Register_Status0, status, sizeof(status));
	// ets_printf("status[0x%x	0x%x]\n",status[0],status[1]);

	return status[0];
}
/*!
 * \brief Blocking read of data status register 1 (::QMI8658Register_Status1).
 * \returns Status byte \see STATUS1 for flag definitions.
 */
uint8_t QMI8658::readStatus1(void)
{
	uint8_t status;

	read_regs(QMI8658Register_Status1, &status, sizeof(status));

	return status;
}

float QMI8658::readTemp(void)
{
	uint8_t buf[2];

	read_regs(QMI8658Register_Tempearture_L, buf, 2);
	float temp = (float)buf[1] + ((float)buf[0] / 256.0);
  return temp * 9 / 5 + 32;
}

void QMI8658::read_acc_xyz(float acc_xyz[3])
{
	uint8_t buf_reg[6];
	short raw_acc_xyz[3];

	read_regs(QMI8658Register_Ax_L, buf_reg, 6); // 0x19, 25
	raw_acc_xyz[0] = (short)((uint16_t)(buf_reg[1] << 8) | (buf_reg[0]));
	raw_acc_xyz[1] = (short)((uint16_t)(buf_reg[3] << 8) | (buf_reg[2]));
	raw_acc_xyz[2] = (short)((uint16_t)(buf_reg[5] << 8) | (buf_reg[4]));

	acc_xyz[0] = (raw_acc_xyz[0] * ONE_G) / acc_lsb_div;
	acc_xyz[1] = (raw_acc_xyz[1] * ONE_G) / acc_lsb_div;
	acc_xyz[2] = (raw_acc_xyz[2] * ONE_G) / acc_lsb_div;
}

void QMI8658::read_gyro_xyz(float gyro_xyz[3])
{
	uint8_t buf_reg[6];
	short raw_gyro_xyz[3];

	read_regs(QMI8658Register_Gx_L, buf_reg, 6); // 0x1f, 31
	raw_gyro_xyz[0] = (short)((uint16_t)(buf_reg[1] << 8) | (buf_reg[0]));
	raw_gyro_xyz[1] = (short)((uint16_t)(buf_reg[3] << 8) | (buf_reg[2]));
	raw_gyro_xyz[2] = (short)((uint16_t)(buf_reg[5] << 8) | (buf_reg[4]));

	gyro_xyz[0] = (raw_gyro_xyz[0] * 1.0f) / gyro_lsb_div;
	gyro_xyz[1] = (raw_gyro_xyz[1] * 1.0f) / gyro_lsb_div;
	gyro_xyz[2] = (raw_gyro_xyz[2] * 1.0f) / gyro_lsb_div;
}

void QMI8658::read_mag_xyz(float mag_xyz[3])
{
  uint8_t buf_reg[6];
  short raw_mag_xyz[3];

  read_regs(QMI8658Register_Mx_L, buf_reg, 6);
  raw_mag_xyz[0] = (short)((uint16_t)(buf_reg[1] << 8) | (buf_reg[0]));
  raw_mag_xyz[1] = (short)((uint16_t)(buf_reg[3] << 8) | (buf_reg[2]));
  raw_mag_xyz[2] = (short)((uint16_t)(buf_reg[5] << 8) | (buf_reg[4]));

  mag_xyz[0] = (raw_mag_xyz[0] * 1.0f) / (1<<2);
  mag_xyz[1] = (raw_mag_xyz[1] * 1.0f) / (1<<2);
  mag_xyz[2] = (raw_mag_xyz[2] * 1.0f) / (1<<2);
}

void QMI8658::read_xyz(float acc[3], float gyro[3], unsigned int *tim_count)
{
	uint8_t buf_reg[12];
	short raw_acc_xyz[3];
	short raw_gyro_xyz[3];

	if (tim_count)
	{
		uint8_t buf[3];
		unsigned int timestamp;
		read_regs(QMI8658Register_Timestamp_L, buf, 3); // 0x18	24
		timestamp = (unsigned int)(((unsigned int)buf[2] << 16) | ((unsigned int)buf[1] << 8) | buf[0]);
		if (timestamp > imu_timestamp)
			imu_timestamp = timestamp;
		else
			imu_timestamp = (timestamp + 0x1000000 - imu_timestamp);

		*tim_count = imu_timestamp;
	}

	read_regs(QMI8658Register_Ax_L, buf_reg, 12); // 0x19, 25
	raw_acc_xyz[0] = (short)((uint16_t)(buf_reg[1] << 8) | (buf_reg[0]));
	raw_acc_xyz[1] = (short)((uint16_t)(buf_reg[3] << 8) | (buf_reg[2]));
	raw_acc_xyz[2] = (short)((uint16_t)(buf_reg[5] << 8) | (buf_reg[4]));

	raw_gyro_xyz[0] = (short)((uint16_t)(buf_reg[7] << 8) | (buf_reg[6]));
	raw_gyro_xyz[1] = (short)((uint16_t)(buf_reg[9] << 8) | (buf_reg[8]));
	raw_gyro_xyz[2] = (short)((uint16_t)(buf_reg[11] << 8) | (buf_reg[10]));

#if defined(QMI8658_UINT_MG_DPS)
	// mg
	acc[AXIS_X] = (float)(raw_acc_xyz[AXIS_X] * 1000.0f) / acc_lsb_div;
	acc[AXIS_Y] = (float)(raw_acc_xyz[AXIS_Y] * 1000.0f) / acc_lsb_div;
	acc[AXIS_Z] = (float)(raw_acc_xyz[AXIS_Z] * 1000.0f) / acc_lsb_div;
#else
	// m/s2
	acc[AXIS_X] = (float)(raw_acc_xyz[AXIS_X] * ONE_G) / acc_lsb_div;
	acc[AXIS_Y] = (float)(raw_acc_xyz[AXIS_Y] * ONE_G) / acc_lsb_div;
	acc[AXIS_Z] = (float)(raw_acc_xyz[AXIS_Z] * ONE_G) / acc_lsb_div;
#endif
	//	acc[AXIS_X] = imu_map.sign[AXIS_X]*acc_t[imu_map.map[AXIS_X]];
	//	acc[AXIS_Y] = imu_map.sign[AXIS_Y]*acc_t[imu_map.map[AXIS_Y]];
	//	acc[AXIS_Z] = imu_map.sign[AXIS_Z]*acc_t[imu_map.map[AXIS_Z]];

#if defined(QMI8658_UINT_MG_DPS)
	// dps
	gyro[0] = (float)(raw_gyro_xyz[0] * 1.0f) / gyro_lsb_div;
	gyro[1] = (float)(raw_gyro_xyz[1] * 1.0f) / gyro_lsb_div;
	gyro[2] = (float)(raw_gyro_xyz[2] * 1.0f) / gyro_lsb_div;
#else
	// rad/s
	gyro[AXIS_X] = (float)(raw_gyro_xyz[AXIS_X] * 0.01745f) / gyro_lsb_div; // *pi/180
	gyro[AXIS_Y] = (float)(raw_gyro_xyz[AXIS_Y] * 0.01745f) / gyro_lsb_div;
	gyro[AXIS_Z] = (float)(raw_gyro_xyz[AXIS_Z] * 0.01745f) / gyro_lsb_div;
#endif
	//	gyro[AXIS_X] = imu_map.sign[AXIS_X]*gyro_t[imu_map.map[AXIS_X]];
	//	gyro[AXIS_Y] = imu_map.sign[AXIS_Y]*gyro_t[imu_map.map[AXIS_Y]];
	//	gyro[AXIS_Z] = imu_map.sign[AXIS_Z]*gyro_t[imu_map.map[AXIS_Z]];
}

void QMI8658::read_xyz_raw(short raw_acc_xyz[3], short raw_gyro_xyz[3], unsigned int *tim_count)
{
	uint8_t buf_reg[12];

	if (tim_count)
	{
		uint8_t buf[3];
		unsigned int timestamp;
		read_regs(QMI8658Register_Timestamp_L, buf, 3); // 0x18	24
		timestamp = (unsigned int)(((unsigned int)buf[2] << 16) | ((unsigned int)buf[1] << 8) | buf[0]);
		if (timestamp > imu_timestamp)
			imu_timestamp = timestamp;
		else
			imu_timestamp = (timestamp + 0x1000000 - imu_timestamp);

		*tim_count = imu_timestamp;
	}
	read_regs(QMI8658Register_Ax_L, buf_reg, 12); // 0x19, 25

	raw_acc_xyz[0] = (int16_t)((uint16_t)(buf_reg[1] << 8) | (buf_reg[0]));
	raw_acc_xyz[1] = (int16_t)((uint16_t)(buf_reg[3] << 8) | (buf_reg[2]));
	raw_acc_xyz[2] = (int16_t)((uint16_t)(buf_reg[5] << 8) | (buf_reg[4]));

	raw_gyro_xyz[0] = (int16_t)((uint16_t)(buf_reg[7] << 8) | (buf_reg[6]));
	raw_gyro_xyz[1] = (int16_t)((uint16_t)(buf_reg[9] << 8) | (buf_reg[8]));
	raw_gyro_xyz[2] = (int16_t)((uint16_t)(buf_reg[11] << 8) | (buf_reg[10]));
}

void QMI8658::read_ae(float quat[4], float velocity[3])
{
	uint8_t buf_reg[14];
	int16_t raw_q_xyz[4];
	int16_t raw_v_xyz[3];

	read_regs(QMI8658Register_Q1_L, buf_reg, 14);
	raw_q_xyz[0] = (int16_t)((uint16_t)(buf_reg[1] << 8) | (buf_reg[0]));
	raw_q_xyz[1] = (int16_t)((uint16_t)(buf_reg[3] << 8) | (buf_reg[2]));
	raw_q_xyz[2] = (int16_t)((uint16_t)(buf_reg[5] << 8) | (buf_reg[4]));
	raw_q_xyz[3] = (int16_t)((uint16_t)(buf_reg[7] << 8) | (buf_reg[6]));

	raw_v_xyz[1] = (int16_t)((uint16_t)(buf_reg[9] << 8) | (buf_reg[8]));
	raw_v_xyz[2] = (int16_t)((uint16_t)(buf_reg[11] << 8) | (buf_reg[10]));
	raw_v_xyz[3] = (int16_t)((uint16_t)(buf_reg[13] << 8) | (buf_reg[12]));

	quat[0] = (float)(raw_q_xyz[0] * 1.0f) / ae_q_lsb_div;
	quat[1] = (float)(raw_q_xyz[1] * 1.0f) / ae_q_lsb_div;
	quat[2] = (float)(raw_q_xyz[2] * 1.0f) / ae_q_lsb_div;
	quat[3] = (float)(raw_q_xyz[3] * 1.0f) / ae_q_lsb_div;

	velocity[0] = (float)(raw_v_xyz[0] * 1.0f) / ae_v_lsb_div;
	velocity[1] = (float)(raw_v_xyz[1] * 1.0f) / ae_v_lsb_div;
	velocity[2] = (float)(raw_v_xyz[2] * 1.0f) / ae_v_lsb_div;
}

void QMI8658::reset(bool waitResult, uint32_t timeout)
{
  uint8_t val = 0;
  write_reg(QMI8658Register_Reset, 0xB0);
  // Maximum 15ms for the Reset process to be finished
  if (waitResult)
  {
    uint32_t start = millis();
    while (millis() - start < timeout)
    {
      read_regs(0x4D, &val, 1);
      if (val == 0x80)
      {
        //EN.ADDR_AI
        read_regs(QMI8658Register_Ctrl1, &val, 1);
        write_reg(QMI8658Register_Ctrl1, val | 0x40);
        return;
      }
      delay(10);
    }
    ets_printf("Reset chip failed, respone val = %d - 0x%X\n", val, val);
    return;
  }

  //EN.ADDR_AI
  read_regs(QMI8658Register_Ctrl1, &val, 1);
  write_reg(QMI8658Register_Ctrl1, val | 0x40);
}


void QMI8658::setWakeOnMotion(bool bEnable)
{
  reset();
	enum QMI8658_Interrupt interrupt = QMI8658_Int2;
	enum QMI8658_InterruptState initialState = QMI8658State_low;
	enum QMI8658_WakeOnMotionThreshold threshold = QMI8658WomThreshold_high;
	uint8_t blankingTime = 0x00;
	const uint8_t blankingTimeMask = 0x3F;

	enableSensors(QMI8658_CTRL7_DISABLE_ALL);
	config_acc(QMI8658AccRange_16g, QMI8658AccOdr_LowPower_21Hz, QMI8658Lpf_Disable, QMI8658St_Disable);

  if(bEnable)
  {
    uint8_t womCmd = (uint8_t)interrupt | (uint8_t)initialState | (blankingTime & blankingTimeMask);
  	write_reg(QMI8658Register_Cal1_L, threshold);
  	write_reg(QMI8658Register_Cal1_H, womCmd);
  
    uint8_t val = 0;
    write_reg(QMI8658Register_Ctrl9, 0x08);
    while(val != 0x80)
      read_regs(QMI8658Register_StatusInt, &val, 1);
    write_reg(QMI8658Register_Ctrl9, 0x00);
    while(val != 0x00)
      read_regs(QMI8658Register_StatusInt, &val, 1);
  
    read_regs(QMI8658Register_Ctrl1, &val, 1);
    write_reg(QMI8658Register_Ctrl1, val | 0x10);
  }
	enableSensors(QMI8658_CTRL7_ACC_ENABLE);
}

void QMI8658::enableSensors(uint8_t enableFlags)
{
	if (enableFlags & QMI8658_CONFIG_AE_ENABLE)
	{
		enableFlags |= QMI8658_CTRL7_ACC_ENABLE | QMI8658_CTRL7_GYR_ENABLE;
	}

	write_reg(QMI8658Register_Ctrl7, enableFlags & QMI8658_CTRL7_ENABLE_MASK);
}

void QMI8658::Config_apply(struct QMI8658Config const *config)
{
	uint8_t fisSensors = config->inputSelection;

	if (fisSensors & QMI8658_CONFIG_AE_ENABLE)
	{
		config_ae(config->aeOdr);
	}
	else
	{
		if (config->inputSelection & QMI8658_CONFIG_ACC_ENABLE)
		{
			config_acc(config->accRange, config->accOdr, QMI8658Lpf_Enable, QMI8658St_Disable);
		}
		if (config->inputSelection & QMI8658_CONFIG_GYR_ENABLE)
		{
			config_gyro(config->gyrRange, config->gyrOdr, QMI8658Lpf_Enable, QMI8658St_Disable);
		}
	}

	if (config->inputSelection & QMI8658_CONFIG_MAG_ENABLE)
	{
		config_mag(config->magDev, config->magOdr);
	}
	enableSensors(fisSensors);
}
