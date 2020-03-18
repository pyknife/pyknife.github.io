/*
 * Project: N|Watch
 * Author: Zak Kemble, contact@zakkemble.co.uk
 * Copyright: (C) 2013 by Zak Kemble
 * License: GNU GPL v3 (see License.txt)
 * Web: http://blog.zakkemble.co.uk/diy-digital-wristwatch/
 */

// Deals with sleeping and waking up
#include "common.h"

#ifdef __AVR_ATmega32U4__
#undef  sleep_bod_disable
#define sleep_bod_disable()
// No peripherals are in use other than Timer0, Timer3, usart, and usb
#define IS_PWR_SAVE_READY() false
#else
// No peripherals are in use other than Timer2
#define PRR_BITS (_BV(PRTWI)|_BV(PRTIM0)|_BV(PRTIM1)|_BV(PRSPI)|_BV(PRUSART0)|_BV(PRADC))
#define IS_PWR_SAVE_READY() (((PRR & ~PRR_BITS) == PRR_BITS))
#endif

#define BATTERY_CUTOFF	3320

typedef enum
{
	SYS_AWAKE,
	SYS_CRTANIM,
	SYS_SLEEP
} sys_t;

typedef enum
{
	USER_ACTIVE,
	USER_INACTIVE
} user_t;

static pwr_state_t active[PWR_ACTIVE_COUNT];
static sys_t systemState;
static user_t userState;

static void batteryCutoff(void);
static void userWake(void);
static void userSleep(void);

#ifdef __AVR_ATmega32U4__
extern void usb_init();

static bool usb_connected()
{
  bool rv;
  uint8_t pr = PRR1 & _BV(PRUSB);
  if (pr)
    power_usb_enable();
  rv = USB_CONNECTED();
  if (pr)
    power_usb_disable();
  return rv;
}

#undef  USB_CONNECTED
#define USB_CONNECTED() usb_connected()

static void usb_power(bool on)
{
  uint8_t pr = PRR1 & _BV(PRUSB);
  if (on)
  {
    if (pr)
    {
      power_usb_enable();
      usb_init();
    }
  }
  else
  {
    if (!pr)
    {
      USBCON &= ~_BV(USBE);  // Disable the USB  
      PLLCSR &= ~_BV(PLLE);  // Disable the USB Clock (PLL)
      power_usb_disable();
    }
  }
}
#endif

void pwrmgr_init()
{
	systemState = SYS_AWAKE;
	userState = USER_ACTIVE;
	set_sleep_mode(SLEEP_MODE_IDLE);
}

static void batteryCutoff()
{
	// If the battery voltage goes below a threshold then disable
	// all wakeup sources apart from USB plug-in and go to power down sleep.
	// This helps protect the battery from undervoltage and since the battery's own PCM hasn't kicked in yet the RTC will continue working.

	uint voltage = battery_voltage();
	if(voltage < BATTERY_CUTOFF && !USB_CONNECTED() && voltage)
	{
		// Turn everything off
		buttons_shutdown();
		tune_stop(PRIO_MAX);
		led_stop();
		oled_power(OLED_PWR_OFF);
		time_shutdown();
		
#ifdef __AVR_ATmega32U4__
    usb_power(false);
#endif

		// Stay in this loop until USB is plugged in or the battery voltage is above the threshold
		do
		{
			set_sleep_mode(SLEEP_MODE_PWR_DOWN);

			cli();
			sleep_enable();
			sleep_bod_disable();
			sei();
			sleep_cpu();
      sleep_disable();
			
			// Get battery voltage
			battery_updateNow();

		} while(!USB_CONNECTED() && battery_voltage() < BATTERY_CUTOFF);

		// Wake up
		time_wake();
		buttons_startup();
		buttons_wake();
		oled_power(OLED_PWR_ON);
	}
}

void pwrmgr_update()
{
	batteryCutoff();
	
  bool idle = (UDADDR != 0); // if USB Configuard, no sleep
	LOOPR(PWR_ACTIVE_COUNT, i)
  {
 		if(active[i] == PWR_STATE_BUSY) // Something busy, no sleep stuff
	 		return;
	  else if(active[i] == PWR_STATE_IDLE)
		  idle = true;
	}

	bool buttonsActive = buttons_isActive();

	if(idle || buttonsActive)
	{
#if COMPILE_ANIMATIONS
		if(systemState == SYS_CRTANIM && buttonsActive) // Cancel CRT anim if a button is pressed
		{
			display_startCRTAnim(CRTANIM_OPEN);
			systemState = SYS_AWAKE;
		}
		else // Idle sleep mode
#endif
		{
      if(IS_PWR_SAVE_READY())
				set_sleep_mode(SLEEP_MODE_PWR_SAVE); // Also disable BOD?
			else
				set_sleep_mode(SLEEP_MODE_IDLE);

			debugPin_sleepIdle(HIGH);
			sleep_mode();
			debugPin_sleepIdle(LOW);
		}
	}
	else
	{
#if COMPILE_ANIMATIONS
		if(systemState == SYS_AWAKE) // Begin CRT anim
		{
			systemState = SYS_CRTANIM;
			display_startCRTAnim(CRTANIM_CLOSE);
		}
		else if(systemState == SYS_CRTANIM)
#endif
		{
			// Shutdown
#ifdef __AVR_ATmega32U4__
      usb_power(false);
#endif

			if(userState == USER_ACTIVE)
				userSleep();

			systemState = SYS_SLEEP;
			set_sleep_mode(SLEEP_MODE_PWR_DOWN);

			time_sleep();

      debugPin_sleepPowerDown(HIGH);

			// need to make sure no interrupts fired here!
			cli();
			sleep_enable();
			sleep_bod_disable();
      sei();
			sleep_cpu();
      sleep_disable();
      debugPin_sleepPowerDown(LOW);

			systemState = SYS_AWAKE;

			if(time_wake() != RTCWAKE_SYSTEM) // Woken by button press, USB plugged in or by RTC user alarm
				userWake();

			//set_sleep_mode(SLEEP_MODE_IDLE);

		}
	}

#ifdef __AVR_ATmega32U4__
  // USB Power Control
  usb_power(USB_CONNECTED());
#endif
}

void pwrmgr_setState(pwr_active_t thing, pwr_state_t state)
{
	active[thing] = state;
}

bool pwrmgr_userActive()
{
	return userState == USER_ACTIVE;
}

static void userWake()
{
	userState = USER_ACTIVE;
	buttons_wake();
	display_startCRTAnim(CRTANIM_OPEN);
	oled_power(OLED_PWR_ON);
	battery_setUpdate(3);
}

static void userSleep()
{
	userState = USER_INACTIVE;
	oled_power(OLED_PWR_OFF);
}
