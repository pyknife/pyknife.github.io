
#include "SSD1306.h"


/*
 * SSD1306.c
 * Author: Harris Shallcross
 * Year: 2014-ish
 *
 * Modified for STM32F103 by Vlad Stolyarov, Cavendish Lab, 01-2015
 * 
 * SSD1306 driver library using the STM32F103 minimum development board
 * STM32F1 communicates with OLED display through SPI, pinouts described below.
 *
 * This code is provided AS IS and no warranty is included!
 */

GPIO_InitTypeDef G;
SPI_InitTypeDef S;
#include <misc.h>
#include <stm32f10x_spi.h>


void SPI_SendData8(SPI_TypeDef* SPIx, uint8_t Data)
{
uint32_t spixbase = 0x00;

/* Check the parameters */
assert_param(IS_SPI_ALL_PERIPH(SPIx));

spixbase = (uint32_t)SPIx;
spixbase += 0x0C;

*(__IO uint8_t *) spixbase = Data;
}


void SSD1306_InitSetup(void){
	static uint8_t Init = 1;
	if(Init == 1){
		Init = 0;

		RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
		G.GPIO_Pin = DC | CE | RS;
		G.GPIO_Mode = GPIO_Mode_Out_PP;
//		G.GPIO_OType = GPIO_OType_PP;
//		G.GPIO_PuPd = GPIO_PuPd_UP;
		G.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_Init(IOGPIO, &G);

		G.GPIO_Pin = Clk | DIn;
		G.GPIO_Mode = GPIO_Mode_AF_PP;
		GPIO_Init(IOGPIO, &G);

		/* Screen power is taken directly from supply!
		G.GPIO_Pin = VCC;
		G.GPIO_OType = GPIO_OType_OD;
		G.GPIO_Mode = GPIO_Mode_OUT;
		GPIO_Init(IOGPIO, &G);
		*/

		//GPIO_PinAFConfig(IOGPIO, ClkPS, GPIO_AF_0);
		//GPIO_PinAFConfig(IOGPIO, DInPS, GPIO_AF_0);

		SPI_I2S_DeInit( SPI1 );
		SPI_Cmd(SPI1, DISABLE);
		S.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2;
		S.SPI_CPHA = SPI_CPHA_1Edge;
		S.SPI_CPOL = SPI_CPOL_Low;
		S.SPI_DataSize = SPI_DataSize_8b;
		S.SPI_Direction = SPI_Direction_1Line_Tx;
		S.SPI_FirstBit = SPI_FirstBit_MSB;
		S.SPI_Mode = SPI_Mode_Master;
		S.SPI_NSS = SPI_NSS_Soft;
		S.SPI_CRCPolynomial = 7;

		RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);

		SPI_Init(SPI1, &S);

		SPI_Cmd(SPI1, ENABLE);

	}


	//GPIO_ResetBits(IOGPIO, VCC);
	//Delay(100);
//	GPIO_ResetBits(IOGPIO, Clk|DIn|DC|CE|RS);
	GPIO_ResetBits(IOGPIO, Clk|DIn|DC|CE);
	Delay(1);
	GPIO_SetBits(IOGPIO, RS);
	Delay(1);
	GPIO_ResetBits(IOGPIO, RS);
	Delay(10);
	GPIO_SetBits(IOGPIO, RS|DC|CE);
	Delay(10);

	SB(SetMuxRatio, Reg, 1);
	SB(0x3F, Reg, 1);
	SB(SetDispOffset, Reg, 1);
	SB(0x00, Reg, 1);
	SB(SetDispStartLine|0, Reg, 1);
	SB(SetRemap|0, Reg, 1);
	SB(SetComPinHW, Reg, 1);
	SB(0x12, Reg, 1);
	SB(SetComScanDir|0, Reg, 1);

	SB(Contrast, Reg, 1);
	SB(0xA0, Reg, 1);
	SB(MemAddMode, Reg, 1);
	SB(MModeH, Reg, 1);
	SB(SetColAdd, Reg, 1);
	SB(0x00, Reg, 1);
	SB(0x7F, Reg, 1);
	SB(SetPageAdd, Reg, 1);
	SB(0x00, Reg, 1);
	SB(0x07, Reg, 1);

	SB(SetPreChrg, Reg, 1);
	SB(0xFF, Reg, 1);

	SB(NormDisp, Reg, 1);
	SB(SetComHLvl, Reg, 1);
	SB(0x00, Reg, 1);

	//SB(DispOnAll, Reg, 1); //Test whole display
	SB(DispOnRAM, Reg, 1);
	SB(SetDispFreq, Reg, 1);
	SB(0x00, Reg, 1);
	SB(ChargePump, Reg, 1);
	SB(0x14, Reg, 1);
	SB(DispOn, Reg, 1);

	ClrBuf();
	PScrn();
}

void SB(uint8_t Data, WMode CmdDat, uint8_t En){
	if(CmdDat == Reg) GPIO_WriteBit(IOGPIO, DC, 0);
	else GPIO_WriteBit(IOGPIO, DC, 1);

	if(En) GPIO_ResetBits(IOGPIO, CE);

	SPI_SendData8(SPI1, Data);
	while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY) == SET);

	if(En) GPIO_SetBits(IOGPIO, CE);
}

void LCDScreenMode(LCDScrnMode Mode){
	if(Mode == LCDInv) SB(InvDisp, Reg, 1);
	else SB(NormDisp, Reg, 1);
}

void LCDSleepMode(LCDPwrMode Mode){
	if(Mode == LCDSleep){
		SB(DispOff, Reg, 1);
		SB(ChargePump, Reg, 1);
		SB(0x10, Reg, 1);
	}
	else{
		SB(ChargePump, Reg, 1);
		SB(0x14, Reg, 1);
		SB(DispOn, Reg, 1);
	}
}

extern volatile uint32_t MSec;

void Delay(uint32_t ms){
	uint32_t MSS = MSec;
	uint32_t i = 0;
//	while((MSec-MSS)<ms) asm volatile ("nop");
	while((MSec-MSS)<ms) i++;

}
