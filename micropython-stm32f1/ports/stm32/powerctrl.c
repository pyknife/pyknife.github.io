/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013-2018 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "py/mperrno.h"
#include "py/mphal.h"
#include "powerctrl.h"
#include "rtc.h"
#include "genhdr/pllfreqtable.h"

/*--------------------------------*/
/*----Add to support stm32f1------*/
/*--------------------------------*/
#if !defined(STM32F0) && !defined(STM32F1)

// Assumes that PLL is used as the SYSCLK source
int powerctrl_rcc_clock_config_pll(RCC_ClkInitTypeDef *rcc_init, uint32_t sysclk_mhz, bool need_pllsai) {
    uint32_t flash_latency;

    #if defined(STM32F7)

    if (need_pllsai) {
        // Configure PLLSAI at 48MHz for those peripherals that need this freq
        const uint32_t pllsain = 192;
        const uint32_t pllsaip = 4;
        const uint32_t pllsaiq = 2;
        RCC->PLLSAICFGR = pllsaiq << RCC_PLLSAICFGR_PLLSAIQ_Pos
            | (pllsaip / 2 - 1) << RCC_PLLSAICFGR_PLLSAIP_Pos
            | pllsain << RCC_PLLSAICFGR_PLLSAIN_Pos;
        RCC->CR |= RCC_CR_PLLSAION;
        uint32_t ticks = mp_hal_ticks_ms();
        while (!(RCC->CR & RCC_CR_PLLSAIRDY)) {
            if (mp_hal_ticks_ms() - ticks > 200) {
                return -MP_ETIMEDOUT;
            }
        }
        RCC->DCKCFGR2 |= RCC_DCKCFGR2_CK48MSEL;
    }

    // If possible, scale down the internal voltage regulator to save power
    uint32_t volt_scale;
    if (sysclk_mhz <= 151) {
        volt_scale = PWR_REGULATOR_VOLTAGE_SCALE3;
    } else if (sysclk_mhz <= 180) {
        volt_scale = PWR_REGULATOR_VOLTAGE_SCALE2;
    } else {
        volt_scale = PWR_REGULATOR_VOLTAGE_SCALE1;
    }
    if (HAL_PWREx_ControlVoltageScaling(volt_scale) != HAL_OK) {
        return -MP_EIO;
    }

    // These flash_latency values assume a supply voltage between 2.7V and 3.6V
    if (sysclk_mhz <= 30) {
        flash_latency = FLASH_LATENCY_0;
    } else if (sysclk_mhz <= 60) {
        flash_latency = FLASH_LATENCY_1;
    } else if (sysclk_mhz <= 90) {
        flash_latency = FLASH_LATENCY_2;
    } else if (sysclk_mhz <= 120) {
        flash_latency = FLASH_LATENCY_3;
    } else if (sysclk_mhz <= 150) {
        flash_latency = FLASH_LATENCY_4;
    } else if (sysclk_mhz <= 180) {
        flash_latency = FLASH_LATENCY_5;
    } else if (sysclk_mhz <= 210) {
        flash_latency = FLASH_LATENCY_6;
    } else {
        flash_latency = FLASH_LATENCY_7;
    }

    #elif defined(MICROPY_HW_FLASH_LATENCY)
    flash_latency = MICROPY_HW_FLASH_LATENCY;
    #else
    flash_latency = FLASH_LATENCY_5;
    #endif

    rcc_init->SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    if (HAL_RCC_ClockConfig(rcc_init, flash_latency) != HAL_OK) {
        return -MP_EIO;
    }

    return 0;
}

#endif

#if !(defined(STM32F0) || defined(STM32L4) )

/*--------------------------------*/
/*----Add to support stm32f1------*/
/*--------------------------------*/
#if !defined(STM32F1)
STATIC uint32_t calc_ahb_div(uint32_t wanted_div) {
    if (wanted_div <= 1) { return RCC_SYSCLK_DIV1; }
    else if (wanted_div <= 2) { return RCC_SYSCLK_DIV2; }
    else if (wanted_div <= 4) { return RCC_SYSCLK_DIV4; }
    else if (wanted_div <= 8) { return RCC_SYSCLK_DIV8; }
    else if (wanted_div <= 16) { return RCC_SYSCLK_DIV16; }
    else if (wanted_div <= 64) { return RCC_SYSCLK_DIV64; }
    else if (wanted_div <= 128) { return RCC_SYSCLK_DIV128; }
    else if (wanted_div <= 256) { return RCC_SYSCLK_DIV256; }
    else { return RCC_SYSCLK_DIV512; }
}

