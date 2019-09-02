//the task that handles WSPR transmission operations
//This is part of the CarelessWSPR project.

#ifndef __TASK_WSPR_H
#define __TASK_WSPR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "cmsis_os.h"
#include "task_notification_bits.h"

extern osThreadId g_thWSPR;
extern uint32_t g_tbWSPR[ 128 ];
extern osStaticThreadDef_t g_tcbWSPR;


//the WSPR message we transmit
extern uint8_t g_abyWSPR[162];


//called once at reset to get things ready
void WSPR_Initialize ( void );

//start and stop emitting WSPR messages
void WSPR_StartWSPR ( void );
void WSPR_StopWSPR ( void );

//if the WSPR message needs re-encoding (e.g. data changed), cause that to
//happen prior to transmission.
void WSPR_ReEncode ( void );


//for tuning of the synthesizer correction value, we can just emit a CW signal.
//this can let us beat against WWV, or even tweak it via WSJT-X waterfall.
void WSPR_StartReference ( uint32_t nRefFreq );
void WSPR_StopReference ( void );


//called when the timer resource expires (at ISR time)
void WSPR_Timer_Timeout ( void );
//called when the RTC Alarm resource fires (at ISR time)
void WSPR_RTC_Alarm ( void );

//these are for the benefit of the command processor to provide status info
int WSPR_isWSPRing ( void );			//is sending WSPR messages
int WSPR_isRefSignaling ( void );		//is emitting a reference signal
int WSPR_isTransmitting ( void );		//are we emitting signal right now?



void thrdfxnWSPRTask ( void const* argument );



#ifdef __cplusplus
}
#endif

#endif
