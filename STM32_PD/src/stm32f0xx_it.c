/**
  ******************************************************************************
  * @file    stm32f0xx_it.c
  * @author  Ac6
  * @version V1.0
  * @date    02-Feb-2015
  * @brief   Default Interrupt Service Routines.
  ******************************************************************************
*/

/* Includes ------------------------------------------------------------------*/
#include "stm32f0xx_hal.h"
#include "stm32f0xx.h"
#ifdef USE_RTOS_SYSTICK
#include <cmsis_os.h>
#endif
#include "stm32f0xx_it.h"
#include "pd_config.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/******************************************************************************/
/*            	  	    Processor Exceptions Handlers                         */
/******************************************************************************/

/**
  * @brief  This function handles SysTick Handler, but only if no RTOS defines it.
  * @param  None
  * @retval None
  */
void SysTick_Handler(void)
{
	HAL_IncTick();
	HAL_SYSTICK_IRQHandler();
#ifdef USE_RTOS_SYSTICK
	osSystickHandler();
#endif
}

extern void pd_rx_isr_handler(void);
void EXTI0_1_IRQHandler(void) {
#ifndef CONFIG_PD_USE_INTERNAL_COMP
	pd_rx_isr_handler();
#endif
}

void ADC_IRQHandler(void){
#ifdef CONFIG_PD_USE_INTERNAL_COMP
	pd_rx_isr_handler();
#endif
}

extern void pd_tx_isr_handler(void);
void DMA1_CH2_3_IRQHandler(void) {
	pd_tx_isr_handler();
}
