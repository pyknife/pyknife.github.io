//
// This file is part of the GNU ARM Eclipse distribution.
// Copyright (c) 2014 Liviu Ionescu.
//

// ----------------------------------------------------------------------------

#include <stdio.h>
#include "diag/Trace.h"

// ----------------------------------------------------------------------------
//
// STM32F1 empty sample (trace via ITM).
//
// Trace support is enabled by adding the TRACE macro definition.
// By default the trace messages are forwarded to the ITM output,
// but can be rerouted to any device or completely suppressed, by
// changing the definitions required in system/src/diag/trace_impl.c
// (currently OS_USE_TRACE_ITM, OS_USE_TRACE_SEMIHOSTING_DEBUG/_STDOUT).
//
/* STM32 includes */
#include <stm32f10x.h>
//#ifdef USE_STDPERIPH_DRIVER
#include <stm32f10x_conf.h>
//#endif



// ----- main() ---------------------------------------------------------------

#include <SSD1306.h>
#include <GFX.h>
#include <BlinkLed.h>
#include <vertices.h>


// Sample pragmas to cope with warnings. Please note the related line at
// the end of this function, used to pop the compiler diagnostics status.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#pragma GCC diagnostic ignored "-Wreturn-type"

//GPIO_InitTypeDef G;

volatile uint32_t MSec = 0;
volatile uint8_t Hour = 0, Min = 0, Sec = 0, iAng = 0;


void SysTick_Handler(void){
	MSec++;

	if((MSec%50)==49){
		iAng++;
		if(iAng == 180) iAng = 0;
	}

	if((MSec%1000)==999){
		Sec++;
		if(Sec%2){
			blink_led_on();
		}else{
			blink_led_off();
		}

		if(Sec==60){
			Sec = 0;
			Min++;
			if(Min==60){
				Hour++;
				Min = 0;
				if(Hour == 24){
					Hour = 0;
				}
			}
		}
	}
}


int
main(int argc, char* argv[])
{
  // At this stage the system clock should have already been configured
  // at high speed.

	SystemInit();

	SysTick_Config(SystemCoreClock/1000); //1ms timebase

	blink_led_init();

	SSD1306_InitSetup();
	LCDSleepMode(LCDWake);

	int Cnt, Cntv;
	uint8_t vert[8][3];

	uint8_t XPos = 0;
	uint8_t x1, x2, y1, y2, status;

	  		// Infinite loop
	      while(1)
	      {

	  		XPos = PStr("Wireframe cube test", 0, 0, 0, 0);

	    	XPos = PStr("Time: ", 0, 56, 0, 0);
	  		XPos = PNum(Hour, XPos, 56, 1, 0, 0);
	  		XPos = PChar(':', XPos, 56, 0, 0);
	  		XPos = PNum(Min, XPos, 56, 1, 0, 0);
	  		XPos = PChar(':', XPos, 56, 0, 0);
	  		XPos = PNum(Sec, XPos, 56, 1, 0, 0);


	  		/*
	  		XPos = PStr("Date: ", 0, 16, 0, 0);
	  		XPos = PNum(10, XPos, 16, 1, 0, 0);
	  		XPos = PChar('/', XPos, 16, 0, 0);
	  		XPos = PNum(11, XPos, 16, 1, 0, 0);
	  		XPos = PChar('/', XPos, 16, 0, 0);
	  		XPos = PNum(12, XPos, 16, 1, 0, 0);
			*/

	  		// Get vertices for the current angle
	  		for(Cntv = 0; Cntv < nvert; ++Cntv){
	  				vert[Cntv][0] = vertices[iAng*nvert + Cntv][0];
	  			  	vert[Cntv][1] = vertices[iAng*nvert + Cntv][1];
	  			  	vert[Cntv][2] = vertices[iAng*nvert + Cntv][2];
	  			  	}
	  		// Draw wireframe for each face of the cube
	  		for(Cnt = 0;Cnt < nfaces; ++Cnt){

	  			x1 = vert[faces[Cnt][0]][0];
	  			y1 = vert[faces[Cnt][0]][1];
	  			x2 = vert[faces[Cnt][1]][0];
	  			y2 = vert[faces[Cnt][1]][1];
	  			status = LineL(x1, y1, x2, y2, 0);

	  			x1 = vert[faces[Cnt][1]][0];
	  			y1 = vert[faces[Cnt][1]][1];
	  			x2 = vert[faces[Cnt][2]][0];
	  			y2 = vert[faces[Cnt][2]][1];
	  			status = LineL(x1, y1, x2, y2, 0);

	  			x1 = vert[faces[Cnt][2]][0];
	  			y1 = vert[faces[Cnt][2]][1];
	  			x2 = vert[faces[Cnt][3]][0];
	  			y2 = vert[faces[Cnt][3]][1];
	  			status = LineL(x1, y1, x2, y2, 0);

	  			x1 = vert[faces[Cnt][3]][0];
	  			y1 = vert[faces[Cnt][3]][1];
	  			x2 = vert[faces[Cnt][0]][0];
	  			y2 = vert[faces[Cnt][0]][1];
	  			status = LineL(x1, y1, x2, y2, 0);

	  		}


	  		PScrn();
	  		ClrBuf();

	      }
}

#pragma GCC diagnostic pop

// ----------------------------------------------------------------------------