STATIC uint32_t calc_apb_div(uint32_t wanted_div) {
    if (wanted_div <= 1) { return RCC_HCLK_DIV1; }
    else if (wanted_div <= 2) { return RCC_HCLK_DIV2; }
    else if (wanted_div <= 4) { return RCC_HCLK_DIV4; }
    else if (wanted_div <= 8) { return RCC_HCLK_DIV8; }
    else { return RCC_SYSCLK_DIV16; }
}
#endif



int powerctrl_set_sysclk(uint32_t sysclk, uint32_t ahb, uint32_t apb1, uint32_t apb2) {
    // Return straightaway if the clocks are already at the desired frequency
    if (sysclk == HAL_RCC_GetSysClockFreq()
        && ahb == HAL_RCC_GetHCLKFreq()
        && apb1 == HAL_RCC_GetPCLK1Freq()
        && apb2 == HAL_RCC_GetPCLK2Freq()) {
        return 0;
    }
	#if defined(STM32F1)
	/*--------------------------------*/
	/*----Add to support stm32f1------*/
	/*--------------------------------*/
	// RCC_PLL_MUL2 ~ RCC_PLL_MUL15
	HAL_RCC_DeInit();
	mp_hal_delay_ms(5);
	uint32_t PLL;
	uint32_t ahb_div;
	uint32_t apb1_div;
	uint32_t apb2_div;
	uint32_t flash_latency;
	uint32_t hse_prediv;
	uint32_t HSI_CLOCK; 
	//return -MP_EIO;      //無法變更頻率
	//return -MP_EINVAL;   //無效的頻率
	if ( sysclk >= 72000000) sysclk = 72000000;
	if ( ahb    >= 72000000) ahb    = 72000000;
	if ( apb1   >= 36000000) apb1   = 36000000;
	if ( apb2   >= 72000000) apb2   = 72000000;
	
	
	if( sysclk > 60000000)
	{
		hse_prediv = RCC_HSE_PREDIV_DIV1;
		HSI_CLOCK  = HSI_VALUE ; 
	}
	else
	{
		hse_prediv = RCC_HSE_PREDIV_DIV2;
		HSI_CLOCK  = HSI_VALUE / 2; 
	}
	//use RCC_HSE_PREDIV_DIV2
	int pllvalue = sysclk / HSI_CLOCK ;  
	if (	sysclk % HSI_CLOCK  != 0 || \
			sysclk % ahb        != 0 || \
			ahb    % apb1       != 0 || \
			ahb    % apb2       != 0    \
		)
	{
		return -MP_EINVAL;
	}
	switch(pllvalue)
	{
		case 2 : PLL = RCC_PLL_MUL2 ; break;
		case 3 : PLL = RCC_PLL_MUL3 ; break;
		case 4 : PLL = RCC_PLL_MUL4 ; break;
		case 5 : PLL = RCC_PLL_MUL5 ; break;
		case 6 : PLL = RCC_PLL_MUL6 ; break;
		case 7 : PLL = RCC_PLL_MUL7 ; break;
		case 8 : PLL = RCC_PLL_MUL8 ; break;
		case 9 : PLL = RCC_PLL_MUL9 ; break;
		case 10: PLL = RCC_PLL_MUL10; break;
		case 11: PLL = RCC_PLL_MUL11; break;
		case 12: PLL = RCC_PLL_MUL12; break;
		case 13: PLL = RCC_PLL_MUL13; break;
		case 14: PLL = RCC_PLL_MUL14; break;
		case 15: PLL = RCC_PLL_MUL15; break;
		default: return -MP_EINVAL;   break;
	}
	//printf("%ld %ld %ld %ld\n",sysclk,ahb,apb1,apb2);
	//printf("%ld\n",HSI_CLOCK);
	ahb_div   = sysclk / ahb;
	apb1_div  = ahb    / apb1;
	apb2_div  = ahb    / apb2;
	//printf("%ld %ld %ld %ld\n",sysclk,ahb,apb1,apb2);
	//printf("%d %ld %ld %ld \n",pllvalue,ahb_div,apb1_div,apb2_div);
	
	switch(ahb_div)
	{
		case 1   : ahb_div = RCC_SYSCLK_DIV1  ; break;
		case 2   : ahb_div = RCC_SYSCLK_DIV2  ; break;
		case 4   : ahb_div = RCC_SYSCLK_DIV4  ; break;
		case 8   : ahb_div = RCC_SYSCLK_DIV8  ; break;
		case 16  : ahb_div = RCC_SYSCLK_DIV16 ; break;
		case 64  : ahb_div = RCC_SYSCLK_DIV64 ; break;
		case 128 : ahb_div = RCC_SYSCLK_DIV128; break;
		case 256 : ahb_div = RCC_SYSCLK_DIV256; break;
		case 512 : ahb_div = RCC_SYSCLK_DIV512; break;
		default:   return -MP_EINVAL;           break;
	}
	
	switch(apb1_div)
	{
		case 1   : apb1_div = RCC_HCLK_DIV1  ; break;
		case 2   : apb1_div = RCC_HCLK_DIV2  ; break;
		case 4   : apb1_div = RCC_HCLK_DIV4  ; break;
		case 8   : apb1_div = RCC_HCLK_DIV8  ; break;
		case 16  : apb1_div = RCC_HCLK_DIV16 ; break;
		default:   return -MP_EINVAL;          break;
	}
	
	switch(apb2_div)
	{
		case 1   : apb2_div = RCC_HCLK_DIV1  ; break;
		case 2   : apb2_div = RCC_HCLK_DIV2  ; break;
		case 4   : apb2_div = RCC_HCLK_DIV4  ; break;
		case 8   : apb2_div = RCC_HCLK_DIV8  ; break;
		case 16  : apb2_div = RCC_HCLK_DIV16 ; break;
		default:   return -MP_EINVAL;          break;
	}
	
	HAL_StatusTypeDef ret = HAL_OK;
	RCC_OscInitTypeDef RCC_OscInitStructure; 
	RCC_ClkInitTypeDef RCC_ClkInitStructure;

	RCC_OscInitStructure.OscillatorType	=RCC_OSCILLATORTYPE_HSE;    
	RCC_OscInitStructure.HSEState		=RCC_HSE_ON;                      
	RCC_OscInitStructure.HSEPredivValue	=hse_prediv;		
	RCC_OscInitStructure.PLL.PLLState	=RCC_PLL_ON;					
	RCC_OscInitStructure.PLL.PLLSource	=RCC_PLLSOURCE_HSE;			
	RCC_OscInitStructure.PLL.PLLMUL		=PLL; 							
	ret=HAL_RCC_OscConfig(&RCC_OscInitStructure);
	mp_hal_delay_ms(5);
	
	if(ret!=HAL_OK) return -MP_EIO;

	RCC_ClkInitStructure.ClockType		=(RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2);
	RCC_ClkInitStructure.SYSCLKSource	=RCC_SYSCLKSOURCE_PLLCLK;		
	RCC_ClkInitStructure.AHBCLKDivider	=ahb_div;				
	RCC_ClkInitStructure.APB1CLKDivider	=apb1_div; 				
	RCC_ClkInitStructure.APB2CLKDivider	=apb2_div; 	
	/*	
	000 Zero wait state, if 0 < SYSCLK≤ 24 MHz
	001 One wait state, if 24 MHz < SYSCLK ≤ 48 MHz
	010 Two wait states, if 48 MHz < SYSCLK ≤ 72 MHz
	*/
	if( sysclk <= 24000000)
		flash_latency = FLASH_LATENCY_0;
	else if( sysclk <= 48000000)
		flash_latency = FLASH_LATENCY_1;
	else
		flash_latency = FLASH_LATENCY_2;
	
	ret=HAL_RCC_ClockConfig(&RCC_ClkInitStructure,flash_latency);	
	mp_hal_delay_ms(5);
	if(ret!=HAL_OK) return -MP_EIO;
	
	#else
		
		// Default PLL parameters that give 48MHz on PLL48CK
		uint32_t m = MICROPY_HW_CLK_VALUE / 1000000, n = 336, p = 2, q = 7;
		uint32_t sysclk_source;
		bool need_pllsai = false;

		// Search for a valid PLL configuration that keeps USB at 48MHz
		uint32_t sysclk_mhz = sysclk / 1000000;
		for (const uint16_t *pll = &pll_freq_table[MP_ARRAY_SIZE(pll_freq_table) - 1]; pll >= &pll_freq_table[0]; --pll) {
			uint32_t sys = *pll & 0xff;
			if (sys <= sysclk_mhz) {
				m = (*pll >> 10) & 0x3f;
				p = ((*pll >> 7) & 0x6) + 2;
				if (m == 0) {
					// special entry for using HSI directly
					sysclk_source = RCC_SYSCLKSOURCE_HSI;
				} else if (m == 1) {
					// special entry for using HSE directly
					sysclk_source = RCC_SYSCLKSOURCE_HSE;
				} else {
					// use PLL
					sysclk_source = RCC_SYSCLKSOURCE_PLLCLK;
					uint32_t vco_out = sys * p;
					n = vco_out * m / (MICROPY_HW_CLK_VALUE / 1000000);
					q = vco_out / 48;
					#if defined(STM32F7)
					need_pllsai = vco_out % 48 != 0;
					#endif
				}
				goto set_clk;
			}
		}
		return -MP_EINVAL;

	set_clk:
		// Let the USB CDC have a chance to process before we change the clock
		mp_hal_delay_ms(5);

		// Desired system clock source is in sysclk_source
		RCC_ClkInitTypeDef RCC_ClkInitStruct;
		RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
		if (sysclk_source == RCC_SYSCLKSOURCE_PLLCLK) {
			// Set HSE as system clock source to allow modification of the PLL configuration
			// We then change to PLL after re-configuring PLL
			#if MICROPY_HW_CLK_USE_HSI
			RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
			#else
			RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSE;
			#endif
		} else {
			// Directly set the system clock source as desired
			RCC_ClkInitStruct.SYSCLKSource = sysclk_source;
		}

		// Determine the bus clock dividers
		// Note: AHB freq required to be >= 14.2MHz for USB operation
		RCC_ClkInitStruct.AHBCLKDivider = calc_ahb_div(sysclk / ahb);
		#if !defined(STM32H7)
		ahb = sysclk >> AHBPrescTable[RCC_ClkInitStruct.AHBCLKDivider >> RCC_CFGR_HPRE_Pos];
		#endif
		RCC_ClkInitStruct.APB1CLKDivider = calc_apb_div(ahb / apb1);
		RCC_ClkInitStruct.APB2CLKDivider = calc_apb_div(ahb / apb2);

		#if MICROPY_HW_CLK_LAST_FREQ
		// Save the bus dividers for use later
		uint32_t h = RCC_ClkInitStruct.AHBCLKDivider >> 4;
		uint32_t b1 = RCC_ClkInitStruct.APB1CLKDivider >> 10;
		uint32_t b2 = RCC_ClkInitStruct.APB2CLKDivider >> 10;
		#endif
		
		
		
		// Configure clock
		if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK) {
			return -MP_EIO;
		}

		#if defined(STM32F7)
		// Deselect PLLSAI as 48MHz source if we were using it
		RCC->DCKCFGR2 &= ~RCC_DCKCFGR2_CK48MSEL;
		// Turn PLLSAI off because we are changing PLLM (which drives PLLSAI)
		RCC->CR &= ~RCC_CR_PLLSAION;
		#endif

		// Re-configure PLL
		// Even if we don't use the PLL for the system clock, we still need it for USB, RNG and SDIO
		RCC_OscInitTypeDef RCC_OscInitStruct;
		RCC_OscInitStruct.OscillatorType = MICROPY_HW_RCC_OSCILLATOR_TYPE;
		RCC_OscInitStruct.HSEState = MICROPY_HW_RCC_HSE_STATE;
		RCC_OscInitStruct.HSIState = MICROPY_HW_RCC_HSI_STATE;
		RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
		RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
		RCC_OscInitStruct.PLL.PLLSource = MICROPY_HW_RCC_PLL_SRC;
		RCC_OscInitStruct.PLL.PLLM = m;
		RCC_OscInitStruct.PLL.PLLN = n;
		RCC_OscInitStruct.PLL.PLLP = p;
		RCC_OscInitStruct.PLL.PLLQ = q;
		if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
			return -MP_EIO;
		}

		// Set PLL as system clock source if wanted
		if (sysclk_source == RCC_SYSCLKSOURCE_PLLCLK) {
			RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_SYSCLK;
			int ret = powerctrl_rcc_clock_config_pll(&RCC_ClkInitStruct, sysclk_mhz, need_pllsai);
			if (ret != 0) {
				return ret;
			}
		}

		#if MICROPY_HW_CLK_LAST_FREQ
		// Save settings in RTC backup register to reconfigure clocks on hard-reset
		#if defined(STM32F7)
		#define FREQ_BKP BKP31R
		#else
		#define FREQ_BKP BKP19R
		#endif
		// qqqqqqqq pppppppp nnnnnnnn nnmmmmmm
		// qqqqQQQQ ppppppPP nNNNNNNN NNMMMMMM
		// 222111HH HHQQQQPP nNNNNNNN NNMMMMMM
		p = (p / 2) - 1;
		RTC->FREQ_BKP = m
			| (n << 6) | (p << 16) | (q << 18)
			| (h << 22)
			| (b1 << 26)
			| (b2 << 29);
		#endif
	
	
	#endif

    return 0;
}

