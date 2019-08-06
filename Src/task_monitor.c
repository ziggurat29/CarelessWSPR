

#include "task_monitor.h"
#include "task_notification_bits.h"
#include "command_processor.h"
#include "CarelessWSPR_commands.h"


//the task that runs an interactive monitor on the USB data
osThreadId g_thMonitor = NULL;
uint32_t g_tbMonitor[ 256 ];
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
	for(;;)
	{
		while ( CMDPROC_QUIT != CMDPROC_process ( g_pMonitorIOIf, g_aceCommands, g_nAceCommands ) )
		{
			//XXX
		}
	}
}



