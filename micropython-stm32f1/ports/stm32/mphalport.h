// We use the ST Cube HAL library for most hardware peripherals
#include STM32_HAL_H
#include "pin.h"

extern const unsigned char mp_hal_status_to_errno_table[4];

NORETURN void mp_hal_raise(HAL_StatusTypeDef status);
void mp_hal_set_interrupt_char(int c); // -1 to disable

// timing functions

#include "irq.h"

#if __CORTEX_M == 0
// Don't have raise_irq_pri on Cortex-M0 so keep IRQs enabled to have SysTick timing
#define mp_hal_quiet_timing_enter() (1)
#define mp_hal_quiet_timing_exit(irq_state) (void)(irq_state)
#else
#define mp_hal_quiet_timing_enter() raise_irq_pri(1)
#define mp_hal_quiet_timing_exit(irq_state) restore_irq_pri(irq_state)
#endif
#define mp_hal_delay_us_fast(us) mp_hal_delay_us(us)

void mp_hal_ticks_cpu_enable(void);
static inline mp_uint_t mp_hal_ticks_cpu(void) {
    #if __CORTEX_M == 0
    return 0;
    #else
    if (!(DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk)) {
        mp_hal_ticks_cpu_enable();
    }
    return DWT->CYCCNT;
    #endif
}

// C-level pin HAL

#include "pin.h"

/*--------------------------------*/
/*----Add to support stm32f1------*/
/*--------------------------------*/
#if defined(STM32F1)
#define GPIO_AF1_TIM1          ((uint8_t)0x01)
#define GPIO_AF1_TIM8          ((uint8_t)0x01)
#define GPIO_AF2_TIM6          ((uint8_t)0x02)
#define GPIO_AF3_TIM7          ((uint8_t)0x02)
#define GPIO_AF3_TIM2          ((uint8_t)0x03)
#define GPIO_AF3_TIM3          ((uint8_t)0x03)
#define GPIO_AF3_TIM4          ((uint8_t)0x03)
#define GPIO_AF3_TIM5          ((uint8_t)0x03)
#define GPIO_AF4_I2C1          ((uint8_t)0x04)
#define GPIO_AF4_I2C2          ((uint8_t)0x04)
#define GPIO_AF5_SPI1          ((uint8_t)0x05)
#define GPIO_AF5_SPI2          ((uint8_t)0x05)
#define GPIO_AF5_SPI3          ((uint8_t)0x05)
#define GPIO_AF6_USART1        ((uint8_t)0x06)
#define GPIO_AF6_USART2        ((uint8_t)0x06)
#define GPIO_AF6_USART3        ((uint8_t)0x06)
#define GPIO_AF7_UART4         ((uint8_t)0x07)
#define GPIO_AF7_UART5         ((uint8_t)0x07)
#define GPIO_AF8_CAN1          ((uint8_t)0x08)
#endif







#define MP_HAL_PIN_FMT                  "%q"
#define MP_HAL_PIN_MODE_INPUT           (0)
#define MP_HAL_PIN_MODE_OUTPUT          (1)
#define MP_HAL_PIN_MODE_ALT             (2)
#define MP_HAL_PIN_MODE_ANALOG          (3)
#if defined(GPIO_ASCR_ASC0)
#define MP_HAL_PIN_MODE_ADC             (11)
#else
#define MP_HAL_PIN_MODE_ADC             (3)
#endif
#define MP_HAL_PIN_MODE_OPEN_DRAIN      (5)
#define MP_HAL_PIN_MODE_ALT_OPEN_DRAIN  (6)
#define MP_HAL_PIN_PULL_NONE            (GPIO_NOPULL)
#define MP_HAL_PIN_PULL_UP              (GPIO_PULLUP)
#define MP_HAL_PIN_PULL_DOWN            (GPIO_PULLDOWN)
#define MP_HAL_PIN_SPEED_LOW            (GPIO_SPEED_FREQ_LOW)
#define MP_HAL_PIN_SPEED_MEDIUM         (GPIO_SPEED_FREQ_MEDIUM)
#define MP_HAL_PIN_SPEED_HIGH           (GPIO_SPEED_FREQ_HIGH)
#define MP_HAL_PIN_SPEED_VERY_HIGH      (GPIO_SPEED_FREQ_VERY_HIGH) 


#define mp_hal_pin_obj_t const pin_obj_t*
#define mp_hal_get_pin_obj(o)   pin_find(o)
#define mp_hal_pin_name(p)      ((p)->name)
#define mp_hal_pin_input(p)     mp_hal_pin_config((p), MP_HAL_PIN_MODE_INPUT, MP_HAL_PIN_PULL_NONE, 0)
#define mp_hal_pin_output(p)    mp_hal_pin_config((p), MP_HAL_PIN_MODE_OUTPUT, MP_HAL_PIN_PULL_NONE, 0)
#define mp_hal_pin_open_drain(p) mp_hal_pin_config((p), MP_HAL_PIN_MODE_OPEN_DRAIN, MP_HAL_PIN_PULL_NONE, 0)
#if defined(STM32H7)
#define mp_hal_pin_high(p)      (((p)->gpio->BSRRL) = (p)->pin_mask)
#define mp_hal_pin_low(p)       (((p)->gpio->BSRRH) = (p)->pin_mask)
#else
#define mp_hal_pin_high(p)      (((p)->gpio->BSRR) = (p)->pin_mask)
#define mp_hal_pin_low(p)       (((p)->gpio->BSRR) = ((p)->pin_mask << 16))
#endif
#define mp_hal_pin_od_low(p)    mp_hal_pin_low(p)
#define mp_hal_pin_od_high(p)   mp_hal_pin_high(p)
#define mp_hal_pin_read(p)      (((p)->gpio->IDR >> (p)->pin) & 1)
#define mp_hal_pin_write(p, v)  do { if (v) { mp_hal_pin_high(p); } else { mp_hal_pin_low(p); } } while (0)

void mp_hal_gpio_clock_enable(GPIO_TypeDef *gpio);

/*--------------------------------*/
/*----Add to support stm32f1------*/
/*--------------------------------*/
#if defined(STM32F1)
void mp_hal_pin_config(mp_hal_pin_obj_t pin_obj, uint32_t mode, uint32_t pull, const pin_af_obj_t *af);
#else
void mp_hal_pin_config(mp_hal_pin_obj_t pin, uint32_t mode, uint32_t pull, uint32_t alt);
#endif

bool mp_hal_pin_config_alt(mp_hal_pin_obj_t pin, uint32_t mode, uint32_t pull, uint8_t fn, uint8_t unit);
void mp_hal_pin_config_speed(mp_hal_pin_obj_t pin_obj, uint32_t speed);

enum {
    MP_HAL_MAC_WLAN0 = 0,
    MP_HAL_MAC_WLAN1,
    MP_HAL_MAC_BDADDR,
    MP_HAL_MAC_ETH0,
};

void mp_hal_get_mac(int idx, uint8_t buf[6]);