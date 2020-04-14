#ifndef OLED_CONFIG_H
#define OLED_CONFIG_H
//����ͼ�ο�
/*****************************************************************/
//ѡ����Ļ����
//1.HW_IIC    Ӳ��IIC 
//2.SW_IIC    ���IIC
//3.HW_SPI		Ӳ��SPI
//4.SW_SPI		���IIC
//5.HW_8080   Ӳ��8080
//����ֻ֧��Ӳ��iic
/*****************************************************************/
#define  TRANSFER_METHOD   (HW_IIC)

/*****************************************************************/
//ѡ���ֿ���Դ
//0.FONT_FROM_NULL		 ��ʹ���ֿ�
//1.FONT_FROM_SCREEN	 ��Ļ�Դ�
//2.FONT_FROM_EXTL		 �����ⲿ
//3.FONT_FROM_FLASH		 ��flash��ȡ
//4.FONT_FROM_EEMPROM  ��eeprom��ȡ
//5.FONT_FROM_SOFT 		 ���ȡģ�������ռ�úܶ��ڴ�
//����ֻ�����ȡģ�ֿ⣬
/*****************************************************************/
#define FONT_SOURCE   (FONT_FROM_SOFT)

/*****************************************************************/
//ѡ����Ļ����
//1.OLED_12864
//2.OLED_12832  
//3.CUSTOMIZE  �Զ���
//���ڽ�֧��OLED_12864
/*****************************************************************/
#define  SCREEN_TYPE   (OLED_12864)



#if (SCREEN_TYPE==OLED_12864)
	#define   OLED_12864
	#define 	SCREEN_PAGE_NUM			   	(8)
	#define   SCREEN_PAGEDATA_NUM   	(128)
	#define 	SCREEN_COLUMN 					(128)
	#define  	SCREEN_ROW 							(64)
	#if 0
	#define 	SCREEN_PHY_X             (21.744)
	#define 	SCREEN_PHY_Y						 (10.864) 
	#endif

#elif (SCREEN_TYPE==OLED_12832)
	#define 	SCREEN_PAGE_NUM			   	(4)
	#define   SCREEN_PAGEDATA_NUM   	(128)
	#define 	SCREEN_COLUMN 					(128)
	#define  	SCREEN_ROW 							(32)
	#if 0
	#define 	SCREEN_PHY_X             ()
	#define 	SCREEN_PHY_Y						 ()
	#endif

#elif (SCREEN_TYPE==CUSTOMIZE)
	/*��������ֱ���128*64,���:128�У��߶�:64��*/
	#define 	SCREEN_COLUMN 					(128)
	#define  	SCREEN_ROW 							(64)
	/*����һ֡������д����ҳ*/
	#define 	SCREEN_PAGE_NUM			   	(8)
	/*����һҳ��д��������*/
	#define   SCREEN_PAGEDATA_NUM   	(128)
	#if 0
	/*��������ߴ�*/
	#define 	SCREEN_PHY_X             ()
	#define 	SCREEN_PHY_Y						 ()
	#endif
#endif

//������δ���ƣ�����ʹ�õ�IIC1
#if (TRANSFER_METHOD==HW_IIC)
//IIC1: PB6 -- SCL; PB7 -- SDA */
	#define USE_HW_IIC		IIC1
#elif (TRANSFER_METHOD==SW_IIC)
	
#endif

#endif


