//the task that runs an interactive monitor on the USB data

#ifndef __TASK_MONITOR_H
#define __TASK_MONITOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "cmsis_os.h"
#include "system_interfaces.h"

extern osThreadId g_thMonitor;
extern uint32_t g_tbMonitor[ 128 ];
extern osStaticThreadDef_t g_tcbMonitor;

extern const IOStreamIF* g_pMonitorIOIf;	//the IO device to which the monitor is attached

void thrdfxnMonitorTask ( void const* argument );

#ifdef __cplusplus
}
#endif

#endif
