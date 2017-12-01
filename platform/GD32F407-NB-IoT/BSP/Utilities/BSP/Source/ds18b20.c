#include "ds18b20.h"

void delay_us(uint32_t micro_sec)
{
    uint32_t tick_want;
    uint32_t tick_old;
    uint32_t tick_now;
    uint32_t cnt = 0;
    uint32_t reload;
       
    reload = SysTick->LOAD;                
    tick_want = micro_sec * (SystemCoreClock / 1000000);
    
    tick_old = SysTick->VAL;             /* �ս���ʱ�ļ�����ֵ */

    while (1)
    {
        tick_now = SysTick->VAL;    
        if (tick_now != tick_old)
        {    
            /* SYSTICK��һ���ݼ��ļ����� */    
            if (tick_now < tick_old)
            {
                cnt += tick_old - tick_now;    
            }
            /* ����װ�صݼ� */
            else
            {
                cnt += reload - tick_now + tick_old;    
            }        
            tick_old = tick_now;

            /* ʱ�䳬��/����Ҫ�ӳٵ�ʱ��,���˳� */
            if (cnt >= tick_want)
            {
                break;
            }
        }  
    }
}

void ds18b20_init(void)
{
    rcu_periph_clock_enable(DQ_GPIO_CLK);

    gpio_mode_set(DQ_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, DQ_GPIO_PIN);
    gpio_output_options_set(DQ_GPIO_PORT, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, DQ_GPIO_PIN);
    
    DQ_PIN_SET;
}

/*
*********************************************************************************************************
*	�� �� ��: DS18B20_Reset
*	����˵��: ��λDS18B20�� ����DQΪ�ͣ���������480us��Ȼ��ȴ�
*	��    ��: ��
*	�� �� ֵ: 0 ʧ�ܣ� 1 ��ʾ�ɹ�
*********************************************************************************************************
*/
uint8_t DS18B20_Reset(void)
{
    
    uint8_t retry = 0;
    uint16_t k;

    /* ��λ�����ʧ���򷵻�0 */
    for (retry = 0; retry < 3; retry++){
        DQ_PIN_RESET;
        delay_us(520);  /* �ӳ� 520uS�� Ҫ������ӳٴ��� 480us */
        DQ_PIN_SET;				/* �ͷ�DQ */

        delay_us(15);	/* �ȴ�15us */

        /* ���DQ��ƽ�Ƿ�Ϊ�� */
        for (k = 0; k < 10; k++)
        {
            if (IS_DQ_LOW)
			{
				break;
			}
			delay_us(10);	/* �ȴ�65us */
		}
		if (k >= 10)
		{
			continue;		/* ʧ�� */
		}

		/* �ȴ�DS18B20�ͷ�DQ */
		for (k = 0; k < 30; k++)
		{
			if (!IS_DQ_LOW)
			{
				break;
			}
			delay_us(10);	/* �ȴ�65us */
		}
		if (k >= 30)
		{
			continue;		/* ʧ�� */
		}

		break;
	}

	delay_us(5);

	if (retry >= 1)
	{
		return 0;
	}

	return 1;
}

/*
*********************************************************************************************************
*	�� �� ��: DS18B20_WriteByte
*	����˵��: ��DS18B20д��1�ֽ�����
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void DS18B20_WriteByte(uint8_t _val)
{
	/*
		д����ʱ��, ��DS18B20 page 16
	*/
	uint8_t i;

	for (i = 0; i < 8; i++)
	{
		DQ_PIN_RESET;
		delay_us(2);

		if (_val & 0x01)
		{
			DQ_PIN_SET;
		}
		else
		{
			DQ_PIN_RESET;
		}
		delay_us(60);
		DQ_PIN_SET;
		delay_us(2);
		_val >>= 1;
	}
}

