/*
*********************************************************************************************************
*
*	ģ������ : ����������MPU-6050����ģ��
*	�ļ����� : bsp_mpu6050.c
*	��    �� : V1.0
*	˵    �� : ʵ��MPU-6050�Ķ�д������
*
*	�޸ļ�¼ :
*		�汾��  ����        ����     ˵��
*		V1.0    2013-02-01 armfly  ��ʽ����
*
*	Copyright (C), 2013-2014, ���������� www.armfly.com
*
*********************************************************************************************************
*/

/*
	Ӧ��˵��������MPU-6050ǰ�����ȵ���һ�� bsp_InitI2C()�������ú�I2C��ص�GPIO.
*/

#include "mpu6050.h"

MPU6050_T g_tMPU6050;		/* ����һ��ȫ�ֱ���������ʵʱ���� */

/*
*********************************************************************************************************
*	�� �� ��: bsp_InitMPU6050
*	����˵��: ��ʼ��MPU-6050
*	��    ��:  ��
*	�� �� ֵ: 1 ��ʾ������ 0 ��ʾ������
*********************************************************************************************************
*/
void mpu6050_init(void)
{
	mpu6050_writebyte(PWR_MGMT_1, 0x00);	//�������״̬
	mpu6050_writebyte(SMPLRT_DIV, 0x07);
	mpu6050_writebyte(CONFIG, 0x06);
	mpu6050_writebyte(GYRO_CONFIG, 0xE8);
	mpu6050_writebyte(ACCEL_CONFIG, 0x01);
}

/*
*********************************************************************************************************
*	�� �� ��: mpu6050_writebyte
*	����˵��: �� MPU-6050 �Ĵ���д��һ������
*	��    ��: _ucRegAddr : �Ĵ�����ַ
*			  _ucRegData : �Ĵ�������
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void mpu6050_writebyte(uint8_t _regaddr, uint8_t _regdata)
{
    gpio_i2c_start();							/* ���߿�ʼ�ź� */

    gpio_i2c_sendbyte(MPU6050_SLAVE_ADDRESS);	/* �����豸��ַ+д�ź� */
	gpio_i2c_waitack();

    gpio_i2c_sendbyte(_regaddr);				/* �ڲ��Ĵ�����ַ */
	gpio_i2c_waitack();

    gpio_i2c_sendbyte(_regdata);				/* �ڲ��Ĵ������� */
	gpio_i2c_waitack();

    gpio_i2c_stop();                   			/* ����ֹͣ�ź� */
}

/*
*********************************************************************************************************
*	�� �� ��: MPU6050_ReadByte
*	����˵��: ��ȡ MPU-6050 �Ĵ���������
*	��    ��: _ucRegAddr : �Ĵ�����ַ
*	�� �� ֵ: ��
*********************************************************************************************************
*/
uint8_t mpu6050_readbyte(uint8_t _regaddr)
{
	uint8_t ucData;

	gpio_i2c_start();                  			/* ���߿�ʼ�ź� */
	gpio_i2c_sendbyte(MPU6050_SLAVE_ADDRESS);	/* �����豸��ַ+д�ź� */
	gpio_i2c_waitack();
	gpio_i2c_sendbyte(_regaddr);     			/* ���ʹ洢��Ԫ��ַ */
	gpio_i2c_waitack();

	gpio_i2c_start();                  			/* ���߿�ʼ�ź� */

	gpio_i2c_sendbyte(MPU6050_SLAVE_ADDRESS+1); 	/* �����豸��ַ+���ź� */
	gpio_i2c_waitack();

	ucData = gpio_i2c_readbyte();       			/* �����Ĵ������� */
	gpio_i2c_nack();
	gpio_i2c_stop();                  			/* ����ֹͣ�ź� */
	return ucData;
}


/*
*********************************************************************************************************
*	�� �� ��: MPU6050_ReadData
*	����˵��: ��ȡ MPU-6050 ���ݼĴ����� ���������ȫ�ֱ��� g_tMPU6050.  ��������Զ�ʱ���øó���ˢ������
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void mpu6050_readdata(void)
{
	uint8_t ucReadBuf[14];
	uint8_t i;

#if 1 /* ������ */
	gpio_i2c_start();                  			/* ���߿�ʼ�ź� */
	gpio_i2c_sendbyte(MPU6050_SLAVE_ADDRESS);	/* �����豸��ַ+д�ź� */
	gpio_i2c_waitack();
	gpio_i2c_sendbyte(ACCEL_XOUT_H);     		/* ���ʹ洢��Ԫ��ַ  */
	gpio_i2c_waitack();

	gpio_i2c_start();                  			/* ���߿�ʼ�ź� */

	gpio_i2c_sendbyte(MPU6050_SLAVE_ADDRESS + 1); /* �����豸��ַ+���ź� */
	gpio_i2c_waitack();

	for (i = 0; i < 13; i++)
	{
		ucReadBuf[i] = gpio_i2c_readbyte();       			/* �����Ĵ������� */
		gpio_i2c_ack();
	}

	/* �����һ���ֽڣ�ʱ�� NAck */
	ucReadBuf[13] = gpio_i2c_readbyte();
	gpio_i2c_nack();

	gpio_i2c_stop();                  			/* ����ֹͣ�ź� */

#else	/* ���ֽڶ� */
	for (i = 0 ; i < 14; i++)
	{
		ucReadBuf[i] = MPU6050_ReadByte(ACCEL_XOUT_H + i);
	}
#endif

	/* �����������ݱ��浽ȫ�ֽṹ����� */
	g_tMPU6050.Accel_X = (ucReadBuf[0] << 8) + ucReadBuf[1];
	g_tMPU6050.Accel_Y = (ucReadBuf[2] << 8) + ucReadBuf[3];
	g_tMPU6050.Accel_Z = (ucReadBuf[4] << 8) + ucReadBuf[5];

	g_tMPU6050.Temp = (int16_t)((ucReadBuf[6] << 8) + ucReadBuf[7]);

	g_tMPU6050.GYRO_X = (ucReadBuf[8] << 8) + ucReadBuf[9];
	g_tMPU6050.GYRO_Y = (ucReadBuf[10] << 8) + ucReadBuf[11];
	g_tMPU6050.GYRO_Z = (ucReadBuf[12] << 8) + ucReadBuf[13];
}

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
