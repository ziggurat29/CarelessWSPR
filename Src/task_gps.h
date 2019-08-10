//the task that handles incoming GPS NMEA data
//This is part of the BluepillSi5351Exp001 project.
//Note:  no effort has been made to make this a general-purpose NMEA processor;
//in particular we only parse the few messages in which we are interested, and
//moreover for the GPS device we are particularly using.  If you use a
//different GPS module, you'll probably want to review and possibly change the
//parsing done in GPS_process() to handle any different messages for your unit.

#ifndef __TASK_GPS_H
#define __TASK_GPS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "cmsis_os.h"
#include "system_interfaces.h"

extern osThreadId g_thGPS;
extern uint32_t g_tbGPS[ 128 ];
extern osStaticThreadDef_t g_tcbGPS;

extern const IOStreamIF* g_pGPSIOIf;	//the IO device to which the monitor is attached

extern volatile int g_bLock;		//if we have a GPS lock

extern volatile int g_nGPSHour;		//UTC time from satellites
extern volatile int g_nGPSMinute;
extern volatile int g_nGPSSecond;
extern volatile int g_nGPSMonth;
extern volatile int g_nGPSDay;
extern volatile int g_nGPSYear;
extern volatile int g_nGPSTzHour;
extern volatile int g_nGPSTzMinute;

extern volatile float g_fLat;	//+ is N, - is S
extern volatile float g_fLon;	//+ is E, - is W


void thrdfxnGPSTask ( void const* argument );




#ifdef __cplusplus
}
#endif

#endif