#endif

void powerctrl_enter_stop_mode(void) {
    // Disable IRQs so that the IRQ that wakes the device from stop mode is not
    // executed until after the clocks are reconfigured
    uint32_t irq_state = disable_irq();

    #if defined(STM32L4)
    // Configure the MSI as the clock source after waking up
    __HAL_RCC_WAKEUPSTOP_CLK_CONFIG(RCC_STOP_WAKEUPCLOCK_MSI);
    #endif
	
	/*--------------------------------*/
	/*----Add to support stm32f1------*/
	/*--------------------------------*/
    #if !defined(STM32F0) && !defined(STM32L4) && !defined(STM32F1)
    // takes longer to wake but reduces stop current
    HAL_PWREx_EnableFlashPowerDown();
    #endif

    # if defined(STM32F7)
    HAL_PWR_EnterSTOPMode((PWR_CR1_LPDS | PWR_CR1_LPUDS | PWR_CR1_FPDS | PWR_CR1_UDEN), PWR_STOPENTRY_WFI);
	/*--------------------------------*/
	/*----Add to support stm32f1------*/
	/*--------------------------------*/
	# elif defined(STM32F1)
	HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
    # else
    HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
    #endif

    // reconfigure the system clock after waking up

    #if defined(STM32F0)

    // Enable HSI48
    __HAL_RCC_HSI48_ENABLE();
    while (!__HAL_RCC_GET_FLAG(RCC_FLAG_HSI48RDY)) {
    }

    // Select HSI48 as system clock source
    MODIFY_REG(RCC->CFGR, RCC_CFGR_SW, RCC_SYSCLKSOURCE_HSI48);
    while (__HAL_RCC_GET_SYSCLK_SOURCE() != RCC_CFGR_SWS_HSI48) {
    }
	
	/*--------------------------------*/
	/*----Add to support stm32f1------*/
	/*--------------------------------*/
	# elif defined(STM32F1)
	
	while(0);
    #else

    #if !defined(STM32L4)
    // enable clock
    __HAL_RCC_HSE_CONFIG(MICROPY_HW_RCC_HSE_STATE);
    #if MICROPY_HW_CLK_USE_HSI
    __HAL_RCC_HSI_ENABLE();
    #endif
    while (!__HAL_RCC_GET_FLAG(MICROPY_HW_RCC_FLAG_HSxRDY)) {
    }
    #endif

    // enable PLL
    __HAL_RCC_PLL_ENABLE();
    while (!__HAL_RCC_GET_FLAG(RCC_FLAG_PLLRDY)) {
    }

    // select PLL as system clock source
    MODIFY_REG(RCC->CFGR, RCC_CFGR_SW, RCC_SYSCLKSOURCE_PLLCLK);
    #if defined(STM32H7)
    while (__HAL_RCC_GET_SYSCLK_SOURCE() != RCC_CFGR_SWS_PLL1) {
    }
    #else
    while (__HAL_RCC_GET_SYSCLK_SOURCE() != RCC_CFGR_SWS_PLL) {
    }
    #endif

    #if defined(STM32F7)
    if (RCC->DCKCFGR2 & RCC_DCKCFGR2_CK48MSEL) {
        // Enable PLLSAI if it is selected as 48MHz source
        RCC->CR |= RCC_CR_PLLSAION;
        while (!(RCC->CR & RCC_CR_PLLSAIRDY)) {
        }
    }
    #endif

    #if defined(STM32L4)
    // Enable PLLSAI1 for peripherals that use it
    RCC->CR |= RCC_CR_PLLSAI1ON;
    while (!(RCC->CR & RCC_CR_PLLSAI1RDY)) {
    }
    #endif

    #endif

    // Enable IRQs now that all clocks are reconfigured
    enable_irq(irq_state);
}

