//==============================================================
//This declares stuff related to indicator lamps.
//This module is part of the CarelessWSPR project.

#include "lamps.h"
#include "main.h"
#include "stm32f1xx_hal.h"
#include "cmsis_os.h"

#include "task_notification_bits.h"


extern osThreadId defaultTaskHandle;	//in main.c





//========================================================================
//indicator lamp functions
//simple on/off methods, and also state data to independently maintain timed
//'on' status for each lamp

//various lights
void _ledOnGn ( void ) { HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET); }

void _ledOffGn ( void ) { HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET); }

void _ledToggleGn ( void ) { HAL_GPIO_TogglePin (LED2_GPIO_Port, LED2_Pin); }


//diagnostic LED start times and durations
struct LED_LightTime
{
	uint32_t _nStart;
	uint32_t _nDur;
};

LED_LightTime g_lltGn = {0};



//turn a light on for a while
//this is used wherever it is desired to turn on a lamp for a period of time.
void LightLamp ( uint32_t durMS, LED_LightTime* pllt, void(*pfnOn)(void) )
{
	pllt->_nDur = durMS;
	pllt->_nStart = HAL_GetTick();
	pfnOn();

	//
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	xTaskNotifyFromISR ( defaultTaskHandle, TNB_LIGHTSCHANGED, eSetBits, &xHigherPriorityTaskWoken );
	portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}


//turn a light off if it is time, and update remaining time otherwise
//this is used in the task that turns off lamps after a period of time
void ProcessLightOffTime ( uint32_t now, uint32_t* pnRemMin, LED_LightTime* pllt, void(*pfnOff)(void) )
{
	if ( 0 != pllt->_nStart )
	{
		uint32_t dur = now - pllt->_nStart;
		if ( dur > pllt->_nDur )
		{
			pfnOff();
			pllt->_nStart = 0;
		}
		else
		{
			uint32_t rem = pllt->_nDur - dur;
			if ( rem < *pnRemMin )
				*pnRemMin = rem;
		}
	}
}