/*
*********************************************************************************************************
*	�� �� ��: DS18B20_ReadByte
*	����˵��: ��DS18B20��ȡ1�ֽ�����
*	��    ��: ��
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static uint8_t DS18B20_ReadByte(void)
{
	/*
		д����ʱ��, ��DS18B20 page 16
	*/
	uint8_t i;
	uint8_t read = 0;

	for (i = 0; i < 8; i++)
	{
		read >>= 1;

		DQ_PIN_RESET;
		delay_us(3);
		DQ_PIN_SET;
		delay_us(3);

		if (IS_DQ_LOW)
		{
			;
		}
		else
		{
			read |= 0x80;
		}
		delay_us(60);
	}

	return read;
}

/*
*********************************************************************************************************
*	�� �� ��: DS18B20_ReadTempReg
*	����˵��: ���¶ȼĴ�����ֵ��ԭʼ���ݣ�
*	��    ��: ��
*	�� �� ֵ: �¶ȼĴ������� ������16�õ� 1���϶ȵ�λ, Ҳ����С����ǰ�������)
*********************************************************************************************************
*/
int16_t DS18B20_ReadTempReg(void)
{
	uint8_t temp1, temp2;

	/* ���߸�λ */
	if (DS18B20_Reset() == 0)
	{
		return 0;
	}		

	DS18B20_WriteByte(0xcc);	/* ������ */
	DS18B20_WriteByte(0x44);	/* ��ת������ */

	DS18B20_Reset();		/* ���߸�λ */

	DS18B20_WriteByte(0xcc);	/* ������ */
	DS18B20_WriteByte(0xbe);

	temp1 = DS18B20_ReadByte();	/* ���¶�ֵ���ֽ� */
	temp2 = DS18B20_ReadByte();	/* ���¶�ֵ���ֽ� */

	return ((temp2 << 8) | temp1);	/* ����16λ�Ĵ���ֵ */
}

/*
*********************************************************************************************************
*	�� �� ��: DS18B20_ReadID
*	����˵��: ��DS18B20��ROM ID�� �����ϱ���ֻ��1��оƬ
*	��    ��: _id �洢ID
*	�� �� ֵ: 0 ��ʾʧ�ܣ� 1��ʾ��⵽��ȷID
*********************************************************************************************************
*/
uint8_t DS18B20_ReadID(uint8_t *_id)
{
	uint8_t i;

	/* ���߸�λ */
	if (DS18B20_Reset() == 0)
	{
		return 0;
	}

	DS18B20_WriteByte(0x33);	/* ������ */
	for (i = 0; i < 8; i++)
	{
		_id[i] = DS18B20_ReadByte();
	}

	DS18B20_Reset();		/* ���߸�λ */
	
	return 1;
}

/*
*********************************************************************************************************
*	�� �� ��: DS18B20_ReadTempByID
*	����˵��: ��ָ��ID���¶ȼĴ�����ֵ��ԭʼ���ݣ�
*	��    ��: ��
*	�� �� ֵ: �¶ȼĴ������� ������16�õ� 1���϶ȵ�λ, Ҳ����С����ǰ�������)
*********************************************************************************************************
*/
int16_t DS18B20_ReadTempByID(uint8_t *_id)
{
	uint8_t temp1, temp2;
	uint8_t i;

	DS18B20_Reset();		/* ���߸�λ */

	DS18B20_WriteByte(0x55);	/* ������ */
	for (i = 0; i < 8; i++)
	{
		DS18B20_WriteByte(_id[i]);
	}
	DS18B20_WriteByte(0x44);	/* ��ת������ */

	DS18B20_Reset();		/* ���߸�λ */

	DS18B20_WriteByte(0x55);	/* ������ */
	for (i = 0; i < 8; i++)
	{
		DS18B20_WriteByte(_id[i]);
	}	
	DS18B20_WriteByte(0xbe);

	temp1 = DS18B20_ReadByte();	/* ���¶�ֵ���ֽ� */
	temp2 = DS18B20_ReadByte();	/* ���¶�ֵ���ֽ� */

	return ((temp2 << 8) | temp1);	/* ����16λ�Ĵ���ֵ */
}

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
