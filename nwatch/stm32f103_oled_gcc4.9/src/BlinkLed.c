//
// This file is part of the GNU ARM Eclipse distribution.
// Copyright (c) 2014 Liviu Ionescu.
//

#include "BlinkLed.h"
#include <stm32f10x_gpio.h>
#define BLINK_ACTIVE_LOW                (1)
// ----------------------------------------------------------------------------

void
blink_led_on(void)
{
#if (BLINK_ACTIVE_LOW)
  GPIO_ResetBits(BLINK_GPIOx(BLINK_PORT_NUMBER),
      BLINK_PIN1);
#else
  GPIO_SetBits(BLINK_GPIOx(BLINK_PORT_NUMBER),
      BLINK_PIN1);
#endif
}

void
blink_led_off(void)
{
#if (BLINK_ACTIVE_LOW)
  GPIO_SetBits(BLINK_GPIOx(BLINK_PORT_NUMBER),
      BLINK_PIN1);
#else
  GPIO_ResetBits(BLINK_GPIOx(BLINK_PORT_NUMBER),
      BLINK_PIN1);
#endif
}



void
blink_led_init()
{
  // Enable GPIO Peripheral clock
  RCC_APB2PeriphClockCmd(BLINK_RCC_MASKx(BLINK_PORT_NUMBER), ENABLE);

  GPIO_InitTypeDef GPIO_InitStructure;

  // Configure pin in output push/pull mode
  GPIO_InitStructure.GPIO_Pin = BLINK_PIN1;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_Init(BLINK_GPIOx(BLINK_PORT_NUMBER), &GPIO_InitStructure);

  // Start with led turned on
  blink_led_off();

}

// ----------------------------------------------------------------------------
