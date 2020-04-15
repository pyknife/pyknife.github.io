#ifndef __SYS_H
#define __SYS_H	  
#include <stm32f103xe.h>   
#include <definename.h>   

//0,�����OS
//1,���OS
#define SYSTEM_SUPPORT_OS		0		//�w�q�t�Τ�󧨬O�_���OS
																	    
	 
//��a�ާ@,��{51������GPIO����\��
//�����{��Q,�Ѧ�<<CM3�v�«��n>>�Ĥ���(87��~92��).
//IO�f�ާ@���w�q
#define BITBAND(addr, bitnum) ((addr & 0xF0000000)+0x2000000+((addr &0xFFFFF)<<5)+(bitnum<<2)) 
#define MEM_ADDR(addr)  *((volatile unsigned long  *)(addr)) 
#define BIT_ADDR(addr, bitnum)   MEM_ADDR(BITBAND(addr, bitnum)) 
//IO�f�a�}�M�g
#define GPIOA_ODR_Addr    (GPIOA_BASE+12) //0x4001080C 
#define GPIOB_ODR_Addr    (GPIOB_BASE+12) //0x40010C0C 
#define GPIOC_ODR_Addr    (GPIOC_BASE+12) //0x4001100C 
#define GPIOD_ODR_Addr    (GPIOD_BASE+12) //0x4001140C 
#define GPIOE_ODR_Addr    (GPIOE_BASE+12) //0x4001180C 
#define GPIOF_ODR_Addr    (GPIOF_BASE+12) //0x40011A0C    
#define GPIOG_ODR_Addr    (GPIOG_BASE+12) //0x40011E0C    

#define GPIOA_IDR_Addr    (GPIOA_BASE+8) //0x40010808 
#define GPIOB_IDR_Addr    (GPIOB_BASE+8) //0x40010C08 
#define GPIOC_IDR_Addr    (GPIOC_BASE+8) //0x40011008 
#define GPIOD_IDR_Addr    (GPIOD_BASE+8) //0x40011408 
#define GPIOE_IDR_Addr    (GPIOE_BASE+8) //0x40011808 
#define GPIOF_IDR_Addr    (GPIOF_BASE+8) //0x40011A08 
#define GPIOG_IDR_Addr    (GPIOG_BASE+8) //0x40011E08 
 
//IO�f�ާ@,�u���@��IO�f!
//�T�On���Ȥp��16!
#define PAout(n)   BIT_ADDR(GPIOA_ODR_Addr,n)  //��X 
#define PAin(n)    BIT_ADDR(GPIOA_IDR_Addr,n)  //��J 

#define PBout(n)   BIT_ADDR(GPIOB_ODR_Addr,n)  //��X 
#define PBin(n)    BIT_ADDR(GPIOB_IDR_Addr,n)  //��J 

#define PCout(n)   BIT_ADDR(GPIOC_ODR_Addr,n)  //��X 
#define PCin(n)    BIT_ADDR(GPIOC_IDR_Addr,n)  //��J 

#define PDout(n)   BIT_ADDR(GPIOD_ODR_Addr,n)  //��X 
#define PDin(n)    BIT_ADDR(GPIOD_IDR_Addr,n)  //��J 

#define PEout(n)   BIT_ADDR(GPIOE_ODR_Addr,n)  //��X 
#define PEin(n)    BIT_ADDR(GPIOE_IDR_Addr,n)  //��J

#define PFout(n)   BIT_ADDR(GPIOF_ODR_Addr,n)  //��X 
#define PFin(n)    BIT_ADDR(GPIOF_IDR_Addr,n)  //��J

#define PGout(n)   BIT_ADDR(GPIOG_ODR_Addr,n)  //��X 
#define PGin(n)    BIT_ADDR(GPIOG_IDR_Addr,n)  //��J
/////////////////////////////////////////////////////////////////
//Ex_NVIC_Config�M�Ωw�q
#define GPIO_A 0
#define GPIO_B 1
#define GPIO_C 2
#define GPIO_D 3
#define GPIO_E 4
#define GPIO_F 5
#define GPIO_G 6 
#define FTIR   1  //�U���uĲ�o
#define RTIR   2  //�W�ɪuĲ�o
								   

//JTAG�Ҧ��]�m�w�q
#define JTAG_SWD_DISABLE   0X02
#define SWD_ENABLE         0X01
#define JTAG_SWD_ENABLE    0X00	

/////////////////////////////////////////////////////////////////  
void Stm32_Clock_Init(u8 PLL);  //������l��  
void Sys_Soft_Reset(void);      //�t�γn�_��
void Sys_Standby(void);         //�ݾ��Ҧ� 	
void MY_NVIC_SetVectorTable(u32 NVIC_VectTab, u32 Offset);//�]�m�����a�}
void MY_NVIC_PriorityGroupConfig(u8 NVIC_Group);//�]�mNVIC����
void MY_NVIC_Init(u8 NVIC_PreemptionPriority,u8 NVIC_SubPriority,u8 NVIC_Channel,u8 NVIC_Group);//�]�m���_
void Ex_NVIC_Config(u8 GPIOx,u8 BITx,u8 TRIM);//�~�����_�t�m���(�u��GPIOA~G)
void JTAG_Set(u8 mode);
//////////////////////////////////////////////////////////////////////////////
//�H�U���J�s���
void WFI_SET(void);		//����WFI���O
void INTX_DISABLE(void);//�����Ҧ����_
void INTX_ENABLE(void);	//�}�ҩҦ����_
void MSR_MSP(u32 addr);	//�]�m��̦a�}

#endif











