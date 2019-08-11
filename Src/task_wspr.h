//the task that handles WSPR transmission operations
//This is part of the CarelessWSPR project.

#ifndef __TASK_WSPR_H
#define __TASK_WSPR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "cmsis_os.h"

extern osThreadId g_thWSPR;
extern uint32_t g_tbWSPR[ 128 ];
extern osStaticThreadDef_t g_tcbWSPR;


//the WSPR message we transmit
extern uint8_t g_abyWSPR[162];


//called once at reset to get things ready
void WSPR_Initialize ( void );

//start and stop emitting WSPR messages; note, presumes g_abyWSPR has been set
//up with a valid message!
void WSPR_StartWSPR ( void );
void WSPR_StopWSPR ( void );


//called when the timer resource expires (at ISR time)
void WSPR_Timer_Timeout ( void );
//called when the RTC Alarm resource fires (at ISR time)
void WSPR_RTC_Alarm ( void );


void thrdfxnWSPRTask ( void const* argument );



#ifdef __cplusplus
}
#endif

#endif
