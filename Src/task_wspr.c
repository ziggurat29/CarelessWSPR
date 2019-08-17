//the task that handles WSPR transmission operations
//This is part of the CarelessWSPR project.


#include "task_wspr.h"
#include "stm32f1xx_hal.h"
#include "cmsis_os.h"

#include "CarelessWSPR_settings.h"
#include "task_notification_bits.h"
#include "lamps.h"
#include "wspr.h"
#include "maidenhead.h"

#include "task_gps.h"	//for global status

#include <stdlib.h>


#ifndef COUNTOF
#define COUNTOF(arr) (sizeof(arr)/sizeof(arr[0]))
#endif


//the task that runs an interactive monitor on the USB data
osThreadId g_thWSPR = NULL;
uint32_t g_tbWSPR[ 128 ];
osStaticThreadDef_t g_tcbWSPR;

//the WSPR message we transmit
uint8_t g_abyWSPR[162];
//state machine
int g_bDoWSPR = 0;		//boolean; should we be WSPR'ing sweet nothings at all
int g_nSymbolIndex;		//which of g_abyWSPR are we on
uint32_t g_nBaseFreq;	//this base frequency of this sub-band; Hz



//====================================================
//WSPR task


//==============================================================
//timer-related things.  These inlines are here to make it a little
//easier to change the timer resource later, if I ever need to, by
//having all the references consolidated in one area of the code.

extern TIM_HandleTypeDef htim4;	//in main.c


inline static void StartBitClock(void)
{
	htim4.Instance->CNT = 0;	//clear counter to 'now' for start
	htim4.Instance->PSC = 4095;
	htim4.Instance->ARR = 11999;
	__HAL_TIM_CLEAR_FLAG ( &htim4, TIM_FLAG_UPDATE );	//clear old interrupts
	HAL_TIM_Base_Start_IT(&htim4);	//go
}


inline static void StopBitClock ( void )
{
	HAL_TIM_Base_Stop_IT(&htim4);
}


extern RTC_HandleTypeDef hrtc;	//in main.c



//cancel any scheduled future WSPR transmissions
static void _impl_WSPR_CancelSchedule ( void )
{
	HAL_StatusTypeDef ret = HAL_RTC_DeactivateAlarm ( &hrtc, RTC_ALARM_A );
	(void)ret;
}


//schedule a WSPR transmission at the next interval
static void _impl_WSPR_ScheduleNext ( void )
{
	_impl_WSPR_CancelSchedule();	//ensure any alarms are off

	//get current time
	RTC_TimeTypeDef sTime;
	HAL_RTC_GetTime ( &hrtc, &sTime, RTC_FORMAT_BIN );

	//round up to next even minute start
	RTC_AlarmTypeDef sAlarm;
	sAlarm.Alarm = RTC_ALARM_A;
	sAlarm.AlarmTime = sTime;
	sAlarm.AlarmTime.Seconds = 0;	//always at start of minute
	sAlarm.AlarmTime.Minutes = (sAlarm.AlarmTime.Minutes + 2) & 0xfe;
	if ( sAlarm.AlarmTime.Minutes > 59 )	//check for rollover minute
	{
		sAlarm.AlarmTime.Minutes -= 60;
		++sAlarm.AlarmTime.Hours;
	}
	if ( sAlarm.AlarmTime.Hours > 23 )	//check for rollover hour
	{
		sAlarm.AlarmTime.Hours -= 24;
	}

	//set the alarm
	HAL_StatusTypeDef ret = HAL_RTC_SetAlarm_IT ( &hrtc, &sAlarm, RTC_FORMAT_BIN );
	(void)ret;
}


//our bit clock timed out; time to shift a new bit
void WSPR_Timer_Timeout ( void )
{
	//we are at ISR time, so we avoid doing work here
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	xTaskNotifyFromISR ( g_thWSPR, TNB_WSPRNEXTBIT, eSetBits, &xHigherPriorityTaskWoken );
	portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}


//our WSPR scheduled transmission should now begin
void WSPR_RTC_Alarm ( void )
{
	//we are at ISR time, so we avoid doing work here
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	xTaskNotifyFromISR ( g_thWSPR, TNB_WSPRSTART, eSetBits, &xHigherPriorityTaskWoken );
	portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}



void WSPR_Initialize ( void )
{
//XXX extinguish signal; if any
	StopBitClock();	//can be running at app startup
	_impl_WSPR_CancelSchedule();	//unlikely at app startup, but ensure
	g_bDoWSPR = 0;
}


void WSPR_StartWSPR ( void )
{
	if ( ! g_bDoWSPR )	//not if we're already doing it
	{
		g_bDoWSPR = 1;
		_impl_WSPR_ScheduleNext();
	}
}


void WSPR_StopWSPR ( void )
{
	_impl_WSPR_CancelSchedule();
	g_bDoWSPR = 0;
}