void powerctrl_enter_standby_mode(void) {
    rtc_init_finalise();

    // We need to clear the PWR wake-up-flag before entering standby, since
    // the flag may have been set by a previous wake-up event.  Furthermore,
    // we need to disable the wake-up sources while clearing this flag, so
    // that if a source is active it does actually wake the device.
    // See section 5.3.7 of RM0090.

    // Note: we only support RTC ALRA, ALRB, WUT and TS.
    // TODO support TAMP and WKUP (PA0 external pin).
    #if defined(STM32F0)
    #define CR_BITS (RTC_CR_ALRAIE | RTC_CR_WUTIE | RTC_CR_TSIE)
    #define ISR_BITS (RTC_ISR_ALRAF | RTC_ISR_WUTF | RTC_ISR_TSF)
    #else
    #define CR_BITS (RTC_CR_ALRAIE | RTC_CR_ALRBIE | RTC_CR_WUTIE | RTC_CR_TSIE)
    #define ISR_BITS (RTC_ISR_ALRAF | RTC_ISR_ALRBF | RTC_ISR_WUTF | RTC_ISR_TSF)
    #endif
	/*--------------------------------*/
	/*----Add to support stm32f1------*/
	/*--------------------------------*/
	#if defined(STM32F1)
	while(0);
	
	#else
    // save RTC interrupts
    uint32_t save_irq_bits = RTC->CR & CR_BITS;

    // disable RTC interrupts
    RTC->CR &= ~CR_BITS;

    // clear RTC wake-up flags
    RTC->ISR &= ~ISR_BITS;

    #if defined(STM32F7)
    // disable wake-up flags
    PWR->CSR2 &= ~(PWR_CSR2_EWUP6 | PWR_CSR2_EWUP5 | PWR_CSR2_EWUP4 | PWR_CSR2_EWUP3 | PWR_CSR2_EWUP2 | PWR_CSR2_EWUP1);
    // clear global wake-up flag
    PWR->CR2 |= PWR_CR2_CWUPF6 | PWR_CR2_CWUPF5 | PWR_CR2_CWUPF4 | PWR_CR2_CWUPF3 | PWR_CR2_CWUPF2 | PWR_CR2_CWUPF1;
    #elif defined(STM32H7)
    // TODO
    #elif defined(STM32L4)
    // clear all wake-up flags
    PWR->SCR |= PWR_SCR_CWUF5 | PWR_SCR_CWUF4 | PWR_SCR_CWUF3 | PWR_SCR_CWUF2 | PWR_SCR_CWUF1;
    // TODO
    #else
    // clear global wake-up flag
    PWR->CR |= PWR_CR_CWUF;
    #endif

    // enable previously-enabled RTC interrupts
    RTC->CR |= save_irq_bits;

    #if defined(STM32F7)
    // Enable the internal (eg RTC) wakeup sources
    // See Errata 2.2.2 "Wakeup from Standby mode when the back-up SRAM regulator is enabled"
    PWR->CSR1 |= PWR_CSR1_EIWUP;
    #endif

    // enter standby mode
    HAL_PWR_EnterSTANDBYMode();
    // we never return; MCU is reset on exit from standby
	#endif
}
