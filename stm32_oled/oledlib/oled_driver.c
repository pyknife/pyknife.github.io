
#include "oled_driver.h"


//static void delay_ms(u16 time);

 /**
  * @brief  I2C_Configuration����ʼ��Ӳ��IIC����
  * @param  ��
  * @retval ��
  */
void I2C_Configuration(void)
{
	I2C_InitTypeDef  I2C_InitStructure;
	GPIO_InitTypeDef  GPIO_InitStructure; 

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2CX,ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOX,ENABLE);

	
	GPIO_InitStructure.GPIO_Pin =  GPIO_SCL_Pin_X | GPIO_SDA_Pin_X;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;//I2C���뿪©���
	GPIO_Init(GPIOX, &GPIO_InitStructure);

	I2C_DeInit(I2CX);
	I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
	I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_2;
	I2C_InitStructure.I2C_OwnAddress1 = 0x30;//������I2C��ַ,���д
	I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
	I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
	I2C_InitStructure.I2C_ClockSpeed = 1000000;//

	I2C_Cmd(I2CX, ENABLE);
	I2C_Init(I2CX, &I2C_InitStructure);
}
extern void DelayMs(unsigned int nms);
void OLED_Init(void)
{
	DelayMs(200); //�������ʱ����Ҫ
	WriteCmd(0xAE); //display off
	WriteCmd(0x20);	//Set Memory Addressing Mode	
	WriteCmd(0x10);	//00,Horizontal Addressing Mode;01,Vertical Addressing Mode;10,Page Addressing Mode (RESET);11,Invalid
	WriteCmd(0xb0);	//Set Page Start Address for Page Addressing Mode,0-7
	WriteCmd(0xc8);	//Set COM Output Scan Direction
	WriteCmd(0x00); //---set low column address
	WriteCmd(0x10); //---set high column address
	WriteCmd(0x40); //--set start line address
	WriteCmd(0x81); //--set contrast control register
	WriteCmd(0xff); //���ȵ��� 0x00~0xff
	WriteCmd(0xa1); //--set segment re-map 0 to 127
	WriteCmd(0xa6); //--set normal display
	WriteCmd(0xa8); //--set multiplex ratio(1 to 64)
	WriteCmd(0x3F); //
	WriteCmd(0xa4); //0xa4,Output follows RAM content;0xa5,Output ignores RAM content
	WriteCmd(0xd3); //-set display offset
	WriteCmd(0x00); //-not offset
	WriteCmd(0xd5); //--set display clock divide ratio/oscillator frequency
	WriteCmd(0xf0); //--set divide ratio
	WriteCmd(0xd9); //--set pre-charge period
	WriteCmd(0x22); //
	WriteCmd(0xda); //--set com pins hardware configuration
	WriteCmd(0x12);
	WriteCmd(0xdb); //--set vcomh
	WriteCmd(0x20); //0x20,0.77xVcc
	WriteCmd(0x8d); //--set DC-DC enable
	WriteCmd(0x14); //
	WriteCmd(0xaf); //--turn on oled panel
	OLED_CLS();
}


//static void delay_ms(u16 time)  
//{    
//   u16 i=0;  
//   while(time--)
//   {
//      i=4050;  //�Լ�����
//      while(i--) ;    
//   }
//}

 /**
  * @brief  I2C_WriteByte����OLED�Ĵ�����ַдһ��byte������
  * @param  addr���Ĵ�����ַ
	*					data��Ҫд�������
  * @retval ��
  */
void I2C_WriteByte(uint8_t addr,uint8_t data)
{
  while(I2C_GetFlagStatus(I2CX, I2C_FLAG_BUSY));
	
	I2C_GenerateSTART(I2CX, ENABLE);//����I2C1
	while(!I2C_CheckEvent(I2CX, I2C_EVENT_MASTER_MODE_SELECT));/*EV5,��ģʽ*/

	I2C_Send7bitAddress(I2CX, OLED_ADDRESS, I2C_Direction_Transmitter);//������ַ -- Ĭ��0x78
	while(!I2C_CheckEvent(I2CX, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));

	I2C_SendData(I2CX, addr);//�Ĵ�����ַ
	while (!I2C_CheckEvent(I2CX, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

	I2C_SendData(I2CX, data);//��������
	while (!I2C_CheckEvent(I2CX, I2C_EVENT_MASTER_BYTE_TRANSMITTED));
	
	I2C_GenerateSTOP(I2CX, ENABLE);//�ر�I2C1����
}


void WriteCmd(unsigned char I2C_Command)//д����
{
	I2C_WriteByte(0x00, I2C_Command);
}

void WriteDat(unsigned char I2C_Data)//д����
{
	I2C_WriteByte(0x40, I2C_Data);
}


void OLED_CLS(void)//����
{
	unsigned char m,n;
	for(m=0;m<8;m++)
	{
		WriteCmd(0xb0+m);		//page0-page1
		WriteCmd(0x00);		//low column start address
		WriteCmd(0x10);		//high column start address
		for(n=0;n<128;n++)
			{
				WriteDat(0x00);
			}
	}
}


void OLED_ON(void)
{
	WriteCmd(0X8D);  //���õ�ɱ�
	WriteCmd(0X14);  //������ɱ�
	WriteCmd(0XAF);  //OLED����
}



void OLED_OFF(void)
{
	WriteCmd(0X8D);  //���õ�ɱ�
	WriteCmd(0X10);  //�رյ�ɱ�
	WriteCmd(0XAE);  //OLED����
}


void OLED_FILL(unsigned char BMP[])
{
  unsigned char m,n;
	unsigned char *p;
	p=BMP;
	for(m=0;m<8;m++)
	{
		WriteCmd(0xb0+m);		//page0-page1
		WriteCmd(0x00);		//low column start address
		WriteCmd(0x10);		//high column start address
		while(I2C_GetFlagStatus(I2CX, I2C_FLAG_BUSY));	
		I2C_GenerateSTART(I2C1, ENABLE);//����I2C1
		while(!I2C_CheckEvent(I2CX, I2C_EVENT_MASTER_MODE_SELECT));/*EV5,��ģʽ*/
		I2C_Send7bitAddress(I2CX, OLED_ADDRESS, I2C_Direction_Transmitter);//������ַ -- Ĭ��0x78
		while(!I2C_CheckEvent(I2CX, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));
		I2C_SendData(I2CX, 0x40);//�Ĵ�����ַ
		while (!I2C_CheckEvent(I2CX, I2C_EVENT_MASTER_BYTE_TRANSMITTED));	
		for(n=0;n<128;n++)
		{
			I2C_SendData(I2CX, *p++);//��������
			while (!I2C_CheckEvent(I2CX, I2C_EVENT_MASTER_BYTE_TRANSMITTED));
		}	
		I2C_GenerateSTOP(I2CX, ENABLE);//�ر�I2C1����
	}
}