//implementation for the WSPR task
void thrdfxnWSPRTask ( void const* argument )
{
	uint32_t msWait = 1000;
	for(;;)
	{
		//wait on various task notifications
		uint32_t ulNotificationValue;
		BaseType_t xResult = xTaskNotifyWait( pdFALSE,	//Don't clear bits on entry.
				0xffffffff,	//Clear all bits on exit.
				&ulNotificationValue,	//Stores the notified value.
				pdMS_TO_TICKS(msWait) );
		if( xResult == pdPASS )
		{

			//if we gained or lost GPS lock, do some things
			if ( ulNotificationValue & TNB_WSPR_GPSLOCK )
			{
				PersistentSettings* psettings = Settings_getStruct();
				if ( g_bLock )	//got a lock
				{
					//first, update the RTC time
					RTC_TimeTypeDef sTime;
					RTC_DateTypeDef sDate;
					sTime.Hours = g_nGPSHour;
					sTime.Minutes = g_nGPSMinute;
					sTime.Seconds = g_nGPSSecond;
					sDate.WeekDay = RTC_WEEKDAY_SUNDAY;	//(arbitrary)
					sDate.Date = g_nGPSDay;
					sDate.Month = g_nGPSMonth;
					sDate.Year = g_nGPSYear - 2000;
					HAL_RTC_SetTime ( &hrtc, &sTime, RTC_FORMAT_BIN );
					HAL_RTC_SetDate ( &hrtc, &sDate, RTC_FORMAT_BIN );

					if ( psettings->_bUseGPS )	//do we care about GPS?
					{
						//now, update the maidenhead
						toMaidenhead ( g_fLat, g_fLon, psettings->_achMaidenhead, 4 );

						//now, update our WSPR message
						if ( wspr_encode ( g_abyWSPR, psettings->_achCallSign, 
								psettings->_achMaidenhead, psettings->_nTxPowerDbm ) )
						{
							//now start WSPRing
							WSPR_StartWSPR();
						}
						else
						{
							//XXX horror
						}
					}
				}
				else	//lost lock
				{
					//XXX we can carry on with our previous lock and RTC
					//WSPR_StopWSPR();
				}
			}

			//if our scheduled WSPR start has occurred, start
			if ( ulNotificationValue & TNB_WSPRSTART )
			{
				if ( g_bDoWSPR )	//should be wspr'ing at all?
				{
					//randomize duty cycle selection
					int chance = rand() / (RAND_MAX/100);
					PersistentSettings* psettings = Settings_getStruct();
					if ( chance < psettings->_nDutyPct )
					{
						//start transmission
						StartBitClock();	//get bit clock cranked up
						//compute the base frequency.  first determine the
						//6 Hz sub-band within the 200 Hz window.
						int nIdxSubBand;
						if ( psettings->_nSubBand < 0 )	//random sub-band, 0 - 32
						{
							nIdxSubBand = rand() / (RAND_MAX/32);
						}
						else	//explicitly chosen sub-band
						{
							nIdxSubBand = psettings->_nSubBand;
						}
						//now compute the sub-band base frequency
						// fDial + 1.5 KHz - 100 Hz + nIdxSubBand * 6Hz
						//Note; adjusted 100 to 99 because there is 1/3 sub-
						//band extra in the 200 Hz, and this spreads that
						//evenly at the top and bottom.  Probably unneeded.
						g_nBaseFreq = psettings->_dialFreqHz + 1500 - 99 +
								nIdxSubBand * 6;
						//emit this first symbol's tone now
						//note, the frequency is in centihertz so we can get
						//the sub-Hertz precision we need
						g_nSymbolIndex = 0;
						uint64_t nToneCentiHz = g_nBaseFreq * 100ULL + 
								g_abyWSPR[g_nSymbolIndex] * 146ULL;
//XXX emit signal
_ledOnGn();
						g_nSymbolIndex = 1;	//prepare for next symbol
					}
					//irrespective of whether we are actually transmitting this
					//period, we should schedule to check in the next one.
					_impl_WSPR_ScheduleNext();
				}
			}

			//if it is time to shift out the next WSPR symbol, do so
			if ( ulNotificationValue & TNB_WSPRNEXTBIT )
			{
				if ( g_nSymbolIndex >= 162 )	//done; turn off
				{
_ledOffGn();
//XXX terminate signal
					StopBitClock();	//stop shifting bits
					g_nSymbolIndex = 0;	//setup to start at beginning next time
				}
				else
				{
					//emit this next symbol's tone now
					//note, the frequency is in centihertz so we can get the
					//sub-Hertz precision we need
					uint64_t nToneCentiHz = g_nBaseFreq * 100ULL + 
							g_abyWSPR[g_nSymbolIndex] * 146ULL;
//XXX emit signal
_ledToggleGn();
					++g_nSymbolIndex;
				}
			}
		}
		else	//timeout on wait
		{
			//things to do on periodic idle timeout
		}
	}
}


