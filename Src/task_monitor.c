

#include "task_monitor.h"
#include "command_processor.h"
#include "CarelessWSPR_commands.h"

#include <string.h>


//the task that runs an interactive monitor on the USB data
osThreadId g_thMonitor = NULL;
uint32_t g_tbMonitor[ 128 ];
osStaticThreadDef_t g_tcbMonitor;


const IOStreamIF* g_pMonitorIOIf = NULL;	//the IO device to which the monitor is attached



//====================================================
//Monitor task
//The monitor is a command processing interface attached to the USB CDC virtual
//serial port.  It processes incoming commands from the user.



//XXX might want to have these direct to whatever device based on config
void USBCDC_DataAvailable ( void )
{
	//YYY you could use this opportunity to signal an event
	//Note, this is called at ISR time
	if ( NULL != g_thMonitor )	//only if we have a notificand
	{
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		xTaskNotifyFromISR ( g_thMonitor, TNB_DAV, eSetBits, &xHigherPriorityTaskWoken );
		portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
	}
}


//XXX might want to have these direct to whatever device based on config
void USBCDC_TransmitEmpty ( void )
{
	//YYY you could use this opportunity to signal an event
	//Note, this is called at ISR time
	if ( NULL != g_thMonitor )	//only if we have a notificand
	{
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		xTaskNotifyFromISR ( g_thMonitor, TNB_TBMT, eSetBits, &xHigherPriorityTaskWoken );
		portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
	}
}


//implementation for the command processor; bind IO to the USB CDC port


void thrdfxnMonitorTask ( void const* argument )
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
			//if we got a new client connection, do a greeting
			if ( ulNotificationValue & TNB_CLIENT_CONNECT )
			{
				CWCMD_SendGreeting ( g_pMonitorIOIf );
				CWCMD_SendPrompt ( g_pMonitorIOIf );
			}
			if ( ulNotificationValue & TNB_DAV )
			{
				//we use the non-blocking version in this notification loop
				CMDPROC_process_nb ( g_pMonitorIOIf, g_aceCommands, g_nAceCommands );
			}
		}
	}
}



