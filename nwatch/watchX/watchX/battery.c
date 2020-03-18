/*
 * Project: N|Watch
 * Author: Zak Kemble, contact@zakkemble.co.uk
 * Copyright: (C) 2013 by Zak Kemble
 * License: GNU GPL v3 (see License.txt)
 * Web: http://blog.zakkemble.co.uk/diy-digital-wristwatch/
 */

// Battery
// Battery is connected to a P-MOSFET then to a voltage divider. This is so we don't have a load of current being wasted through the voltage divider all the time.
// Polling based, interrupts not needed here since a conversion only takes ~500us and is only done every-so-often

#include "common.h"

#ifdef __AVR_ATmega32U4__
#define ADC_CHANNEL		0x21 // ADC9
#define DIVIDER				D4
#else
#define ADC_CHANNEL		0x06 // ADC6
#define DIVIDER				C0
#endif

#define enable_divider()	(pinWrite(DIVIDER, HIGH))
#define disable_divider()	(pinWrite(DIVIDER, LOW))

#define MAX_VOLTAGE	4200 // Max battery voltage
#define MAX_VOLTAGE_DIAG 4200

#ifdef __AVR_ATmega32U4__
#define R1      10000 // R1 resistance
#define R2      10000 // R2 resistance
#define VREF    2560  // Reference voltage (2.56V Internal ref)
#elif HW_VERSION == 1
#define R1			7500 // R1 resistance
#define R2			2700 // R2 resistance
#define VREF		1100 // Reference voltage (1.1V Internal ref)
#else
#define R1			10000 // R1 resistance
#define R2			10000 // R2 resistance
#define VREF		2500  // Reference voltage (VCC)
#endif

#define MAX_ADCVAL	((uint16_t)((((R2 / (float)(R1 + R2)) * MAX_VOLTAGE) / VREF) * ADC_MAX))

/*
#if MAX_ADCVAL > ADC_MAX
//	#undef MAX_ADCVAL
//	#define MAX_ADCVAL ADCMAX
	#warning "MAX_ADCVAL > ADCMAX"
#endif
*/

// Resolution = MAX_VOLTAGE / MAX_ADCVAL
// Resolution = 4.88mV

static uint voltage;
static byte lastSecs;
static byte changeCount;

void battery_init()
{
	// MOSFET pin
	pinMode(DIVIDER, OUTPUT);
	disable_divider();
}

// Set next update to happen in a x seconds
void battery_setUpdate(byte secs)
{
	changeCount = secs;
	lastSecs = timeDate.time.secs;
}

void battery_update()
{
	// See if seconds has changed
	if(lastSecs == timeDate.time.secs)
		return;
	lastSecs = timeDate.time.secs;

	if(changeCount)
	{
		changeCount--;
		return;
	}

	// Next update in 5 seconds
	battery_setUpdate(3);

	battery_updateNow();
}

// Update voltage
void battery_updateNow()
{
	// Enable P-MOSFET
	enable_divider();

	// Wait a bit for things to turn on
	delay_us(200);

	// Get ADC value
	uint adc = adc_read(ADC_CHANNEL);

	// Turn off MOSFET
	disable_divider();

	// Convert ADC value to voltage
	voltage = ((ulong)adc * MAX_VOLTAGE) / MAX_ADCVAL;
}

// Get voltage
uint battery_voltage()
{
	return voltage;
}
